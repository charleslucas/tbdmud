project(TBDMUD_SERVER)

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
cmake_policy(VERSION 3.2)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS NO_OUTPUT_DIRS)

# The required C++ Standard for Server++ is C++14.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set (TBDMUD_SERVER_PUBLIC_SOURCE_FILES
    src/connection.cpp
    #src/player.cpp
    src/tbdmud_server.cpp
)

set (TBDMUD_SERVER_PUBLIC_HEADER_FILES
    include/connection.hpp
    #include/player.h
)

add_executable(tbdmud_server
    ${TBDMUD_SERVER_PUBLIC_HEADER_FILES}
    ${TBDMUD_SERVER_PUBLIC_SOURCE_FILES}
)

target_include_directories(tbdmud_server 
    PRIVATE 
        include
)

target_link_libraries(tbdmud_server
    CONAN_PKG::serverpp
    CONAN_PKG::telnetpp
    CONAN_PKG::boost_range
)
