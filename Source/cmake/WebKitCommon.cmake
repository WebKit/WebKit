# -----------------------------------------------------------------------------
# This file is included individually from various subdirectories (JSC, WTF,
# WebCore, WebKit) in order to allow scripts to build only part of WebKit.
# We want to run this file only once.
# -----------------------------------------------------------------------------
if (NOT HAS_RUN_WEBKIT_COMMON)
    set(HAS_RUN_WEBKIT_COMMON TRUE)

    if (NOT CMAKE_BUILD_TYPE)
        message(WARNING "No CMAKE_BUILD_TYPE value specified, defaulting to Release.")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    else ()
        message(STATUS "The CMake build type is: ${CMAKE_BUILD_TYPE}")
    endif ()

    # Exporting compile commands is available for Ninja and Makefile generators
    # See https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html
    if (DEVELOPER_MODE AND (CMAKE_GENERATOR MATCHES "Makefile" OR CMAKE_GENERATOR MATCHES "Ninja"))
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    endif ()

    option(ENABLE_JAVASCRIPTCORE "Enable building JavaScriptCore" ON)
    option(ENABLE_WEBCORE "Enable building JavaScriptCore" ON)
    option(ENABLE_WEBKIT "Enable building WebKit" ON)

    if (NOT ENABLE_JAVASCRIPTCORE)
        set(ENABLE_WEBCORE OFF)
    endif ()

    if (NOT ENABLE_WEBCORE)
        set(ENABLE_WEBKIT OFF)
    endif ()

    if (NOT DEFINED ENABLE_TOOLS AND EXISTS "${CMAKE_SOURCE_DIR}/Tools")
        set(ENABLE_TOOLS ON)
    endif ()

    if (NOT DEFINED ENABLE_WEBINSPECTORUI)
        set(ENABLE_WEBINSPECTORUI ON)
    endif ()

    # -----------------------------------------------------------------------------
    # Determine which port will be built
    # -----------------------------------------------------------------------------
    set(ALL_PORTS
        GTK
        JSCOnly
        Mac
        PlayStation
        WPE
        Win
    )
    set(PORT "NOPORT" CACHE STRING "choose which WebKit port to build (one of ${ALL_PORTS})")

    list(FIND ALL_PORTS ${PORT} RET)
    if (${RET} EQUAL -1)
        if (APPLE)
            set(PORT "Mac")
        else ()
            message(FATAL_ERROR "Please choose which WebKit port to build (one of ${ALL_PORTS})")
        endif ()
    endif ()

    string(TOLOWER ${PORT} WEBKIT_PORT_DIR)

    # -----------------------------------------------------------------------------
    # Determine the compiler
    # -----------------------------------------------------------------------------
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
        set(COMPILER_IS_CLANG ON)
    endif ()

    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "11.2.0")
            message(FATAL_ERROR "GCC 11.2 or newer is required to build WebKit. Use a newer GCC version or Clang.")
        endif ()
    endif ()

    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "QCC")
        set(COMPILER_IS_QCC ON)
        set(COMPILER_IS_GCC_OR_CLANG ON)
    endif ()

    if (CMAKE_COMPILER_IS_GNUCXX OR COMPILER_IS_CLANG)
        set(COMPILER_IS_GCC_OR_CLANG ON)
    endif ()

    if (MSVC AND COMPILER_IS_CLANG)
        set(COMPILER_IS_CLANG_CL ON)
    endif ()

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
    if (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(^aarch64|^arm64|^cortex-?[am][2-7][2-8])")
        set(WTF_CPU_ARM64 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(^arm|^cortex)")
        set(WTF_CPU_ARM 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips64")
        set(WTF_CPU_MIPS64 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
        set(WTF_CPU_MIPS 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(x64|x86_64|amd64)")
        # FORCE_32BIT is set in the build script when --32-bit is passed
        # on a Linux/intel 64bit host. This allows us to produce 32bit
        # binaries without setting the build up as a crosscompilation,
        # which is the only way to modify CMAKE_SYSTEM_PROCESSOR.
        if (FORCE_32BIT)
            set(WTF_CPU_X86 1)
        else ()
            set(WTF_CPU_X86_64 1)
        endif ()
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86)")
        set(WTF_CPU_X86 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(ppc|powerpc)")
        set(WTF_CPU_PPC 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64")
        set(WTF_CPU_PPC64 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
        set(WTF_CPU_PPC64LE 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv64")
        set(WTF_CPU_RISCV64 1)
    elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^loongarch64")
        set(WTF_CPU_LOONGARCH64 1)
    else ()
        set(WTF_CPU_UNKNOWN 1)
    endif ()

    # -----------------------------------------------------------------------------
    # Determine the operating system
    # -----------------------------------------------------------------------------
    if (UNIX)
        if (APPLE)
            set(WTF_OS_MACOS 1)
        elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
            set(WTF_OS_LINUX 1)
        else ()
            set(WTF_OS_UNIX 1)
        endif ()
    elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
        set(WTF_OS_WINDOWS 1)
    elseif (CMAKE_SYSTEM_NAME MATCHES "Fuchsia")
        set(WTF_OS_FUCHSIA 1)
    else ()
        message(FATAL_ERROR "Unknown OS '${CMAKE_SYSTEM_NAME}'")
    endif ()

    # -----------------------------------------------------------------------------
    # Default library types
    # -----------------------------------------------------------------------------
    # By default, only the highest-level libraries, WebKitLegacy and WebKit, are
    # shared, because properly building shared libraries that depend on each other
    # can be tricky. Override these in Options*.cmake for your port as needed.
    set(bmalloc_LIBRARY_TYPE STATIC)
    set(WTF_LIBRARY_TYPE STATIC)
    set(JavaScriptCore_LIBRARY_TYPE STATIC)
    set(PAL_LIBRARY_TYPE STATIC)
    set(WebCore_LIBRARY_TYPE STATIC)
    set(WebKitLegacy_LIBRARY_TYPE SHARED)
    set(WebKit_LIBRARY_TYPE SHARED)
    set(WebCoreTestSupport_LIBRARY_TYPE STATIC)

    if (NOT USE_BUN_JSC_ADDITIONS)
        set(CMAKE_POSITION_INDEPENDENT_CODE True)
    endif ()

    # -----------------------------------------------------------------------------
    # Install JavaScript shell
    # -----------------------------------------------------------------------------
    option(SHOULD_INSTALL_JS_SHELL "generate an installation rule to install the built JavaScript shell")

    # -----------------------------------------------------------------------------
    # Default output directories, which can be overwritten by ports
    #------------------------------------------------------------------------------
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    # -----------------------------------------------------------------------------
    # Find common packages (used by all ports)
    # -----------------------------------------------------------------------------
    if (WIN32)
        list(APPEND CMAKE_PROGRAM_PATH $ENV{SystemDrive}/cygwin/bin)
    endif ()

    # TODO Enforce version requirement for perl
    find_package(Perl 5.10.0 REQUIRED)
    find_package(PerlModules COMPONENTS English FindBin JSON::PP REQUIRED)

    # This module looks preferably for version 3 of Python. If not found, version 2 is searched.
    find_package(Python COMPONENTS Interpreter REQUIRED)
    # Set the variable with uppercase name to keep compatibility with code and users expecting it.
    set(PYTHON_EXECUTABLE ${Python_EXECUTABLE} CACHE FILEPATH "Path to the Python interpreter")

    # We cannot check for RUBY_FOUND because it is set only when the full package is installed and
    # the only thing we need is the interpreter. Unlike Python, cmake does not provide a macro
    # for finding only the Ruby interpreter.
    find_package(Ruby 2.5)
    if (NOT RUBY_EXECUTABLE OR RUBY_VERSION VERSION_LESS 2.5)
        message(FATAL_ERROR "Ruby 2.5 or higher is required.")
    endif ()

    # -----------------------------------------------------------------------------
    # Helper macros and feature defines
    # -----------------------------------------------------------------------------

    # To prevent multiple inclusion, most modules should be included once here.
    include(CheckCCompilerFlag)
    include(CheckCSourceCompiles)
    include(CheckCXXCompilerFlag)
    include(CheckCXXSourceCompiles)
    include(CheckFunctionExists)
    include(CheckIncludeFile)
    include(CheckSymbolExists)
    include(CheckStructHasMember)
    include(CheckTypeSize)
    include(CMakeDependentOption)
    include(CMakeParseArguments)
    include(CMakePushCheckState)
    include(ProcessorCount)

    include(WebKitPackaging)
    include(WebKitMacros)
    include(WebKitFS)
    include(WebKitCCache)
    include(WebKitCompilerFlags)
    include(WebKitStaticAnalysis)
    include(WebKitFeatures)
    include(WebKitFindPackage)

    include(OptionsCommon)
    include(Options${PORT})

    # Check gperf after including OptionsXXX.cmake since gperf is required only when ENABLE_WEBCORE is true,
    # and ENABLE_WEBCORE is configured in OptionsXXX.cmake.
    if (ENABLE_WEBCORE)
        # TODO Enforce version requirement for gperf
        find_package(Gperf 3.0.1 REQUIRED)
    endif ()

    # -----------------------------------------------------------------------------
    # Generate a usable compile_commands.json when using unified builds
    # -----------------------------------------------------------------------------
    # Ideally this would be done during configure or generation, but CMake doesn't
    # produce compile_commands.json until generation has ended. Having a build
    # target that processes it is the next best thing we can do.
    if (CMAKE_EXPORT_COMPILE_COMMANDS AND ENABLE_UNIFIED_BUILDS)
        # compile_commands.json
        add_custom_command(
            OUTPUT  ${CMAKE_BINARY_DIR}/DeveloperTools/compile_commands.json
            DEPENDS ${CMAKE_SOURCE_DIR}/Tools/Scripts/rewrite-compile-commands
                    ${CMAKE_BINARY_DIR}/compile_commands.json
            COMMAND ${Python_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/Tools/Scripts/rewrite-compile-commands
                    ${CMAKE_BINARY_DIR}/compile_commands.json
                    ${CMAKE_BINARY_DIR}/DeveloperTools/compile_commands.json
                    ${CMAKE_SOURCE_DIR}
                    ${CMAKE_BINARY_DIR}
            VERBATIM
        )
        add_custom_target(RewriteCompileCommands
            ALL
            DEPENDS ${CMAKE_BINARY_DIR}/DeveloperTools/compile_commands.json
        )
    elseif (CMAKE_EXPORT_COMPILE_COMMANDS)
        # Create a simple symlink in DeveloperTools/compile_commands.json so that
        # it can be relied upon regardless of unified or non-unified builds.
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/DeveloperTools)
        set(COMPILE_COMMANDS_PATH "../compile_commands.json")
        cmake_path(NATIVE_PATH COMPILE_COMMANDS_PATH COMPILE_COMMANDS_PATH)
        file(CREATE_LINK
            ${COMPILE_COMMANDS_PATH}
            ${CMAKE_BINARY_DIR}/DeveloperTools/compile_commands.json
            SYMBOLIC
        )
    endif ()

    # CLANGD_AUTO_SETUP defaults to ON if ENABLE_DEVELOPER_MODE or if building inside <SOURCE>/WebKitBuild
    set(webkitbuild_dir ${CMAKE_SOURCE_DIR}/WebKitBuild)
    cmake_path(IS_PREFIX webkitbuild_dir ${CMAKE_BINARY_DIR} NORMALIZE building_inside_WebKitBuild)
    if (building_inside_WebKitBuild OR ENABLE_DEVELOPER_MODE)
        set(clangd_auto_setup_default ON)
    else ()
        set(clangd_auto_setup_default OFF)
    endif ()
    cmake_dependent_option(CLANGD_AUTO_SETUP
        "Install a .clangd configuration file and a compile_commands.json symlink
         in the root of the source tree to have out-of-the-box code completion
         in editors."
        ${clangd_auto_setup_default}
        CMAKE_EXPORT_COMPILE_COMMANDS
        OFF
    )

    if (CLANGD_AUTO_SETUP)
        # update-compile-commands-symlink.conf
        add_custom_command(
            OUTPUT ${CMAKE_SOURCE_DIR}/update-compile-commands-symlink.conf
            DEPENDS ${TOOLS_DIR}/clangd/update-compile-commands-symlink-conf
                    ${TOOLS_DIR}/clangd/update-compile-commands-symlink.conf.example
            COMMAND ${Python_EXECUTABLE}
                    ${TOOLS_DIR}/clangd/update-compile-commands-symlink-conf
                    ${TOOLS_DIR}/clangd/update-compile-commands-symlink.conf.example
                    ${CMAKE_SOURCE_DIR}/update-compile-commands-symlink.conf
            VERBATIM
        )
        # compile_commands.json
        add_custom_target(UpdateCompileCommandsSymlink
            ALL
            DEPENDS ${CMAKE_BINARY_DIR}/DeveloperTools/compile_commands.json
                    ${CMAKE_SOURCE_DIR}/update-compile-commands-symlink.conf
            COMMAND ${Python_EXECUTABLE}
                    ${TOOLS_DIR}/clangd/update-compile-commands-symlink
                    ${CMAKE_SOURCE_DIR}/compile_commands.json
                    ${CMAKE_SOURCE_DIR}/update-compile-commands-symlink.conf
            VERBATIM
        )
        # .clangd
        add_custom_command(
            OUTPUT ${CMAKE_SOURCE_DIR}/.clangd
            DEPENDS ${TOOLS_DIR}/clangd/update-clangd-config
                    ${TOOLS_DIR}/clangd/clangd-config.yaml.tpl
            COMMAND ${Python_EXECUTABLE}
                    ${TOOLS_DIR}/clangd/update-clangd-config
                    ${TOOLS_DIR}/clangd/clangd-config.yaml.tpl
                    ${CMAKE_SOURCE_DIR}/.clangd
            VERBATIM
        )
        add_custom_target(UpdateClangdConf
            ALL
            DEPENDS ${CMAKE_SOURCE_DIR}/.clangd
        )
    endif ()

    # -----------------------------------------------------------------------------
    # Job pool to avoid running too many memory hungry processes
    # -----------------------------------------------------------------------------
    # if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    #     set_property(GLOBAL PROPERTY JOB_POOLS link_pool_jobs=4)
    # else ()
    #     list(APPEND WK_POOLS link_pool_jobs=2)
    # endif ()
    # set(CMAKE_JOB_POOL_LINK link_pool_jobs)
    # if (DEFINED ENV{WEBKIT_NINJA_COMPILE_MAX})
    #     list(APPEND WK_POOLS "compile_pool_jobs=$ENV{WEBKIT_NINJA_COMPILE_MAX}")
    #     set(CMAKE_JOB_POOL_COMPILE compile_pool_jobs)
    # endif ()
    # set_property(GLOBAL PROPERTY JOB_POOLS ${WK_POOLS})

    # -----------------------------------------------------------------------------
    # Create derived sources directories
    # -----------------------------------------------------------------------------

    file(MAKE_DIRECTORY ${WTF_DERIVED_SOURCES_DIR})
    file(MAKE_DIRECTORY ${JavaScriptCore_DERIVED_SOURCES_DIR})

    if (ENABLE_WEBCORE)
        file(MAKE_DIRECTORY ${PAL_DERIVED_SOURCES_DIR})
        file(MAKE_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR})
    endif ()

    if (ENABLE_WEBKIT)
        file(MAKE_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR})
    endif ()

    if (ENABLE_WEBKIT_LEGACY)
        file(MAKE_DIRECTORY ${WebKitLegacy_DERIVED_SOURCES_DIR})
    endif ()

    if (ENABLE_WEBDRIVER)
        file(MAKE_DIRECTORY ${WebDriver_DERIVED_SOURCES_DIR})
    endif ()

    # -----------------------------------------------------------------------------
    # config.h
    # -----------------------------------------------------------------------------
    CREATE_CONFIGURATION_HEADER()
endif ()
