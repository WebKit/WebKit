/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"
#include "Options.h"
#include <wtf/CompilationThread.h>

#if OS(LINUX)
#include "PerfLog.h"
#endif

namespace JSC {

#if ENABLE(SEPARATED_WX_HEAP)
extern JS_EXPORT_PRIVATE bool useFastPermisionsJITCopy;
#endif // ENABLE(SEPARATED_WX_HEAP)

bool shouldDumpDisassemblyFor(CodeBlock* codeBlock)
{
    if (codeBlock && JITCode::isOptimizingJIT(codeBlock->jitType()) && Options::dumpDFGDisassembly())
        return true;
    return Options::dumpDisassembly();
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

    if (!dumpDisassembly || m_alreadyDisassembled)
        return result;
    
    StringPrintStream out;
    out.printf("Generated JIT code for ");
    va_list argList;
    va_start(argList, format);
    out.vprintf(format, argList);
    va_end(argList);
    out.printf(":\n");

    uint8_t* executableAddress = result.code().untaggedExecutableAddress<uint8_t*>();
    out.printf("    Code at [%p, %p):\n", executableAddress, executableAddress + result.size());
    
    CString header = out.toCString();
    
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
static ALWAYS_INLINE void recordLinkOffsets(AssemblerData& assemblerData, int32_t regionStart, int32_t regionEnd, int32_t offset)
{
    int32_t ptr = regionStart / sizeof(int32_t);
    const int32_t end = regionEnd / sizeof(int32_t);
    int32_t* offsets = reinterpret_cast_ptr<int32_t*>(assemblerData.buffer());
    while (ptr < end)
        offsets[ptr++] = offset;
}

template <typename InstructionType>
void LinkBuffer::copyCompactAndLinkCode(MacroAssembler& macroAssembler, void* ownerUID, JITCompilationEffort effort)
{
#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
#if ENABLE(SEPARATED_WX_HEAP)
    const bool isUsingFastPermissionsJITCopy = useFastPermisionsJITCopy;
#else
    const bool isUsingFastPermissionsJITCopy = true;
#endif
#endif

    allocate(macroAssembler, ownerUID, effort);
    const size_t initialSize = macroAssembler.m_assembler.codeSize();
    if (didFailToAllocate())
        return;

    Vector<LinkRecord, 0, UnsafeVectorOverflow>& jumpsToLink = macroAssembler.jumpsToLink();
    m_assemblerStorage = macroAssembler.m_assembler.buffer().releaseAssemblerData();
    uint8_t* inData = reinterpret_cast<uint8_t*>(m_assemblerStorage.buffer());

    uint8_t* codeOutData = m_code.dataLocation<uint8_t*>();
#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
    const ARM64EHash assemblerBufferHash = macroAssembler.m_assembler.buffer().hash();
    ARM64EHash verifyUncompactedHash(assemblerBufferHash.randomSeed());
    uint8_t* outData = codeOutData;
#if ENABLE(SEPARATED_WX_HEAP)
    AssemblerData outBuffer(m_size);
    if (!isUsingFastPermissionsJITCopy)
        outData = reinterpret_cast<uint8_t*>(outBuffer.buffer());
#endif // ENABLE(SEPARATED_WX_HEAP)
#else
    AssemblerData outBuffer(m_size);
    uint8_t* outData = reinterpret_cast<uint8_t*>(outBuffer.buffer());
#endif
#if CPU(ARM64)
    RELEASE_ASSERT(roundUpToMultipleOf<sizeof(unsigned)>(outData) == outData);
    RELEASE_ASSERT(roundUpToMultipleOf<sizeof(unsigned)>(codeOutData) == codeOutData);
#endif

    int readPtr = 0;
    int writePtr = 0;
    unsigned jumpCount = jumpsToLink.size();

#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
    if (isUsingFastPermissionsJITCopy)
        os_thread_self_restrict_rwx_to_rw();
#endif

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
#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
            unsigned index = readPtr;
#endif
            while (copySource != copyEnd) {
                InstructionType insn = *copySource++;
#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
                static_assert(sizeof(InstructionType) == 4, "");
                verifyUncompactedHash.update(insn, index);
                index += sizeof(InstructionType);
#endif
                *copyDst++ = insn;
            }
            recordLinkOffsets(m_assemblerStorage, readPtr, jumpsToLink[i].from(), offset);
            readPtr += regionSize;
            writePtr += regionSize;
                
            // Calculate absolute address of the jump target, in the case of backwards
            // branches we need to be precise, forward branches we are pessimistic
            const uint8_t* target;
            if (jumpsToLink[i].to() >= jumpsToLink[i].from())
                target = codeOutData + jumpsToLink[i].to() - offset; // Compensate for what we have collapsed so far
            else
                target = codeOutData + jumpsToLink[i].to() - executableOffsetFor(jumpsToLink[i].to());
                
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
            jumpsToLink[i].setFrom(writePtr);
        }
    } else {
        if (!ASSERT_DISABLED) {
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

#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
        unsigned index = readPtr;
#endif

        for (size_t i = 0; i < bytes; i += sizeof(InstructionType)) {
            InstructionType insn = *src++;
#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
            verifyUncompactedHash.update(insn, index);
            index += sizeof(InstructionType);
#endif
            *dst++ = insn;
        }
    }

#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
    if (verifyUncompactedHash.hash() != assemblerBufferHash.hash()) {
        dataLogLn("Hashes don't match: ", RawPointer(bitwise_cast<void*>(verifyUncompactedHash.hash())), " ", RawPointer(bitwise_cast<void*>(assemblerBufferHash.hash())));
        dataLogLn("Crashing!");
        CRASH();
    }
#endif

    recordLinkOffsets(m_assemblerStorage, readPtr, initialSize, readPtr - writePtr);
        
    for (unsigned i = 0; i < jumpCount; ++i) {
#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
        auto memcpyFunction = memcpy;
        if (!isUsingFastPermissionsJITCopy)
            memcpyFunction = performJITMemcpy;
#else
        auto memcpyFunction = performJITMemcpy;
#endif

        uint8_t* location = codeOutData + jumpsToLink[i].from();
        uint8_t* target = codeOutData + jumpsToLink[i].to() - executableOffsetFor(jumpsToLink[i].to());
        MacroAssembler::link(jumpsToLink[i], outData + jumpsToLink[i].from(), location, target, memcpyFunction);
    }

    size_t compactSize = writePtr + initialSize - readPtr;
    if (!m_executableMemory) {
        size_t nopSizeInBytes = initialSize - compactSize;
        MacroAssembler::AssemblerType_T::fillNops(outData + compactSize, nopSizeInBytes, memcpy);
    }

#if CPU(ARM64E) && ENABLE(FAST_JIT_PERMISSIONS)
    if (isUsingFastPermissionsJITCopy)
        os_thread_self_restrict_rwx_to_rx();
#endif

    if (m_executableMemory) {
        m_size = compactSize;
        m_executableMemory->shrink(m_size);
    }

#if !CPU(ARM64E) || !ENABLE(FAST_JIT_PERMISSIONS)
    ASSERT(codeOutData != outData);
    performJITMemcpy(codeOutData, outData, m_size);
#else
    if (isUsingFastPermissionsJITCopy)
        ASSERT(codeOutData == outData);
    else {
        ASSERT(codeOutData != outData);
        performJITMemcpy(codeOutData, outData, m_size);
    }
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


void LinkBuffer::linkCode(MacroAssembler& macroAssembler, void* ownerUID, JITCompilationEffort effort)
{
    // Ensure that the end of the last invalidation point does not extend beyond the end of the buffer.
    macroAssembler.label();

#if !ENABLE(BRANCH_COMPACTION)
#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL
    macroAssembler.m_assembler.buffer().flushConstantPool(false);
#endif
    allocate(macroAssembler, ownerUID, effort);
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
    copyCompactAndLinkCode<uint16_t>(macroAssembler, ownerUID, effort);
#elif CPU(ARM64)
    copyCompactAndLinkCode<uint32_t>(macroAssembler, ownerUID, effort);
#endif // !ENABLE(BRANCH_COMPACTION)

    m_linkTasks = WTFMove(macroAssembler.m_linkTasks);
}

void LinkBuffer::allocate(MacroAssembler& macroAssembler, void* ownerUID, JITCompilationEffort effort)
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

    m_executableMemory = ExecutableAllocator::singleton().allocate(initialSize, ownerUID, effort);
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

#ifndef NDEBUG
    ASSERT(!isCompilationThread());
    ASSERT(!m_completed);
    ASSERT(isValid());
    m_completed = true;
#endif
    
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

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
