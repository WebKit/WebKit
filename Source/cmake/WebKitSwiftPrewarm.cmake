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

    get_target_property(_consumer_bindir ${_consumer} BINARY_DIR)
    set_property(SOURCE "${_swift_source}" APPEND PROPERTY OBJECT_DEPENDS
        "${_consumer_bindir}/${_consumer}.platform-swift-args.resp")

    add_dependencies(${_consumer} ${_prewarm})
endfunction()
