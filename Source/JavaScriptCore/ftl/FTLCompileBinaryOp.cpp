/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "FTLCompileBinaryOp.h"

#if ENABLE(FTL_JIT)

#include "DFGNodeType.h"
#include "FTLInlineCacheDescriptor.h"
#include "GPRInfo.h"
#include "JITAddGenerator.h"
#include "JITDivGenerator.h"
#include "JITMulGenerator.h"
#include "JITSubGenerator.h"
#include "ScratchRegisterAllocator.h"

namespace JSC { namespace FTL {

using namespace DFG;

class BinarySnippetRegisterContext {
    // The purpose of this class is to shuffle registers to get them into the state
    // that baseline code expects so that we can use the baseline snippet generators i.e.
    //    1. Ensure that the inputs and output are not in reserved registers (which
    //       include the tag registers). The snippet will use these reserved registers.
    //       Hence, we need to put the inputs and output in other scratch registers.
    //    2. Tag registers are loaded with the expected values.
    //
    // When the snippet is done:
    //    1. If we had re-assigned the result register to a scratch, we need to copy the
    //       result back from the scratch.
    //    2. Restore the input and tag registers to the values that LLVM put there originally.
    //       That is unless when one of them is also the result register. In that case, we
    //       don't want to trash the result, and hence, should not restore into it.

public:
    BinarySnippetRegisterContext(ScratchRegisterAllocator& allocator, GPRReg& result, GPRReg& left, GPRReg& right)
        : m_allocator(allocator)
        , m_result(result)
        , m_left(left)
        , m_right(right)
        , m_origResult(result)
        , m_origLeft(left)
        , m_origRight(right)
    {
        m_allocator.lock(m_result);
        m_allocator.lock(m_left);
        m_allocator.lock(m_right);

        RegisterSet inputAndOutputRegisters = RegisterSet(m_left, m_right, m_result);
        RegisterSet reservedRegisters;
        for (GPRReg reg : GPRInfo::reservedRegisters())
            reservedRegisters.set(reg);

        if (reservedRegisters.get(m_left))
            m_left = m_allocator.allocateScratchGPR();
        if (reservedRegisters.get(m_right)) {
            if (m_origRight == m_origLeft)
                m_right = m_left;
            else
                m_right = m_allocator.allocateScratchGPR();
        }
        if (reservedRegisters.get(m_result)) {
            if (m_origResult == m_origLeft)
                m_result = m_left;
            else if (m_origResult == m_origRight)
                m_result = m_right;
            else
                m_result = m_allocator.allocateScratchGPR();
        }

        if (!inputAndOutputRegisters.get(GPRInfo::tagMaskRegister))
            m_savedTagMaskRegister = m_allocator.allocateScratchGPR();
        if (!inputAndOutputRegisters.get(GPRInfo::tagTypeNumberRegister))
            m_savedTagTypeNumberRegister = m_allocator.allocateScratchGPR();
    }

    void initializeRegisters(CCallHelpers& jit)
    {
        if (m_left != m_origLeft)
            jit.move(m_origLeft, m_left);
        if (m_right != m_origRight && m_origRight != m_origLeft)
            jit.move(m_origRight, m_right);
        
        if (m_savedTagMaskRegister != InvalidGPRReg)
            jit.move(GPRInfo::tagMaskRegister, m_savedTagMaskRegister);
        if (m_savedTagTypeNumberRegister != InvalidGPRReg)
            jit.move(GPRInfo::tagTypeNumberRegister, m_savedTagTypeNumberRegister);

        jit.emitMaterializeTagCheckRegisters();
    }

    void restoreRegisters(CCallHelpers& jit)
    {
        if (m_origResult != m_result)
            jit.move(m_result, m_origResult);
        if (m_origLeft != m_left && m_origLeft != m_origResult)
            jit.move(m_left, m_origLeft);
        if (m_origRight != m_right && m_origRight != m_origResult && m_origRight != m_origLeft)
            jit.move(m_right, m_origRight);

        // We are guaranteed that the tag registers are not the same as the original input
        // or output registers. Otherwise, we would not have allocated a scratch for them.
        // Hence, we don't need to need to check for overlap like we do for the input registers.
        if (m_savedTagMaskRegister != InvalidGPRReg) {
            ASSERT(GPRInfo::tagMaskRegister != m_origLeft);
            ASSERT(GPRInfo::tagMaskRegister != m_origRight);
            ASSERT(GPRInfo::tagMaskRegister != m_origResult);
            jit.move(m_savedTagMaskRegister, GPRInfo::tagMaskRegister);
        }
        if (m_savedTagTypeNumberRegister != InvalidGPRReg) {
            ASSERT(GPRInfo::tagTypeNumberRegister != m_origLeft);
            ASSERT(GPRInfo::tagTypeNumberRegister != m_origRight);
            ASSERT(GPRInfo::tagTypeNumberRegister != m_origResult);
            jit.move(m_savedTagTypeNumberRegister, GPRInfo::tagTypeNumberRegister);
        }
    }

private:
    ScratchRegisterAllocator& m_allocator;

    GPRReg& m_result;
    GPRReg& m_left;
    GPRReg& m_right;

    GPRReg m_origResult;
    GPRReg m_origLeft;
    GPRReg m_origRight;

    GPRReg m_savedTagMaskRegister { InvalidGPRReg };
    GPRReg m_savedTagTypeNumberRegister { InvalidGPRReg };
};

enum ScratchFPRUsage {
    DontNeedScratchFPR,
    NeedScratchFPR
};

template<typename BinaryArithOpGenerator, ScratchFPRUsage scratchFPRUsage = DontNeedScratchFPR>
void generateBinaryArithOpFastPath(BinaryOpDescriptor& ic, CCallHelpers& jit,
    GPRReg result, GPRReg left, GPRReg right, RegisterSet usedRegisters,
    CCallHelpers::Jump& done, CCallHelpers::Jump& slowPathStart)
{
    ScratchRegisterAllocator allocator(usedRegisters);

    BinarySnippetRegisterContext context(allocator, result, left, right);

    GPRReg scratchGPR = allocator.allocateScratchGPR();
    FPRReg leftFPR = allocator.allocateScratchFPR();
    FPRReg rightFPR = allocator.allocateScratchFPR();
    FPRReg scratchFPR = InvalidFPRReg;
    if (scratchFPRUsage == NeedScratchFPR)
        scratchFPR = allocator.allocateScratchFPR();

    BinaryArithOpGenerator gen(ic.leftOperand(), ic.rightOperand(), JSValueRegs(result),
        JSValueRegs(left), JSValueRegs(right), leftFPR, rightFPR, scratchGPR, scratchFPR);

    auto numberOfBytesUsedToPreserveReusedRegisters =
    allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

    context.initializeRegisters(jit);
    gen.generateFastPath(jit);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(&jit);
    context.restoreRegisters(jit);
    allocator.restoreReusedRegistersByPopping(jit, numberOfBytesUsedToPreserveReusedRegisters,
        ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);
    done = jit.jump();

    gen.slowPathJumpList().link(&jit);
    context.restoreRegisters(jit);
    allocator.restoreReusedRegistersByPopping(jit, numberOfBytesUsedToPreserveReusedRegisters,
        ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);
    slowPathStart = jit.jump();
}

void generateBinaryOpFastPath(BinaryOpDescriptor& ic, CCallHelpers& jit,
    GPRReg result, GPRReg left, GPRReg right, RegisterSet usedRegisters,
    CCallHelpers::Jump& done, CCallHelpers::Jump& slowPathStart)
{
    switch (ic.nodeType()) {
    case ArithDiv:
        generateBinaryArithOpFastPath<JITDivGenerator, NeedScratchFPR>(ic, jit, result, left, right, usedRegisters, done, slowPathStart);
        break;
    case ArithMul:
        generateBinaryArithOpFastPath<JITMulGenerator>(ic, jit, result, left, right, usedRegisters, done, slowPathStart);
        break;
    case ArithSub:
        generateBinaryArithOpFastPath<JITSubGenerator>(ic, jit, result, left, right, usedRegisters, done, slowPathStart);
        break;
    case ValueAdd:
        generateBinaryArithOpFastPath<JITAddGenerator>(ic, jit, result, left, right, usedRegisters, done, slowPathStart);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)
