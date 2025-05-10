cmake_minimum_required(VERSION 3.14)
project(OSLab5)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.66 REQUIRED COMPONENTS system)

add_executable(Client Client.cpp)
add_executable(Server Server.cpp)

target_link_libraries(Client PRIVATE Boost::system)
target_link_libraries(Server PRIVATE Boost::system)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(UnitTests UnitTests.cpp)
target_link_libraries(UnitTests gtest_main gtest)
target_include_directories(UnitTests PRIVATE ${CMAKE_SOURCE_DIR})
