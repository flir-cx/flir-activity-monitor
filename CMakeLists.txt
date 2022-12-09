cmake_minimum_required(VERSION 3.4)
project(flir-activity-monitor)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FAM_DEPS REQUIRED libevdev libudev libsystemd)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(FAM_SOURCES
    main.cpp
    state_handler.cpp
    settings_handler.cpp
    input_monitor.cpp
    network_monitor.cpp
    battery_monitor.cpp
    utils.cpp
    logger.cpp
    )

add_executable(flir-activity-monitor ${FAM_SOURCES})
target_link_libraries(flir-activity-monitor
    ${FAM_DEPS_LIBRARIES}
    Threads::Threads
    )

target_include_directories(flir-activity-monitor
    PUBLIC
    ${FAM_DEPS_INCLUDE_DIRS}
    PRIVATE
	${CMAKE_SOURCE_DIR}
)

add_library(flir-activity-monitor_lib STATIC ${FAM_SOURCES})
target_include_directories(flir-activity-monitor_lib
    PUBLIC
    ${FAM_DEPS_INCLUDE_DIRS}
    PRIVATE
	${CMAKE_SOURCE_DIR}
)

add_subdirectory(tests)

install(TARGETS flir-activity-monitor DESTINATION bin)
