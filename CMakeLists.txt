cmake_minimum_required(VERSION 3.15)
project(sero2-node CXX)

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
        message(FATAL_ERROR "Missing ETL_PATH. Please set ENV{ETL_PATH}")
    endif()
endif()

# Download / include ETL target etl::etl
include(${CMAKE_CURRENT_LIST_DIR}/src/cmake/fetchcontent_etl.cmake)

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

# Add linux-al static library
add_subdirectory(linux-al)

# Custom target for message generation
add_custom_target(generate_app_messages ALL DEPENDS ${MY_SERIALIZABLE_MESSAGES})
add_dependencies(linux-al generate_app_messages)

# Query Node.js and node-addon-api include directories
execute_process(
    COMMAND node -p "path.resolve(process.execPath, '../../include/node')"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND node -p "require('node-addon-api').include"
    OUTPUT_VARIABLE NAPI_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "\"" "" NAPI_INCLUDE_DIR "${NAPI_INCLUDE_DIR}")

# Sources
set(SERO2_NODE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sero2-node.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/MessageChannel/ApplicationMessageChannel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/MessageDispatcher/ApplicationMessageDispatcher.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SERO2_NODE_SOURCES})

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
    PocoNet
    PocoUtil
    PocoFoundation
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
