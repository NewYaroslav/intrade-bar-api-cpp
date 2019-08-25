# Author: Ivan Blagopoluchnyy 2019 (c)

#- Debug path is hardcoded in target_link_libraries
#- need to handle targets triggering only by IntradeAPI target
#- ZLIB_LIBRARY path to lib static file hardcoded
#- libcurl-d.dll must be manually placed near binary execution file

#- compile steps: libcurl -> curl -> IntradeAPI

#compilations steps with https:
#https://stackoverflow.com/questions/197444/building-libcurl-with-ssl-support-on-windows

cmake_minimum_required(VERSION 3.0.2)
project(IntradeAPI)

find_library(ZLIB NAMES zlib.lib PATHS ${ZLIB_LIBRARY}/lib REQUIRED)
find_package(OpenSSL REQUIRED)
link_directories(${ZLIB_LIBRARY})
add_subdirectory(./lib/curl)

if(WIN32)
	add_definitions(-DWINVER=0x0600)
	add_definitions(-DUNICODE -D_UNICODE)
	#example why we linking winapi extensions
	#https://github.com/microsoft/vcpkg/issues/2621
	list(APPEND LIBUV_LIBRARIES iphlpapi)
	list(APPEND LIBUV_LIBRARIES psapi)
	list(APPEND LIBUV_LIBRARIES userenv)
	list(APPEND LIBUV_LIBRARIES ws2_32)
	list(APPEND LIBUV_LIBRARIES wsock32)
	list(APPEND LIBUV_LIBRARIES Crypt32)
	list(APPEND LIBUV_LIBRARIES Wldap32)
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
list(APPEND _LIBS ${LIB_OPENSSL}})
set (PROJECT_SOURCE_DEFINITION_DIRECTORY ${PROJECT_SOURCE_DIR}/src)
set (MAIN_FILE ${PROJECT_SOURCE_DEFINITION_DIRECTORY}/MainHandler.cpp)
file(GLOB _HEADERS SYSTEM ${PROJECT_SOURCE_DIR}/include ${OPENSSL_ROOT_DIR}/include)

set(EXECUTABLE ${PROJECT_NAME})
SET(BASEPATH "${CMAKE_SOURCE_DIR}")

include_directories(${_HEADERS} ${BASEPATH} ${PROJECT_SOURCE_DIR} ${DZLIB_INCLUDE_DIR})
add_executable(${EXECUTABLE} ${MAIN_FILE})
target_link_libraries(${PROJECT_NAME} ./lib/curl/lib/Debug/libcurl-d_imp)
#cmake -DCMAKE_BUILD_TYPE=release cmake -G "Visual Studio 15 2017 Win64" -DOPENSSL_ROOT_DIR=C:\OpenSSL-Win64-1.1.1 -DZLIB_LIBRARY=C:\LIBS_ETC\ZLIB_bin\lib\zlib.lib -DZLIB_INCLUDE_DIR=C:\LIBS_ETC\ZLIB_bin\include ..

#set ZLIB_PATH=
#set OPENSSL_PATH=