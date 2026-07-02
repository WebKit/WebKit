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
    list(FILTER _opts EXCLUDE REGEX "-emit-clang-header-path")
    target_compile_options(${_prewarm} PRIVATE ${_opts})

    get_target_property(_consumer_bindir ${_consumer} BINARY_DIR)
    set_property(SOURCE "${_swift_source}" APPEND PROPERTY OBJECT_DEPENDS
        "${_consumer_bindir}/${_consumer}.platform-swift-args.resp")

    add_dependencies(${_consumer} ${_prewarm})
endfunction()
