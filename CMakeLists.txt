cmake_minimum_required(VERSION 3.16)
project(assessment LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(nlohmann_json REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

add_executable(loader loader/main.cpp)
target_link_libraries(loader ${PQXX_LIBRARIES})
target_include_directories(loader PRIVATE ${PQXX_INCLUDE_DIRS} include)

add_executable(query 
    query/main.cpp
    include/json_parser.cpp
    include/query_builder.cpp)
target_link_libraries(query ${PQXX_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(query PRIVATE ${PQXX_INCLUDE_DIRS} include)