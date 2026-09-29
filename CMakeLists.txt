cmake_minimum_required(VERSION 3.15)
project(tinybuf-node C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default OSAL_PATH and ETL_PATH if not set in environment
if (NOT DEFINED ENV{OSAL_PATH})
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../osal")
        set(ENV{OSAL_PATH} "${CMAKE_CURRENT_LIST_DIR}/../osal")
    else()
        message(FATAL_ERROR "Missing OSAL_PATH. Please set ENV{OSAL_PATH}")
    endif()
endif()

if (NOT DEFINED ENV{ETL_PATH})
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../etl")
        set(ENV{ETL_PATH} "${CMAKE_CURRENT_LIST_DIR}/../etl")
    else()
        set(ENV{ETL_PATH} "${CMAKE_CURRENT_BINARY_DIR}/etl-src")
    endif()
endif()

# Download / include ETL target etl::etl
include(${CMAKE_CURRENT_LIST_DIR}/src/cmake/fetchcontent_etl.cmake)

# Download / include static POCO
include(${CMAKE_CURRENT_LIST_DIR}/cmake/fetchcontent_poco.cmake)

# Setup Python interpreter
find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "Using Python Interpreter: ${Python3_EXECUTABLE}")

# Generate serializable Messages (creates ApplicationSerializableMessages.hpp)
include(${CMAKE_CURRENT_LIST_DIR}/src/cmake/create_serializable_messages.cmake)

# Generate Proxy Subsystem from config.json (creates ProxyImplConfig.hpp)
include(${CMAKE_CURRENT_LIST_DIR}/src/cmake/create_proxy_config.cmake)

# Determine build type (Release by default)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build mode" FORCE)
endif()
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

# Set output directories so .node addon is placed in build/Release
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release")

# Add linux-al static library
add_subdirectory(linux-al)
target_link_libraries(linux-al PUBLIC ${POCO_LIBRARIES})

# Custom target for message generation
add_custom_target(generate_app_messages ALL DEPENDS ${MY_SERIALIZABLE_MESSAGES})
add_dependencies(linux-al generate_app_messages)

# Query Node.js and node-addon-api include directories
if(DEFINED CMAKE_JS_INC)
    set(NODE_INCLUDE_DIR ${CMAKE_JS_INC})
else()
    execute_process(
        COMMAND node -p "path.resolve(process.execPath, '../../include/node')"
        OUTPUT_VARIABLE NODE_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

execute_process(
    COMMAND node -p "require('node-addon-api').include"
    OUTPUT_VARIABLE NAPI_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "\"" "" NAPI_INCLUDE_DIR "${NAPI_INCLUDE_DIR}")

# Sources
set(TINYBUF_NODE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tinybuf-node.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/MessageChannel/ApplicationMessageChannel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/MessageDispatcher/ApplicationMessageDispatcher.cpp"
)

if(DEFINED CMAKE_JS_SRC)
    list(APPEND TINYBUF_NODE_SOURCES ${CMAKE_JS_SRC})
endif()

add_library(${PROJECT_NAME} SHARED ${TINYBUF_NODE_SOURCES})

add_dependencies(${PROJECT_NAME} generate_app_messages)

# Include paths
target_include_directories(${PROJECT_NAME} PRIVATE
    ${NODE_INCLUDE_DIR}
    ${NAPI_INCLUDE_DIR}
    "$ENV{OSAL_PATH}/src/main/interface"
    "$ENV{OSAL_PATH}/src/main/cpp/Core"
    "$ENV{OSAL_PATH}/src/main/cpp/Proxy"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/messages"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/Generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux-al/src/main/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux-al/src/main/cpp/MessageDispatcher"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux-al/src/main/cpp/MessageChannel"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux-al/src/main/cpp/Proxy"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux-al/src/main/cpp/BootLoader"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux-al/src/main/cpp/LoggingManager"
)

# Imported OSAL static library
add_library(osal STATIC IMPORTED)
set_target_properties(osal PROPERTIES
    IMPORTED_LOCATION "$ENV{OSAL_PATH}/target/linux/${CMAKE_BUILD_TYPE_LOWER}/libosal.a"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    linux-al
    osal
    etl::etl
    ${POCO_LIBRARIES}
    ${CMAKE_JS_LIB}
)

# Enable C++ exceptions for node-addon-api and POCO
target_compile_definitions(${PROJECT_NAME} PRIVATE
    NAPI_CPP_EXCEPTIONS
)

# Set output name as a Node.js addon
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".node"
)
