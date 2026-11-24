
#####################
### ComponentType ###
#####################

waLBerla_add_test_executable( ComponentTypeTest ComponentTypeTest.cpp)
target_link_libraries( ComponentTypeTest component )
waLBerla_execute_test( NAME ComponentTypeTest )

get_property(existing_labels TEST ComponentTypeTest PROPERTY LABELS)
list(APPEND existing_labels "walberlawind::unitTests::CPU")
set_tests_properties(ComponentTypeTest
    PROPERTIES LABELS "${existing_labels}"
)
