cmake_minimum_required(VERSION 3.14)

project(sframe
  VERSION 0.1
  LANGUAGES CXX
)

option(TESTING    "Build tests" OFF)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)
option(SANITIZERS "Enable sanitizers" OFF)
option(NO_ALLOC   "Build without needing an allocator" OFF)
option(NAMESPACE_SUFFIX "Namespace Suffix for CXX and CMake Export")

if(NAMESPACE_SUFFIX)
    set(SFRAME_CXX_NAMESPACE "sframe_${NAMESPACE_SUFFIX}" CACHE STRING "Top-level Namespace for CXX")
    set(SFRAME_EXPORT_NAMESPACE "SFrame${NAMESPACE_SUFFIX}" CACHE STRING "Namespace for CMake Export")
else()
    set(SFRAME_CXX_NAMESPACE "sframe" CACHE STRING "Top-level Namespace for CXX")
    set(SFRAME_EXPORT_NAMESPACE "SFrame" CACHE STRING "Namespace for CMake Export")
endif()
message(STATUS "CXX Namespace: ${SFRAME_CXX_NAMESPACE}")
message(STATUS "CMake Export Namespace: ${SFRAME_EXPORT_NAMESPACE}")

# Use -DCRYPTO=(OPENSSL_1_1 | OPENSSL_3 | BORINGSSL) to configure crypto
if(NOT DEFINED CRYPTO)
  set(CRYPTO "OPENSSL_3")
endif()

###
### Global Config
###
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/namespace.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/namespace.h"
  @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options(-Wall -pedantic -Wextra -Werror -Wmissing-declarations)
elseif(MSVC)
  add_compile_options(/W4 /WX)

  # MSVC helpfully recommends safer equivalents for things like
  # getenv, but they are not portable.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (SANITIZERS AND ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")) AND NOT WIN32)
  set (SANITIZERS "-fsanitize=address -fsanitize=undefined")
  set (CMAKE_C_FLAGS              "${CMAKE_C_FLAGS}             ${SANITIZERS}")
  set (CMAKE_CXX_FLAGS            "${CMAKE_CXX_FLAGS}           ${SANITIZERS}")
  set (CMAKE_EXE_LINKER_FLAGS     "${CMAKE_EXE_LINKER_FLAGS}    ${SANITIZERS}")
  set (CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZERS}")
  set (CMAKE_MODULE_LINKER_FLAGS  "${CMAKE_MODULE_LINKER_FLAGS} ${SANITIZERS}")
endif()

if(CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY  ${CLANG_TIDY_EXE})
  else()
    message(WARNING "clang-tidy requested, but not found")
  endif()
endif()

if(NO_ALLOC)
  message(STATUS "Configuring no-allocator version")
  add_definitions(-DNO_ALLOC)
endif()

###
### Dependencies
###

# External libraries
if(${CRYPTO} STREQUAL "OPENSSL_1_1")
  message(STATUS "Configuring with OpenSSL 1.1")
  find_package(OpenSSL 1.1 EXACT REQUIRED)
  add_compile_definitions(OPENSSL_1_1)
  set(CRYPTO_LIB OpenSSL::Crypto)
elseif(${CRYPTO} STREQUAL "OPENSSL_3")
  message(STATUS "Configuring with OpenSSL 3")
  find_package(OpenSSL 3 EXACT REQUIRED)
  add_compile_definitions(OPENSSL_3)
  set(CRYPTO_LIB OpenSSL::Crypto)
elseif(${CRYPTO} STREQUAL "BORINGSSL")
  message(STATUS "Configuring with BoringSSL")
  find_package(OpenSSL REQUIRED)
  add_compile_definitions(BORINGSSL)
  set(CRYPTO_LIB OpenSSL::Crypto)
else()
  message(FATAL_ERROR "Please select a crypto back-end (OPENSSL_1_1 or OPENSSL_3) [${CRYPTO}]")
endif()


###
### Library Config
###

set(LIB_NAME "${PROJECT_NAME}")

file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE LIB_GENERATED_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/include/*.h")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(${LIB_NAME} ${LIB_HEADERS} ${LIB_GENERATED_HEADERS} ${LIB_SOURCES})
target_link_libraries(${LIB_NAME} PRIVATE ${CRYPTO_LIB})
target_include_directories(${LIB_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

###
### Tests
###
if(TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

###
### Fuzzers
###
option(FUZZING "Build fuzz targets" OFF)
if(FUZZING)
  add_subdirectory(fuzz)
endif()
