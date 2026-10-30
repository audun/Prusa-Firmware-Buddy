add_executable(
  pre_print_prep_tests ${CMAKE_CURRENT_SOURCE_DIR}/prepared_state_tests.cpp
                       ${CMAKE_SOURCE_DIR}/src/feature/pre_print_prep/prepared_state.cpp
  )
target_include_directories(pre_print_prep_tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_catch_test(pre_print_prep_tests)
