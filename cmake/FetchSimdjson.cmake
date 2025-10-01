if(TARGET simdjson::simdjson)
    return()
endif()

include(FetchContent)

set(SIMDJSON_VERSION v4.0.6 CACHE STRING "simdjson version")
FetchContent_Declare(simdjson URL https://github.com/simdjson/simdjson/archive/refs/tags/${SIMDJSON_VERSION}.tar.gz)
FetchContent_MakeAvailable(simdjson)
