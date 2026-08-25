# Swift macro plugin used by WebKit's Swift sources.
#
# The plugin is a compiler plugin: it runs inside swiftc on the *build* machine,
# so it is always compiled for the host, never for the target being
# cross-compiled to. It links against the swift-syntax libraries shipped inside
# the toolchain (usr/lib/swift/host), so WebKit does not vendor swift-syntax.
#
# CMake's Swift language support is configured for the target platform, which is
# why this uses a raw swiftc invocation rather than add_library().

get_filename_component(_swift_bin_dir "${ORIGINAL_Swift_COMPILER}" DIRECTORY)
get_filename_component(_swift_usr_dir "${_swift_bin_dir}" DIRECTORY)
set(SWIFT_HOST_LIBRARY_DIR "${_swift_usr_dir}/lib/swift/host")

if (NOT EXISTS "${SWIFT_HOST_LIBRARY_DIR}/libSwiftSyntaxMacros.dylib")
    message(FATAL_ERROR "swift-syntax host libraries not found in ${SWIFT_HOST_LIBRARY_DIR}")
endif ()

set(WEBKIT_SWIFT_MACRO_PLUGIN "${CMAKE_CURRENT_BINARY_DIR}/libWebKitSwiftMacros.dylib" CACHE INTERNAL "")

set(WebKitSwiftMacros_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CxxCopyMacros.swift
)

add_custom_command(
    OUTPUT ${WEBKIT_SWIFT_MACRO_PLUGIN}
    COMMAND ${ORIGINAL_Swift_COMPILER}
        -emit-library
        -o ${WEBKIT_SWIFT_MACRO_PLUGIN}
        -module-name WebKitSwiftMacros
        -swift-version 5
        -I ${SWIFT_HOST_LIBRARY_DIR}
        -L ${SWIFT_HOST_LIBRARY_DIR}
        -lSwiftSyntax
        -lSwiftSyntaxMacros
        -Xlinker -rpath -Xlinker ${SWIFT_HOST_LIBRARY_DIR}
        ${WebKitSwiftMacros_SOURCES}
    DEPENDS ${WebKitSwiftMacros_SOURCES}
    COMMENT "Building Swift macro plugin WebKitSwiftMacros"
    VERBATIM
)

add_custom_target(WebKitSwiftMacros ALL DEPENDS ${WEBKIT_SWIFT_MACRO_PLUGIN})
