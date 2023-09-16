/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayProfile.h"
#include "BytecodeStructs.h"
#include "ValueProfile.h"

namespace JSC {

template <typename Bytecode>
unsigned valueProfileOffsetFor(const Bytecode& bytecode, unsigned checkpointIndex)
{
    UNUSED_PARAM(checkpointIndex);
    if constexpr (Bytecode::opcodeID == op_iterator_open) {
        switch (checkpointIndex) {
        case OpIteratorOpen::symbolCall: return bytecode.m_iteratorValueProfile;
        case OpIteratorOpen::getNext: return bytecode.m_nextValueProfile;
        default: RELEASE_ASSERT_NOT_REACHED();
        }

    } else if constexpr (Bytecode::opcodeID == op_iterator_next) {
        switch (checkpointIndex) {
        case OpIteratorNext::computeNext: return bytecode.m_nextResultValueProfile;
        case OpIteratorNext::getDone: return bytecode.m_doneValueProfile;
        case OpIteratorNext::getValue: return bytecode.m_valueValueProfile;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
    } else 
        return bytecode.m_valueProfile;
}

template<typename Bytecode>
Operand destinationFor(const Bytecode& bytecode, unsigned checkpointIndex, JITType type = JITType::None)
{
    UNUSED_PARAM(checkpointIndex);
    if constexpr (Bytecode::opcodeID == op_iterator_open) {
        switch (checkpointIndex) {
        case OpIteratorOpen::symbolCall: return bytecode.m_iterator;
        case OpIteratorOpen::getNext: return bytecode.m_next;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
        return { };
    } else if constexpr (Bytecode::opcodeID == op_iterator_next) {
        switch (checkpointIndex) {
        case OpIteratorNext::computeNext: {
            if (type == JITType::DFGJIT || type == JITType::FTLJIT)
                return Operand::tmp(OpIteratorNext::nextResult);
            return bytecode.m_value; // We reuse value as a temp because its either not used in subsequent bytecodes or written as the temp object .
        }
        case OpIteratorNext::getDone: return bytecode.m_done;
        case OpIteratorNext::getValue: return bytecode.m_value;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
        return { };
    } else if constexpr (Bytecode::opcodeID == op_call_ignore_result) {
        return { };
    } else
        return bytecode.m_dst;
}

// Call-like opcode locations

template<typename Bytecode>
VirtualRegister calleeFor(const Bytecode& bytecode, unsigned checkpointIndex)
{
    UNUSED_PARAM(checkpointIndex);
    if constexpr (Bytecode::opcodeID == op_iterator_open) {
        ASSERT(checkpointIndex == OpIteratorOpen::symbolCall);
        return bytecode.m_symbolIterator;
    } else if constexpr (Bytecode::opcodeID == op_iterator_next) {
        ASSERT(checkpointIndex == OpIteratorNext::computeNext);
        return bytecode.m_next;
    } else
        return bytecode.m_callee;
}

template<typename Bytecode>
unsigned argumentCountIncludingThisFor(const Bytecode& bytecode, unsigned checkpointIndex)
{
    UNUSED_PARAM(checkpointIndex);
    if constexpr (Bytecode::opcodeID == op_iterator_open) {
        ASSERT(checkpointIndex == OpIteratorOpen::symbolCall);
        return 1;
    } else if constexpr (Bytecode::opcodeID == op_iterator_next) {
        ASSERT(checkpointIndex == OpIteratorNext::computeNext);
        return 1;
    } else
        return bytecode.m_argc;
}

template<typename Bytecode>
ptrdiff_t stackOffsetInRegistersForCall(const Bytecode& bytecode, unsigned checkpointIndex)
{
    UNUSED_PARAM(checkpointIndex);
    if constexpr (Bytecode::opcodeID == op_iterator_open) {
        ASSERT(checkpointIndex == OpIteratorOpen::symbolCall);
        return CallFrame::headerSizeInRegisters - bytecode.m_iterable.offset();
    } else if constexpr (Bytecode::opcodeID == op_iterator_next) {
        ASSERT(checkpointIndex == OpIteratorNext::computeNext);
        return CallFrame::headerSizeInRegisters - bytecode.m_iterator.offset();
    } else
        return bytecode.m_argv;
}

template<typename BytecodeMetadata>
CallLinkInfo& callLinkInfoFor(BytecodeMetadata& metadata, unsigned checkpointIndex)
{
    UNUSED_PARAM(checkpointIndex);
    return metadata.m_callLinkInfo;
}

} // namespace JSC
