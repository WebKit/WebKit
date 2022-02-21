include(inspector/remote/Socket.cmake)

if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # With the VisualStudio generator, the compiler complains about -std=c++* for C sources.
    set_source_files_properties(
        disassembler/zydis/Zydis/ZycoreAPIMemory.c
        disassembler/zydis/Zydis/ZycoreAPIProcess.c
        disassembler/zydis/Zydis/ZycoreAPISynchronization.c
        disassembler/zydis/Zydis/ZycoreAPITerminal.c
        disassembler/zydis/Zydis/ZycoreAPIThread.c
        disassembler/zydis/Zydis/ZycoreAllocator.c
        disassembler/zydis/Zydis/ZycoreArgParse.c
        disassembler/zydis/Zydis/ZycoreBitset.c
        disassembler/zydis/Zydis/ZycoreFormat.c
        disassembler/zydis/Zydis/ZycoreList.c
        disassembler/zydis/Zydis/ZycoreString.c
        disassembler/zydis/Zydis/ZycoreVector.c
        disassembler/zydis/Zydis/Zycore.c
        disassembler/zydis/Zydis/Zydis.c
        disassembler/zydis/Zydis/ZydisDecoder.c
        disassembler/zydis/Zydis/ZydisDecoderData.c
        disassembler/zydis/Zydis/ZydisFormatter.c
        disassembler/zydis/Zydis/ZydisFormatterATT.c
        disassembler/zydis/Zydis/ZydisFormatterBase.c
        disassembler/zydis/Zydis/ZydisFormatterBuffer.c
        disassembler/zydis/Zydis/ZydisFormatterIntel.c
        disassembler/zydis/Zydis/ZydisMetaInfo.c
        disassembler/zydis/Zydis/ZydisMnemonic.c
        disassembler/zydis/Zydis/ZydisRegister.c
        disassembler/zydis/Zydis/ZydisSharedData.c
        disassembler/zydis/Zydis/ZydisString.c
        disassembler/zydis/Zydis/ZydisUtils.c
        PROPERTIES LANGUAGE CXX
    )
endif ()

# This overrides the default value of 1GB for the structure heap address size
list(APPEND JavaScriptCore_DEFINITIONS
    STRUCTURE_HEAP_ADDRESS_SIZE_IN_MB=128
)

# This overrides the default x64 value of 1GB for the memory pool size
list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64
)
