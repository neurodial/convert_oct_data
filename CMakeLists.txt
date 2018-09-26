cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(convert_oct_data)

set( CMAKE_CXX_STANDARD 11 )

find_package(LibOctData REQUIRED)
find_package(Boost 1.40 COMPONENTS filesystem system program_options REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})

add_executable(convert_oct_data main.cpp)

target_link_libraries(convert_oct_data ${Boost_LIBRARIES})
target_link_libraries(convert_oct_data LibOctData::octdata)

install(TARGETS convert_oct_data RUNTIME DESTINATION bin)
