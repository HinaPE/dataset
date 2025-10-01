if(TARGET spng_static)
    return()
endif()

include(FetchContent)

set(SPNG_VERSION v0.7.4 CACHE STRING "libspng version")
set(SPNG_SHARED OFF CACHE BOOL "" FORCE)
set(SPNG_STATIC ON CACHE BOOL "" FORCE)
set(SPNG_TESTS OFF CACHE BOOL "" FORCE)
set(SPNG_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPNG_USE_MINIZ OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libspng URL https://github.com/randy408/libspng/archive/refs/tags/${SPNG_VERSION}.tar.gz)
FetchContent_MakeAvailable(libspng)
