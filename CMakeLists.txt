CMAKE_MINIMUM_REQUIRED(VERSION 2.8.3)
PROJECT(WebKit)

# Set a default build type if and only if user did not define one as command
# line options and he did not give custom CFLAGS or CXXFLAGS. Otherwise, flags
# from default build type would overwrite user-defined ones.
IF (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS AND NOT CMAKE_CXX_FLAGS)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF ()

# -----------------------------------------------------------------------------
# Default library type
# -----------------------------------------------------------------------------
SET(WTF_DIR "${CMAKE_SOURCE_DIR}/Source/WTF")
SET(JAVASCRIPTCORE_DIR "${CMAKE_SOURCE_DIR}/Source/JavaScriptCore")
SET(WEBCORE_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore")
SET(WEBKIT_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit")
SET(WEBKIT2_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit2")
SET(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/Source/ThirdParty")

SET(TOOLS_DIR "${CMAKE_SOURCE_DIR}/Tools")

SET(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
SET(DERIVED_SOURCES_JAVASCRIPTCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore")
SET(DERIVED_SOURCES_WEBCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebCore")
SET(DERIVED_SOURCES_WEBKIT_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit")
SET(DERIVED_SOURCES_WEBKIT2_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit2")

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Source/cmake")

SET(ENABLE_WEBCORE ON)

IF (NOT DEFINED ENABLE_WEBKIT)
    SET(ENABLE_WEBKIT ON)
ENDIF ()

IF (NOT DEFINED ENABLE_TOOLS AND EXISTS ${TOOLS_DIR})
    SET(ENABLE_TOOLS ON)
ENDIF()

INCLUDE(WebKitMacros)
INCLUDE(WebKitFS)
INCLUDE(WebKitHelpers)
INCLUDE(WebKitFeatures)

# -----------------------------------------------------------------------------
# Determine which port will be built
# -----------------------------------------------------------------------------
SET(ALL_PORTS Efl WinCE BlackBerry)
SET(PORT "NOPORT" CACHE STRING "choose which WebKit port to build (one of ${ALL_PORTS})")

LIST(FIND ALL_PORTS ${PORT} RET)
IF (${RET} EQUAL -1)
    MESSAGE(FATAL_ERROR "Please choose which WebKit port to build (one of ${ALL_PORTS})")
ENDIF ()

STRING(TOLOWER ${PORT} WEBKIT_PORT_DIR)

# -----------------------------------------------------------------------------
# Find common packages (used by all ports)
# -----------------------------------------------------------------------------
FIND_PACKAGE(BISON REQUIRED)
FIND_PACKAGE(FLEX REQUIRED)
FIND_PACKAGE(Gperf REQUIRED)
FIND_PACKAGE(Perl REQUIRED)
FIND_PACKAGE(PythonInterp REQUIRED)
FIND_PACKAGE(Ruby)

# -----------------------------------------------------------------------------
# Determine the target processor
# -----------------------------------------------------------------------------
STRING(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
IF (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    SET(WTF_CPU_ARM 1)
ELSEIF (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    SET(WTF_CPU_MIPS 1)
ELSEIF (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64)")
    SET(WTF_CPU_X86_64 1)
ELSEIF (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86)")
    SET(WTF_CPU_X86 1)
ELSE ()
    MESSAGE(FATAL_ERROR "Unknown CPU '${CMAKE_SYSTEM_PROCESSOR}'")
ENDIF ()

# -----------------------------------------------------------------------------
# Determine the operating system
# -----------------------------------------------------------------------------
IF (UNIX)
    IF (APPLE)
        SET(WTF_OS_MAC_OS_X 1)
    ELSE ()
        SET(WTF_OS_UNIX 1)
    ENDIF ()
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(WTF_OS_WINDOWS 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "QNX")
    SET(WTF_OS_QNX 1)
    SET(WTF_OS_UNIX 1)
ELSE ()
    MESSAGE(FATAL_ERROR "Unknown OS '${CMAKE_SYSTEM_NAME}'")
ENDIF ()

# -----------------------------------------------------------------------------
# Default target names (can be overrriden in Options${PORT}.cmake file)
# -----------------------------------------------------------------------------
SET(JSC_EXECUTABLE_NAME JSC)
SET(WTF_LIBRARY_NAME WTF)
SET(JavaScriptCore_LIBRARY_NAME JavaScriptCore)
SET(WebCore_LIBRARY_NAME WebCore)
SET(WebKit_LIBRARY_NAME WebKit)
SET(WebKit2_LIBRARY_NAME WebKit2)
SET(WebCoreTestSupport_LIBRARY_NAME WebCoreTestSupport)

# -----------------------------------------------------------------------------
# Default library types
# -----------------------------------------------------------------------------
OPTION(SHARED_CORE "build JavaScriptCore and WebCore as shared libraries")

IF (SHARED_CORE)
    SET(JavaScriptCore_LIBRARY_TYPE SHARED)
    SET(WebCore_LIBRARY_TYPE SHARED)
ELSE ()
    SET(JavaScriptCore_LIBRARY_TYPE STATIC)
    SET(WebCore_LIBRARY_TYPE STATIC)
ENDIF ()

SET(WebKit_LIBRARY_TYPE SHARED)
SET(WebKit2_LIBRARY_TYPE SHARED)
SET(WebCoreTestSupport_LIBRARY_TYPE STATIC)

# -----------------------------------------------------------------------------
# Common options
#------------------------------------------------------------------------------
INCLUDE(OptionsCommon)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${LIB_INSTALL_DIR})
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${LIB_INSTALL_DIR})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${EXEC_INSTALL_DIR})

# -----------------------------------------------------------------------------
# Port-specific options
# -----------------------------------------------------------------------------
INCLUDE(Options${PORT})

# -----------------------------------------------------------------------------
# Enable API unit tests and create a target for the test runner
# -----------------------------------------------------------------------------
IF (ENABLE_API_TESTS)
    ENABLE_TESTING()
ENDIF ()

# -----------------------------------------------------------------------------
# Install JavaScript shell
# -----------------------------------------------------------------------------
OPTION(SHOULD_INSTALL_JS_SHELL "generate an installation rule to install the built JavaScript shell")

# -----------------------------------------------------------------------------
# Define packaging
# -----------------------------------------------------------------------------
INCLUDE(WebKitPackaging)

# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
ADD_SUBDIRECTORY(Source)

# -----------------------------------------------------------------------------
# Add tools
# -----------------------------------------------------------------------------
IF (ENABLE_TOOLS)
    ADD_SUBDIRECTORY(Tools)
ENDIF ()

