cmake_minimum_required(VERSION 2.8.3)
project(WebKit)

# -----------------------------------------------------------------------------
# Default library type
# -----------------------------------------------------------------------------
set(WTF_DIR "${CMAKE_SOURCE_DIR}/Source/WTF")
set(JAVASCRIPTCORE_DIR "${CMAKE_SOURCE_DIR}/Source/JavaScriptCore")
set(WEBCORE_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore")
set(WEBKIT_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit")
set(WEBKIT2_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit2")
set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/Source/ThirdParty")
set(PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Source/Platform")

set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/Tools")

set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(DERIVED_SOURCES_JAVASCRIPTCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore")
set(DERIVED_SOURCES_WEBCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebCore")
set(DERIVED_SOURCES_WEBKIT_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit")
set(DERIVED_SOURCES_WEBKIT2_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit2")
set(DERIVED_SOURCES_WEBINSPECTORUI_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebInspectorUI")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Source/cmake")

set(ENABLE_WEBCORE ON)

if (NOT DEFINED ENABLE_WEBKIT)
    set(ENABLE_WEBKIT ON)
endif ()

if (NOT DEFINED ENABLE_TOOLS AND EXISTS ${TOOLS_DIR})
    set(ENABLE_TOOLS ON)
endif ()

include(WebKitMacros)
include(WebKitFS)
include(WebKitHelpers)
include(WebKitFeatures)

# -----------------------------------------------------------------------------
# Determine which port will be built
# -----------------------------------------------------------------------------
set(ALL_PORTS Efl WinCE GTK)
set(PORT "NOPORT" CACHE STRING "choose which WebKit port to build (one of ${ALL_PORTS})")

list(FIND ALL_PORTS ${PORT} RET)
if (${RET} EQUAL -1)
    message(FATAL_ERROR "Please choose which WebKit port to build (one of ${ALL_PORTS})")
endif ()

string(TOLOWER ${PORT} WEBKIT_PORT_DIR)

# -----------------------------------------------------------------------------
# Find common packages (used by all ports)
# -----------------------------------------------------------------------------
find_package(BISON 2.4.1 REQUIRED)
find_package(FLEX 2.5.34 REQUIRED)

# TODO Enforce version requirement for gperf
find_package(Gperf 3.0.1 REQUIRED)

# TODO Enforce version requirement for perl
find_package(Perl 5.10.0 REQUIRED)

find_package(PythonInterp 2.6.0 REQUIRED)
find_package(Ruby 1.8.7)

# -----------------------------------------------------------------------------
# Determine the target processor
# -----------------------------------------------------------------------------
# Use MSVC_CXX_ARCHITECTURE_ID instead of CMAKE_SYSTEM_PROCESSOR when defined,
# since the later one just resolves to the host processor on Windows.
if (MSVC_CXX_ARCHITECTURE_ID)
    string(TOLOWER ${MSVC_CXX_ARCHITECTURE_ID} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
else ()
    string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
endif ()
if (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(WTF_CPU_ARM 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
    set(WTF_CPU_ARM64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    set(WTF_CPU_MIPS 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "sh4")
    set(WTF_CPU_SH4 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(x64|x86_64|amd64)")
    set(WTF_CPU_X86_64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86)")
    set(WTF_CPU_X86 1)
else ()
    message(FATAL_ERROR "Unknown CPU '${LOWERCASE_CMAKE_SYSTEM_PROCESSOR}'")
endif ()

# -----------------------------------------------------------------------------
# Determine the operating system
# -----------------------------------------------------------------------------
if (UNIX)
    if (APPLE)
        set(WTF_OS_MAC_OS_X 1)
    else ()
        set(WTF_OS_UNIX 1)
    endif ()
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(WTF_OS_WINDOWS 1)
else ()
    message(FATAL_ERROR "Unknown OS '${CMAKE_SYSTEM_NAME}'")
endif ()

# -----------------------------------------------------------------------------
# Default library types
# -----------------------------------------------------------------------------
option(SHARED_CORE "build JavaScriptCore and WebCore as shared libraries")

if (SHARED_CORE)
    set(JavaScriptCore_LIBRARY_TYPE SHARED)
    set(WebCore_LIBRARY_TYPE SHARED)
else ()
    if (${PORT} STREQUAL GTK)
        set(JavaScriptCore_LIBRARY_TYPE SHARED)
    else ()
        set(JavaScriptCore_LIBRARY_TYPE STATIC)
    endif ()
    set(WebCore_LIBRARY_TYPE STATIC)
endif ()

set(WebKit_LIBRARY_TYPE SHARED)
set(WebKit2_LIBRARY_TYPE SHARED)
set(WebCoreTestSupport_LIBRARY_TYPE STATIC)

# -----------------------------------------------------------------------------
# Common options
#------------------------------------------------------------------------------
include(OptionsCommon)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${LIB_INSTALL_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${LIB_INSTALL_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${EXEC_INSTALL_DIR})

# -----------------------------------------------------------------------------
# Port-specific options
# -----------------------------------------------------------------------------
include(Options${PORT})

# -----------------------------------------------------------------------------
# Enable API unit tests and create a target for the test runner
# -----------------------------------------------------------------------------
if (ENABLE_API_TESTS)
    enable_testing()
endif ()

# -----------------------------------------------------------------------------
# Install JavaScript shell
# -----------------------------------------------------------------------------
option(SHOULD_INSTALL_JS_SHELL "generate an installation rule to install the built JavaScript shell")

# -----------------------------------------------------------------------------
# Define packaging
# -----------------------------------------------------------------------------
include(WebKitPackaging)

# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
add_subdirectory(Source)

# -----------------------------------------------------------------------------
# Add tools
# -----------------------------------------------------------------------------
if (ENABLE_TOOLS)
    add_subdirectory(Tools)
endif ()
