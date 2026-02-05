/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64DOpcode.h"

#include "Binja.h"
#include "Disassembler.h"
#include "ExecutableAllocator.h"
#include "Integrity.h"
#include "LLIntPCRanges.h"
#include "VM.h"
#include "VMManager.h"
#include <array>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <wtf/PtrTag.h>
#include <wtf/Range.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace ARM64Disassembler {

const char* A64DOpcode::disassemble(uint32_t* currentPC)
{
    m_currentPC = currentPC;
    m_bufferOffset = 0;
    m_formatBuffer[0] = '\0';

    {
        std::array<char, 256> buffer;
        arm64Disassemble(currentPC, buffer.data(), buffer.size());
        bufferPrintf("   %s", buffer.data());
    }

    // Get instruction info for metadata analysis
    ARM64InstructionInfo info;
    if (!arm64GetInstructionInfo(*currentPC, (uint64_t)currentPC, &info))
        return m_formatBuffer;

    // Apply JSC-specific metadata based on instruction category
    switch (info.category) {
    case ARM64_CATEGORY_BRANCH_UNCONDITIONAL:
    case ARM64_CATEGORY_BRANCH_CONDITIONAL:
    case ARM64_CATEGORY_BRANCH_COMPARE:
    case ARM64_CATEGORY_BRANCH_TEST:
        // For branches, immediate is the byte offset from PC
        appendPCRelativeOffset(currentPC, static_cast<int32_t>(info.immediate / 4));
        break;

    case ARM64_CATEGORY_ADR:
    case ARM64_CATEGORY_ADRP:
        appendPCRelativeOffset(currentPC, static_cast<int32_t>(info.immediate / 4));
        break;

    case ARM64_CATEGORY_MOVZ:
    case ARM64_CATEGORY_MOVN:
    case ARM64_CATEGORY_MOVK:
    case ARM64_CATEGORY_MOV:
        trackMoveWideConstant(info.category, info.immediate, info.shiftAmount, info.destRegister, info.is64Bit);
        maybeAnnotateBuiltConstant();
        break;

    default:
        // Reset MoveWide tracking if this is a different instruction
        // that uses a different destination register
        if (info.destRegister != m_moveWideDestReg)
            m_moveWideDestReg = 255;
        break;
    }

    return m_formatBuffer;
}

void A64DOpcode::bufferPrintf(const char* format, ...)
{
    if (m_bufferOffset >= bufferSize)
        return;

    va_list argList;
    va_start(argList, format);

    m_bufferOffset += vsnprintf(m_formatBuffer + m_bufferOffset, bufferSize - m_bufferOffset, format, argList);

    va_end(argList);
}

void A64DOpcode::appendPCRelativeOffset(uint32_t* pc, int32_t immediate)
{
    uint32_t* targetPC = pc + immediate;
    constexpr size_t localBufferSize = 101;
    char buffer[localBufferSize];
    const char* targetInfo = buffer;

    if (!m_startPC)
        return;

    if (targetPC >= m_startPC && targetPC < m_endPC)
        snprintf(buffer, localBufferSize - 1, " -> <%u>", static_cast<unsigned>((targetPC - m_startPC) * sizeof(uint32_t)));
    else if (const char* label = labelFor(targetPC))
        snprintf(buffer, localBufferSize - 1, " -> %s", label);
    else if (isJITPC(targetPC))
        targetInfo = " -> JIT PC";
    else if (LLInt::isLLIntPC(targetPC))
        targetInfo = " -> LLInt PC";
    else
        targetInfo = " -> <unknown>";

    bufferPrintf("%s", targetInfo);
}

void A64DOpcode::trackMoveWideConstant(int category, int64_t immediate, uint8_t shiftAmount, uint8_t destRegister, uint8_t is64Bit)
{
    UNUSED_PARAM(is64Bit);

    switch (category) {
    case ARM64_CATEGORY_MOVZ:
    case ARM64_CATEGORY_MOV: {
        m_builtConstant = static_cast<uint64_t>(immediate) << shiftAmount;
        m_moveWideDestReg = destRegister;
        break;
    }

    case ARM64_CATEGORY_MOVN: {
        m_builtConstant = ~(static_cast<uint64_t>(immediate) << shiftAmount);
        m_moveWideDestReg = destRegister;
        break;
    }

    case ARM64_CATEGORY_MOVK: {
        if (destRegister == m_moveWideDestReg) {
            uint64_t mask = ~(static_cast<uint64_t>(0xFFFF) << shiftAmount);
            m_builtConstant = (m_builtConstant & mask) | (static_cast<uint64_t>(immediate) << shiftAmount);
            break;
        }
        m_builtConstant = 0;
        m_moveWideDestReg = 255;
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void A64DOpcode::maybeAnnotateBuiltConstant()
{
    if (!m_startPC)
        return;

    // Check if the next instruction continues building the constant
    uint32_t* nextPC = m_currentPC + 1;
    if (nextPC < m_endPC) {
        ARM64InstructionInfo nextInfo;
        if (arm64GetInstructionInfo(*nextPC, (uint64_t)nextPC, &nextInfo)) {
            // If next instruction is MOVK with same dest register, don't annotate yet
            if (nextInfo.category == ARM64_CATEGORY_MOVK && nextInfo.destRegister == m_moveWideDestReg)
                return;
        }
    }

    uint64_t constant = m_builtConstant;
    m_builtConstant = 0;
    m_moveWideDestReg = 255;

    // Done building constant - annotate it
    void* ptr = removeCodePtrTag(std::bit_cast<void*>(constant));
    if (!ptr)
        return;

    if (Integrity::isSanePointer(ptr)) {
        bufferPrintf(" -> %p", ptr);
        if (const char* label = labelFor(ptr)) {
            bufferPrintf(" %s", label);
            return;
        }
        if (isJITPC(ptr)) {
            bufferPrintf(" JIT PC");
            return;
        }
        if (LLInt::isLLIntPC(ptr)) {
            bufferPrintf(" LLInt PC");
            return;
        }
        handlePotentialDataPointer(ptr);
        return;
    }

#if CPU(ARM64E)
    if (handlePotentialPtrTag(constant))
        return;
#endif

    if (constant < 0x10000)
        bufferPrintf(" -> %u", static_cast<unsigned>(constant));
    else
        bufferPrintf(" -> %p", reinterpret_cast<void*>(constant));
}

bool A64DOpcode::handlePotentialDataPointer(void* ptr)
{
    ASSERT(Integrity::isSanePointer(ptr));

    bool handled = false;
    VMManager::forEachVM([&] (VM& vm) {
        if (ptr == &vm) {
            bufferPrintf(" vm");
            handled = true;
            return IterationStatus::Done;
        }

        if (!vm.isInService())
            return IterationStatus::Continue;

        auto* vmStart = reinterpret_cast<uint8_t*>(&vm);
        auto* vmEnd = vmStart + sizeof(VM);
        auto* u8Ptr = reinterpret_cast<uint8_t*>(ptr);
        Range vmRange(vmStart, vmEnd);
        if (vmRange.contains(u8Ptr)) {
            unsigned offset = u8Ptr - vmStart;
            bufferPrintf(" vm +%u", offset);

            const char* description = nullptr;
            if (ptr == &vm.topCallFrame)
                description = "vm.topCallFrame";
            else if (offset == VM::topEntryFrameOffset())
                description = "vm.topEntryFrame";
            else if (offset == VM::exceptionOffset())
                description = "vm.m_exception";
            else if (offset == VM::offsetOfHeapBarrierThreshold())
                description = "vm.heap.m_barrierThreshold";
            else if (offset == VM::callFrameForCatchOffset())
                description = "vm.callFrameForCatch";
            else if (ptr == vm.addressOfSoftStackLimit())
                description = "vm.softStackLimit()";
            else if (ptr == &vm.osrExitIndex)
                description = "vm.osrExitIndex";
            else if (ptr == &vm.osrExitJumpDestination)
                description = "vm.osrExitJumpDestination";
            else if (ptr == vm.smallStrings.singleCharacterStrings())
                description = "vm.smallStrings.m_singleCharacterStrings";
            else if (ptr == &vm.targetMachinePCForThrow)
                description = "vm.targetMachinePCForThrow";
            else if (ptr == vm.traps().trapBitsAddress())
                description = "vm.m_traps.m_trapBits";
#if ENABLE(DFG_DOES_GC_VALIDATION)
            else if (ptr == vm.addressOfDoesGC())
                description = "vm.m_doesGC";
#endif
            if (description)
                bufferPrintf(": %s", description);

            handled = true;
            return IterationStatus::Done;
        }

        if (vm.isScratchBuffer(ptr)) {
            bufferPrintf(" vm scratchBuffer.m_buffer");
            handled = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return handled;
}

#if CPU(ARM64E)
bool A64DOpcode::handlePotentialPtrTag(uintptr_t value)
{
    if (!value || value > 0xffff)
        return false;

    PtrTag tag = static_cast<PtrTag>(value);
#if ENABLE(PTRTAG_DEBUGGING)
    const char* name = WTF::ptrTagName(tag);
    if (name[0] == '<')
        return false; // Only result that starts with '<' is "<unknown>".
#else
    // Without ENABLE(PTRTAG_DEBUGGING), not all PtrTags are registered for
    // printing. So, we'll just do the minimum with only the JSC specific tags.
    const char* name = ptrTagName(tag);
    if (!name)
        return false;
#endif

    // Also print '?' to indicate that this is a maybe. We do not know for certain
    // if the constant is meant to be used as a PtrTag.
    bufferPrintf(" -> %p %s ?", reinterpret_cast<void*>(value), name);
    return true;
}
#endif

} } // namespace JSC::ARM64Disassembler

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(ARM64_DISASSEMBLER)
