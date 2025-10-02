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

# Provide a minimal config file so find_package(TBB) used by OpenEXR succeeds when using FetchContent
# Place it directly in the oneTBB binary dir so setting TBB_DIR or adding to CMAKE_PREFIX_PATH works.
if(DEFINED oneTBB_BINARY_DIR)
    set(_TBB_FAKE_CONFIG "${oneTBB_BINARY_DIR}/TBBConfig.cmake")
    if(NOT EXISTS "${_TBB_FAKE_CONFIG}")
        file(WRITE "${_TBB_FAKE_CONFIG}" "# Auto-generated minimal TBBConfig for FetchContent integration\n"
            "if(NOT TARGET TBB::tbb)\n"
            "  if(TARGET tbb)\n"
            "    add_library(TBB::tbb ALIAS tbb)\n"
            "  else()\n"
            "    message(FATAL_ERROR 'Expected tbb target not found when loading TBBConfig.cmake')\n"
            "  endif()\n"
            "endif()\n"
            "set(TBB_FOUND TRUE)\n"
            "set(TBB_VERSION ${TBB_VERSION})\n"
        )
    endif()
endif()
