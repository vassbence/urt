cmake_minimum_required(VERSION 3.22.2)
project(example)
set(CMAKE_CXX_STANDARD 17)

if(NOT TARGET urt)
    include(FetchContent)
    FetchContent_Declare(
            urt
            GIT_REPOSITORY https://github.com/vassbence/urt.git
            GIT_TAG v0.2.0
    )
    FetchContent_MakeAvailable(urt)
endif()

add_executable("bootstrap" "main.cpp")
target_link_libraries("bootstrap" PRIVATE urt)