if(TARGET spng_static)
    return()
endif()

include(FetchContent)

set(SPNG_VERSION v0.7.4 CACHE STRING "libspng version")
FetchContent_Declare(libspng URL https://github.com/randy408/libspng/archive/refs/tags/${SPNG_VERSION}.tar.gz)
FetchContent_GetProperties(libspng)
if(NOT libspng_POPULATED)
    FetchContent_Populate(libspng)
    # Minimal static library build: just compile spng.c, link to zlib
    add_library(spng_static STATIC "${libspng_SOURCE_DIR}/spng/spng.c")
    target_include_directories(spng_static PUBLIC "${libspng_SOURCE_DIR}/spng")
    if(TARGET ZLIB::ZLIB)
        target_link_libraries(spng_static PUBLIC ZLIB::ZLIB)
    elseif(ZLIB_LIBRARIES)
        target_link_libraries(spng_static PUBLIC ${ZLIB_LIBRARIES})
    endif()
    target_compile_definitions(spng_static PRIVATE SPNG_USE_ZLIB)
    # Propagate SPNG_STATIC to consumers so headers don't use dllimport with static lib
    target_compile_definitions(spng_static INTERFACE SPNG_STATIC)
    set_target_properties(spng_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
