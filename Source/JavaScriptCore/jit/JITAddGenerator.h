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

#ifndef JITAddGenerator_h
#define JITAddGenerator_h

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "ResultType.h"

namespace JSC {

class JITAddGenerator {
public:
    enum OperandsConstness {
        NeitherAreConstInt32,

        // Since addition is commutative, it doesn't matter which operand we make the
        // ConstInt32. Let's always put the const in the right, and the variable operand
        // in the left.
        RightIsConstInt32,

        // We choose not to implement any optimization for the case where both operands
        // are ConstInt32 here. The client may choose to do that optimzation and not
        // invoke this snippet generator, or may load both operands into registers
        // and pass them as variables to the snippet generator instead.
    };

    JITAddGenerator(JSValueRegs result, JSValueRegs left, JSValueRegs right,
        OperandsConstness operandsConstness, int32_t rightConstInt32,
        ResultType leftType, ResultType rightType, FPRReg leftFPR, FPRReg rightFPR,
        GPRReg scratchGPR, FPRReg scratchFPR)
        : m_result(result)
        , m_left(left)
        , m_right(right)
        , m_operandsConstness(operandsConstness)
        , m_rightConstInt32(rightConstInt32)
        , m_leftType(leftType)
        , m_rightType(rightType)
        , m_leftFPR(leftFPR)
        , m_rightFPR(rightFPR)
        , m_scratchGPR(scratchGPR)
        , m_scratchFPR(scratchFPR)
    { }

    void generateFastPath(CCallHelpers&);

    CCallHelpers::JumpList endJumpList() { return m_endJumpList; }
    CCallHelpers::JumpList slowPathJumpList() { return m_slowPathJumpList; }

private:
    JSValueRegs m_result;
    JSValueRegs m_left;
    JSValueRegs m_right;
    OperandsConstness m_operandsConstness;
    int32_t m_rightConstInt32;
    ResultType m_leftType;
    ResultType m_rightType;
    FPRReg m_leftFPR;
    FPRReg m_rightFPR;
    GPRReg m_scratchGPR;
    FPRReg m_scratchFPR;

    CCallHelpers::JumpList m_endJumpList;
    CCallHelpers::JumpList m_slowPathJumpList;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITAddGenerator_h
