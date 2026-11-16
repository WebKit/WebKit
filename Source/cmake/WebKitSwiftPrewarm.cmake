# WEBKIT_ADD_SWIFT_PREWARM(<consumer> <swift-source>)
#   <consumer>     Target that contains .swift files with expensive imports.
#   <swift-source> .swift file that imports the consumer's expensive Swift imports.
#
# Warms the implicit Clang module cache, so the consumer's swiftc finds them
# already built instead of compiling them serially.
function(WEBKIT_ADD_SWIFT_PREWARM _consumer _swift_source)
    # Options that name the consumer's own build artifacts. The prewarm compiles
    # a different set of sources, so letting it inherit these would have it
    # write the consumer's dependency file and rebuild stamp with its own
    # dependencies.
    set(_excluded_options
        "(-emit-clang-header-path|-import-underlying-module|--emit-ninja-depfile|--ninja-depfile-target|--ninja-depfile-exclude|--emit-compile-stamp)"
    )

    cmake_path(GET _swift_source STEM _prewarm)

    add_library(${_prewarm} OBJECT "${_swift_source}")

    get_target_property(_opts ${_consumer} COMPILE_OPTIONS)
    list(FILTER _opts EXCLUDE REGEX "${_excluded_options}")
    target_compile_options(${_prewarm} PRIVATE ${_opts})

    get_target_property(_opts ${_consumer} COMPILE_DEFINITIONS)
    target_compile_definitions(${_prewarm} PRIVATE ${_opts})

    get_target_property(_opts ${_consumer} INCLUDE_DIRECTORIES)
    target_include_directories(${_prewarm} PRIVATE ${_opts})
    target_include_directories(${_prewarm} PRIVATE ${${_consumer}_SYSTEM_INCLUDE_DIRECTORIES})

    get_target_property(_linked_libraries ${_consumer} LINK_LIBRARIES)
    foreach (_target ${_linked_libraries})
        if (NOT TARGET ${_target})
            continue()
        endif ()

        get_target_property(_opts ${_target} INTERFACE_COMPILE_OPTIONS)
        if (_opts)
            list(FILTER _opts EXCLUDE REGEX "${_excluded_options}")
            target_compile_options(${_prewarm} PRIVATE ${_opts})
        endif ()

        get_target_property(_opts ${_target} INTERFACE_COMPILE_DEFINITIONS)
        if (_opts)
            target_compile_definitions(${_prewarm} PRIVATE ${_opts})
        endif ()

        get_target_property(_opts ${_target} INTERFACE_INCLUDE_DIRECTORIES)
        if (_opts)
            target_include_directories(${_prewarm} PRIVATE ${_opts})
        endif ()
    endforeach ()

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
    if (_staging_deps)
        list(REMOVE_DUPLICATES _staging_deps)
        add_dependencies(${_prewarm} ${_staging_deps})
    endif ()

    # Depend on platform-swift-args.resp
    set_source_files_properties(${_swift_source} OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/${_consumer}.platform-swift-args.resp")

    WEBKIT_RAISE_SWIFT_NINJA_PRIORITY(${_prewarm})
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

# WEBKIT_ORDER_SWIFT_COMPILE_AFTER(<target> <predecessor>)
# Compile <target>'s Swift after <predecessor>'s. All Swift targets share one module cache, and an SDK
# module that two compile steps build at once can end up rebuilt by every later build.
function(WEBKIT_ORDER_SWIFT_COMPILE_AFTER _target _predecessor)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_target}_SwiftRebuildTrigger.swift" APPEND
        DEPENDS "$<FILTER:$<TARGET_OBJECTS:${_predecessor}>,INCLUDE,\\.swift\\.o>")
endfunction()
