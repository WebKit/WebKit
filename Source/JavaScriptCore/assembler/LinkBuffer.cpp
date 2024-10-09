/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "PerfLog.h"
#include "WasmCallee.h"
#include "YarrJIT.h"
#include <wtf/ScopedPrintStream.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace JSC {

size_t LinkBuffer::s_profileCummulativeLinkedSizes[LinkBuffer::numberOfProfiles];
size_t LinkBuffer::s_profileCummulativeLinkedCounts[LinkBuffer::numberOfProfiles];

WTF_MAKE_TZONE_ALLOCATED_IMPL(LinkBuffer);

static const char* profileName(LinkBuffer::Profile profile)
{
#define RETURN_LINKBUFFER_PROFILE_NAME(name) case LinkBuffer::Profile::name: return #name;
    switch (profile) {
        FOR_EACH_LINKBUFFER_PROFILE(RETURN_LINKBUFFER_PROFILE_NAME)
    }
    RELEASE_ASSERT_NOT_REACHED();
#undef RETURN_LINKBUFFER_PROFILE_NAME
    return "";
}

LinkBuffer::CodeRef<LinkBufferPtrTag> LinkBuffer::finalizeCodeWithoutDisassemblyImpl(ASCIILiteral simpleName)
{
    performFinalization();
    
    ASSERT(m_didAllocate);
    CodeRef<LinkBufferPtrTag> codeRef(m_executableMemory ? CodeRef<LinkBufferPtrTag>(*m_executableMemory) : CodeRef<LinkBufferPtrTag>::createSelfManagedCodeRef(m_code));

    if (UNLIKELY(Options::logJITCodeForPerf()))
        logJITCodeForPerf(codeRef, simpleName);

    return codeRef;
}

void LinkBuffer::logJITCodeForPerf(CodeRef<LinkBufferPtrTag>& codeRef, ASCIILiteral simpleName)
{
#if OS(LINUX) || OS(DARWIN)
    auto dumpSimpleName = [&](StringPrintStream& out, ASCIILiteral simpleName) {
        if (simpleName.isNull())
            out.print("unspecified");
        else
            out.print(simpleName);
    };

    StringPrintStream out;
    out.print(profileName(m_profile), ": ");
    switch (m_profile) {
    case Profile::Baseline:
    case Profile::DFG:
    case Profile::FTL: {
        if (m_ownerUID)
            static_cast<CodeBlock*>(m_ownerUID)->dumpSimpleName(out);
        else
            dumpSimpleName(out, simpleName);
        break;
    }
#if ENABLE(WEBASSEMBLY)
    case Profile::WasmOMG:
    case Profile::WasmBBQ: {
        if (m_ownerUID)
            out.print(makeString(static_cast<Wasm::Callee*>(m_ownerUID)->indexOrName()));
        else
            dumpSimpleName(out, simpleName);
        break;
    }
#endif
#if ENABLE(YARR_JIT)
    case Profile::YarrJIT: {
        if (m_ownerUID)
            static_cast<Yarr::YarrCodeBlock*>(m_ownerUID)->dumpSimpleName(out);
        else
            dumpSimpleName(out, simpleName);
        break;
    }
#endif
    default:
        dumpSimpleName(out, simpleName);
        break;
    }
    if (!m_isRewriting)
        PerfLog::log(out.toCString(), codeRef.code().untaggedPtr<const uint8_t*>(), codeRef.size());
#else
    UNUSED_PARAM(codeRef);
    UNUSED_PARAM(simpleName);
#endif
}

LinkBuffer::CodeRef<LinkBufferPtrTag> LinkBuffer::finalizeCodeWithDisassemblyImpl(bool dumpDisassembly, ASCIILiteral simpleName, const char* format, ...)
{
    CodeRef<LinkBufferPtrTag> result = finalizeCodeWithoutDisassemblyImpl(simpleName);

    if (!dumpDisassembly && !Options::logJIT())
        return result;

    bool justDumpingHeader = !dumpDisassembly || m_alreadyDisassembled;

    ScopedPrintStream out;
    out.printf("Generated JIT code for ");
    va_list argList;
    va_start(argList, format);

    if (m_isThunk) {
        va_list preflightArgs;
        va_copy(preflightArgs, argList);
        size_t stringLength = vsnprintf(nullptr, 0, format, preflightArgs);
        va_end(preflightArgs);

        const char prefix[] = "thunk: ";
        char* buffer = 0;
        size_t length = stringLength + sizeof(prefix);
        CString label = CString::newUninitialized(length, buffer);
        snprintf(buffer, length, "%s", prefix);
        vsnprintf(buffer + sizeof(prefix) - 1, stringLength + 1, format, argList);
        out.printf("%s", buffer);

        registerLabel(result.code().untaggedPtr(), WTFMove(label));
    } else
        out.vprintf(format, argList);

    va_end(argList);

    uint8_t* executableAddress = result.code().untaggedPtr<uint8_t*>();
    out.printf(": [%p, %p) %zu bytes%s\n", executableAddress, executableAddress + result.size(), result.size(), justDumpingHeader ? "." : ":");

    if (justDumpingHeader) {
        if (!Options::logJIT())
            out.reset();
        return result;
    }
    
    void* codeStart = entrypoint<DisassemblyPtrTag>().untaggedPtr();
    void* codeEnd = bitwise_cast<uint8_t*>(codeStart) + size();

    disassemble(result.retaggedCode<DisassemblyPtrTag>(), m_size, codeStart, codeEnd, "    ", out);
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

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(BranchCompactionLinkBuffer, WTF_INTERNAL);
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
#if OS(DARWIN)
    memset_pattern4(bitwise_cast<uint8_t*>(assemblerData.buffer()) + regionStart, &offset, regionEnd - regionStart);
#else
    int32_t ptr = regionStart / sizeof(int32_t);
    const int32_t end = regionEnd / sizeof(int32_t);
    int32_t* offsets = reinterpret_cast_ptr<int32_t*>(assemblerData.buffer());
    while (ptr < end)
        offsets[ptr++] = offset;
#endif
}

template <typename InstructionType>
void LinkBuffer::copyCompactAndLinkCode(MacroAssembler& macroAssembler, JITCompilationEffort effort)
{
    allocate(macroAssembler, effort);
    const size_t initialSize = macroAssembler.m_assembler.codeSize();
    if (didFailToAllocate())
        return;

    auto& jumpsToLink = macroAssembler.jumpsToLink();
    m_assemblerStorage = macroAssembler.m_assembler.buffer().releaseAssemblerData();
    uint8_t* inData = bitwise_cast<uint8_t*>(m_assemblerStorage.buffer());
#if ENABLE(JIT_SIGN_ASSEMBLER_BUFFER)
    ARM64EHash<ShouldSign::No> verifyUncompactedHash;
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
#if ENABLE(JIT_SIGN_ASSEMBLER_BUFFER)
        unsigned index = (bitwise_cast<uint8_t*>(ptr) - inData) / 4;
        uint32_t hash = verifyUncompactedHash.update(value, index);
        RELEASE_ASSERT(inHashes[index] == hash);
#endif
        return value;
    };

    if (g_jscConfig.useFastJITPermissions)
        threadSelfRestrict<MemoryRestriction::kRwxToRw>();
#if ENABLE(MPROTECT_RX_TO_RWX)
    ExecutableAllocator::singleton().startWriting(outData, initialSize);
#endif

    if (m_shouldPerformBranchCompaction) {
        for (unsigned i = 0; i < jumpCount; ++i) {
            auto& linkRecord = jumpsToLink[i];
            int offset = readPtr - writePtr;
            ASSERT(!(offset & 1));

            // Copy the instructions from the last jump to the current one.
            size_t regionSize = linkRecord.from() - readPtr;
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
            recordLinkOffsets(m_assemblerStorage, readPtr, linkRecord.from(), offset);
            readPtr += regionSize;
            writePtr += regionSize;
                
            // Calculate absolute address of the jump target, in the case of backwards
            // branches we need to be precise, forward branches we are pessimistic
            const uint8_t* target;
            const intptr_t to = linkRecord.to(&macroAssembler.m_assembler);
            if (linkRecord.isThunk())
                target = bitwise_cast<uint8_t*>(to);
            else if (to >= linkRecord.from())
                target = codeOutData + to - offset; // Compensate for what we have collapsed so far
            else
                target = codeOutData + to - executableOffsetFor(to);

            JumpLinkType jumpLinkType = MacroAssembler::computeJumpType(linkRecord, codeOutData + writePtr, target);
            // Compact branch if we can...
            if (MacroAssembler::canCompact(linkRecord.type())) {
                // Step back in the write stream
                int32_t delta = MacroAssembler::jumpSizeDelta(linkRecord.type(), jumpLinkType);
                if (delta) {
                    writePtr -= delta;
                    recordLinkOffsets(m_assemblerStorage, linkRecord.from() - delta, readPtr, readPtr - writePtr);
                }
            }
            linkRecord.setFrom(&macroAssembler.m_assembler, writePtr);
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
        auto& linkRecord = jumpsToLink[i];
        uint8_t* location = codeOutData + linkRecord.from();
        const intptr_t to = linkRecord.to(&macroAssembler.m_assembler);
        uint8_t* target = nullptr;
        if (linkRecord.isThunk())
            target = bitwise_cast<uint8_t*>(to);
        else
            target = codeOutData + to - executableOffsetFor(to);
        if (g_jscConfig.useFastJITPermissions)
            MacroAssembler::link<MachineCodeCopyMode::Memcpy>(linkRecord, outData + linkRecord.from(), location, target);
        else
            MacroAssembler::link<MachineCodeCopyMode::JITMemcpy>(linkRecord, outData + linkRecord.from(), location, target);
    }

    size_t compactSize = writePtr + initialSize - readPtr;
    if (!m_executableMemory) {
        size_t nopSizeInBytes = initialSize - compactSize;

        if (g_jscConfig.useFastJITPermissions)
            Assembler::fillNops<MachineCodeCopyMode::Memcpy>(outData + compactSize, nopSizeInBytes);
        else
            Assembler::fillNops<MachineCodeCopyMode::JITMemcpy>(outData + compactSize, nopSizeInBytes);
    }
    if (g_jscConfig.useFastJITPermissions)
        threadSelfRestrict<MemoryRestriction::kRwxToRx>();

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

#if ENABLE(MPROTECT_RX_TO_RWX)
    ExecutableAllocator::singleton().finishWriting(outData, initialSize);
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
    macroAssembler.padBeforePatch();

#if ENABLE(JIT)
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
#elif CPU(ARM_THUMB2)
    copyCompactAndLinkCode<uint16_t>(macroAssembler, effort);
#elif CPU(ARM64)
    copyCompactAndLinkCode<uint32_t>(macroAssembler, effort);
#endif // !ENABLE(BRANCH_COMPACTION)
#else  // ENABLE(JIT)
UNUSED_PARAM(effort);
#endif // ENABLE(JIT)

    m_linkTasks = WTFMove(macroAssembler.m_linkTasks);
    m_lateLinkTasks = WTFMove(macroAssembler.m_lateLinkTasks);

    linkComments(macroAssembler);
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

#if ENABLE(JIT_SIGN_ASSEMBLER_BUFFER)
    macroAssembler.m_assembler.buffer().arm64eHash().deallocatePinForCurrentThread();
#endif

    m_executableMemory = ExecutableAllocator::singleton().allocate(initialSize, effort);
    if (!m_executableMemory)
        return;
    m_code = CodePtr<LinkBufferPtrTag>(m_executableMemory->start().retaggedPtr<LinkBufferPtrTag>());
    m_size = initialSize;
    m_didAllocate = true;
}

void LinkBuffer::linkComments(MacroAssembler& assembler)
{
    if (LIKELY(!Options::needDisassemblySupport()) || !m_executableMemory)
        return;
    AssemblyCommentRegistry::CommentMap map;
    for (auto& comment : assembler.m_comments) {
        void* commentLocation = locationOf<DisassemblyPtrTag>(comment.first).dataLocation();
        auto key = reinterpret_cast<uintptr_t>(commentLocation);

        auto& string = comment.second;
        auto addResult = map.ensure(key, [&] {
            return string.isolatedCopy();
        });
        if (!addResult.isNewEntry)
            addResult.iterator->value = makeString(addResult.iterator->value, "\n; "_s, string);
    }

    AssemblyCommentRegistry::singleton().registerCodeRange(m_executableMemory->start().untaggedPtr(), m_executableMemory->end().untaggedPtr(), WTFMove(map));
}

void LinkBuffer::performFinalization()
{
#if ENABLE(MPROTECT_RX_TO_RWX)
    ExecutableAllocator::singleton().startWriting(code(), m_size);
#endif
    for (auto& task : m_linkTasks)
        task->run(*this);
    for (auto& task : m_lateLinkTasks)
        task->run(*this);
#if ENABLE(MPROTECT_RX_TO_RWX)
    ExecutableAllocator::singleton().finishWriting(code(), m_size);
#endif

#ifndef NDEBUG
    ASSERT(!m_completed);
    ASSERT(isValid());
    m_completed = true;
#endif
    
    s_profileCummulativeLinkedSizes[static_cast<unsigned>(m_profile)] += m_size;
    s_profileCummulativeLinkedCounts[static_cast<unsigned>(m_profile)]++;
    MacroAssembler::cacheFlush(code(), m_size);
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

    size_t totalOfAllProfilesSize = 0;
    auto dumpStat = [&] (const Stat& stat) {
        char formattedName[21];
        snprintf(formattedName, 21, "%20s", profileName(stat.profile));

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
