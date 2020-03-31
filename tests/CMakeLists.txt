enable_testing()
find_package(GTest)
if(${GTEST_FOUND})
  include_directories(${GTEST_INCLUDE_DIRS})
else()
  if(DEFINED ENV{GTEST_ROOT})
    message(STATUS "ENV GTEST_ROOT SET: \"$ENV{GTEST_ROOT}\"")
    set(GTEST_SRC_DIR "$ENV{GTEST_ROOT}")
  else()
    set(GTEST_SRC_DIR "/usr/src/googletest/googletest")
  endif()
  message(STATUS "using GTEST_SRC_DIR: \"${GTEST_SRC_DIR}\"")
  add_subdirectory(${GTEST_SRC_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/gtest-build
    EXCLUDE_FROM_ALL
    )
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(fam_test
    main.cpp
    test_state_handler.cpp
    test_input_listener.cpp
    test_rolling_window.cpp
    )
target_link_libraries(fam_test
  PUBLIC
  ${CMAKE_PROJECT_NAME}_lib
  gtest
  Threads::Threads
)
add_test(NAME fam_test COMMAND fam_test)
