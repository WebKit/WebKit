CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(WebKit)

# Set a default build type if and only if user did not define one as command
# line options and he did not give custom CFLAGS or CXXFLAGS. Otherwise, flags
# from default build type would overwrite user-defined ones.
IF (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS AND NOT CMAKE_CXX_FLAGS)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF ()
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

SET(JAVASCRIPTCORE_DIR "${CMAKE_SOURCE_DIR}/JavaScriptCore")
SET(WEBCORE_DIR "${CMAKE_SOURCE_DIR}/WebCore")
SET(WEBKIT_DIR "${CMAKE_SOURCE_DIR}/WebKit")
SET(WEBKITTOOLS_DIR "${CMAKE_SOURCE_DIR}/WebKitTools")
SET(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")

INCLUDE(WebKitMacros)
INCLUDE(WebKitGenerators)
INCLUDE(WebKitHelpers)
INCLUDE(WebKitFeatures)

# -----------------------------------------------------------------------------
# Determine which port will be built
# -----------------------------------------------------------------------------
SET(ALL_PORTS Efl)
OPTION(PORT "choose which WebKit port to build (one of ${ALL_PORTS})" "NOPORT")

# -----------------------------------------------------------------------------
# Find common packages (used by all ports)
# -----------------------------------------------------------------------------
FIND_PACKAGE(BISON REQUIRED)
FIND_PACKAGE(FLEX REQUIRED)
FIND_PACKAGE(Gperf REQUIRED)
FIND_PACKAGE(Perl REQUIRED)
FIND_PACKAGE(PythonInterp REQUIRED)

# -----------------------------------------------------------------------------
# Determine the target processor
# -----------------------------------------------------------------------------
IF (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    SET(WTF_CPU_ARM 1)
ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)")
    SET(WTF_CPU_X86_64 1)
ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86)")
    SET(WTF_CPU_X86 1)
ELSE ()
    MESSAGE(FATAL_ERROR "Unknown CPU '${CMAKE_SYSTEM_PROCESSOR}'")
ENDIF ()

# -----------------------------------------------------------------------------
# Determine the operating system
# -----------------------------------------------------------------------------
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(WTF_OS_UNIX 1)

    IF (APPLE)
        SET(WTF_OS_MAC_OS_X 1)
    ENDIF ()
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(WTF_OS_WINDOWS 1)
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

# -----------------------------------------------------------------------------
# Default library types
# -----------------------------------------------------------------------------
OPTION(SHARED_CORE "build WTF, JavaScriptCore and WebCore as shared libraries")

IF (SHARED_CORE)
    SET(WTF_LIBRARY_TYPE SHARED)
    SET(JavaScriptCore_LIBRARY_TYPE SHARED)
    SET(WebCore_LIBRARY_TYPE SHARED)
ELSE ()
    SET(WTF_LIBRARY_TYPE STATIC)
    SET(JavaScriptCore_LIBRARY_TYPE STATIC)
    SET(WebCore_LIBRARY_TYPE STATIC)
ENDIF ()

SET(WebKit_LIBRARY_TYPE SHARED)

# -----------------------------------------------------------------------------
# Port-specific options
# -----------------------------------------------------------------------------
INCLUDE(OptionsCommon)
INCLUDE(Options${PORT})

# -----------------------------------------------------------------------------
# Define packaging
# -----------------------------------------------------------------------------
INCLUDE(WebKitPackaging)

# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
ADD_SUBDIRECTORY(JavaScriptCore)
ADD_SUBDIRECTORY(WebCore)
ADD_SUBDIRECTORY(WebKit)

# -----------------------------------------------------------------------------
# Add test browser if exists
# -----------------------------------------------------------------------------
INCLUDE_IF_EXISTS(${WEBKITTOOLS_DIR}/CMakeLists${PORT}.txt)

# -----------------------------------------------------------------------------
# Set compiler flags for all targets
# -----------------------------------------------------------------------------
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WTF_LIBRARY_NAME})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${JavaScriptCore_LIBRARY_NAME})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebCore_LIBRARY_NAME})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebKit_LIBRARY_NAME})

# -----------------------------------------------------------------------------
# Create the configuration file
# -----------------------------------------------------------------------------
CONFIGURE_FILE(cmakeconfig.h.cmake ${CMAKE_BINARY_DIR}/cmakeconfig.h @ONLY)

# -----------------------------------------------------------------------------
# Prints a summary of all enabled/disabled features
# -----------------------------------------------------------------------------
WEBKIT_PRINT_FEATURES()
