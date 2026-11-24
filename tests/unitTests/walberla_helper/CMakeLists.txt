#################
### BlockInfo ###
#################

waLBerla_add_test_executable( BlockInfoTest blockforest/BlockInfoTest.cpp)
target_link_libraries( BlockInfoTest walberla::core walberla::blockforest )
waLBerla_execute_test( NAME BlockInfoTest )

get_property(existing_labels TEST BlockInfoTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(BlockInfoTest
    PROPERTIES LABELS "${existing_labels}"
)

#############
### Field ###
#############

waLBerla_add_test_executable( TurbineCoreFieldTest field/TurbineCoreFieldTest.cpp)
target_link_libraries( TurbineCoreFieldTest walberla::core walberla::field walberla_helper )
waLBerla_execute_test( NAME TurbineCoreFieldTest )

get_property(existing_labels TEST TurbineCoreFieldTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(TurbineCoreFieldTest
    PROPERTIES LABELS "${existing_labels}"
)

########################################
### Macroscopic Variable Calculation ###
########################################

waLBerla_add_test_executable( TurbineCoreMacroscopicVariableCalculatorTest field/TurbineCoreMacroscopicVariableCalculatorTest.cpp)
target_link_libraries( TurbineCoreMacroscopicVariableCalculatorTest walberla::core walberla::field walberla_helper )
waLBerla_execute_test( NAME TurbineCoreMacroscopicVariableCalculatorTest )

get_property(existing_labels TEST TurbineCoreMacroscopicVariableCalculatorTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(TurbineCoreMacroscopicVariableCalculatorTest
    PROPERTIES LABELS "${existing_labels}"
)

#####################
### Interpolators ###
#####################

waLBerla_add_test_executable( TurbineCoreFieldInterpolationTest field/interpolators/TurbineCoreFieldInterpolationTest.cpp)
target_link_libraries( TurbineCoreFieldInterpolationTest walberla::core walberla::field walberla_helper)
waLBerla_execute_test( NAME TurbineCoreFieldInterpolationTest )

get_property(existing_labels TEST TurbineCoreFieldInterpolationTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(TurbineCoreFieldInterpolationTest
    PROPERTIES LABELS "${existing_labels}"
)

waLBerla_add_test_executable( TurbineCoreMacroscopicFieldInterpolationTest field/interpolators/TurbineCoreMacroscopicFieldInterpolationTest.cpp)
target_link_libraries( TurbineCoreMacroscopicFieldInterpolationTest walberla::core walberla::field walberla_helper)
waLBerla_execute_test( NAME TurbineCoreMacroscopicFieldInterpolationTest )

get_property(existing_labels TEST TurbineCoreMacroscopicFieldInterpolationTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(TurbineCoreMacroscopicFieldInterpolationTest
    PROPERTIES LABELS "${existing_labels}"
)

####################
### Distributors ###
####################

waLBerla_add_test_executable( TurbineCoreFieldDistributorTest field/distributors/TurbineCoreFieldDistributorTest.cpp)
target_link_libraries( TurbineCoreFieldDistributorTest walberla::core walberla::field walberla_helper)
waLBerla_execute_test( NAME TurbineCoreFieldDistributorTest )

get_property(existing_labels TEST TurbineCoreFieldDistributorTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(TurbineCoreFieldDistributorTest
    PROPERTIES LABELS "${existing_labels}"
)
