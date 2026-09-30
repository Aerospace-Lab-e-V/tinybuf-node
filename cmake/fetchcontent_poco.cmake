include_guard(GLOBAL)
include(FetchContent)

message(STATUS "Configuring static Poco build...")

set(POCO_STATIC ON CACHE BOOL "Build static Poco libraries" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(POCO_UNBUNDLED OFF CACHE BOOL "Use bundled Poco dependencies" FORCE)

# Disable unnecessary Poco components for fast compile and minimal footprint
set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_DATA OFF CACHE BOOL "" FORCE)
set(ENABLE_DATA_SQLITE OFF CACHE BOOL "" FORCE)
set(ENABLE_DATA_MYSQL OFF CACHE BOOL "" FORCE)
set(ENABLE_DATA_ODBC OFF CACHE BOOL "" FORCE)
set(ENABLE_DATA_POSTGRESQL OFF CACHE BOOL "" FORCE)
set(ENABLE_MONGODB OFF CACHE BOOL "" FORCE)
set(ENABLE_REDIS OFF CACHE BOOL "" FORCE)
set(ENABLE_PDF OFF CACHE BOOL "" FORCE)
set(ENABLE_ZIP OFF CACHE BOOL "" FORCE)
set(ENABLE_PAGECOMPILER OFF CACHE BOOL "" FORCE)
set(ENABLE_PAGECOMPILER_FILE2PAGE OFF CACHE BOOL "" FORCE)
set(ENABLE_ACTIVERECORD OFF CACHE BOOL "" FORCE)
set(ENABLE_ACTIVERECORD_COMPILER OFF CACHE BOOL "" FORCE)
set(ENABLE_PROMETHEUS OFF CACHE BOOL "" FORCE)
set(ENABLE_CPPPARSER OFF CACHE BOOL "" FORCE)
set(ENABLE_POCODOC OFF CACHE BOOL "" FORCE)
set(ENABLE_SEVENZIP OFF CACHE BOOL "" FORCE)
set(ENABLE_JWT OFF CACHE BOOL "" FORCE)
set(ENABLE_CRYPTO OFF CACHE BOOL "" FORCE)
set(ENABLE_NETSSL OFF CACHE BOOL "" FORCE)

# Enable only required components
set(ENABLE_FOUNDATION ON CACHE BOOL "" FORCE)
set(ENABLE_JSON ON CACHE BOOL "" FORCE)
set(ENABLE_XML ON CACHE BOOL "" FORCE)
set(ENABLE_UTIL ON CACHE BOOL "" FORCE)
set(ENABLE_NET ON CACHE BOOL "" FORCE)

if (DEFINED ENV{POCO_PATH} AND EXISTS "$ENV{POCO_PATH}")
    message(STATUS "POCO available locally under $ENV{POCO_PATH}")
    FetchContent_Declare(
        poco
        SOURCE_DIR "$ENV{POCO_PATH}"
    )
else()
    message(STATUS "Downloading Poco 1.15.3 release via FetchContent...")
    FetchContent_Declare(
        poco
        GIT_REPOSITORY https://github.com/pocoproject/poco.git
        GIT_TAG        poco-1.15.3-release
        GIT_SHALLOW    TRUE
    )
endif()

FetchContent_MakeAvailable(poco)

set(POCO_LIBRARIES Poco::Net Poco::Util Poco::Foundation CACHE INTERNAL "Poco static libraries")
