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
