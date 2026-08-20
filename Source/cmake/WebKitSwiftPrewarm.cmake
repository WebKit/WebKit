# WEBKIT_ADD_SWIFT_PREWARM(<consumer> <swift-source>)
#   <consumer>     Target that contains .swift files with expensive imports.
#   <swift-source> .swift file that imports the consumer's expensive Swift imports.
#
# Warms the implicit Clang module cache, so the consumer's swiftc finds them
# already built instead of compiling them serially.
function(WEBKIT_ADD_SWIFT_PREWARM _consumer _swift_source)
    cmake_path(GET _swift_source STEM _prewarm)

    add_library(${_prewarm} OBJECT "${_swift_source}")

    get_target_property(_opts ${_consumer} COMPILE_OPTIONS)
    list(FILTER _opts EXCLUDE REGEX "(-emit-clang-header-path|-import-underlying-module)")
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
            list(FILTER _opts EXCLUDE REGEX "(-emit-clang-header-path|-import-underlying-module)")
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

    # ninja prioritizes tasks with a large number of downstream edges, which is
    # only an optimal ordering if tasks take the same amount of time to
    # execute. Swift tasks are large and slow (because ninja is scheduling the
    # swift driver, not individual compilations). To trick it into scheduling
    # prewarm to run early enough to benefit performance, add a chain of
    # meaningless dependent tasks to increase the critical path weight.
    #
    # See ninja-build/ninja#2177 for details of the critical path scheduler.
    set(_dispatch_edges 9)
    set(_dispatch_dep "$<TARGET_OBJECTS:${_prewarm}>")
    foreach (_i RANGE 1 ${_dispatch_edges})
        set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/${_prewarm}-dispatch-${_i}.stamp")
        add_custom_command(
            OUTPUT "${_stamp}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
            DEPENDS "${_dispatch_dep}"
            VERBATIM
        )
        set(_dispatch_dep "${_stamp}")
    endforeach ()
    add_custom_target(${_prewarm}_Dispatch DEPENDS "${_dispatch_dep}")

    # Transitively an ordering dependency on ${_prewarm} itself, through the
    # stamp chain above.
    add_dependencies(${_consumer} ${_prewarm}_Dispatch)
endfunction()
