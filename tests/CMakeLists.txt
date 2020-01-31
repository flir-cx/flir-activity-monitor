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

add_executable(maf_test main.cpp)
target_link_libraries(maf_test PUBLIC ${CMAKE_PROJECT_NAME}_lib gtest)
add_test(NAME maf_test COMMAND maf_test)
