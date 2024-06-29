/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "BaselineJITCode.h"

#if ENABLE(JIT)

#include "JITMathIC.h"
#include "JumpTable.h"
#include "PCToCodeOriginMap.h"
#include "StructureStubInfo.h"

namespace JSC {

JITAddIC* MathICHolder::addJITAddIC(BinaryArithProfile* arithProfile) { return m_addICs.add(arithProfile); }
JITMulIC* MathICHolder::addJITMulIC(BinaryArithProfile* arithProfile) { return m_mulICs.add(arithProfile); }
JITSubIC* MathICHolder::addJITSubIC(BinaryArithProfile* arithProfile) { return m_subICs.add(arithProfile); }
JITNegIC* MathICHolder::addJITNegIC(UnaryArithProfile* arithProfile) { return m_negICs.add(arithProfile); }

void MathICHolder::adoptMathICs(MathICHolder& other)
{
    m_addICs = WTFMove(other.m_addICs);
    m_mulICs = WTFMove(other.m_mulICs);
    m_negICs = WTFMove(other.m_negICs);
    m_subICs = WTFMove(other.m_subICs);
}

BaselineJITCode::BaselineJITCode(CodeRef<JSEntryPtrTag> code, CodePtr<JSEntryPtrTag> withArityCheck)
    : DirectJITCode(WTFMove(code), withArityCheck, JITType::BaselineJIT)
    , MathICHolder()
{ }

BaselineJITCode::~BaselineJITCode() = default;

CodeLocationLabel<JSInternalPtrTag> BaselineJITCode::getCallLinkDoneLocationForBytecodeIndex(BytecodeIndex bytecodeIndex) const
{
    auto* result = binarySearch<const BaselineUnlinkedCallLinkInfo, BytecodeIndex>(m_unlinkedCalls.span().data(), m_unlinkedCalls.size(), bytecodeIndex,
        [](const auto& value) {
            return value->bytecodeIndex;
        });
    if (!result)
        return { };
    return result->doneLocation;
}

BaselineJITData::BaselineJITData(unsigned stubInfoSize, unsigned poolSize, CodeBlock* codeBlock)
    : Base(stubInfoSize, poolSize)
    , m_globalObject(codeBlock->globalObject())
    , m_stackOffset(codeBlock->stackPointerOffset() * sizeof(Register))
{
}

} // namespace JSC

#endif // ENABLE(JIT)
