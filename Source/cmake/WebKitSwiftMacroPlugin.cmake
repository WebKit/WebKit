# Superbuild glue for WebKit's Swift macro plugin (Source/WebKitSwiftMacros).
#
# The plugin is a compiler plugin and must be built for the host. CMake's
# toolchain is a property of the build tree, not of a target, so the plugin
# gets its own nested configure with the host defaults rather than being an
# ordinary target in this build.
#
# Sets WEBKIT_SWIFT_MACRO_PLUGIN to the path of the built plugin, and defines a
# target named WebKitSwiftMacros that produces it. Consumers pass the path to
# swiftc's -load-plugin-library and add_dependencies() on the target; see
# WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER.

include(ExternalProject)

set(_host_prefix "${CMAKE_BINARY_DIR}/WebKitSwiftMacros-host")
set(_host_build "${_host_prefix}/build")

# The plugin is loaded by the host's swiftc, so its file name follows host
# conventions, which need not match this build's target conventions.
if (CMAKE_HOST_WIN32)
    set(_plugin_file "WebKitSwiftMacros.dll")
elseif (CMAKE_HOST_APPLE)
    set(_plugin_file "libWebKitSwiftMacros.dylib")
else ()
    set(_plugin_file "libWebKitSwiftMacros.so")
endif ()

# ExternalProject configures and builds at build time, so find_package() cannot
# see the result during this configure. Derive the paths instead, the way LLVM
# consumes its NATIVE tblgen when cross-compiling.
set(_plugin_built "${_host_build}/${_plugin_file}")
set(WEBKIT_SWIFT_MACRO_PLUGIN "${_host_prefix}/${_plugin_file}" CACHE INTERNAL "")

# The outer build's CMAKE_MAKE_PROGRAM is WebKit's ninja-wrapper, which applies
# WebKit-specific unified-sources and cleandead policy. The host project is not
# WebKit, so hand it plain ninja when one can be found.
set(_host_make_program "${CMAKE_MAKE_PROGRAM}")
if (CMAKE_GENERATOR MATCHES "Ninja")
    find_program(WEBKIT_HOST_NINJA NAMES ninja)
    if (WEBKIT_HOST_NINJA)
        set(_host_make_program "${WEBKIT_HOST_NINJA}")
    endif ()
endif ()

# ExternalProject does not forward this build's cache, so the nested configure
# detects the host SDK, architecture and deployment target by itself. The one
# thing it must share is the compiler: a plugin has to be built by the same
# swiftc that will load it. ORIGINAL_Swift_COMPILER is the real swiftc, before
# WebKit substitutes its wrapper script.
#
# There is deliberately no install step. BUILD_BYPRODUCTS attaches to the build
# step, and ninja restats a step's outputs as soon as that step finishes, so a
# file written by a later install step would still look unchanged to everything
# downstream and would not land until the following build.
ExternalProject_Add(WebKitSwiftMacrosHost
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/Source/WebKitSwiftMacros"
    PREFIX "${_host_prefix}"
    BINARY_DIR "${_host_build}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_MAKE_PROGRAM=${_host_make_program}
        -DCMAKE_Swift_COMPILER=${ORIGINAL_Swift_COMPILER}
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    BUILD_BYPRODUCTS "${_plugin_built}"
)

# Relinking rewrites the plugin even when its behaviour has not changed, and
# WEBKIT_SWIFT_MACRO_PLUGIN is an input to every Swift compile in the build.
# Stage it through copy_if_different so an unchanged plugin keeps its mtime and
# recompiles nothing.
add_custom_command(
    OUTPUT "${WEBKIT_SWIFT_MACRO_PLUGIN}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_plugin_built}" "${WEBKIT_SWIFT_MACRO_PLUGIN}"
    DEPENDS "${_plugin_built}"
    COMMENT "Staging Swift macro plugin"
    VERBATIM
)
add_custom_target(WebKitSwiftMacros ALL DEPENDS "${WEBKIT_SWIFT_MACRO_PLUGIN}")
add_dependencies(WebKitSwiftMacros WebKitSwiftMacrosHost)
