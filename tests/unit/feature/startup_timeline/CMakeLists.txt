add_executable(
  startup_timeline_tests ${CMAKE_SOURCE_DIR}/src/feature/startup_timeline/startup_timeline_core.cpp
                         startup_timeline_tests.cpp
  )
target_include_directories(startup_timeline_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_catch_test(startup_timeline_tests)
