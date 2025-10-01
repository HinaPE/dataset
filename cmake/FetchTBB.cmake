if(TARGET TBB::tbb)
    return()
endif()

include(FetchContent)
set(TBB_VERSION v2022.2.0 CACHE STRING "oneTBB version")
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(oneTBB URL https://github.com/uxlfoundation/oneTBB/archive/refs/tags/${TBB_VERSION}.tar.gz DOWNLOAD_EXTRACT_TIMESTAMP OFF)
FetchContent_MakeAvailable(oneTBB)

if(NOT TARGET TBB::tbb AND TARGET tbb)
    add_library(TBB::tbb ALIAS tbb)
endif()
