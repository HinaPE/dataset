cmake_minimum_required(VERSION 3.26)
project(dataset LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE dataset_src include/*.h src/*.cpp)
add_library(dataset STATIC ${dataset_src})
target_include_directories(dataset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(BUILD_SHARED_LIBS OFF)
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

find_package(Threads REQUIRED)

FetchContent_Declare(simdjson
        URL https://github.com/simdjson/simdjson/archive/refs/tags/v4.0.6.tar.gz
)
FetchContent_MakeAvailable(simdjson)

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
        URL https://zlib.net/zlib-1.3.1.tar.gz
)
FetchContent_MakeAvailable(zlib)
if(MSVC)
    set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/zlibstatic.lib" CACHE FILEPATH "" FORCE)
else()
    set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/libz.a" CACHE FILEPATH "" FORCE)
endif()
set(ZLIB_LIBRARIES "${ZLIB_LIBRARY}" CACHE STRING "" FORCE)
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE STRING "" FORCE)
set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)

set(LIBDEFLATE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
FetchContent_Declare(libdeflate
        URL https://github.com/ebiggers/libdeflate/archive/refs/tags/v1.24.tar.gz
)
FetchContent_MakeAvailable(libdeflate)

set(SPNG_SHARED OFF CACHE BOOL "" FORCE)
set(SPNG_STATIC ON CACHE BOOL "" FORCE)
set(SPNG_TESTS OFF CACHE BOOL "" FORCE)
set(SPNG_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPNG_USE_MINIZ OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libspng
        URL https://github.com/randy408/libspng/archive/refs/tags/v0.7.4.tar.gz
)
FetchContent_MakeAvailable(libspng)

set(OPENEXR_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(OPENEXR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OPENEXR_USE_TBB OFF CACHE BOOL "" FORCE)
set(OPENEXR_USE_INTERNAL_DEFLATE ON CACHE BOOL "" FORCE)
FetchContent_Declare(openexr
        URL https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v3.4.0.tar.gz
)
FetchContent_GetProperties(openexr)
if(NOT openexr_POPULATED)
    FetchContent_Populate(openexr)
    add_subdirectory(${openexr_SOURCE_DIR} ${openexr_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
