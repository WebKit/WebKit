# Swift compiler flags shared by every target that compiles Swift.
#
# Rules of thumb when adding a flag:
#
#   * If Xcode sets it in CommonBase.xcconfig, it belongs here, applied
#     globally, with a comment naming the setting it mirrors.
#   * If Xcode sets it in a per-project xcconfig, it belongs in that project's
#     CMakeLists.txt / Platform*.cmake, applied with
#     WEBKIT_TARGET_ADD_SWIFT_OPTIONS.
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

# Swift driver flags that consume the following token as their value. Used to
# keep multi-token groups in a single SHELL: entry, because CMake deduplicates
# repeated tokens in COMPILE_OPTIONS and would otherwise drop the second
# "-Xcc" of "-Xcc -I/a -Xcc -I/b" and leave "-I/b" unguarded.
set(WEBKIT_SWIFT_FLAGS_TAKING_VALUE
    -Werror
    -Xcc
    -Xfrontend
    -clang-target
    -default-isolation
    -emit-clang-header-path
    -emit-module-interface-path
    -emit-private-module-interface-path
    -enable-experimental-feature
    -enable-upcoming-feature
    -import-objc-header
    -library-level
    -module-cache-path
    -swift-version
)

# Wraps a flat list of swiftc tokens into generator expressions that only apply
# to Swift sources, pairing each value-taking flag with its value.
#
#   -strict-memory-safety                 -> $<$<COMPILE_LANGUAGE:Swift>:-strict-memory-safety>
#   -Werror ExistentialAny                -> $<$<COMPILE_LANGUAGE:Swift>:SHELL:-Werror ExistentialAny>
#   -Xcc -I/foo                           -> $<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I/foo>
#
# Tokens that are already generator expressions are passed through untouched, so
# callers can mix in $<CONFIG:...> genexes.
function(_webkit_swift_flag_genexes _outvar)
    set(_wrapped "")
    set(_pending "")
    foreach (_opt IN LISTS ARGN)
        if (_pending)
            list(APPEND _wrapped "$<$<COMPILE_LANGUAGE:Swift>:SHELL:${_pending} ${_opt}>")
            set(_pending "")
        elseif (_opt MATCHES "^\\$<")
            list(APPEND _wrapped "${_opt}")
        elseif (_opt IN_LIST WEBKIT_SWIFT_FLAGS_TAKING_VALUE)
            set(_pending "${_opt}")
        else ()
            list(APPEND _wrapped "$<$<COMPILE_LANGUAGE:Swift>:${_opt}>")
        endif ()
    endforeach ()
    if (_pending)
        message(FATAL_ERROR "WEBKIT_SWIFT: ${_pending} is missing its value argument")
    endif ()
    set(${_outvar} ${_wrapped} PARENT_SCOPE)
endfunction()

# Directory-scope counterpart of target_compile_options for Swift flags.
function(WEBKIT_ADD_SWIFT_OPTIONS)
    _webkit_swift_flag_genexes(_opts ${ARGN})
    if (_opts)
        add_compile_options(${_opts})
    endif ()
endfunction()

# Target-scope counterpart. Use this instead of hand-writing
# $<$<COMPILE_LANGUAGE:Swift>:SHELL:...> strings.
function(WEBKIT_TARGET_ADD_SWIFT_OPTIONS _target)
    _webkit_swift_flag_genexes(_opts ${ARGN})
    if (_opts)
        target_compile_options(${_target} PRIVATE ${_opts})
    endif ()
endfunction()

# Global flag sets

set(WEBKIT_SWIFT_LANGUAGE_FLAGS
    -swift-version 6
)

set(WEBKIT_SWIFT_UPCOMING_FEATURE_FLAGS
    -enable-upcoming-feature ExistentialAny
    -enable-upcoming-feature InternalImportsByDefault
    -enable-upcoming-feature MemberImportVisibility
)

set(WEBKIT_SWIFT_EXPERIMENTAL_FEATURE_FLAGS
    -enable-experimental-feature ImportCxxMembersLazily
    -enable-experimental-feature SuppressedAssociatedTypes
    -enable-experimental-feature SuppressedAssociatedTypesWithDefaults
)

set(WEBKIT_SWIFT_MEMORY_SAFETY_FLAGS
    -enable-experimental-feature ImportNonPublicCxxMembers
    -enable-experimental-feature LifetimeDependence
    -enable-experimental-feature Lifetimes
    -strict-memory-safety
)

set(WEBKIT_SWIFT_MEMORY_SAFETY_ERROR_FLAGS
    -Werror ForeignReferenceType
    -Werror StrictMemorySafety
)

set(WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS
    -Werror ExistentialAny
    -Werror NoUsage
    -Werror NoUseUnstructuredThrowingTask
    ${WEBKIT_SWIFT_MEMORY_SAFETY_ERROR_FLAGS}
)

set(WEBKIT_SWIFT_CLANG_IMPORTER_FLAGS
    -Xcc -fvisibility=hidden
)

# Sanitizers.xcconfig: WK_SANITIZER_OTHER_SWIFT_FLAGS (workaround
# rdar://170982364).
#
# The clang importer must agree with C++ TUs on every layout-affecting feature
# check; sanitizers gate ASAN_ENABLED -> ENABLE_SECURITY_ASSERTIONS ->
# RefCountDebuggerImpl members. Without this, Swift's inline `new` of a
# RefCounted C++ type undersizes the allocation and the C++ ctor overflows it.
# The importer ignores -Xcc -fsanitize= for __has_feature(), so define
# __SANITIZE_*__ directly and let Compiler.h's #ifdef path set
# ASAN_ENABLED/TSAN_ENABLED. The -sanitize= flag that instruments Swift codegen
# comes from WebKitCompilerFlags.cmake, which already handles both languages.
set(WEBKIT_SWIFT_SANITIZER_FLAGS "")
foreach (_sanitizer IN LISTS ENABLE_SANITIZERS)
    if (_sanitizer STREQUAL "address")
        list(APPEND WEBKIT_SWIFT_SANITIZER_FLAGS -Xcc -D__SANITIZE_ADDRESS__)
    elseif (_sanitizer STREQUAL "thread")
        list(APPEND WEBKIT_SWIFT_SANITIZER_FLAGS -Xcc -D__SANITIZE_THREAD__)
    endif ()
endforeach ()

# No Xcode counterpart; specific to building under an outer sandbox.
#
# swiftc spawns swift-plugin-server under sandbox-exec to expand macros (e.g.
# SwiftUI @State). When the cmake build itself runs inside an outer sandbox that
# disallows nested sandbox_apply, macro expansion fails with "external macro
# implementation type ... could not be found". -disable-sandbox skips the inner
# sandbox; the macros are WebKit's own, so the isolation it provides isn't
# load-bearing here.
set(WEBKIT_SWIFT_MACRO_FLAGS
    -disable-sandbox
)

set(WEBKIT_SWIFT_CONCURRENCY_FLAGS
    -default-isolation nonisolated
    -strict-concurrency=complete
)

# Per-target flag sets

set(WEBKIT_SWIFT_CXX_INTEROP_FLAGS
    -Xcc -std=c++2b
    -cxx-interoperability-mode=default
)

# Apply the global set

WEBKIT_ADD_SWIFT_OPTIONS(
    ${WEBKIT_SWIFT_LANGUAGE_FLAGS}
    ${WEBKIT_SWIFT_UPCOMING_FEATURE_FLAGS}
    ${WEBKIT_SWIFT_CONCURRENCY_FLAGS}
    ${WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS}
    ${WEBKIT_SWIFT_CLANG_IMPORTER_FLAGS}
    ${WEBKIT_SWIFT_SANITIZER_FLAGS}
    ${WEBKIT_SWIFT_MACRO_FLAGS}
)

if (APPLE)
    WEBKIT_ADD_SWIFT_OPTIONS(
        ${WEBKIT_SWIFT_EXPERIMENTAL_FEATURE_FLAGS}
        ${WEBKIT_SWIFT_MEMORY_SAFETY_FLAGS}
    )

    WEBKIT_ADD_SWIFT_OPTIONS(
        -explicit-module-build
        -module-cache-path "${CMAKE_BINARY_DIR}/SwiftModuleCache"
        -track-system-dependencies
    )
    set_property(DIRECTORY "${CMAKE_BINARY_DIR}" APPEND PROPERTY
        ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/SwiftModuleCache")

    WEBKIT_ADD_SWIFT_OPTIONS(
        -Xcc -fexperimental-bounds-safety-attributes
        -Xcc -fexperimental-late-parse-attributes
    )

    # FIXME: Consider building with -wmo in release / performance builds.
    WEBKIT_ADD_SWIFT_OPTIONS(
        -enable-batch-mode
    )
endif ()
