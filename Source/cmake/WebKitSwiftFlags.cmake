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
    return ()
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

function(_webkit_swift_targets_in_directory _dir _outvar)
    get_property(_targets DIRECTORY "${_dir}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_swift_targets "")
    foreach (_target IN LISTS _targets)
        get_target_property(_sources ${_target} SOURCES)
        list(FILTER _sources INCLUDE REGEX "\\.swift$")
        if (_sources)
            list(APPEND _swift_targets ${_target})
        endif ()
    endforeach ()
    get_property(_subdirs DIRECTORY "${_dir}" PROPERTY SUBDIRECTORIES)
    foreach (_subdir IN LISTS _subdirs)
        _webkit_swift_targets_in_directory("${_subdir}" _subdir_targets)
        list(APPEND _swift_targets ${_subdir_targets})
    endforeach ()
    set(${_outvar} ${_swift_targets} PARENT_SCOPE)
endfunction()

# FIXME: Remove once swiftc's dependency scan hash includes the optimization level (rdar://192478621).
# Until then, a saved -incremental-dependency-scan from before a compile option change breaks the build.
function(_webkit_add_discard_swift_dependency_scans_target)
    _webkit_swift_targets_in_directory("${CMAKE_SOURCE_DIR}" _targets)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _config)
    set(_options "${CMAKE_Swift_FLAGS}\n${CMAKE_Swift_FLAGS_${_config}}\n")
    set(_scans "")
    foreach (_target IN LISTS _targets)
        get_target_property(_target_options ${_target} COMPILE_OPTIONS)
        string(APPEND _options "${_target}: ${_target_options}\n")
        get_target_property(_binary_dir ${_target} BINARY_DIR)
        list(APPEND _scans "${_binary_dir}/CMakeFiles/${_target}.dir/$<CONFIG>/${_target}.swiftmoduledeps")
    endforeach ()
    string(SHA256 _key "${_options}")

    set(_key_file "${CMAKE_BINARY_DIR}/SwiftCompileOptions.sha256")
    set(_stamp "${CMAKE_BINARY_DIR}/DiscardSwiftDependencyScans.stamp")
    file(GENERATE OUTPUT "${_key_file}" CONTENT "${_key}\n")
    add_custom_command(
        OUTPUT "${_stamp}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f ${_scans}
        COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
        DEPENDS "${_key_file}"
        COMMENT "Discarding saved Swift dependency scans"
        VERBATIM
    )
    add_custom_target(DiscardSwiftDependencyScans DEPENDS "${_stamp}")
    foreach (_target IN LISTS _targets)
        add_dependencies(${_target} DiscardSwiftDependencyScans)
    endforeach ()
endfunction()

# Global flag sets

set(WEBKIT_SWIFT_LANGUAGE_FLAGS
    "-swift-version 6"
)

set(WEBKIT_SWIFT_UPCOMING_FEATURE_FLAGS
    "-enable-upcoming-feature ExistentialAny"
    "-enable-upcoming-feature InternalImportsByDefault"
    "-enable-upcoming-feature MemberImportVisibility"
    "-enable-upcoming-feature NonisolatedNonsendingByDefault"
)

set(WEBKIT_SWIFT_EXPERIMENTAL_FEATURE_FLAGS
    "-enable-experimental-feature DebugDescriptionMacro"
    "-enable-experimental-feature ForeignReferenceTypeInheritance"
    "-enable-experimental-feature ImportCxxMembersLazily"
    "-enable-experimental-feature SafeInteropWrappers"
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
    "-Werror RegionIsolation"
    "-Werror SendableClosureCaptures"
    "-Werror StrictMemorySafety"
)

set(WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS
    "-Werror ExistentialAny"
    "-Werror NoUsage"
    "-Werror NoUseUnstructuredThrowingTask"
    "-Werror UnnecessaryEffectMarker"
    "-Werror UnusedImportAccess"
    "-Werror VariableNeverMutated"
    ${WEBKIT_SWIFT_MEMORY_SAFETY_ERROR_FLAGS}
)

set(WEBKIT_SWIFT_CLANG_IMPORTER_FLAGS
    "-Xcc -fvisibility=hidden"
)

# Avoid failing due to a nested process sandbox (e.g. when building from an agent session).
# Xcode does not do this by default.
set(WEBKIT_SWIFT_MACRO_FLAGS
    -disable-sandbox
)

# Mirrors -Xfrontend -disable-round-trip-debug-types in CommonBase.xcconfig's OTHER_SWIFT_FLAGS.
# An asserts swiftc aborts in IRGenDebugInfo getMangledName on a captured mutable variable of a
# namespaced C++ type such as IPC.Connection.
# FIXME(rdar://189511928, rdar://189512247): remove when both are resolved.
set(WEBKIT_SWIFT_DEBUG_INFO_FLAGS
    "-Xfrontend -disable-round-trip-debug-types"
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
    ${WEBKIT_SWIFT_DEBUG_INFO_FLAGS}
)

webkit_add_swift_options(
    "-module-cache-path ${CMAKE_BINARY_DIR}/SwiftModuleCache"
    # Needed because WebKit's modules are marked [system].
    -track-system-dependencies
)
set_property(DIRECTORY "${CMAKE_BINARY_DIR}" APPEND PROPERTY
    ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/SwiftModuleCache")

# Ninja tricks compilers into turning off color support. See the matching clang
# flags in WebKitCompilerFlags.cmake.
if (CMAKE_GENERATOR STREQUAL "Ninja")
    webkit_add_swift_options(-color-diagnostics)
endif ()

if (APPLE)
    webkit_add_swift_options(
        ${WEBKIT_SWIFT_EXPERIMENTAL_FEATURE_FLAGS}
        ${WEBKIT_SWIFT_MEMORY_SAFETY_FLAGS}
    )

    webkit_add_swift_options(
        -explicit-module-build
        # Reuse the previous build's dependency scan, revalidated against its
        # inputs, instead of rescanning from scratch: the WebKit target's scan
        # reads ~16K files and takes ~14 s, on every rebuild of its Swift module.
        -incremental-dependency-scan
    )
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _webkit_add_discard_swift_dependency_scans_target)

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

    webkit_add_swift_options(
        "$<$<AND:$<COMPILE_LANGUAGE:Swift>,$<CONFIG:Debug>>:-enable-batch-mode>"
    )
else ()
    webkit_add_swift_options(
        "-Xcc -fmodules-validate-system-headers"
    )
endif ()
