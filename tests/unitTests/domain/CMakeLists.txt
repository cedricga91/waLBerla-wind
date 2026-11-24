############################
### Boundary setup tests ###
############################

waLBerla_add_test_executable( BoundarySetupPeriodicityTest BoundarySetupPeriodicityTest.cpp)
target_link_libraries( BoundarySetupPeriodicityTest walberla::core )
waLBerla_execute_test( NAME BoundarySetupPeriodicityTest )

get_property(existing_labels TEST BoundarySetupPeriodicityTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(BoundarySetupPeriodicityTest
    PROPERTIES LABELS "${existing_labels}"
)

waLBerla_add_test_executable( BoundarySetupFieldFillerTest BoundarySetupFieldFillerTest.cpp)
target_link_libraries( BoundarySetupFieldFillerTest walberla::boundary walberla::core walberla::domain_decomposition walberla::field walberla::geometry )
waLBerla_execute_test( NAME BoundarySetupFieldFillerTest )

get_property(existing_labels TEST BoundarySetupFieldFillerTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(BoundarySetupFieldFillerTest
    PROPERTIES LABELS "${existing_labels}"
)

##########################
### Domain setup tests ###
##########################

waLBerla_add_test_executable( DomainSetupDomainDecompositionTest DomainSetupDomainDecompositionTest.cpp)
target_link_libraries( DomainSetupDomainDecompositionTest walberla::blockforest walberla::core walberla::domain_decomposition )
waLBerla_execute_test( NAME DomainSetupDomainDecompositionTest )

get_property(existing_labels TEST DomainSetupDomainDecompositionTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(DomainSetupDomainDecompositionTest
    PROPERTIES LABELS "${existing_labels}"
)
