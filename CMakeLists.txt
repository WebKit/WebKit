cmake_minimum_required(VERSION 2.8.12)
project(WebKit)

# This should be removed once we require CMake 3.3.
if (POLICY CMP0058)
    cmake_policy(SET CMP0058 NEW)
endif ()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Source/cmake")

if (NOT DEFINED ENABLE_WEBKIT)
    set(ENABLE_WEBKIT ON)
endif ()

if (NOT DEFINED ENABLE_TOOLS AND EXISTS "${CMAKE_SOURCE_DIR}/Tools")
    set(ENABLE_TOOLS ON)
endif ()

# -----------------------------------------------------------------------------
# Determine which port will be built
# -----------------------------------------------------------------------------
set(ALL_PORTS Efl GTK AppleWin WinCairo Mac)
set(PORT "NOPORT" CACHE STRING "choose which WebKit port to build (one of ${ALL_PORTS})")

list(FIND ALL_PORTS ${PORT} RET)
if (${RET} EQUAL -1)
    message(FATAL_ERROR "Please choose which WebKit port to build (one of ${ALL_PORTS})")
endif ()

string(TOLOWER ${PORT} WEBKIT_PORT_DIR)

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
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc")
    set(WTF_CPU_PPC 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64")
    set(WTF_CPU_PPC64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
    set(WTF_CPU_PPC64LE 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "parisc*")
    set(WTF_CPU_HPPA 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "s390")
    set(WTF_CPU_S390 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "s390x")
    set(WTF_CPU_S390X 1)
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
    set(JavaScriptCore_LIBRARY_TYPE SHARED)
    set(WebCore_LIBRARY_TYPE STATIC)
endif ()

set(WebKit_LIBRARY_TYPE SHARED)
set(WebKit2_LIBRARY_TYPE SHARED)
set(WebCoreTestSupport_LIBRARY_TYPE STATIC)

# -----------------------------------------------------------------------------
# Install JavaScript shell
# -----------------------------------------------------------------------------
option(SHOULD_INSTALL_JS_SHELL "generate an installation rule to install the built JavaScript shell")

# -----------------------------------------------------------------------------
# Output directories
#------------------------------------------------------------------------------
if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()
if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

# -----------------------------------------------------------------------------
# Common configuration
#------------------------------------------------------------------------------
include(WebKitCommon)

# -----------------------------------------------------------------------------
# Enable API unit tests and create a target for the test runner
# -----------------------------------------------------------------------------
if (ENABLE_API_TESTS)
    enable_testing()
endif ()

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

# -----------------------------------------------------------------------------
# Print the features list last, for maximum visibility.
# -----------------------------------------------------------------------------
PRINT_WEBKIT_OPTIONS()
