if(TARGET libdeflate)
    return()
endif()

include(FetchContent)

set(LIBDEFLATE_VERSION v1.24 CACHE STRING "libdeflate version")
set(LIBDEFLATE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    libdeflate
    URL https://github.com/ebiggers/libdeflate/archive/refs/tags/${LIBDEFLATE_VERSION}.tar.gz
)

FetchContent_MakeAvailable(libdeflate)
