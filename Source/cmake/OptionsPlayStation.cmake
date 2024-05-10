set(PORT PlayStation)

string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")

include(PlayStationModule)
include(Sign)

add_definitions(-DWTF_PLATFORM_PLAYSTATION=1)
add_definitions(-DBPLATFORM_PLAYSTATION=1)

add_definitions(-DSCE_LIBC_DISABLE_CPP14_HEADER_WARNING= -DSCE_LIBC_DISABLE_CPP17_HEADER_WARNING=)

# bug-224462
WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-dll-attribute-on-redeclaration)

# Set the standard libary version
WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-sce-stdlib=v1)

set(ENABLE_WEBKIT_LEGACY OFF)
set(ENABLE_WEBINSPECTORUI OFF)

# Specify third party library directory
if (NOT WEBKIT_LIBRARIES_DIR)
    if (DEFINED ENV{WEBKIT_LIBRARIES})
        set(WEBKIT_LIBRARIES_DIR "$ENV{WEBKIT_LIBRARIES}" CACHE PATH "Path to PlayStationRequirements")
    else ()
        set(WEBKIT_LIBRARIES_DIR "${CMAKE_SOURCE_DIR}/WebKitLibraries/playstation" CACHE PATH "Path to PlayStationRequirements")
    endif ()
endif ()

if (DEFINED ENV{WEBKIT_IGNORE_PATH})
    set(CMAKE_IGNORE_PATH $ENV{WEBKIT_IGNORE_PATH})
    message(STATUS "Ignoring paths at ${CMAKE_IGNORE_PATH}")
endif ()

# Turn off pkg-config when finding PlayStation libraries
# Prevent CMake from accidentally using the perl install's .pc files
message(STATUS "Disabling pkgconfig")
set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig ON)
macro(pkg_check_modules _prefix _module0)
endmacro()

list(APPEND CMAKE_PREFIX_PATH ${WEBKIT_LIBRARIES_DIR})

# Find libraries
find_library(C_STD_LIBRARY c)
find_library(KERNEL_LIBRARY kernel)
find_package(ICU 61.2 REQUIRED COMPONENTS data i18n uc)
find_package(Threads REQUIRED)

set(USE_WPE_BACKEND_PLAYSTATION OFF)
set(PlayStationModule_TARGETS ICU::uc)

if (ENABLE_WEBCORE)
    set(WebKitRequirements_COMPONENTS WebKitResources)
    set(WebKitRequirements_OPTIONAL_COMPONENTS
        JPEG
        LibPSL
        LibXml2
        SQLite3
        WebP
        ZLIB
    )

    find_package(WPEBackendPlayStation)
    if (WPEBackendPlayStation_FOUND)
        # WPE::libwpe is compiled into the PlayStation backend
        set(WPE_NAMES SceWPE ${WPE_NAMES})
        find_package(WPE 1.14.0 REQUIRED)

        SET_AND_EXPOSE_TO_BUILD(USE_WPE_BACKEND_PLAYSTATION ON)

        set(ProcessLauncher_LIBRARY WPE::PlayStation)
        list(APPEND PlayStationModule_TARGETS WPE::PlayStation)
    else ()
        set(ProcessLauncher_LIBRARY WebKitRequirements::ProcessLauncher)
        list(APPEND WebKitRequirements_COMPONENTS
            ProcessLauncher
            libwpe
        )
    endif ()

    find_package(WebKitRequirements
        REQUIRED COMPONENTS ${WebKitRequirements_COMPONENTS}
        OPTIONAL_COMPONENTS ${WebKitRequirements_OPTIONAL_COMPONENTS}
    )

    set(Brotli_NAMES SceVshBrotli ${Brotli_NAMES})
    set(Brotli_DEC_NAMES ${Brotli_NAMES} ${Brotli_DEC_NAMES})
    set(Cairo_NAMES SceCairoForWebKit ${Cairo_NAMES})
    set(HarfBuzz_NAMES SceVshHarfbuzz ${HarfBuzz_NAMES})
    set(HarfBuzz_ICU_NAMES ${HarfBuzz_NAMES} ${HarfBuzz_ICU_NAMES})
    # The OpenGL ES implementation is in the same library as the EGL implementation
    set(OpenGLES2_NAMES ${EGL_NAMES})

    find_package(Brotli OPTIONAL_COMPONENTS dec)
    find_package(CURL 7.85.0 REQUIRED)
    find_package(EGL REQUIRED)
    find_package(Fontconfig REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(HarfBuzz REQUIRED COMPONENTS ICU)
    find_package(OpenGLES2 REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(PNG REQUIRED)

    list(APPEND PlayStationModule_TARGETS
        Brotli::dec
        CURL::libcurl
        Fontconfig::Fontconfig
        Freetype::Freetype
        HarfBuzz::HarfBuzz
        OpenSSL::SSL
        PNG::PNG
        WebKitRequirements::WebKitResources
    )

    if (NOT TARGET JPEG::JPEG)
        find_package(JPEG 1.5.2 REQUIRED)
        list(APPEND PlayStationModule_TARGETS JPEG::JPEG)
    endif ()

    if (NOT TARGET LibPSL::LibPSL)
        set(LibPSL_NAMES SceVshPsl ${LibPSL_NAMES})
        find_package(LibPSL 0.20.2 REQUIRED)
        list(APPEND PlayStationModule_TARGETS LibPSL::LibPSL)
    endif ()

    if (NOT TARGET LibXml2::LibXml2)
        find_package(LibXml2 2.9.7 REQUIRED)
        list(APPEND PlayStationModule_TARGETS LibXml2::LibXml2)
    endif ()

    if (NOT TARGET SQLite::SQLite3)
        find_package(SQLite3 3.23.1 REQUIRED)
        list(APPEND PlayStationModule_TARGETS SQLite::SQLite3)
    endif ()

    if (NOT TARGET WebP::libwebp)
        set(WebP_NAMES SceVshWebP ${WebP_NAMES})
        set(WebP_DEMUX_NAMES ${WebP_NAMES} ${WebP_DEMUX_NAMES})
        find_package(WebP REQUIRED COMPONENTS demux)
        list(APPEND PlayStationModule_TARGETS WebP::libwebp)
    endif ()

    if (NOT TARGET ZLIB::ZLIB)
        find_package(ZLIB 1.2.11 REQUIRED)
        list(APPEND PlayStationModule_TARGETS ZLIB::ZLIB)
    endif ()
endif ()

WEBKIT_OPTION_BEGIN()

# Developer mode options
SET_AND_EXPOSE_TO_BUILD(ENABLE_DEVELOPER_MODE ${DEVELOPER_MODE})
if (ENABLE_DEVELOPER_MODE)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LAYOUT_TESTS PUBLIC ${USE_WPE_BACKEND_PLAYSTATION})
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC ${ENABLE_WEBKIT})
endif ()

# PlayStation Specific Options
WEBKIT_OPTION_DEFINE(ENABLE_STATIC_JSC "Control whether to build a non-shared JSC" PUBLIC ON)

# Turn off JIT
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_JIT PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTL_JIT PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DFG_JIT PRIVATE OFF)

# Don't use IsoMalloc
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_ISO_MALLOC PRIVATE OFF)

# Enabled features
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FULLSCREEN_API PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_CACHE_STALE_WHILE_REVALIDATE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PERIODIC_MEMORY_MONITOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_REMOTE_INSPECTOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SMOOTH_SCROLLING PRIVATE ON)

if (USE_WPE_BACKEND_PLAYSTATION)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER PRIVATE ON)
endif ()

# Experimental features
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLICATION_MANIFEST PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})

if (USE_WPE_BACKEND_PLAYSTATION)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
endif ()

# Enable variation fonts when cairo >= 1.16, fontconfig >= 2.13.0, freetype >= 2.9.0 and harfbuzz >= 1.4.2
if (Cairo_VERSION VERSION_GREATER_EQUAL 1.16.0 AND Fontconfig_VERSION VERSION_GREATER_EQUAL 2.13.0 AND FREETYPE_VERSION_STRING VERSION_GREATER_EQUAL 2.9.0 AND HarfBuzz_VERSION VERSION_GREATER_EQUAL 1.4.2)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
else ()
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE OFF)
endif ()

# Features to investigate
#
# Features that require additional implementation pieces
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_AVIF PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_LCMS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_JPEGXL PRIVATE OFF)

# Features that are temporarily turned off because an implementation is not
# present at this time
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GPU_PROCESS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE OFF)

# Enable in the future
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTEXT_MENUS PRIVATE OFF)

# No support planned
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTPDIR PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GEOLOCATION PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IMAGE_DIFF PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MATHML PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NOTIFICATIONS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_USER_MESSAGE_HANDLERS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGL PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_XSLT PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_WOFF2 PRIVATE OFF)

WEBKIT_OPTION_END()

# Do not use a separate directory based on configuration when building
# with the Visual Studio generator
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Default to hidden visibility
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_C_STANDARD_LIBRARIES
    "${CMAKE_C_STANDARD_LIBRARIES} ${C_STD_LIBRARY}"
)
set(CMAKE_CXX_STANDARD_LIBRARIES
    "${CMAKE_CXX_STANDARD_LIBRARIES} ${C_STD_LIBRARY}"
)

SET_AND_EXPOSE_TO_BUILD(HAVE_PTHREAD_SETNAME_NP ON)
SET_AND_EXPOSE_TO_BUILD(HAVE_MAP_ALIGNED OFF)

# Platform options
SET_AND_EXPOSE_TO_BUILD(USE_INSPECTOR_SOCKET_SERVER ${ENABLE_REMOTE_INSPECTOR})
SET_AND_EXPOSE_TO_BUILD(USE_UNIX_DOMAIN_SOCKETS ON)

if (ENABLE_WEBCORE)
    SET_AND_EXPOSE_TO_BUILD(USE_CURL ON)
    SET_AND_EXPOSE_TO_BUILD(USE_HARFBUZZ ON)
    SET_AND_EXPOSE_TO_BUILD(USE_LIBWPE ON)
    SET_AND_EXPOSE_TO_BUILD(USE_OPENSSL ON)

    if (NOT USE_SKIA)
        find_package(Cairo REQUIRED)
        list(APPEND PlayStationModule_TARGETS Cairo::Cairo)

        SET_AND_EXPOSE_TO_BUILD(USE_CAIRO ON)
        SET_AND_EXPOSE_TO_BUILD(USE_FREETYPE ON)
    endif ()

    if (USE_LCMS)
        set(LCMS2_NAMES SceVshLCMS2)
        find_package(LCMS2)
        if (NOT LCMS2_FOUND)
            message(FATAL_ERROR "libcms2 is required for USE_LCMS.")
       endif ()
       list(APPEND PlayStationModule_TARGETS LCMS2::LCMS2)
    endif ()

    if (USE_JPEGXL)
        set(JPEGXL_NAMES SceVshJxl)
        find_package(JPEGXL 0.7.0)
        if (NOT JPEGXL_FOUND)
            message(FATAL_ERROR "libjxl is required for USE_JPEGXL")
        endif ()
        list(APPEND PlayStationModule_TARGETS JPEGXL::jxl)
    endif ()

    # See if OpenSSL implementation is BoringSSL
    cmake_push_check_state()
    set(CMAKE_REQUIRED_INCLUDES "${OPENSSL_INCLUDE_DIR}")
    set(CMAKE_REQUIRED_LIBRARIES "${OPENSSL_LIBRARIES}")
    WEBKIT_CHECK_HAVE_SYMBOL(USE_BORINGSSL OPENSSL_IS_BORINGSSL openssl/ssl.h)
    cmake_pop_check_state()

    # See if FreeType implementation supports WOFF2 fonts
    include(WOFF2Checks)
    if (FREETYPE_WOFF2_SUPPORT_IS_AVAILABLE)
        SET_AND_EXPOSE_TO_BUILD(HAVE_WOFF_SUPPORT ON)
    endif ()

    # Rendering options
    SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER ON)

    if (USE_WPE_BACKEND_PLAYSTATION)
        SET_AND_EXPOSE_TO_BUILD(USE_WPE_RENDERER ON)
        SET_AND_EXPOSE_TO_BUILD(HAVE_DISPLAY_LINK ON)
    endif ()

    if (ENABLE_GPU_PROCESS)
        SET_AND_EXPOSE_TO_BUILD(USE_GRAPHICS_LAYER_TEXTURE_MAPPER ON)
        SET_AND_EXPOSE_TO_BUILD(USE_GRAPHICS_LAYER_WC ON)
    else ()
        SET_AND_EXPOSE_TO_BUILD(ENABLE_SCROLLING_THREAD ${ENABLE_ASYNC_SCROLLING})

        SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS ON)
        SET_AND_EXPOSE_TO_BUILD(USE_NICOSIA ON)
    endif ()
endif ()

# WebDriver options
if (ENABLE_WEBDRIVER)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_KEYBOARD_INTERACTIONS ON)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS ON)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_TOUCH_INTERACTIONS OFF)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_WHEEL_INTERACTIONS ON)
endif ()

if (ENABLE_MINIBROWSER)
    find_library(TOOLKIT_LIBRARY ToolKitten)
    if (NOT TOOLKIT_LIBRARY)
        message(FATAL_ERROR "ToolKit library required to run MiniBrowser")
    endif ()

    # ToolKitten has a dependency on Cairo so find it here but don't turn on Cairo
    if (USE_SKIA)
        find_package(Cairo REQUIRED)
        list(APPEND PlayStationModule_TARGETS Cairo::Cairo)
    endif ()
endif ()

# Create a shared JavaScriptCore with WTF and bmalloc exposed through it.
#
# Use OBJECT libraries for bmalloc and WTF. This is the modern CMake way to emulate
# the behavior of --whole-archive. If this is not done then all the exports will
# not be exposed.
set(bmalloc_LIBRARY_TYPE OBJECT)
set(WTF_LIBRARY_TYPE OBJECT)

if (ENABLE_STATIC_JSC)
    set(JavaScriptCore_LIBRARY_TYPE OBJECT)
else ()
    set(JavaScriptCore_LIBRARY_TYPE SHARED)
endif ()

# Create a shared WebKit
#
# Use OBJECT libraries for PAL and WebCore. The size of a libWebCore.a is too much
# for ranlib.
set(PAL_LIBRARY_TYPE OBJECT)
set(WebCore_LIBRARY_TYPE OBJECT)
set(WebKit_LIBRARY_TYPE SHARED)

# Enable multi process builds for Visual Studio
if (NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
    add_definitions(/MP)
endif ()

find_package(libdl)
if (TARGET libdl::dl)
    add_link_options("$<$<OR:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>>:$<TARGET_PROPERTY:libdl::dl,IMPORTED_LOCATION_RELEASE>>")
    add_link_options("$<$<OR:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>>:$<TARGET_PROPERTY:libdl::dl,INTERFACE_LINK_OPTIONS>>")
endif ()

function(add_executable target)
    _add_executable(${ARGV})
    target_link_options(${target} PRIVATE -Wl,--wrap=mmap)
endfunction()

function(add_library target type)
    _add_library(${ARGV})
    if ("${type}" STREQUAL "SHARED")
        target_link_options(${target} PRIVATE -Wl,--wrap=mmap)
        sign(${target})
    endif ()
endfunction()

add_custom_target(playstation_tools_copy
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${WEBKIT_LIBRARIES_DIR}/tools/sce_sys/
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sce_sys/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${WEBKIT_LIBRARIES_DIR}/tools/scripts/
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts/
    COMMAND ${CMAKE_COMMAND} -E touch
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ebootparam.ini
)
set_target_properties(playstation_tools_copy PROPERTIES FOLDER "PlayStation")

macro(WEBKIT_EXECUTABLE _target)
    _WEBKIT_EXECUTABLE(${_target})
    playstation_setup_libc(${_target})
    playstation_setup_fp(${_target})
    if (NOT ${_target} MATCHES "^LLInt")
        sign(${_target})
    endif ()
    if (PLAYSTATION_${_target}_WRAP)
        foreach (WRAP ${PLAYSTATION_${_target}_WRAP})
            target_link_options(${_target} PRIVATE -Wl,--wrap=${WRAP})
        endforeach ()
    endif ()
    add_dependencies(${_target} playstation_tools_copy)
endmacro()

macro(PLAYSTATION_MODULES)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs TARGETS)
    cmake_parse_arguments(opt "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach (_target IN LISTS opt_TARGETS)
        string(REGEX MATCH "^(.+)::(.+)$" _is_stub "${_target}")
        set(_target_name ${CMAKE_MATCH_1})
        playstation_module(${_target_name} TARGET ${_target} FOLDER "PlayStation")

        if (${_target_name}_IS_SYSMODULE)
            EXPOSE_VARIABLE_TO_BUILD(${_target_name}_IS_SYSMODULE)
        endif ()
        if (${_target_name}_LOAD_AT)
            EXPOSE_STRING_VARIABLE_TO_BUILD(${_target_name}_LOAD_AT)
        endif ()
    endforeach ()
endmacro()

macro(PLAYSTATION_COPY_MODULES _target_name)
    set(multiValueArgs TARGETS)
    cmake_parse_arguments(opt "" "" "${multiValueArgs}" ${ARGN})

    foreach (_target IN LISTS opt_TARGETS)
        if (TARGET ${_target}_CopyModule)
            list(APPEND ${_target_name}_INTERFACE_DEPENDENCIES ${_target}_CopyModule)
        endif ()
    endforeach ()
endmacro()

PLAYSTATION_MODULES(TARGETS ${PlayStationModule_TARGETS})

# These should be made into proper CMake targets
if (EGL_LIBRARIES)
    playstation_module(EGL TARGET ${EGL_LIBRARIES} FOLDER "PlayStation")
    if (EGL_LOAD_AT)
        EXPOSE_STRING_VARIABLE_TO_BUILD(EGL_LOAD_AT)
    endif ()
endif ()

if (TOOLKIT_LIBRARY)
    playstation_module(ToolKitten TARGET ${TOOLKIT_LIBRARY} FOLDER "PlayStation")
    if (ToolKitten_LOAD_AT)
        EXPOSE_STRING_VARIABLE_TO_BUILD(ToolKitten_LOAD_AT)
    endif ()
endif ()

check_symbol_exists(memmem string.h HAVE_MEMMEM)
if (HAVE_MEMMEM)
    add_definitions(-DHAVE_MEMMEM=1)
endif ()
