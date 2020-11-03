include(inspector/remote/Socket.cmake)

if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # With the VisualStudio generator, the compiler complains about -std=c++* for C sources.
    set_source_files_properties(
        disassembler/udis86/udis86.c
        disassembler/udis86/udis86_decode.c
        disassembler/udis86/udis86_itab_holder.c
        disassembler/udis86/udis86_syn-att.c
        disassembler/udis86/udis86_syn-intel.c
        disassembler/udis86/udis86_syn.c
        PROPERTIES LANGUAGE CXX
    )
endif ()

# This overrides the default x64 value of 1GB for the memory pool size
add_definitions(-DFIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64)

# Both bmalloc and WTF are built as object libraries. The WebKit:: interface
# targets are used. A limitation of that is the object files are not propagated
# so they are added here.
list(APPEND JavaScriptCore_PRIVATE_LIBRARIES
    $<TARGET_OBJECTS:WTF>
    $<TARGET_OBJECTS:bmalloc>
)

list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    STATICALLY_LINKED_WITH_WTF
    STATICALLY_LINKED_WITH_bmalloc
)
