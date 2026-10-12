# WEBKIT_ADD_SWIFT_PREWARM(<consumer> <swift-source> [<edges>])
#   <consumer>     Target that contains .swift files with expensive imports.
#   <swift-source> .swift file that imports the consumer's expensive Swift imports.
#   <edges>        Passed to WEBKIT_RAISE_SWIFT_NINJA_PRIORITY.
#
# Warms the module cache, so the consumer's swiftc finds its module dependencies
# already built instead of compiling them serially. The target is named after
# <swift-source>'s stem, so other targets with the consumer's flags can depend on it.
function(WEBKIT_ADD_SWIFT_PREWARM _consumer _swift_source)
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

    # Depend on the platform-swift-args.resp files the consumer's flags read.
    get_target_property(_opts ${_consumer} COMPILE_OPTIONS)
    string(REGEX MATCHALL "@[^ ;>]*\\.platform-swift-args\\.resp" _resps "${_opts}")
    list(TRANSFORM _resps REPLACE "^@" "")
    list(REMOVE_DUPLICATES _resps)
    if (_resps)
        set_source_files_properties(${_swift_source} OBJECT_DEPENDS "${_resps}")
    endif ()

    WEBKIT_RAISE_SWIFT_NINJA_PRIORITY(${_prewarm} ${ARGN})
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
    set(_dispatch_dep "$<FILTER:$<TARGET_OBJECTS:${_target}>,INCLUDE,\\.swift\\.o>")
    foreach (_i RANGE 1 ${_dispatch_edges})
        set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/${_target}-dispatch-${_i}.stamp")
        add_custom_command(
            OUTPUT "${_stamp}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
            DEPENDS "${_dispatch_dep}"
            VERBATIM
        )
        set(_dispatch_dep "${_stamp}")
    endforeach ()
    add_custom_target(${_target}_Dispatch ALL DEPENDS "${_dispatch_dep}")
endfunction()
