# Standalone host-tool project for WebKit's Swift macro plugin.
#
# A Swift macro is a compiler plugin: swiftc dlopens it on the *build* machine,
# so it must be built for the host, never for the target WebKit is being
# cross-compiled to. CMake fixes its toolchain per build tree rather than per
# target, so this is a separate project, configured natively by the superbuild
# glue in Source/cmake/WebKitSwiftMacroPlugin.cmake, which consumes the plugin
# straight out of this project's build tree.
#
# Do NOT add_subdirectory() this from the WebKit build; that would give it the
# target toolchain and produce a plugin swiftc cannot load.
#
# The plugin links the swift-syntax libraries shipped inside the toolchain
# (usr/lib/swift/host), so WebKit does not vendor swift-syntax.

cmake_minimum_required(VERSION 3.20)

if (POLICY CMP0157)
    cmake_policy(SET CMP0157 NEW)
endif ()

project(WebKitSwiftMacros LANGUAGES Swift)

get_filename_component(_swift_bin_dir "${CMAKE_Swift_COMPILER}" DIRECTORY)
get_filename_component(_swift_usr_dir "${_swift_bin_dir}" DIRECTORY)
set(SWIFT_HOST_LIBRARY_DIR "${_swift_usr_dir}/lib/swift/host")

set(_swift_syntax_probe
    "${SWIFT_HOST_LIBRARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}SwiftSyntaxMacros${CMAKE_SHARED_LIBRARY_SUFFIX}")
if (NOT EXISTS "${_swift_syntax_probe}")
    message(FATAL_ERROR "swift-syntax host libraries not found in ${SWIFT_HOST_LIBRARY_DIR}")
endif ()

add_library(WebKitSwiftMacros SHARED CxxCopyMacros.swift)

target_compile_options(WebKitSwiftMacros PRIVATE "SHELL:-swift-version 5")
target_include_directories(WebKitSwiftMacros PRIVATE "${SWIFT_HOST_LIBRARY_DIR}")
# Link by name rather than by path: the .swiftmodule files carry autolink
# directives for further swift-syntax libraries (SwiftSyntaxBuilder and
# friends), which only resolve if the host directory is on the search path.
target_link_directories(WebKitSwiftMacros PRIVATE "${SWIFT_HOST_LIBRARY_DIR}")
target_link_libraries(WebKitSwiftMacros PRIVATE SwiftSyntax SwiftSyntaxMacros)

set_target_properties(WebKitSwiftMacros PROPERTIES
    BUILD_RPATH "${SWIFT_HOST_LIBRARY_DIR}"
)
