# Swift compiler flags shared by every target that compiles Swift.
#
# Rules of thumb when adding a flag:
#
#   * If Xcode sets it in CommonBase.xcconfig, it belongs here, applied
#     globally, with a comment naming the setting it mirrors.
#   * If Xcode sets it in a per-project xcconfig, it belongs in that project's
#     CMakeLists.txt / Platform*.cmake, applied with
#     webkit_target_add_swift_options.
#   * Anything that changes the clang importer's -Xcc set must reach *every*
#     Swift target identically, or the module-cache hash forks and the SDK PCMs
#     get rebuilt per-target. See _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS in
#     WebKitMacros.cmake, which is the single home for shared -Xcc -D flags.
#
# Included from WebKitCommon.cmake after Options${PORT}.cmake, which is the
# earliest point where SWIFT_REQUIRED is known for every port.

if (NOT SWIFT_REQUIRED)
    return()
endif ()

# Helpers

# Wraps a flat list of swiftc tokens into generator expressions that only apply
# to Swift sources.
#
#   -strict-memory-safety                 -> $<$<COMPILE_LANGUAGE:Swift>:-strict-memory-safety>
#   "-Werror ExistentialAny"              -> $<$<COMPILE_LANGUAGE:Swift>:SHELL:-Werror ExistentialAny>
#   "-Xcc -I/foo"                         -> $<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I/foo>
#
function(_webkit_swift_flag_genexes _outvar)
    set(_wrapped "")
    foreach (_opt IN LISTS ARGN)
        if (_opt MATCHES "^\\$<")
            list(APPEND _wrapped "${_opt}")
        elseif (_opt MATCHES " ")
            list(APPEND _wrapped "$<$<COMPILE_LANGUAGE:Swift>:SHELL:${_opt}>")
        else ()
            list(APPEND _wrapped "$<$<COMPILE_LANGUAGE:Swift>:${_opt}>")
        endif ()
    endforeach ()
    set(${_outvar} ${_wrapped} PARENT_SCOPE)
endfunction()

# Directory-scope counterpart of target_compile_options for Swift flags.
function(webkit_add_swift_options)
    _webkit_swift_flag_genexes(_opts ${ARGN})
    if (_opts)
        add_compile_options(${_opts})
    endif ()
endfunction()

# Target-scope counterpart. Use this instead of hand-writing
# $<$<COMPILE_LANGUAGE:Swift>:SHELL:...> strings.
function(webkit_target_add_swift_options _target)
    _webkit_swift_flag_genexes(_opts ${ARGN})
    if (_opts)
        target_compile_options(${_target} PRIVATE ${_opts})
    endif ()
endfunction()

# Global flag sets

set(WEBKIT_SWIFT_LANGUAGE_FLAGS
    "-swift-version 6"
)

set(WEBKIT_SWIFT_UPCOMING_FEATURE_FLAGS
    "-enable-upcoming-feature ExistentialAny"
    "-enable-upcoming-feature InternalImportsByDefault"
    "-enable-upcoming-feature MemberImportVisibility"
)

set(WEBKIT_SWIFT_EXPERIMENTAL_FEATURE_FLAGS
    "-enable-experimental-feature DebugDescriptionMacro"
    "-enable-experimental-feature ImportCxxMembersLazily"
    "-enable-experimental-feature SuppressedAssociatedTypes"
    "-enable-experimental-feature SuppressedAssociatedTypesWithDefaults"
)

set(WEBKIT_SWIFT_MEMORY_SAFETY_FLAGS
    "-enable-experimental-feature ImportNonPublicCxxMembers"
    "-enable-experimental-feature LifetimeDependence"
    "-enable-experimental-feature Lifetimes"
    -strict-memory-safety
)

set(WEBKIT_SWIFT_MEMORY_SAFETY_ERROR_FLAGS
    "-Werror ForeignReferenceType"
    "-Werror StrictMemorySafety"
)

set(WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS
    "-Werror ExistentialAny"
    ${WEBKIT_SWIFT_MEMORY_SAFETY_ERROR_FLAGS}
)

if (CMAKE_Swift_LANGUAGE_VERSION VERSION_GREATER_EQUAL 6.4)
    list(APPEND WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS "-Werror NoUsage")
    list(APPEND WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS "-Werror NoUseUnstructuredThrowingTask")
endif ()

set(WEBKIT_SWIFT_CLANG_IMPORTER_FLAGS
    "-Xcc -fvisibility=hidden"
)

# Avoid failing due to a nested process sandbox (e.g. when building from an agent session). 
# Xcode does not do this by default.
set(WEBKIT_SWIFT_MACRO_FLAGS
    -disable-sandbox
)

set(WEBKIT_SWIFT_CONCURRENCY_FLAGS
    "-default-isolation nonisolated"
    -strict-concurrency=complete
)

# Per-target flag sets

set(WEBKIT_SWIFT_CXX_INTEROP_FLAGS
    "-Xcc -std=c++2b"
    -cxx-interoperability-mode=default
)

# Apply the global set

webkit_add_swift_options(
    ${WEBKIT_SWIFT_LANGUAGE_FLAGS}
    ${WEBKIT_SWIFT_UPCOMING_FEATURE_FLAGS}
    ${WEBKIT_SWIFT_CONCURRENCY_FLAGS}
    ${WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS}
    ${WEBKIT_SWIFT_CLANG_IMPORTER_FLAGS}
    ${WEBKIT_SWIFT_MACRO_FLAGS}
)

webkit_add_swift_options(
    "-module-cache-path ${CMAKE_BINARY_DIR}/SwiftModuleCache"
    # Needed because WebKit's modules are marked [system].
    -track-system-dependencies
)
set_property(DIRECTORY "${CMAKE_BINARY_DIR}" APPEND PROPERTY
    ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/SwiftModuleCache")

if (APPLE)
    webkit_add_swift_options(
        ${WEBKIT_SWIFT_EXPERIMENTAL_FEATURE_FLAGS}
        ${WEBKIT_SWIFT_MEMORY_SAFETY_FLAGS}
    )

    webkit_add_swift_options(
        -explicit-module-build
    )

    webkit_add_swift_options(
        # Needed for compatibility with modules in the (internal) SDK:
        # https://bugs.webkit.org/show_bug.cgi?id=312083
        "-Xcc -fexperimental-bounds-safety-attributes"
        "-Xcc -fexperimental-late-parse-attributes"
    )

    if (USE_APPLE_INTERNAL_SDK)
        webkit_add_swift_options(
            "-Xcc -I${WebKitAdditions_FRAMEWORK_HEADERS_DIR}"
        )
    endif ()

    # FIXME: Consider building with -wmo in release / performance builds.
    webkit_add_swift_options(
        -enable-batch-mode
    )
else ()
    webkit_add_swift_options(
        # Implicitly built modules are reused from the module cache without checking
        # whether the headers of a [system] module have changed, so the clang importer
        # could otherwise see stale class layouts after a header like WebPageProxy.h changes.
        "-Xcc -fmodules-validate-system-headers"
    )
endif ()
