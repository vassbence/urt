cmake_minimum_required(VERSION 3.22.2)
include(FetchContent)

project(urt)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_compile_options(${PROJECT_NAME} INTERFACE -Werror -Wall -Wextra -Wpedantic -Wconversion)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_executable(${PROJECT_NAME}_test test/run_test.cpp test/server_fixture.cpp test/address_test.cpp test/connection_test.cpp test/http_test.cpp)
target_include_directories(${PROJECT_NAME}_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} gtest_main)
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)

add_subdirectory(example)