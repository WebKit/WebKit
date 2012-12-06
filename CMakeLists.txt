cmake_minimum_required(VERSION 2.8.3)
project(WebKit)

# Set a default build type if and only if user did not define one as command
# line options and he did not give custom CFLAGS or CXXFLAGS. Otherwise, flags
# from default build type would overwrite user-defined ones.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS AND NOT CMAKE_CXX_FLAGS)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# -----------------------------------------------------------------------------
# Default library type
# -----------------------------------------------------------------------------
set(WTF_DIR "${CMAKE_SOURCE_DIR}/Source/WTF")
set(JAVASCRIPTCORE_DIR "${CMAKE_SOURCE_DIR}/Source/JavaScriptCore")
set(WEBCORE_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore")
set(WEBKIT_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit")
set(WEBKIT2_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit2")
set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/Source/ThirdParty")

set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/Tools")

set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(DERIVED_SOURCES_JAVASCRIPTCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore")
set(DERIVED_SOURCES_WEBCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebCore")
set(DERIVED_SOURCES_WEBKIT_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit")
set(DERIVED_SOURCES_WEBKIT2_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit2")

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
set(ALL_PORTS Efl WinCE BlackBerry)
set(PORT "NOPORT" CACHE STRING "choose which WebKit port to build (one of ${ALL_PORTS})")

list(FIND ALL_PORTS ${PORT} RET)
if (${RET} EQUAL -1)
    message(FATAL_ERROR "Please choose which WebKit port to build (one of ${ALL_PORTS})")
endif ()

string(TOLOWER ${PORT} WEBKIT_PORT_DIR)

# -----------------------------------------------------------------------------
# Find common packages (used by all ports)
# -----------------------------------------------------------------------------
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(Gperf REQUIRED)
find_package(Perl REQUIRED)
find_package(PythonInterp REQUIRED)
find_package(Ruby)

# -----------------------------------------------------------------------------
# Determine the target processor
# -----------------------------------------------------------------------------
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
if (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(WTF_CPU_ARM 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    set(WTF_CPU_MIPS 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64)")
    set(WTF_CPU_X86_64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86)")
    set(WTF_CPU_X86 1)
else ()
    message(FATAL_ERROR "Unknown CPU '${CMAKE_SYSTEM_PROCESSOR}'")
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
elseif (CMAKE_SYSTEM_NAME MATCHES "QNX")
    set(WTF_OS_QNX 1)
    set(WTF_OS_UNIX 1)
else ()
    message(FATAL_ERROR "Unknown OS '${CMAKE_SYSTEM_NAME}'")
endif ()

# -----------------------------------------------------------------------------
# Default target names (can be overrriden in Options${PORT}.cmake file)
# -----------------------------------------------------------------------------
set(JSC_EXECUTABLE_NAME JSC)
set(WTF_LIBRARY_NAME WTF)
set(JavaScriptCore_LIBRARY_NAME JavaScriptCore)
set(WebCore_LIBRARY_NAME WebCore)
set(WebKit_LIBRARY_NAME WebKit)
set(WebKit2_LIBRARY_NAME WebKit2)
set(WebCoreTestSupport_LIBRARY_NAME WebCoreTestSupport)

# -----------------------------------------------------------------------------
# Default library types
# -----------------------------------------------------------------------------
option(SHARED_CORE "build JavaScriptCore and WebCore as shared libraries")

if (SHARED_CORE)
    set(JavaScriptCore_LIBRARY_TYPE SHARED)
    set(WebCore_LIBRARY_TYPE SHARED)
else ()
    set(JavaScriptCore_LIBRARY_TYPE STATIC)
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
