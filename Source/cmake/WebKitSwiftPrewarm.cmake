# WEBKIT_ADD_SWIFT_PREWARM(<consumer> <swift-source> [EDGES <count>] [RESPONSE_FILES <files>...])
#   <consumer>     Target that contains .swift files with expensive imports.
#   <swift-source> .swift file that imports the consumer's expensive Swift imports.
#   EDGES          Passed to WEBKIT_RAISE_SWIFT_NINJA_PRIORITY.
#   RESPONSE_FILES Compiler argument files that the prewarm consumes.
#
# Warms the module cache, so the consumer's swiftc finds its module dependencies
# already built instead of compiling them serially. The target is named after
# <swift-source>'s stem, so other targets with the consumer's flags can depend on it.
function(WEBKIT_ADD_SWIFT_PREWARM _consumer _swift_source)
    cmake_parse_arguments(PARSE_ARGV 2 _warm "" "EDGES" "RESPONSE_FILES")
    if (_warm_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "WEBKIT_ADD_SWIFT_PREWARM: unexpected arguments ${_warm_UNPARSED_ARGUMENTS}")
    endif ()

    # Options that name the consumer's own build artifacts. The prewarm compiles
    # a different set of sources, so letting it inherit these would have it
    # write the consumer's dependency file and rebuild stamp with its own
    # dependencies.
    set(_excluded_options
        "(-emit-clang-header-path|-emit-module-interface-path|-emit-private-module-interface-path|-import-underlying-module|--emit-ninja-depfile|--ninja-depfile-target|--ninja-depfile-exclude|--emit-compile-stamp)"
    )

    cmake_path(GET _swift_source STEM _prewarm)

    add_library(${_prewarm} OBJECT "${_swift_source}")

    # The consumer's exact flags, in order; any difference builds modules it won't reuse.
    target_compile_options(${_prewarm} PRIVATE
        "$<FILTER:$<TARGET_PROPERTY:${_consumer},COMPILE_OPTIONS>,EXCLUDE,${_excluded_options}>")
    target_compile_definitions(${_prewarm} PRIVATE
        "$<TARGET_PROPERTY:${_consumer},COMPILE_DEFINITIONS>")
    target_include_directories(${_prewarm} PRIVATE
        "$<TARGET_PROPERTY:${_consumer},INCLUDE_DIRECTORIES>")

    get_target_property(_linked_libraries ${_consumer} LINK_LIBRARIES)

    # Depend on the headers and modulemaps needed to do the prewarming. WTF is
    # special because it's not directly linked against, but its interface is
    # still needed.
    foreach (_lib IN ITEMS WTF ${${_consumer}_FRAMEWORKS} LISTS _linked_libraries)
        if (_lib STREQUAL _consumer)
            continue ()
        endif ()
        foreach (_suffix IN ITEMS _CopyHeaders _CopyPrivateHeaders _CopyModules _CopyPrivateModuleMap)
            if (TARGET "${_lib}${_suffix}")
                list(APPEND _staging_deps "${_lib}${_suffix}")
            endif ()
        endforeach ()
    endforeach ()
    # The consumer's own module maps, which its flags can name with -fmodule-map-file.
    if (TARGET "${_consumer}_CopyModules")
        list(APPEND _staging_deps "${_consumer}_CopyModules")
    endif ()
    if (_staging_deps)
        list(REMOVE_DUPLICATES _staging_deps)
        add_dependencies(${_prewarm} ${_staging_deps})
    endif ()

    # Swift compiles the whole module in one Ninja edge. Give that edge a
    # source input whose timestamp follows changes to the response files.
    if (_warm_RESPONSE_FILES)
        set(_trigger "${CMAKE_CURRENT_BINARY_DIR}/${_prewarm}FlagsChanged.swift")
        add_custom_command(OUTPUT "${_trigger}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${_trigger}"
            DEPENDS ${_warm_RESPONSE_FILES}
            VERBATIM)
        target_sources(${_prewarm} PRIVATE "${_trigger}")
    endif ()

    WEBKIT_RAISE_SWIFT_NINJA_PRIORITY(${_prewarm} ${_warm_EDGES})
    add_dependencies(${_consumer} ${_prewarm})
endfunction()

# WEBKIT_RAISE_SWIFT_NINJA_PRIORITY(<target> [<edges>])
#
# ninja schedules by longest downstream path rather than duration, so slow Swift
# compiles wait behind cheap C++ ones (ninja-build/ninja#2177). Hang a chain of
# <edges> stamps (default 30, longer than any C++ compile's path) off <target>'s
# Swift objects to raise them. Everything they depend on is raised too, so use a
# short chain for targets that wait for the framework links.
function(WEBKIT_RAISE_SWIFT_NINJA_PRIORITY _target)
    set(_dispatch_edges 30)
    if (ARGC GREATER 1)
        set(_dispatch_edges ${ARGV1})
    endif ()
    _WEBKIT_ADD_NINJA_PRIORITY_CHAIN(${_target}_Dispatch ${_target}-dispatch
        "$<FILTER:$<TARGET_OBJECTS:${_target}>,INCLUDE,\\.swift\\.o>" ${_dispatch_edges})
endfunction()

# WEBKIT_RAISE_CXX_NINJA_PRIORITY(<target> <edges>)
#
# Like WEBKIT_RAISE_SWIFT_NINJA_PRIORITY, for <target>'s non-Swift objects.
function(WEBKIT_RAISE_CXX_NINJA_PRIORITY _target _edges)
    _WEBKIT_ADD_NINJA_PRIORITY_CHAIN(${_target}_CxxDispatch ${_target}-cxx-dispatch
        "$<FILTER:$<TARGET_OBJECTS:${_target}>,EXCLUDE,\\.swift\\.o>" ${_edges})
endfunction()

function(_WEBKIT_ADD_NINJA_PRIORITY_CHAIN _name _stamp_prefix _dispatch_dep _dispatch_edges)
    foreach (_i RANGE 1 ${_dispatch_edges})
        set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/${_stamp_prefix}-${_i}.stamp")
        add_custom_command(
            OUTPUT "${_stamp}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
            DEPENDS "${_dispatch_dep}"
            VERBATIM
        )
        set(_dispatch_dep "${_stamp}")
    endforeach ()
    add_custom_target(${_name} ALL DEPENDS "${_dispatch_dep}")
endfunction()

# WEBKIT_ORDER_SWIFT_COMPILE_AFTER(<target> <predecessor>)
# Compile <target>'s Swift after <predecessor>'s. All Swift targets share one module cache, and an SDK
# module that two compile steps build at once can end up rebuilt by every later build.
function(WEBKIT_ORDER_SWIFT_COMPILE_AFTER _target _predecessor)
    set(_order_source "${CMAKE_CURRENT_BINARY_DIR}/${_target}_SwiftCompileAfter${_predecessor}.swift")
    target_sources(${_target} PRIVATE "${_order_source}")
    set_source_files_properties("${_order_source}" PROPERTIES GENERATED ON)
    target_compile_options(${_target} PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:--ninja-depfile-exclude=${_order_source}>")
    cmake_language(EVAL CODE "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]] CALL _webkit_add_swift_compile_order_rule [[${_order_source}]] ${_predecessor})")
endfunction()

function(_webkit_add_swift_compile_order_rule _order_source _predecessor)
    file(WRITE "${_order_source}.in" "// Auto-generated; orders this module's compile after ${_predecessor}'s.\n")
    add_custom_command(
        OUTPUT "${_order_source}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_order_source}.in" "${_order_source}"
        DEPENDS "$<FILTER:$<TARGET_OBJECTS:${_predecessor}>,INCLUDE,\\.swift\\.o>"
        VERBATIM
    )
    cmake_path(GET _order_source STEM _name)
    add_custom_target(${_name} DEPENDS "${_order_source}")
endfunction()
