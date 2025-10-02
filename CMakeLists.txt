cmake_minimum_required(VERSION 3.26)
project(dataset LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

file(GLOB_RECURSE DATASET_SRC CONFIGURE_DEPENDS include/*.h src/*.cpp)
add_library(dataset STATIC ${DATASET_SRC})
set_target_properties(dataset PROPERTIES EXPORT_NAME dataset)
target_include_directories(dataset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/Dependencies.cmake)
dataset_link_thirdparty(dataset)

include(GNUInstallDirs)
install(TARGETS dataset
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(WIN32)
  # Explicitly install dependency DLLs if those targets are shared
  set(_DATASET_RUNTIME_CANDIDATES
      TBB::tbb
      OpenEXR::OpenEXR
      OpenEXR::IlmThread
      OpenEXR::Iex
      OpenEXR::OpenEXRCore
      OpenEXR::OpenEXRUtil
      openjph
      zlib)
  foreach(t IN LISTS _DATASET_RUNTIME_CANDIDATES)
    if(TARGET ${t})
      install(FILES
        $<$<OR:$<STREQUAL:$<TARGET_PROPERTY:${t},TYPE>,SHARED_LIBRARY>,$<STREQUAL:$<TARGET_PROPERTY:${t},TYPE>,MODULE_LIBRARY>>:$<TARGET_FILE:${t}>>
        DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
    endif()
  endforeach()
endif()
