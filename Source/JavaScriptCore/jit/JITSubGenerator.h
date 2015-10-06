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

#ifndef JITSubGenerator_h
#define JITSubGenerator_h

#include "CCallHelpers.h"

namespace JSC {
    
class JITSubGenerator {
public:

    JITSubGenerator(JSValueRegs result, JSValueRegs left, JSValueRegs right,
        ResultType leftType, ResultType rightType, FPRReg leftFPR, FPRReg rightFPR,
        GPRReg scratchGPR, FPRReg scratchFPR)
        : m_result(result)
        , m_left(left)
        , m_right(right)
        , m_leftType(leftType)
        , m_rightType(rightType)
        , m_leftFPR(leftFPR)
        , m_rightFPR(rightFPR)
        , m_scratchGPR(scratchGPR)
        , m_scratchFPR(scratchFPR)
    { }

    void generateFastPath(CCallHelpers& jit)
    {
        CCallHelpers::JumpList slowPath;

        CCallHelpers::Jump leftNotInt = jit.branchIfNotInt32(m_left);
        CCallHelpers::Jump rightNotInt = jit.branchIfNotInt32(m_right);

        m_slowPathJumpList.append(
            jit.branchSub32(CCallHelpers::Overflow, m_right.payloadGPR(), m_left.payloadGPR()));

        jit.boxInt32(m_left.payloadGPR(), m_result);

        if (!jit.supportsFloatingPoint()) {
            m_slowPathJumpList.append(leftNotInt);
            m_slowPathJumpList.append(rightNotInt);
            return;
        }
        
        CCallHelpers::Jump end = jit.jump();

        leftNotInt.link(&jit);
        if (!m_leftType.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(m_left, m_scratchGPR));
        if (!m_rightType.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(m_right, m_scratchGPR));

        jit.unboxDouble(m_left, m_leftFPR, m_scratchFPR);
        CCallHelpers::Jump rightIsDouble = jit.branchIfNotInt32(m_right);

        jit.convertInt32ToDouble(m_right.payloadGPR(), m_rightFPR);
        CCallHelpers::Jump rightWasInteger = jit.jump();

        rightNotInt.link(&jit);
        if (!m_rightType.definitelyIsNumber())
            m_slowPathJumpList.append(jit.branchIfNotNumber(m_right, m_scratchGPR));

        jit.convertInt32ToDouble(m_left.payloadGPR(), m_leftFPR);

        rightIsDouble.link(&jit);
        jit.unboxDouble(m_right, m_rightFPR, m_scratchFPR);

        rightWasInteger.link(&jit);

        jit.subDouble(m_rightFPR, m_leftFPR);
        jit.boxDouble(m_leftFPR, m_result);

        end.link(&jit);
    }

    CCallHelpers::JumpList slowPathJumpList() { return m_slowPathJumpList; }

private:
    JSValueRegs m_result;
    JSValueRegs m_left;
    JSValueRegs m_right;
    ResultType m_leftType;
    ResultType m_rightType;
    FPRReg m_leftFPR;
    FPRReg m_rightFPR;
    GPRReg m_scratchGPR;
    FPRReg m_scratchFPR;

    CCallHelpers::JumpList m_slowPathJumpList;
};

} // namespace JSC

#endif // JITSubGenerator_h
