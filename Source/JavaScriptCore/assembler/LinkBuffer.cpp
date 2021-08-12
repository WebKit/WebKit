/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "LinkBuffer.h"

#if ENABLE(ASSEMBLER)

#include "CodeBlock.h"
#include "Disassembler.h"
#include "JITCode.h"
#include "Options.h"
#include "WasmCompilationMode.h"

#if OS(LINUX)
#include "PerfLog.h"
#endif

namespace JSC {

size_t LinkBuffer::s_profileCummulativeLinkedSizes[LinkBuffer::numberOfProfiles];
size_t LinkBuffer::s_profileCummulativeLinkedCounts[LinkBuffer::numberOfProfiles];

bool shouldDumpDisassemblyFor(CodeBlock* codeBlock)
{
    if (codeBlock && JITCode::isOptimizingJIT(codeBlock->jitType()) && Options::dumpDFGDisassembly())
        return true;
    return Options::dumpDisassembly();
}

bool shouldDumpDisassemblyFor(Wasm::CompilationMode mode)
{
    if (Options::asyncDisassembly() || Options::dumpDisassembly() || Options::dumpWasmDisassembly())
        return true;
    switch (mode) {
    case Wasm::CompilationMode::BBQMode:
        return Options::dumpBBQDisassembly();
    case Wasm::CompilationMode::OMGMode:
    case Wasm::CompilationMode::OMGForOSREntryMode:
        return Options::dumpOMGDisassembly();
    default:
        break;
    }
    return false;
}

LinkBuffer::CodeRef<LinkBufferPtrTag> LinkBuffer::finalizeCodeWithoutDisassemblyImpl()
{
    performFinalization();
    
    ASSERT(m_didAllocate);
    if (m_executableMemory)
        return CodeRef<LinkBufferPtrTag>(*m_executableMemory);
    
    return CodeRef<LinkBufferPtrTag>::createSelfManagedCodeRef(m_code);
}

LinkBuffer::CodeRef<LinkBufferPtrTag> LinkBuffer::finalizeCodeWithDisassemblyImpl(bool dumpDisassembly, const char* format, ...)
{
    CodeRef<LinkBufferPtrTag> result = finalizeCodeWithoutDisassemblyImpl();

#if OS(LINUX)
    if (Options::logJITCodeForPerf()) {
        StringPrintStream out;
        va_list argList;
        va_start(argList, format);
        va_start(argList, format);
        out.vprintf(format, argList);
        va_end(argList);
        PerfLog::log(out.toCString(), result.code().untaggedExecutableAddress<const uint8_t*>(), result.size());
    }
#endif

    bool justDumpingHeader = !dumpDisassembly || m_alreadyDisassembled;

    StringPrintStream out;
    out.printf("Generated JIT code for ");
    va_list argList;
    va_start(argList, format);
    out.vprintf(format, argList);
    va_end(argList);
    out.printf(":\n");

    uint8_t* executableAddress = result.code().untaggedExecutableAddress<uint8_t*>();
    out.printf("    Code at [%p, %p)%s\n", executableAddress, executableAddress + result.size(), justDumpingHeader ? "." : ":");
    
    CString header = out.toCString();
    
    if (justDumpingHeader) {
        if (Options::logJIT())
            dataLog(header);
        return result;
    }
    
    if (Options::asyncDisassembly()) {
        CodeRef<DisassemblyPtrTag> codeRefForDisassembly = result.retagged<DisassemblyPtrTag>();
        disassembleAsynchronously(header, WTFMove(codeRefForDisassembly), m_size, "    ");
        return result;
    }
    
    dataLog(header);
    disassemble(result.retaggedCode<DisassemblyPtrTag>(), m_size, "    ", WTF::dataFile());
    
    return result;
}

#if ENABLE(BRANCH_COMPACTION)

class BranchCompactionLinkBuffer;

using ThreadSpecificBranchCompactionLinkBuffer = ThreadSpecific<BranchCompactionLinkBuffer, WTF::CanBeGCThread::True>;

static ThreadSpecificBranchCompactionLinkBuffer* threadSpecificBranchCompactionLinkBufferPtr;

static ThreadSpecificBranchCompactionLinkBuffer& threadSpecificBranchCompactionLinkBuffer()
{
static std::once_flag flag;
    std::call_once(
        flag,
        [] () {
            threadSpecificBranchCompactionLinkBufferPtr = new ThreadSpecificBranchCompactionLinkBuffer();
        });
    return *threadSpecificBranchCompactionLinkBufferPtr;
}

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BranchCompactionLinkBuffer);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BranchCompactionLinkBuffer);

class BranchCompactionLinkBuffer {
    WTF_MAKE_NONCOPYABLE(BranchCompactionLinkBuffer);
public:
    BranchCompactionLinkBuffer()
    {
    }

    BranchCompactionLinkBuffer(size_t size, uint8_t* userBuffer = nullptr)
    {
        if (userBuffer) {
            m_data = userBuffer;
            m_size = size;
            m_bufferProvided = true;
            return;
        }

        auto& threadSpecific = threadSpecificBranchCompactionLinkBuffer();

        if (threadSpecific->size() >= size)
            takeBufferIfLarger(*threadSpecific);
        else {
            m_size = size;
            m_data = static_cast<uint8_t*>(BranchCompactionLinkBufferMalloc::malloc(size));
        }
    }

    ~BranchCompactionLinkBuffer()
    {
        if (m_bufferProvided)
            return;

        auto& threadSpecific = threadSpecificBranchCompactionLinkBuffer();
        threadSpecific->takeBufferIfLarger(*this);

        if (m_data)
            BranchCompactionLinkBufferMalloc::free(m_data);
    }

    uint8_t* data()
    {
        return m_data;
    }

private:
    void takeBufferIfLarger(BranchCompactionLinkBuffer& other)
    {
        if (size() >= other.size())
            return;

        if (m_data)
            BranchCompactionLinkBufferMalloc::free(m_data);

        m_data = other.m_data;
        m_size = other.m_size;

        other.m_data = nullptr;
        other.m_size = 0;
    }

    size_t size()
    {
        return m_size;
    }

    uint8_t* m_data { nullptr };
    size_t m_size { 0 };
    bool m_bufferProvided { false };
};

static ALWAYS_INLINE void recordLinkOffsets(AssemblerData& assemblerData, int32_t regionStart, int32_t regionEnd, int32_t offset)
{
    int32_t ptr = regionStart / sizeof(int32_t);
    const int32_t end = regionEnd / sizeof(int32_t);
    int32_t* offsets = reinterpret_cast_ptr<int32_t*>(assemblerData.buffer());
    while (ptr < end)
        offsets[ptr++] = offset;
}

// We use this to prevent compile errors on some platforms that are unhappy
// about the signature of the system's memcpy.
ALWAYS_INLINE void* memcpyWrapper(void* dst, const void* src, size_t bytes)
{
    return memcpy(dst, src, bytes);
}

template <typename InstructionType>
void LinkBuffer::copyCompactAndLinkCode(MacroAssembler& macroAssembler, JITCompilationEffort effort)
{
    allocate(macroAssembler, effort);
    const size_t initialSize = macroAssembler.m_assembler.codeSize();
    if (didFailToAllocate())
        return;

    Vector<LinkRecord, 0, UnsafeVectorOverflow>& jumpsToLink = macroAssembler.jumpsToLink();
    m_assemblerStorage = macroAssembler.m_assembler.buffer().releaseAssemblerData();
    uint8_t* inData = bitwise_cast<uint8_t*>(m_assemblerStorage.buffer());
#if CPU(ARM64E)
    void* bufferPtr = &macroAssembler.m_assembler.buffer();
    ARM64EHash verifyUncompactedHash { bufferPtr };
    m_assemblerHashesStorage = macroAssembler.m_assembler.buffer().releaseAssemblerHashes();
    uint32_t* inHashes = bitwise_cast<uint32_t*>(m_assemblerHashesStorage.buffer());
#endif

    uint8_t* codeOutData = m_code.dataLocation<uint8_t*>();

    BranchCompactionLinkBuffer outBuffer(m_size, g_jscConfig.useFastJITPermissions ? codeOutData : 0);
    uint8_t* outData = outBuffer.data();

#if CPU(ARM64)
    RELEASE_ASSERT(roundUpToMultipleOf<sizeof(unsigned)>(outData) == outData);
    RELEASE_ASSERT(roundUpToMultipleOf<sizeof(unsigned)>(codeOutData) == codeOutData);
#endif

    int readPtr = 0;
    int writePtr = 0;
    unsigned jumpCount = jumpsToLink.size();

    auto read = [&](const InstructionType* ptr) -> InstructionType {
        InstructionType value = *ptr;
#if CPU(ARM64E)
        unsigned index = (bitwise_cast<uint8_t*>(ptr) - inData) / 4;
        uint32_t hash = verifyUncompactedHash.update(value, index, bufferPtr);
        RELEASE_ASSERT(inHashes[index] == hash);
#endif
        return value;
    };

    if (g_jscConfig.useFastJITPermissions)
        threadSelfRestrictRWXToRW();

    if (m_shouldPerformBranchCompaction) {
        for (unsigned i = 0; i < jumpCount; ++i) {
            int offset = readPtr - writePtr;
            ASSERT(!(offset & 1));
                
            // Copy the instructions from the last jump to the current one.
            size_t regionSize = jumpsToLink[i].from() - readPtr;
            InstructionType* copySource = reinterpret_cast_ptr<InstructionType*>(inData + readPtr);
            InstructionType* copyEnd = reinterpret_cast_ptr<InstructionType*>(inData + readPtr + regionSize);
            InstructionType* copyDst = reinterpret_cast_ptr<InstructionType*>(outData + writePtr);
            ASSERT(!(regionSize % 2));
            ASSERT(!(readPtr % 2));
            ASSERT(!(writePtr % 2));
            while (copySource != copyEnd) {
                InstructionType insn = read(copySource++);
                *copyDst++ = insn;
            }
            recordLinkOffsets(m_assemblerStorage, readPtr, jumpsToLink[i].from(), offset);
            readPtr += regionSize;
            writePtr += regionSize;
                
            // Calculate absolute address of the jump target, in the case of backwards
            // branches we need to be precise, forward branches we are pessimistic
            const uint8_t* target;
#if CPU(ARM64)
            const intptr_t to = jumpsToLink[i].to(&macroAssembler.m_assembler);
#else
            const intptr_t to = jumpsToLink[i].to();
#endif
            if (to >= jumpsToLink[i].from())
                target = codeOutData + to - offset; // Compensate for what we have collapsed so far
            else
                target = codeOutData + to - executableOffsetFor(to);
                
            JumpLinkType jumpLinkType = MacroAssembler::computeJumpType(jumpsToLink[i], codeOutData + writePtr, target);
            // Compact branch if we can...
            if (MacroAssembler::canCompact(jumpsToLink[i].type())) {
                // Step back in the write stream
                int32_t delta = MacroAssembler::jumpSizeDelta(jumpsToLink[i].type(), jumpLinkType);
                if (delta) {
                    writePtr -= delta;
                    recordLinkOffsets(m_assemblerStorage, jumpsToLink[i].from() - delta, readPtr, readPtr - writePtr);
                }
            }
#if CPU(ARM64)
            jumpsToLink[i].setFrom(&macroAssembler.m_assembler, writePtr);
#else
            jumpsToLink[i].setFrom(writePtr);
#endif
        }
    } else {
        if (ASSERT_ENABLED) {
            for (unsigned i = 0; i < jumpCount; ++i)
                ASSERT(!MacroAssembler::canCompact(jumpsToLink[i].type()));
        }
    }

    // Copy everything after the last jump
    {
        InstructionType* dst = bitwise_cast<InstructionType*>(outData + writePtr);
        InstructionType* src = bitwise_cast<InstructionType*>(inData + readPtr);
        size_t bytes = initialSize - readPtr;

        RELEASE_ASSERT(bitwise_cast<uintptr_t>(dst) % sizeof(InstructionType) == 0);
        RELEASE_ASSERT(bitwise_cast<uintptr_t>(src) % sizeof(InstructionType) == 0);
        RELEASE_ASSERT(bytes % sizeof(InstructionType) == 0);

        for (size_t i = 0; i < bytes; i += sizeof(InstructionType)) {
            InstructionType insn = read(src++);
            *dst++ = insn;
        }
    }


    recordLinkOffsets(m_assemblerStorage, readPtr, initialSize, readPtr - writePtr);
        
    for (unsigned i = 0; i < jumpCount; ++i) {
        uint8_t* location = codeOutData + jumpsToLink[i].from();
#if CPU(ARM64)
        const intptr_t to = jumpsToLink[i].to(&macroAssembler.m_assembler);
#else
        const intptr_t to = jumpsToLink[i].to();
#endif
        uint8_t* target = codeOutData + to - executableOffsetFor(to);
        if (g_jscConfig.useFastJITPermissions)
            MacroAssembler::link<memcpyWrapper>(jumpsToLink[i], outData + jumpsToLink[i].from(), location, target);
        else
            MacroAssembler::link<performJITMemcpy>(jumpsToLink[i], outData + jumpsToLink[i].from(), location, target);
    }

    size_t compactSize = writePtr + initialSize - readPtr;
    if (!m_executableMemory) {
        size_t nopSizeInBytes = initialSize - compactSize;

        if (g_jscConfig.useFastJITPermissions)
            Assembler::fillNops<memcpyWrapper>(outData + compactSize, nopSizeInBytes);
        else
            Assembler::fillNops<performJITMemcpy>(outData + compactSize, nopSizeInBytes);
    }

    if (g_jscConfig.useFastJITPermissions)
        threadSelfRestrictRWXToRX();

    if (m_executableMemory) {
        m_size = compactSize;
        m_executableMemory->shrink(m_size);
    }

#if ENABLE(JIT)
    if (g_jscConfig.useFastJITPermissions) {
        ASSERT(codeOutData == outData);
        if (UNLIKELY(Options::dumpJITMemoryPath()))
            dumpJITMemory(outData, outData, m_size);
    } else {
        ASSERT(codeOutData != outData);
        performJITMemcpy(codeOutData, outData, m_size);
    }
#else
    ASSERT(codeOutData != outData);
    performJITMemcpy(codeOutData, outData, m_size);
#endif

    jumpsToLink.clear();

#if DUMP_LINK_STATISTICS
    dumpLinkStatistics(codeOutData, initialSize, m_size);
#endif
#if DUMP_CODE
    dumpCode(codeOutData, m_size);
#endif
}
#endif // ENABLE(BRANCH_COMPACTION)


void LinkBuffer::linkCode(MacroAssembler& macroAssembler, JITCompilationEffort effort)
{
    // Ensure that the end of the last invalidation point does not extend beyond the end of the buffer.
    macroAssembler.label();

#if !ENABLE(BRANCH_COMPACTION)
#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL
    macroAssembler.m_assembler.buffer().flushConstantPool(false);
#endif
    allocate(macroAssembler, effort);
    if (!m_didAllocate)
        return;
    ASSERT(m_code);
    AssemblerBuffer& buffer = macroAssembler.m_assembler.buffer();
    void* code = m_code.dataLocation();
#if CPU(ARM64)
    RELEASE_ASSERT(roundUpToMultipleOf<Assembler::instructionSize>(code) == code);
#endif
    performJITMemcpy(code, buffer.data(), buffer.codeSize());
#if CPU(MIPS)
    macroAssembler.m_assembler.relocateJumps(buffer.data(), code);
#endif
#elif CPU(ARM_THUMB2)
    copyCompactAndLinkCode<uint16_t>(macroAssembler, effort);
#elif CPU(ARM64)
    copyCompactAndLinkCode<uint32_t>(macroAssembler, effort);
#endif // !ENABLE(BRANCH_COMPACTION)

    m_linkTasks = WTFMove(macroAssembler.m_linkTasks);
    m_lateLinkTasks = WTFMove(macroAssembler.m_lateLinkTasks);
}

void LinkBuffer::allocate(MacroAssembler& macroAssembler, JITCompilationEffort effort)
{
    size_t initialSize = macroAssembler.m_assembler.codeSize();
    if (m_code) {
        if (initialSize > m_size)
            return;
        
        size_t nopsToFillInBytes = m_size - initialSize;
        macroAssembler.emitNops(nopsToFillInBytes);
        m_didAllocate = true;
        return;
    }
    
    while (initialSize % jitAllocationGranule) {
        macroAssembler.breakpoint();
        initialSize = macroAssembler.m_assembler.codeSize();
    }

    m_executableMemory = ExecutableAllocator::singleton().allocate(initialSize, effort);
    if (!m_executableMemory)
        return;
    m_code = MacroAssemblerCodePtr<LinkBufferPtrTag>(m_executableMemory->start().retaggedPtr<LinkBufferPtrTag>());
    m_size = initialSize;
    m_didAllocate = true;
}

void LinkBuffer::performFinalization()
{
    for (auto& task : m_linkTasks)
        task->run(*this);
    for (auto& task : m_lateLinkTasks)
        task->run(*this);

#ifndef NDEBUG
    ASSERT(!m_completed);
    ASSERT(isValid());
    m_completed = true;
#endif
    
    s_profileCummulativeLinkedSizes[static_cast<unsigned>(m_profile)] += m_size;
    s_profileCummulativeLinkedCounts[static_cast<unsigned>(m_profile)]++;
    MacroAssembler::cacheFlush(code(), m_size);
}

void LinkBuffer::runMainThreadFinalizationTasks()
{
    for (auto& task : m_mainThreadFinalizationTasks)
        task->run();
    m_mainThreadFinalizationTasks.clear();
}

#if DUMP_LINK_STATISTICS
void LinkBuffer::dumpLinkStatistics(void* code, size_t initializeSize, size_t finalSize)
{
    static unsigned linkCount = 0;
    static unsigned totalInitialSize = 0;
    static unsigned totalFinalSize = 0;
    linkCount++;
    totalInitialSize += initialSize;
    totalFinalSize += finalSize;
    dataLogF("link %p: orig %u, compact %u (delta %u, %.2f%%)\n", 
            code, static_cast<unsigned>(initialSize), static_cast<unsigned>(finalSize),
            static_cast<unsigned>(initialSize - finalSize),
            100.0 * (initialSize - finalSize) / initialSize);
    dataLogF("\ttotal %u: orig %u, compact %u (delta %u, %.2f%%)\n", 
            linkCount, totalInitialSize, totalFinalSize, totalInitialSize - totalFinalSize,
            100.0 * (totalInitialSize - totalFinalSize) / totalInitialSize);
}
#endif

#if DUMP_CODE
void LinkBuffer::dumpCode(void* code, size_t size)
{
#if CPU(ARM_THUMB2)
    // Dump the generated code in an asm file format that can be assembled and then disassembled
    // for debugging purposes. For example, save this output as jit.s:
    //   gcc -arch armv7 -c jit.s
    //   otool -tv jit.o
    static unsigned codeCount = 0;
    unsigned short* tcode = static_cast<unsigned short*>(code);
    size_t tsize = size / sizeof(short);
    char nameBuf[128];
    snprintf(nameBuf, sizeof(nameBuf), "_jsc_jit%u", codeCount++);
    dataLogF("\t.syntax unified\n"
            "\t.section\t__TEXT,__text,regular,pure_instructions\n"
            "\t.globl\t%s\n"
            "\t.align 2\n"
            "\t.code 16\n"
            "\t.thumb_func\t%s\n"
            "# %p\n"
            "%s:\n", nameBuf, nameBuf, code, nameBuf);
        
    for (unsigned i = 0; i < tsize; i++)
        dataLogF("\t.short\t0x%x\n", tcode[i]);
#endif
}
#endif

void LinkBuffer::clearProfileStatistics()
{
    for (unsigned i = 0; i < numberOfProfiles; ++i) {
        s_profileCummulativeLinkedSizes[i] = 0;
        s_profileCummulativeLinkedCounts[i] = 0;
    }
}

void LinkBuffer::dumpProfileStatistics(std::optional<PrintStream*> outStream)
{
    struct Stat {
        Profile profile;
        size_t size;
        size_t count;
    };

    Stat sortedStats[numberOfProfiles];
    PrintStream& out = outStream ? *outStream.value() : WTF::dataFile();

#define RETURN_LINKBUFFER_PROFILE_NAME(name) case Profile::name: return #name;
    auto name = [] (Profile profile) -> const char* {
        switch (profile) {
            FOR_EACH_LINKBUFFER_PROFILE(RETURN_LINKBUFFER_PROFILE_NAME)
        }
        RELEASE_ASSERT_NOT_REACHED();
    };
#undef RETURN_LINKBUFFER_PROFILE_NAME

    size_t totalOfAllProfilesSize = 0;
    auto dumpStat = [&] (const Stat& stat) {
        char formattedName[21];
        snprintf(formattedName, 21, "%20s", name(stat.profile));

        const char* largerUnit = nullptr;
        double sizeInLargerUnit = stat.size;
        if (stat.size > 1 * MB) {
            largerUnit = "MB";
            sizeInLargerUnit = sizeInLargerUnit / MB;
        } else if (stat.size > 1 * KB) {
            largerUnit = "KB";
            sizeInLargerUnit = sizeInLargerUnit / KB;
        }

        if (largerUnit)
            out.print("  ", formattedName, ": ", stat.size, " (", sizeInLargerUnit, " ", largerUnit, ")");
        else
            out.print("  ", formattedName, ": ", stat.size);

        if (!stat.count)
            out.println();
        else
            out.println(" count ", stat.count, " avg size ", (stat.size / stat.count));
    };

    for (unsigned i = 0; i < numberOfProfiles; ++i) {
        sortedStats[i].profile = static_cast<Profile>(i);
        sortedStats[i].size = s_profileCummulativeLinkedSizes[i];
        sortedStats[i].count = s_profileCummulativeLinkedCounts[i];
        totalOfAllProfilesSize += s_profileCummulativeLinkedSizes[i];
    }
    sortedStats[static_cast<unsigned>(Profile::Total)].size = totalOfAllProfilesSize;
    std::sort(&sortedStats[0], &sortedStats[numberOfProfilesExcludingTotal],
        [] (Stat& a, Stat& b) -> bool {
            return a.size > b.size;
        });

    out.println("Cummulative LinkBuffer profile sizes:");
    for (unsigned i = 0; i < numberOfProfiles; ++i)
        dumpStat(sortedStats[i]);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
