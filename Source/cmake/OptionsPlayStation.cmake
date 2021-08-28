set(PORT PlayStation)

include(CheckSymbolExists)

string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")

include(Sign)

add_definitions(-DWTF_PLATFORM_PLAYSTATION=1)
add_definitions(-DBPLATFORM_PLAYSTATION=1)

add_definitions(-DSCE_LIBC_DISABLE_CPP14_HEADER_WARNING= -DSCE_LIBC_DISABLE_CPP17_HEADER_WARNING=)

# bug-224462
WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-Wno-dll-attribute-on-redeclaration)

set(ENABLE_API_TESTS ON CACHE BOOL "Build API Tests")
set(ENABLE_WEBCORE ON CACHE BOOL "Build WebCore")
set(ENABLE_WEBKIT ON CACHE BOOL "Build WebKit")
set(ENABLE_WEBKIT_LEGACY OFF)
set(ENABLE_WEBINSPECTORUI OFF)

if (NOT ENABLE_WEBCORE)
    set(ENABLE_WEBKIT OFF)
endif ()

WEBKIT_OPTION_BEGIN()

# PlayStation Specific Options
WEBKIT_OPTION_DEFINE(ENABLE_STATIC_JSC "Control whether to build a non-shared JSC" PUBLIC ON)

# Turn off JIT
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_JIT PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTL_JIT PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DFG_JIT PRIVATE OFF)

# Enabled features
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCESSIBILITY PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FULLSCREEN_API PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PERIODIC_MEMORY_MONITOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SMOOTH_SCROLLING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_LOAD_STATISTICS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE ON)

# Experimental features
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLICATION_MANIFEST PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_PAINTING_API PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_TYPED_OM PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FILTERS_LEVEL_2 PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GPU_PROCESS PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LAYOUT_FORMATTING_CONTEXT PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_REMOTE_INSPECTOR PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_WORKER PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_CRYPTO PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})

# Features to investigate
#
# Features that are temporarily turned off because an implementation is not
# present at this time
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE OFF)

# TLS debugging feature
WEBKIT_OPTION_DEFINE(ENABLE_TLS_DEBUG "Enable TLS key log support" PRIVATE OFF)

# Reenable after updating fontconfig
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE OFF)

# Enable in the future
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTEXT_MENUS PRIVATE OFF)

# No support planned
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTPDIR PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GEOLOCATION PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MATHML PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETSCAPE_PLUGIN_API PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NOTIFICATIONS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_USER_MESSAGE_HANDLERS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGL PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_XSLT PRIVATE OFF)

WEBKIT_OPTION_END()

if (DEFINED ENV{WEBKIT_IGNORE_PATH})
    set(CMAKE_IGNORE_PATH $ENV{WEBKIT_IGNORE_PATH})
endif ()

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

# Specify third party library directory
if (NOT WEBKIT_LIBRARIES_DIR)
    if (DEFINED ENV{WEBKIT_LIBRARIES})
        set(WEBKIT_LIBRARIES_DIR "$ENV{WEBKIT_LIBRARIES}" CACHE PATH "Path to PlayStationRequirements")
    else ()
        set(WEBKIT_LIBRARIES_DIR "${CMAKE_SOURCE_DIR}/WebKitLibraries/playstation" CACHE PATH "Path to PlayStationRequirements")
    endif ()
endif ()

list(APPEND CMAKE_PREFIX_PATH ${WEBKIT_LIBRARIES_DIR})

find_library(C_STD_LIBRARY c)
find_library(KERNEL_LIBRARY kernel)
find_package(WebKitRequirements REQUIRED
    COMPONENTS
        JPEG
        LibPSL
        LibXml2
        ProcessLauncher
        SQLite3
        ZLIB
        libwpe
)

# The OpenGL ES implementation is in the same library as the EGL implementation
set(OpenGLES2_NAMES ${EGL_NAMES})

find_package(Cairo REQUIRED)
find_package(CURL REQUIRED)
find_package(EGL REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(Freetype REQUIRED)
find_package(HarfBuzz REQUIRED COMPONENTS ICU)
find_package(ICU 61.2 REQUIRED COMPONENTS data i18n uc)
find_package(OpenGLES2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)
find_package(WebP REQUIRED COMPONENTS demux)

set(CMAKE_C_STANDARD_LIBRARIES
    "${CMAKE_C_STANDARD_LIBRARIES} ${C_STD_LIBRARY}"
  )
set(CMAKE_CXX_STANDARD_LIBRARIES
    "${CMAKE_CXX_STANDARD_LIBRARIES} ${C_STD_LIBRARY}"
  )

SET_AND_EXPOSE_TO_BUILD(HAVE_PTHREAD_SETNAME_NP ON)

SET_AND_EXPOSE_TO_BUILD(USE_CAIRO ON)
SET_AND_EXPOSE_TO_BUILD(USE_CURL ON)
SET_AND_EXPOSE_TO_BUILD(USE_FREETYPE ON)
SET_AND_EXPOSE_TO_BUILD(USE_HARFBUZZ ON)
SET_AND_EXPOSE_TO_BUILD(USE_LIBWPE ON)
SET_AND_EXPOSE_TO_BUILD(USE_OPENSSL ON)
SET_AND_EXPOSE_TO_BUILD(USE_WPE_RENDERER OFF)

SET_AND_EXPOSE_TO_BUILD(USE_INSPECTOR_SOCKET_SERVER ${ENABLE_REMOTE_INSPECTOR})
SET_AND_EXPOSE_TO_BUILD(USE_UNIX_DOMAIN_SOCKETS ON)

# Rendering options
SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS ON)
SET_AND_EXPOSE_TO_BUILD(USE_EGL ON)
SET_AND_EXPOSE_TO_BUILD(USE_NICOSIA TRUE)
SET_AND_EXPOSE_TO_BUILD(USE_OPENGL_ES ON)
SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER ON)
SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER_GL ON)
SET_AND_EXPOSE_TO_BUILD(USE_TILED_BACKING_STORE ON)

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

function(add_library target type)
    _add_library(${ARGV})
    if ("${type}" STREQUAL "SHARED")
        sign(${target})
    endif ()
endfunction()

add_custom_target(playstation_tools_copy
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${WEBKIT_LIBRARIES_DIR}/tools/sce_sys/
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sce_sys/
    COMMAND ${CMAKE_COMMAND} -E touch
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ebootparam.ini
)

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

set_property(GLOBAL PROPERTY playstation_copied_requirements)

function(PLAYSTATION_COPY_REQUIREMENTS target_name)
    set(oneValueArgs PREFIX DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (opt_PREFIX)
        set(prefix ${opt_PREFIX})
    else ()
        set(prefix ${WEBKIT_LIBRARIES_DIR})
    endif ()
    if (opt_DESTINATION)
        set(destination ${opt_DESTINATION})
    else ()
        set(destination ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif ()

    set(files_to_copy)
    list(REMOVE_DUPLICATES opt_FILES)
    foreach (file IN LISTS opt_FILES)
        if (NOT (${file} MATCHES ".*_stub_weak\.a" OR
                 ${file} MATCHES ".*\.sprx" OR
                 ${file} MATCHES ".*\.elf" OR
                 ${file} MATCHES ".*\.self"))
            continue()
        endif ()
        file(RELATIVE_PATH _relative ${prefix} ${file})
        if (NOT ${_relative} MATCHES "^\.\./.*")
            get_filename_component(lib ${file} NAME)
            list(APPEND files_to_copy ${lib})
        endif ()
    endforeach ()

    set(dst_requirements)
    get_property(copied_requirements GLOBAL PROPERTY playstation_copied_requirements)
    foreach (basefilename IN LISTS files_to_copy)
        string(REPLACE "_stub_weak.a" ".sprx" filename ${basefilename})
        set(src_file "${prefix}/bin/${filename}")
        list(FIND copied_requirements ${src_file} found)
        if (${found} GREATER_EQUAL 0)
            continue()
        endif ()
        if (NOT EXISTS ${src_file})
            continue()
        endif ()
        list(APPEND copied_requirements ${src_file})
        set(dst_file "${destination}/${filename}")
        add_custom_command(OUTPUT ${dst_file}
            COMMAND ${CMAKE_COMMAND} -E copy ${src_file} ${dst_file}
            MAIN_DEPENDENCY ${file}
            VERBATIM
        )
        list(APPEND dst_requirements ${dst_file})
    endforeach ()
    add_custom_target(${target_name} ALL DEPENDS ${dst_requirements})
    set_property(GLOBAL PROPERTY playstation_copied_requirements ${copied_requirements} ${dst_requirements})
endfunction()

check_symbol_exists(memmem string.h HAVE_MEMMEM)
if (HAVE_MEMMEM)
    add_definitions(-DHAVE_MEMMEM=1)
endif ()
