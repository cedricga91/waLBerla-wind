#pragma once

#ifndef TURBINECORE_WINDFARM_H
#define TURBINECORE_WINDFARM_H

#include <vector>

#include "core/mpi/MPIManager.h"

#include <domain_decomposition/IBlock.h>
#include "AirfoilCache.h"

#include "domain/TurbineDomain.h"

#include "force_model/TipLossModel.h"
#include "wind_turbine_core/ProjectDefines.h"

#include "TurbineCreator.h"
#include "mpi/SynchronisationMode.h"

namespace turbine_core {

    namespace farm {

        template<typename Topology_T, template<typename,typename,typename,typename,typename> class Creator_T,
        typename DensityField_T, typename VelocityField_T, typename ForceField_T,
        typename Interpolator_T, typename ForceDistributor_T, typename TipLossModel_T = force_model::NoTipLoss,
        typename = std::enable_if<std::is_base_of<creator::TurbineCreator<Topology_T>, Creator_T<Topology_T,ForceField_T,Interpolator_T,ForceDistributor_T,TipLossModel_T>>::value>>
        class WindFarm {

        public:

            using Interpolator = Interpolator_T;
            using Distributor = ForceDistributor_T;

            //NOTE pass all arguments to creator
            // this gets rid of the necessity to pass them in create
            // -> uniform signature for create function (cannot template that one as it calls virtual doCreate)
            template<typename... Args>
            HOST_PREFIX explicit WindFarm(const std::string & outputDirectory, Args&&... args)
            : creator_(std::make_unique<Creator_T<Topology_T,ForceField_T,Interpolator_T,ForceDistributor_T,TipLossModel_T>>(outputDirectory, &descriptionCache_, std::forward<Args>(args)...)),
              output_(!outputDirectory.empty()), outputDirectory_(outputDirectory)
            {

            }

            HOST_PREFIX ~WindFarm() {
                //TODO not nice as delete should be called by the same class that calls setup...
                for(auto & turbine : turbines_) {
                    if (turbine)
                        turbine->deleteEnvironment();
                }
            }

            HOST_PREFIX void createGeometry() {
                creator_->createGeometry(turbines_);
            }

            HOST_PREFIX void initialiseForceAndControlModel() {
                creator_->initialiseForceAndControlModel(turbines_, turbineAABBs_, domain_, isControlled_,
                                                         nPointsComponents_, nComponents_, nPointsComponentsForSpread_);

                // creator not needed anymore -> can be destroyed
                creator_.reset(nullptr);
            }

            HOST_PREFIX void setBlockForest( const std::shared_ptr<walberla::StructuredBlockForest> & sbf ) {
                forest_ = sbf;
                domain_ = std::make_shared<domain::TurbineDomain>(forest_->getBlockForestPointer(), turbineAABBs_);
            }

            //! Drop the turbines' cached communication setup. Must be called (together with
            //! setBlockForest) after the domain decomposition changed, e.g. after a blockforest
            //! refresh during adaptive mesh refinement.
            HOST_PREFIX void resetTurbineCommunication() {
                for(auto & turbine : turbines_) {
                    if (turbine)
                        turbine->resetCommunication();
                }
            }

            HOST_PREFIX void setFieldIDs( walberla::BlockDataID & densityField, walberla::BlockDataID & velocityField,
                                          walberla::BlockDataID & forceFieldID) {
                densityFieldID_ = densityField;
                velocityFieldID_ = velocityField;
                forceFieldID_ = forceFieldID;
            }

            HOST_PREFIX void setTimeloop( walberla::timeloop::ITimeloop * timeloop ) {
                timeloop_ = timeloop;
            }

            HOST_PREFIX void applyControl() {

                const uint_t timestep = timeloop_ ? timeloop_->getCurrentTimeStep() : 0;
                for( uint_t i = 0; i < turbines_.size(); ++i ) {

                    if (!turbines_[i])
                        continue;

                    if(!isControlled_[i])
                        continue;

                    auto & turbine = turbines_[i];

                    bool needsMeanVelocity{false}, needsTorque{false}, needsWindVane{false};
                    if(turbine)
                        turbine->getControlNeeds(needsMeanVelocity, needsTorque, needsWindVane);

                    WALBERLA_MPI_SECTION() {
                        walberla::mpi::allReduceInplace(needsMeanVelocity, walberla::mpi::LOGICAL_OR);
                        walberla::mpi::allReduceInplace(needsTorque, walberla::mpi::LOGICAL_OR);
                        walberla::mpi::allReduceInplace(needsWindVane, walberla::mpi::LOGICAL_OR);
                    }

                    real_t rotorMeanWind{0.};
                    uint_t totalPoints{};
                    if (turbine && needsMeanVelocity){
                        // Compute the rotor mean velocity
                        for (auto &block: *forest_) {
                            real_t blockLocalMeanVelocity{};
                            uint_t blockNbPoints{};
                            turbine->calculateMeanVelocity(&block, forest_, blockLocalMeanVelocity, blockNbPoints);
                            rotorMeanWind += blockLocalMeanVelocity;
                            totalPoints += blockNbPoints;
                        }

                        rotorMeanWind /= real_t(totalPoints);
                    }

                    WALBERLA_MPI_SECTION() {
                        walberla::mpi::allReduceInplace(totalPoints, walberla::mpi::SUM);
                        walberla::mpi::allReduceInplace(rotorMeanWind, walberla::mpi::SUM);
                    }

                    real_t uxWindVane = 0;
                    real_t uyWindVane = 0;
                    real_t uzWindVane = 0;
                    totalPoints = 0;
                    if (turbine && needsWindVane){
                        // Compute the rotor wind vane velocity
                        for (auto &block: *forest_) {
                            Vector3<real_t> blockLocalWindVane(0,0,0);
                            uint_t blockNbPoints{};
                            turbine->calculateWindVaneVelocity(&block, forest_, blockLocalWindVane, blockNbPoints);
                            uxWindVane += blockLocalWindVane[0];
                            uyWindVane += blockLocalWindVane[1];
                            uzWindVane += blockLocalWindVane[2];
                            totalPoints += blockNbPoints;
                        }
                    }

                    WALBERLA_MPI_SECTION() {
                        walberla::mpi::allReduceInplace(totalPoints, walberla::mpi::SUM);
                        walberla::mpi::allReduceInplace(uxWindVane, walberla::mpi::SUM);
                        walberla::mpi::allReduceInplace(uyWindVane, walberla::mpi::SUM);
                        walberla::mpi::allReduceInplace(uzWindVane, walberla::mpi::SUM);
                    }
                    Vector3<real_t> rotorWindVaneVelocity(uxWindVane /= real_t(totalPoints),
                        uyWindVane /= real_t(totalPoints), uzWindVane /= real_t(totalPoints));

                    real_t rotorTorque{0.}, rotorOmega{0.};
                    if (turbine && needsTorque){
                        // Compute the rotor power and thrust
                        for (auto &block: *forest_) {
                            real_t blockLocalPower{}, blockLocalThrust{};
                            real_t blockLocalTorque{};
                            turbines_[i]->calculatePowerAndThrust(&block, forest_, blockLocalPower, blockLocalThrust);
                            turbines_[i]->calculateTorque(&block, forest_, blockLocalTorque);
                            rotorTorque += blockLocalTorque;
                        }
                        turbine->getOmega(rotorOmega);
                    }

                    WALBERLA_MPI_SECTION() {
                        walberla::mpi::allReduceInplace(rotorTorque, walberla::mpi::SUM);
                    }

                    if(turbine) turbine->applyControl(timestep, rotorMeanWind, rotorTorque, rotorOmega, rotorWindVaneVelocity);
                }
            }

            HOST_PREFIX void callback(const Component::Function & fct, const ComponentType & type = ComponentType::ANY) {

                const uint_t timestep = timeloop_ ? timeloop_->getCurrentTimeStep() : 0;

                for(auto& turbine : turbines_) {
                    if(turbine)
                        turbine->callback(fct, type, timestep);
                }
            }


            HOST_PREFIX void callback(const Component::Output & fct, const ComponentType & type = ComponentType::ANY) {
                if(!output_)
                    return;

                const uint_t timestep = timeloop_ ? timeloop_->getCurrentTimeStep() : 0;

                for(uint_t i = 0; i < turbines_.size(); ++i) {
                    auto baseFolder = std::filesystem::path(outputDirectory_).append("Turbine" + std::to_string(i+1));
                    if(turbines_[i])
                        turbines_[i]->callback(fct, baseFolder, timestep, type);
                }
            }

            HOST_PREFIX void evaluateDensityAndVelocity( walberla::IBlock * block ) {
                //for (auto &turbine: turbines_) {
                for (uint_t i = 0; i < turbines_.size(); ++i) {
                    if (turbines_[i])
                        turbines_[i]->template evaluateDensityAndVelocity<DensityField_T, VelocityField_T, Interpolator>
                                (block, forest_, densityFieldID_, velocityFieldID_, nPointsComponents_[i],
                                 nComponents_[i]);
                }
            }

            HOST_PREFIX void syncNextNeighbour( const mpi::SynchronisationMode mode, const uint_t impactWidth = uint_t(0),
                                                const bool cudaAwareMPI = false ) {
                for( uint_t i = 0; i < turbines_.size(); ++i ) {
                    if(turbines_[i]) {
                        turbines_[i]->syncNextNeighbour(*domain_, mode, impactWidth, int(i), cudaAwareMPI);
                    }
                }
            }

            HOST_PREFIX void spreadForces( walberla::IBlock * block ) {
                for (uint_t i = 0; i < turbines_.size(); ++i) {
                    if (turbines_[i] && turbines_[i]->getAABB().intersects(block->getAABB())) {
                        turbines_[i]->template spreadForces<ForceField_T, ForceDistributor_T>(block, forest_,
                                                                                              forceFieldID_,
                                                                                              nPointsComponentsForSpread_[i]);
                    }
                }
            }

#ifdef WALBERLA_BUILD_WITH_GPU_SUPPORT
            HOST_PREFIX void synchroniseGpuStreams()  {
                for (auto &turbine: turbines_) {
                    if(turbine) {
                        turbine->synchroniseGpuStreams();
                    }
                }
            }
#endif


            HOST_PREFIX void writeForceOutput( walberla::IBlock * block, const uint_t startingTimeStep = uint_t(0),
                                               const uint_t outputFrequency = uint_t(0)) {
                if(!output_)
                    return;

                const uint_t timestep = timeloop_ ? timeloop_->getCurrentTimeStep() : 0;

                if( timestep < startingTimeStep || timestep % outputFrequency != 0)
                   return;

                for( uint_t i = 0; i < turbines_.size(); ++i ) {
                    auto baseFolder = std::filesystem::path(outputDirectory_).append("Turbine" + std::to_string(i+1));
                    if(turbines_[i])
                        turbines_[i]->writeForceOutput(block, forest_, baseFolder, timestep);
                }

            }

            HOST_PREFIX void writeTurbinePowerAndThrust() {
                if(!output_)
                    return;
                const uint_t timestep = timeloop_ ? timeloop_->getCurrentTimeStep() : 0;

                for( uint_t i = 0; i < turbines_.size(); ++i ) {
                    auto baseFolder = std::filesystem::path(outputDirectory_).append("Turbine" + std::to_string(i+1));

                    real_t power{0};
                    real_t thrust{0};
                    real_t omega{0};
                    if(turbines_[i]) {
                        for (auto &block: *forest_) {
                            real_t blockLocalPower{}, blockLocalThrust{};
                            turbines_[i]->calculatePowerAndThrust(&block, forest_, blockLocalPower, blockLocalThrust);
                            power += blockLocalPower;
                            thrust += blockLocalThrust;
                        }
                        turbines_[i]->getOmega(omega);
                    }

                    WALBERLA_MPI_SECTION() {
                        walberla::mpi::reduceInplace(power, walberla::mpi::SUM);
                        walberla::mpi::reduceInplace(thrust, walberla::mpi::SUM);
                    }

                    WALBERLA_ROOT_SECTION() {
                        const std::string filename{"Turbine_Performances.txt"};
                        auto filepath = baseFolder / filename;
                        std::ofstream os(filepath.string(), std::ios::app);
                        if (os.is_open()) {
                            os << timestep << "\t" << real_t(timestep)*conversion::ConversionFactors::C_t() << "\t";
                            os << -omega / Conversion::C_t() << "\t";
                            os << power << "\t" << thrust;
                            os << "\n";
                            os.close();
                        }
                    }
                }

            }

            HOST_PREFIX void calculatePowerAndThrust() {
                for( uint_t i = 0; i < turbines_.size(); ++i ) {
                    real_t power{}, thrust{};

                    if(turbines_[i]) {
                        for (auto &block: *forest_) {
                            real_t blockLocalPower{}, blockLocalThrust{};
                            turbines_[i]->calculatePowerAndThrust(&block, forest_, blockLocalPower, blockLocalThrust);
                            power += blockLocalPower;
                            thrust += blockLocalThrust;
                        }
                    }

                    WALBERLA_MPI_SECTION() {
                        walberla::mpi::reduceInplace(power, walberla::mpi::SUM);
                        walberla::mpi::reduceInplace(thrust, walberla::mpi::SUM);
                    }

                    WALBERLA_LOG_INFO_ON_ROOT("Power = " << power << ", thrust = " << thrust )
                }
            }

            HOST_PREFIX void calculateTurbineAABBs() {

                turbineAABBs_.clear();

                for( uint_t i = 0; i < turbines_.size(); ++i ) {
                    if(turbines_[i])
                        turbineAABBs_.push_back( turbines_[i]->getAABB() );
                }

            }

            HOST_PREFIX std::vector<AABB> & getTurbineAABBs() {
                return turbineAABBs_;
            }

        private:

            std::unique_ptr<creator::TurbineCreator<Topology_T>> creator_;
            creator::CachedAirfoilPolars<Topology_T, false> descriptionCache_;

            std::vector<std::shared_ptr<Topology_T>> turbines_;
            std::vector<AABB> turbineAABBs_;

            std::vector<bool> isControlled_;
            std::vector <uint_t> nPointsComponents_;
            std::vector <uint_t> nComponents_;
            std::vector <uint_t> nPointsComponentsForSpread_;

            std::shared_ptr<walberla::StructuredBlockForest> forest_{nullptr};
            std::shared_ptr<domain::TurbineDomain> domain_{nullptr};

            walberla::BlockDataID densityFieldID_;
            walberla::BlockDataID velocityFieldID_;
            walberla::BlockDataID forceFieldID_;


            const bool output_;
            const std::string outputDirectory_;

            walberla::timeloop::ITimeloop * timeloop_{nullptr};

        };

    }

    using farm::WindFarm;

}

#endif //TURBINECORE_WINDFARM_H
