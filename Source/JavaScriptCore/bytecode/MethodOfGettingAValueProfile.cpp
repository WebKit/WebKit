/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "MethodOfGettingAValueProfile.h"

#if ENABLE(DFG_JIT)

#include "ArithProfile.h"
#include "CCallHelpers.h"
#include "CodeBlock.h"
#include "JSCJSValueInlines.h"

namespace JSC {

void MethodOfGettingAValueProfile::emitReportValue(CCallHelpers& jit, CodeBlock* optimizedCodeBlock, JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode) const
{
    if (m_kind == Kind::None)
        return;

    CodeBlock* baselineCodeBlock = optimizedCodeBlock->baselineAlternative();
    CodeBlock* profiledBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(m_codeOrigin, baselineCodeBlock);
    switch (m_kind) {
    case Kind::None:
        RELEASE_ASSERT_NOT_REACHED();
        return;
        
    case Kind::LazyOperandValueProfile: {
        LazyOperandValueProfileKey key(m_codeOrigin.bytecodeIndex(), Operand::fromBits(m_rawOperand));
        
        ConcurrentJSLocker locker(profiledBlock->m_lock);
        LazyOperandValueProfile* profile = profiledBlock->lazyOperandValueProfiles(locker).add(locker, key);
        jit.storeValue(regs, profile->specFailBucket(0));
        return;
    }
        
    case Kind::UnaryArithProfile: {
        if (UnaryArithProfile* result = profiledBlock->unaryArithProfileForBytecodeIndex(m_codeOrigin.bytecodeIndex()))
            result->emitObserveResult(jit, regs, tempGPR, mode);
        return;
    }

    case Kind::BinaryArithProfile: {
        if (BinaryArithProfile* result = profiledBlock->binaryArithProfileForBytecodeIndex(m_codeOrigin.bytecodeIndex()))
            result->emitObserveResult(jit, regs, tempGPR, mode);
        return;
    }

    case Kind::ArgumentValueProfile: {
        auto& valueProfile = profiledBlock->valueProfileForArgument(Operand::fromBits(m_rawOperand).toArgument());
        jit.storeValue(regs, valueProfile.specFailBucket(0));
        return;
    }

    case Kind::BytecodeValueProfile: {
        auto& valueProfile = profiledBlock->valueProfileForBytecodeIndex(m_codeOrigin.bytecodeIndex());
        jit.storeValue(regs, valueProfile.specFailBucket(0));
        return;
    }
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

#endif // ENABLE(DFG_JIT)

