# -----------------------------------------------------------------------------
# This file is included individually from various subdirectories (JSC, WTF,
# WebCore, WebKit) in order to allow scripts to build only part of WebKit.
# We want to run this file only once.
# -----------------------------------------------------------------------------
if (NOT HAS_RUN_WEBKIT_COMMON)
    set(HAS_RUN_WEBKIT_COMMON TRUE)

    # -----------------------------------------------------------------------------
    # Find common packages (used by all ports)
    # -----------------------------------------------------------------------------
    if (WIN32)
        list(APPEND CMAKE_PROGRAM_PATH $ENV{SystemDrive}/cygwin/bin)
    endif ()

    # TODO Enforce version requirement for perl
    find_package(Perl 5.10.0 REQUIRED)
    find_package(PerlModules COMPONENTS JSON::PP REQUIRED)

    set(Python_ADDITIONAL_VERSIONS 3)
    find_package(PythonInterp 2.7.0 REQUIRED)

    # We cannot check for RUBY_FOUND because it is set only when the full package is installed and
    # the only thing we need is the interpreter. Unlike Python, cmake does not provide a macro
    # for finding only the Ruby interpreter.
    find_package(Ruby 1.9)
    if (NOT RUBY_EXECUTABLE OR RUBY_VERSION VERSION_LESS 1.9)
        message(FATAL_ERROR "Ruby 1.9 or higher is required.")
    endif ()

    # -----------------------------------------------------------------------------
    # Helper macros and feature defines
    # -----------------------------------------------------------------------------

    # To prevent multiple inclusion, most modules should be included once here.
    include(CheckCCompilerFlag)
    include(CheckCXXCompilerFlag)
    include(CheckCXXSourceCompiles)
    include(CheckFunctionExists)
    include(CheckIncludeFile)
    include(CheckSymbolExists)
    include(CheckStructHasMember)
    include(CheckTypeSize)
    include(CMakeDependentOption)
    include(CMakeParseArguments)
    include(ProcessorCount)

    include(WebKitPackaging)
    include(WebKitMacros)
    include(WebKitFS)
    include(WebKitCCache)
    include(WebKitCompilerFlags)
    include(WebKitFeatures)

    include(OptionsCommon)
    include(Options${PORT})

    # Check gperf after including OptionsXXX.cmake since gperf is required only when ENABLE_WEBCORE is true,
    # and ENABLE_WEBCORE is configured in OptionsXXX.cmake.
    if (ENABLE_WEBCORE)
        # TODO Enforce version requirement for gperf
        find_package(Gperf 3.0.1 REQUIRED)
    endif ()

    # -----------------------------------------------------------------------------
    # Job pool to avoid running too many memory hungry linker processes
    # -----------------------------------------------------------------------------
    if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
        set_property(GLOBAL PROPERTY JOB_POOLS link_pool_jobs=4)
    else ()
        set_property(GLOBAL PROPERTY JOB_POOLS link_pool_jobs=2)
    endif ()
    set(CMAKE_JOB_POOL_LINK link_pool_jobs)

    # -----------------------------------------------------------------------------
    # Create derived sources directories
    # -----------------------------------------------------------------------------

    if (ENABLE_WEBCORE)
        file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR})
    endif ()

    if (ENABLE_WEBKIT)
        file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT_DIR})
    endif ()

    if (ENABLE_WEBKIT_LEGACY)
        file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR})
    endif ()

    if (ENABLE_WEBDRIVER)
        file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBDRIVER_DIR})
    endif ()

    # -----------------------------------------------------------------------------
    # config.h
    # -----------------------------------------------------------------------------
    CREATE_CONFIGURATION_HEADER()

    SET_CONFIGURATION_FOR_UNIFIED_SOURCE_LISTS()
endif ()
