cmake_minimum_required(VERSION 3.10)
project(IoTAgent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Check for nlohmann/json (optional)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp")
    add_definitions(-DHAVE_NLOHMANN_JSON)
    message(STATUS "Found nlohmann/json - using it for JSON handling")
else()
    message(STATUS "nlohmann/json not found - using manual JSON formatting")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/http_client.cpp
    src/config.cpp
)

set(HEADERS
    include/http_client.hpp
    include/config.hpp
)

# Create executable
add_executable(agent ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(agent ${CURL_LIBRARIES})

# Compiler options
target_compile_options(agent PRIVATE -Wall -Wextra -O2)

# Install target
install(TARGETS agent DESTINATION bin)

