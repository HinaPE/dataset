if (TARGET OpenEXR::OpenEXR)
    return()
endif ()

include(FetchContent)

set(OPENEXR_VERSION v3.4.0 CACHE STRING "OpenEXR version")
set(OPENEXR_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(OPENEXR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OPENEXR_USE_TBB ON CACHE BOOL "" FORCE)
set(ILMTHREAD_USE_TBB ON CACHE BOOL "" FORCE)
set(OPENEXR_USE_INTERNAL_DEFLATE ON CACHE BOOL "" FORCE)

if (DEFINED oneTBB_BINARY_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${oneTBB_BINARY_DIR}")
    if (NOT DEFINED TBB_DIR)
        set(TBB_DIR "${oneTBB_BINARY_DIR}")
    endif ()
endif ()

FetchContent_Declare(openexr URL https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/${OPENEXR_VERSION}.tar.gz)
FetchContent_MakeAvailable(openexr)
