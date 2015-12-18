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
#include "JITMulGenerator.h"

#if ENABLE(JIT)

namespace JSC {

void JITMulGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_scratchGPR != InvalidGPRReg);
    ASSERT(m_scratchGPR != m_left.payloadGPR());
    ASSERT(m_scratchGPR != m_right.payloadGPR());
#if USE(JSVALUE32_64)
    ASSERT(m_scratchGPR != m_left.tagGPR());
    ASSERT(m_scratchGPR != m_right.tagGPR());
    ASSERT(m_scratchFPR != InvalidFPRReg);
#endif

    ASSERT(!m_leftOperand.isPositiveConstInt32() || !m_rightOperand.isPositiveConstInt32());

    if (!m_leftOperand.mightBeNumber() || !m_rightOperand.mightBeNumber()) {
        ASSERT(!m_didEmitFastPath);
        return;
    }

    m_didEmitFastPath = true;

    if (m_leftOperand.isPositiveConstInt32() || m_rightOperand.isPositiveConstInt32()) {
        JSValueRegs var = m_leftOperand.isPositiveConstInt32() ? m_right : m_left;
        SnippetOperand& varOpr = m_leftOperand.isPositiveConstInt32() ? m_rightOperand : m_leftOperand;
        SnippetOperand& constOpr = m_leftOperand.isPositiveConstInt32() ? m_leftOperand : m_rightOperand;

        // Try to do intVar * intConstant.
        CCallHelpers::Jump notInt32 = jit.branchIfNotInt32(var);

        m_slowPathJumpList.append(jit.branchMul32(CCallHelpers::Overflow, var.payloadGPR(), CCallHelpers::Imm32(constOpr.asConstInt32()), m_scratchGPR));

        jit.boxInt32(m_scratchGPR, m_result);
        m_endJumpList.append(jit.jump());

        if (!jit.supportsFloatingPoint()) {
            m_slowPathJumpList.append(notInt32);
            return;
        }

        // Try to do doubleVar * double(intConstant).
        notInt32.link(&jit);
        if (!varOpr.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(var, m_scratchGPR));

        jit.unboxDoubleNonDestructive(var, m_leftFPR, m_scratchGPR, m_scratchFPR);

        jit.move(CCallHelpers::Imm32(constOpr.asConstInt32()), m_scratchGPR);
        jit.convertInt32ToDouble(m_scratchGPR, m_rightFPR);

        // Fall thru to doubleVar * doubleVar.

    } else {
        ASSERT(!m_leftOperand.isPositiveConstInt32() && !m_rightOperand.isPositiveConstInt32());

        CCallHelpers::Jump leftNotInt;
        CCallHelpers::Jump rightNotInt;

        // Try to do intVar * intVar.
        leftNotInt = jit.branchIfNotInt32(m_left);
        rightNotInt = jit.branchIfNotInt32(m_right);

        m_slowPathJumpList.append(jit.branchMul32(CCallHelpers::Overflow, m_right.payloadGPR(), m_left.payloadGPR(), m_scratchGPR));
        if (!m_resultProfile) {
            m_slowPathJumpList.append(jit.branchTest32(CCallHelpers::Zero, m_scratchGPR)); // Go slow if potential negative zero.

        } else {
            CCallHelpers::JumpList notNegativeZero;
            notNegativeZero.append(jit.branchTest32(CCallHelpers::NonZero, m_scratchGPR));

            CCallHelpers::Jump negativeZero = jit.branch32(CCallHelpers::LessThan, m_left.payloadGPR(), CCallHelpers::TrustedImm32(0));
            notNegativeZero.append(jit.branch32(CCallHelpers::GreaterThanOrEqual, m_right.payloadGPR(), CCallHelpers::TrustedImm32(0)));

            negativeZero.link(&jit);
            // Record this, so that the speculative JIT knows that we failed speculation
            // because of a negative zero.
            jit.add32(CCallHelpers::TrustedImm32(1), CCallHelpers::AbsoluteAddress(m_resultProfile->addressOfSpecialFastPathCount()));
            m_slowPathJumpList.append(jit.jump());

            notNegativeZero.link(&jit);
        }

        jit.boxInt32(m_scratchGPR, m_result);
        m_endJumpList.append(jit.jump());

        if (!jit.supportsFloatingPoint()) {
            m_slowPathJumpList.append(leftNotInt);
            m_slowPathJumpList.append(rightNotInt);
            return;
        }

        leftNotInt.link(&jit);
        if (!m_leftOperand.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(m_left, m_scratchGPR));
        if (!m_rightOperand.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(m_right, m_scratchGPR));

        jit.unboxDoubleNonDestructive(m_left, m_leftFPR, m_scratchGPR, m_scratchFPR);
        CCallHelpers::Jump rightIsDouble = jit.branchIfNotInt32(m_right);

        jit.convertInt32ToDouble(m_right.payloadGPR(), m_rightFPR);
        CCallHelpers::Jump rightWasInteger = jit.jump();

        rightNotInt.link(&jit);
        if (!m_rightOperand.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(m_right, m_scratchGPR));

        jit.convertInt32ToDouble(m_left.payloadGPR(), m_leftFPR);

        rightIsDouble.link(&jit);
        jit.unboxDoubleNonDestructive(m_right, m_rightFPR, m_scratchGPR, m_scratchFPR);

        rightWasInteger.link(&jit);

        // Fall thru to doubleVar * doubleVar.
    }

    // Do doubleVar * doubleVar.
    jit.mulDouble(m_rightFPR, m_leftFPR);
    jit.boxDouble(m_leftFPR, m_result);
}

} // namespace JSC

#endif // ENABLE(JIT)
