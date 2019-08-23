# Author: Ivan Blagopoluchnyy 2019 (c)

cmake_minimum_required(VERSION 3.0.2)
project(IntradeAPI)

if(WIN32)
	add_definitions(-DWINVER=0x0600)
	add_definitions(-DUNICODE -D_UNICODE)
endif(WIN32)

if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc" )
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

file(GLOB _SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
#list(APPEND _LIBS uWS ${LIB_OPENSSL})
set (PROJECT_SOURCE_DEFINITION_DIRECTORY ${PROJECT_SOURCE_DIR}/src)
set (MAIN_FILE ${PROJECT_SOURCE_DEFINITION_DIRECTORY}/MainHandler.cpp)
file(GLOB _HEADERS SYSTEM ${PROJECT_SOURCE_DIR}/include)

set(EXECUTABLE ${PROJECT_NAME})
SET(BASEPATH "${CMAKE_SOURCE_DIR}")

include_directories(${_HEADERS} ${BASEPATH} ${PROJECT_SOURCE_DIR})
add_executable(${EXECUTABLE} ${MAIN_FILE})
target_link_libraries(${PROJECT_NAME})
#cmake -DCMAKE_BUILD_TYPE=release cmake -G "Visual Studio 15 2017 Win64" ..
