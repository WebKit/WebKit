/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "JIT.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "JITInlineMethods.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

#define __ m_assembler.

using namespace std;

#if PLATFORM(MAC)

static inline bool isSSE2Present()
{
    return true; // All X86 Macs are guaranteed to support at least SSE2
}

#else

static bool isSSE2Present()
{
    static const int SSE2FeatureBit = 1 << 26;
    struct SSE2Check {
        SSE2Check()
        {
            int flags;
#if COMPILER(MSVC)
            _asm {
                mov eax, 1 // cpuid function 1 gives us the standard feature set
                cpuid;
                mov flags, edx;
            }
#elif COMPILER(GCC)
            asm (
                 "movl $0x1, %%eax;"
                 "pushl %%ebx;"
                 "cpuid;"
                 "popl %%ebx;"
                 "movl %%edx, %0;"
                 : "=g" (flags)
                 :
                 : "%eax", "%ecx", "%edx"
                 );
#else
            flags = 0;
#endif
            present = (flags & SSE2FeatureBit) != 0;
        }
        bool present;
    };
    static SSE2Check check;
    return check.present;
}

#endif

namespace JSC {

void JIT::compileFastArith_op_lshift(unsigned result, unsigned op1, unsigned op2)
{
    emitGetVirtualRegisters(op1, regT0, op2, regT2);
    // FIXME: would we be better using 'emitJumpSlowCaseIfNotImmediateIntegers'? - we *probably* ought to be consistent.
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT2);
    emitFastArithImmToInt(regT0);
    emitFastArithImmToInt(regT2);
#if !PLATFORM(X86)
    // Mask with 0x1f as per ecma-262 11.7.2 step 7.
    // On 32-bit x86 this is not necessary, since the shift anount is implicitly masked in the instruction.
    and32(Imm32(0x1f), regT2);
#endif
    lshift32(regT2, regT0);
#if !USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(branchAdd32(Overflow, regT0, regT0));
    signExtend32ToPtr(regT0, regT0);
#endif
    emitFastArithReTagImmediate(regT0, regT0);
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_lshift(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator& iter)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    UNUSED_PARAM(op1);
    UNUSED_PARAM(op2);
    linkSlowCase(iter);
    linkSlowCase(iter);
#else
    // If we are limited to 32-bit immediates there is a third slow case, which required the operands to have been reloaded.
    Jump notImm1 = getSlowCase(iter);
    Jump notImm2 = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegisters(op1, regT0, op2, regT2);
    notImm1.link(this);
    notImm2.link(this);
#endif
    JITStubCall stubCall(this, JITStubs::cti_op_lshift);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT2);
    stubCall.call(result);
}

void JIT::compileFastArith_op_rshift(unsigned result, unsigned op1, unsigned op2)
{
    if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        // Mask with 0x1f as per ecma-262 11.7.2 step 7.
#if USE(ALTERNATE_JSIMMEDIATE)
        rshift32(Imm32(getConstantOperandImmediateInt(op2) & 0x1f), regT0);
#else
        rshiftPtr(Imm32(getConstantOperandImmediateInt(op2) & 0x1f), regT0);
#endif
    } else {
        emitGetVirtualRegisters(op1, regT0, op2, regT2);
        if (isSSE2Present()) {
            Jump lhsIsInt = emitJumpIfImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
            addSlowCase(emitJumpIfNotImmediateNumber(regT0));
            __ movq_rr(regT0, X86::xmm0);
#else
            emitJumpSlowCaseIfNotJSCell(regT0, op1);
            addSlowCase(checkStructure(regT0, m_globalData->numberStructure.get()));
            __ movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT0, X86::xmm0);
#endif
            __ cvttsd2si_rr(X86::xmm0, regT0);
            addSlowCase(branch32(Equal, regT0, Imm32(0x80000000)));
#if !USE(ALTERNATE_JSIMMEDIATE)
            add32(regT0, regT0);
            addSlowCase(__ jo());
#endif
            lhsIsInt.link(this);
        } else
            emitJumpSlowCaseIfNotImmediateInteger(regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT2);
        emitFastArithImmToInt(regT2);
#if !PLATFORM(X86)
        // Mask with 0x1f as per ecma-262 11.7.2 step 7.
        // On 32-bit x86 this is not necessary, since the shift anount is implicitly masked in the instruction.
        and32(Imm32(0x1f), regT2);
#endif
#if USE(ALTERNATE_JSIMMEDIATE)
        rshift32(regT2, regT0);
#else
        rshiftPtr(regT2, regT0);
#endif
    }
#if USE(ALTERNATE_JSIMMEDIATE)
    emitFastArithIntToImmNoCheck(regT0, regT0);
#else
    orPtr(Imm32(JSImmediate::TagTypeNumber), regT0);
#endif
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_rshift(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    if (isOperandConstantImmediateInt(op2))
        emitPutJITStubArgFromVirtualRegister(op2, 2, regT2);
    else {
        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            linkSlowCase(iter);
#else
            linkSlowCaseIfNotJSCell(iter, op1);
            linkSlowCase(iter);
            linkSlowCase(iter);
#endif
            linkSlowCase(iter);
            // We're reloading op1 to regT0 as we can no longer guarantee that
            // we have not munged the operand.  It may have already been shifted
            // correctly, but it still will not have been tagged.
            emitGetVirtualRegister(op1, regT0);
        } else {
            linkSlowCase(iter);
            linkSlowCase(iter);
        }
        emitPutJITStubArg(regT2, 2);
    }

    JITStubCall stubCall(this, JITStubs::cti_op_rshift);
    stubCall.addArgument(regT0);
    stubCall.call(result);
}

void JIT::compileFastArith_op_jnless(unsigned op1, unsigned op2, unsigned target)
{
    // We generate inline code for the following cases in the fast path:
    // - int immediate to constant int immediate
    // - constant int immediate to int immediate
    // - int immediate to int immediate

    if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t op2imm = getConstantOperandImmediateInt(op2);
#else
        int32_t op2imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)));
#endif
        addJump(branch32(GreaterThanOrEqual, regT0, Imm32(op2imm)), target + 3);
    } else if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t op1imm = getConstantOperandImmediateInt(op1);
#else
        int32_t op1imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op1)));
#endif
        addJump(branch32(LessThanOrEqual, regT1, Imm32(op1imm)), target + 3);
    } else {
        emitGetVirtualRegisters(op1, regT0, op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);

        addJump(branch32(GreaterThanOrEqual, regT0, regT1), target + 3);
    }
}
void JIT::compileFastArithSlow_op_jnless(unsigned op1, unsigned op2, unsigned target, Vector<SlowCaseEntry>::iterator& iter)
{
    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);

        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            addPtr(tagTypeNumberRegister, regT0);
            m_assembler.movq_rr(regT0, X86::xmm0);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2 = checkStructure(regT0, m_globalData->numberStructure.get());
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT0, X86::xmm0);
#endif
            
            int32_t op2imm = getConstantOperand(op2).getInt32Fast();;
                    
            m_assembler.movl_i32r(op2imm, regT1);
            m_assembler.cvtsi2sd_rr(regT1, X86::xmm1);

            m_assembler.ucomisd_rr(X86::xmm0, X86::xmm1);
            emitJumpSlowToHot(Jump::Jump(m_assembler.jbe()), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnless));

#if USE(ALTERNATE_JSIMMEDIATE)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, JITStubs::cti_op_jless);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else if (isOperandConstantImmediateInt(op1)) {
        linkSlowCase(iter);

        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT1);
            addPtr(tagTypeNumberRegister, regT1);
            m_assembler.movq_rr(regT1, X86::xmm1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1 = emitJumpIfNotJSCell(regT1);
            
            Jump fail2 = checkStructure(regT1, m_globalData->numberStructure.get());
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT1, X86::xmm1);
#endif
            
            int32_t op1imm = getConstantOperand(op1).getInt32Fast();;
                    
            m_assembler.movl_i32r(op1imm, regT0);
            m_assembler.cvtsi2sd_rr(regT0, X86::xmm0);

            m_assembler.ucomisd_rr(X86::xmm0, X86::xmm1);
            emitJumpSlowToHot(Jump::Jump(m_assembler.jbe()), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnless));

#if USE(ALTERNATE_JSIMMEDIATE)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, JITStubs::cti_op_jless);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else {
        linkSlowCase(iter);

        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            Jump fail2 = emitJumpIfNotImmediateNumber(regT1);
            Jump fail3 = emitJumpIfImmediateInteger(regT1);
            addPtr(tagTypeNumberRegister, regT0);
            addPtr(tagTypeNumberRegister, regT1);
            m_assembler.movq_rr(regT0, X86::xmm0);
            m_assembler.movq_rr(regT1, X86::xmm1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail2 = emitJumpIfNotJSCell(regT1);

            Jump fail3 = checkStructure(regT0, m_globalData->numberStructure.get());
            Jump fail4 = checkStructure(regT1, m_globalData->numberStructure.get());
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT0, X86::xmm0);
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT1, X86::xmm1);
#endif

            m_assembler.ucomisd_rr(X86::xmm0, X86::xmm1);
            emitJumpSlowToHot(Jump::Jump(m_assembler.jbe()), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnless));

#if USE(ALTERNATE_JSIMMEDIATE)
            fail1.link(this);
            fail2.link(this);
            fail3.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1.link(this);
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail2.link(this);
            fail3.link(this);
            fail4.link(this);
#endif
        }

        linkSlowCase(iter);
        JITStubCall stubCall(this, JITStubs::cti_op_jless);
        stubCall.addArgument(regT0);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);
    }
}

void JIT::compileFastArith_op_jnlesseq(unsigned op1, unsigned op2, unsigned target)
{
    // We generate inline code for the following cases in the fast path:
    // - int immediate to constant int immediate
    // - constant int immediate to int immediate
    // - int immediate to int immediate

    if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t op2imm = getConstantOperandImmediateInt(op2);
#else
        int32_t op2imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)));
#endif
        addJump(branch32(GreaterThan, regT0, Imm32(op2imm)), target + 3);
    } else if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t op1imm = getConstantOperandImmediateInt(op1);
#else
        int32_t op1imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op1)));
#endif
        addJump(branch32(LessThan, regT1, Imm32(op1imm)), target + 3);
    } else {
        emitGetVirtualRegisters(op1, regT0, op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);

        addJump(branch32(GreaterThan, regT0, regT1), target + 3);
    }
}
void JIT::compileFastArithSlow_op_jnlesseq(unsigned op1, unsigned op2, unsigned target, Vector<SlowCaseEntry>::iterator& iter)
{
    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);

        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            addPtr(tagTypeNumberRegister, regT0);
            m_assembler.movq_rr(regT0, X86::xmm0);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2 = checkStructure(regT0, m_globalData->numberStructure.get());
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT0, X86::xmm0);
#endif
            
            int32_t op2imm = getConstantOperand(op2).getInt32Fast();;
                    
            m_assembler.movl_i32r(op2imm, regT1);
            m_assembler.cvtsi2sd_rr(regT1, X86::xmm1);

            m_assembler.ucomisd_rr(X86::xmm0, X86::xmm1);
            emitJumpSlowToHot(Jump::Jump(m_assembler.jb()), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnlesseq));

#if USE(ALTERNATE_JSIMMEDIATE)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, JITStubs::cti_op_jlesseq);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else if (isOperandConstantImmediateInt(op1)) {
        linkSlowCase(iter);

        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT1);
            addPtr(tagTypeNumberRegister, regT1);
            m_assembler.movq_rr(regT1, X86::xmm1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1 = emitJumpIfNotJSCell(regT1);
            
            Jump fail2 = checkStructure(regT1, m_globalData->numberStructure.get());
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT1, X86::xmm1);
#endif
            
            int32_t op1imm = getConstantOperand(op1).getInt32Fast();;
                    
            m_assembler.movl_i32r(op1imm, regT0);
            m_assembler.cvtsi2sd_rr(regT0, X86::xmm0);

            m_assembler.ucomisd_rr(X86::xmm0, X86::xmm1);
            emitJumpSlowToHot(Jump::Jump(m_assembler.jb()), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnlesseq));

#if USE(ALTERNATE_JSIMMEDIATE)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, JITStubs::cti_op_jlesseq);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else {
        linkSlowCase(iter);

        if (isSSE2Present()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            Jump fail2 = emitJumpIfNotImmediateNumber(regT1);
            Jump fail3 = emitJumpIfImmediateInteger(regT1);
            addPtr(tagTypeNumberRegister, regT0);
            addPtr(tagTypeNumberRegister, regT1);
            m_assembler.movq_rr(regT0, X86::xmm0);
            m_assembler.movq_rr(regT1, X86::xmm1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail2 = emitJumpIfNotJSCell(regT1);

            Jump fail3 = checkStructure(regT0, m_globalData->numberStructure.get());
            Jump fail4 = checkStructure(regT1, m_globalData->numberStructure.get());
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT0, X86::xmm0);
            m_assembler.movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), regT1, X86::xmm1);
#endif

            m_assembler.ucomisd_rr(X86::xmm0, X86::xmm1);
            emitJumpSlowToHot(Jump::Jump(m_assembler.jb()), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnlesseq));

#if USE(ALTERNATE_JSIMMEDIATE)
            fail1.link(this);
            fail2.link(this);
            fail3.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1.link(this);
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail2.link(this);
            fail3.link(this);
            fail4.link(this);
#endif
        }

        linkSlowCase(iter);
        JITStubCall stubCall(this, JITStubs::cti_op_jlesseq);
        stubCall.addArgument(regT0);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);
    }
}

void JIT::compileFastArith_op_bitand(unsigned result, unsigned op1, unsigned op2)
{
    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t imm = getConstantOperandImmediateInt(op1);
        andPtr(Imm32(imm), regT0);
        if (imm >= 0)
            emitFastArithIntToImmNoCheck(regT0, regT0);
#else
        andPtr(Imm32(static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op1)))), regT0);
#endif
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t imm = getConstantOperandImmediateInt(op2);
        andPtr(Imm32(imm), regT0);
        if (imm >= 0)
            emitFastArithIntToImmNoCheck(regT0, regT0);
#else
        andPtr(Imm32(static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)))), regT0);
#endif
    } else {
        emitGetVirtualRegisters(op1, regT0, op2, regT1);
        andPtr(regT1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
    }
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_bitand(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    if (isOperandConstantImmediateInt(op1)) {
        JITStubCall stubCall(this, JITStubs::cti_op_bitand);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT0);
        stubCall.call(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        JITStubCall stubCall(this, JITStubs::cti_op_bitand);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
    } else {
        JITStubCall stubCall(this, JITStubs::cti_op_bitand);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT1);
        stubCall.call(result);
    }
}

#if PLATFORM(X86) || PLATFORM(X86_64)
void JIT::compileFastArith_op_mod(unsigned result, unsigned op1, unsigned op2)
{
    emitGetVirtualRegisters(op1, X86::eax, op2, X86::ecx);
    emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
    emitJumpSlowCaseIfNotImmediateInteger(X86::ecx);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(branchPtr(Equal, X86::ecx, ImmPtr(JSValue::encode(jsNumber(m_globalData, 0)))));
    m_assembler.cdq();
    m_assembler.idivl_r(X86::ecx);
#else
    emitFastArithDeTagImmediate(X86::eax);
    addSlowCase(emitFastArithDeTagImmediateJumpIfZero(X86::ecx));
    m_assembler.cdq();
    m_assembler.idivl_r(X86::ecx);
    signExtend32ToPtr(X86::edx, X86::edx);
#endif
    emitFastArithReTagImmediate(X86::edx, X86::eax);
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_mod(unsigned result, unsigned, unsigned, Vector<SlowCaseEntry>::iterator& iter)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
#else
    Jump notImm1 = getSlowCase(iter);
    Jump notImm2 = getSlowCase(iter);
    linkSlowCase(iter);
    emitFastArithReTagImmediate(X86::eax, X86::eax);
    emitFastArithReTagImmediate(X86::ecx, X86::ecx);
    notImm1.link(this);
    notImm2.link(this);
#endif
    JITStubCall stubCall(this, JITStubs::cti_op_mod);
    stubCall.addArgument(X86::eax);
    stubCall.addArgument(X86::ecx);
    stubCall.call(result);
}
#else
void JIT::compileFastArith_op_mod(unsigned result, unsigned op1, unsigned op2)
{
    JITStubCall stubCall(this, JITStubs::cti_op_mod);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}
void JIT::compileFastArithSlow_op_mod(unsigned, unsigned, unsigned, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}
#endif

void JIT::compileFastArith_op_post_inc(unsigned result, unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, regT0);
    move(regT0, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(branchAdd32(Overflow, Imm32(1), regT1));
    emitFastArithIntToImmNoCheck(regT1, regT1);
#else
    addSlowCase(branchAdd32(Overflow, Imm32(1 << JSImmediate::IntegerPayloadShift), regT1));
    signExtend32ToPtr(regT1, regT1);
#endif
    emitPutVirtualRegister(srcDst, regT1);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArithSlow_op_post_inc(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, JITStubs::cti_op_post_inc);
    stubCall.addArgument(regT0);
    stubCall.call(result, srcDst);
}

void JIT::compileFastArith_op_post_dec(unsigned result, unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, regT0);
    move(regT0, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(branchSub32(Zero, Imm32(1), regT1));
    emitFastArithIntToImmNoCheck(regT1, regT1);
#else
    addSlowCase(branchSub32(Zero, Imm32(1 << JSImmediate::IntegerPayloadShift), regT1));
    signExtend32ToPtr(regT1, regT1);
#endif
    emitPutVirtualRegister(srcDst, regT1);
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_post_dec(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, JITStubs::cti_op_post_dec);
    stubCall.addArgument(regT0);
    stubCall.call(result, srcDst);
}

void JIT::compileFastArith_op_pre_inc(unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(branchAdd32(Overflow, Imm32(1), regT0));
    emitFastArithIntToImmNoCheck(regT0, regT0);
#else
    addSlowCase(branchAdd32(Overflow, Imm32(1 << JSImmediate::IntegerPayloadShift), regT0));
    signExtend32ToPtr(regT0, regT0);
#endif
    emitPutVirtualRegister(srcDst);
}
void JIT::compileFastArithSlow_op_pre_inc(unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    Jump notImm = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(srcDst, regT0);
    notImm.link(this);
    JITStubCall stubCall(this, JITStubs::cti_op_pre_inc);
    stubCall.addArgument(regT0);
    stubCall.call(srcDst);
}

void JIT::compileFastArith_op_pre_dec(unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(branchSub32(Zero, Imm32(1), regT0));
    emitFastArithIntToImmNoCheck(regT0, regT0);
#else
    addSlowCase(branchSub32(Zero, Imm32(1 << JSImmediate::IntegerPayloadShift), regT0));
    signExtend32ToPtr(regT0, regT0);
#endif
    emitPutVirtualRegister(srcDst);
}
void JIT::compileFastArithSlow_op_pre_dec(unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    Jump notImm = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(srcDst, regT0);
    notImm.link(this);
    JITStubCall stubCall(this, JITStubs::cti_op_pre_dec);
    stubCall.addArgument(regT0);
    stubCall.call(srcDst);
}


#if !ENABLE(JIT_OPTIMIZE_ARITHMETIC)

void JIT::compileFastArith_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_add);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::compileFastArithSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::compileFastArith_op_mul(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_mul);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::compileFastArithSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::compileFastArith_op_sub(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_sub);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::compileFastArithSlow_op_sub(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

#elif USE(ALTERNATE_JSIMMEDIATE) // *AND* ENABLE(JIT_OPTIMIZE_ARITHMETIC)

void JIT::compileBinaryArithOp(OpcodeID opcodeID, unsigned, unsigned op1, unsigned op2, OperandTypes)
{
    emitGetVirtualRegisters(op1, X86::eax, op2, X86::edx);
    emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
    emitJumpSlowCaseIfNotImmediateInteger(X86::edx);
    if (opcodeID == op_add)
        addSlowCase(branchAdd32(Overflow, X86::edx, X86::eax));
    else if (opcodeID == op_sub)
        addSlowCase(branchSub32(Overflow, X86::edx, X86::eax));
    else {
        ASSERT(opcodeID == op_mul);
        addSlowCase(branchMul32(Overflow, X86::edx, X86::eax));
        addSlowCase(branchTest32(Zero, X86::eax));
    }
    emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
}

void JIT::compileBinaryArithOpSlowCase(OpcodeID opcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned result, unsigned op1, unsigned, OperandTypes types)
{
    // We assume that subtracting TagTypeNumber is equivalent to adding DoubleEncodeOffset.
    COMPILE_ASSERT(((JSImmediate::TagTypeNumber + JSImmediate::DoubleEncodeOffset) == 0), TagTypeNumber_PLUS_DoubleEncodeOffset_EQUALS_0);

    Jump notImm1 = getSlowCase(iter);
    Jump notImm2 = getSlowCase(iter);

    linkSlowCase(iter); // Integer overflow case - we could handle this in JIT code, but this is likely rare.
    if (opcodeID == op_mul) // op_mul has an extra slow case to handle 0 * negative number.
        linkSlowCase(iter);
    emitGetVirtualRegister(op1, X86::eax);

    Label stubFunctionCall(this);
    JITStubCall stubCall(this, opcodeID == op_add ? JITStubs::cti_op_add : opcodeID == op_sub ? JITStubs::cti_op_sub : JITStubs::cti_op_mul);
    stubCall.addArgument(X86::eax);
    stubCall.addArgument(X86::edx);
    stubCall.call(result);
    Jump end = jump();

    // if we get here, eax is not an int32, edx not yet checked.
    notImm1.link(this);
    if (!types.first().definitelyIsNumber())
        emitJumpIfNotImmediateNumber(X86::eax).linkTo(stubFunctionCall, this);
    if (!types.second().definitelyIsNumber())
        emitJumpIfNotImmediateNumber(X86::edx).linkTo(stubFunctionCall, this);
    addPtr(tagTypeNumberRegister, X86::eax);
    m_assembler.movq_rr(X86::eax, X86::xmm1);
    Jump op2isDouble = emitJumpIfNotImmediateInteger(X86::edx);
    m_assembler.cvtsi2sd_rr(X86::edx, X86::xmm2);
    Jump op2wasInteger = jump();

    // if we get here, eax IS an int32, edx is not.
    notImm2.link(this);
    if (!types.second().definitelyIsNumber())
        emitJumpIfNotImmediateNumber(X86::edx).linkTo(stubFunctionCall, this);
    m_assembler.cvtsi2sd_rr(X86::eax, X86::xmm1);
    op2isDouble.link(this);
    addPtr(tagTypeNumberRegister, X86::edx);
    m_assembler.movq_rr(X86::edx, X86::xmm2);
    op2wasInteger.link(this);

    if (opcodeID == op_add)
        m_assembler.addsd_rr(X86::xmm2, X86::xmm1);
    else if (opcodeID == op_sub)
        m_assembler.subsd_rr(X86::xmm2, X86::xmm1);
    else {
        ASSERT(opcodeID == op_mul);
        m_assembler.mulsd_rr(X86::xmm2, X86::xmm1);
    }
    m_assembler.movq_rr(X86::xmm1, X86::eax);
    subPtr(tagTypeNumberRegister, X86::eax);
    emitPutVirtualRegister(result, X86::eax);

    end.link(this);
}

void JIT::compileFastArith_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!types.first().mightBeNumber() || !types.second().mightBeNumber()) {
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(op1, X86::ecx);
        stubCall.addArgument(op2, X86::ecx);
        stubCall.call(result);
        return;
    }

    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op1)), X86::eax));
        emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op2)), X86::eax));
        emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
    } else
        compileBinaryArithOp(op_add, result, op1, op2, types);

    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1) || isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);
        linkSlowCase(iter);
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(op1, X86::ecx);
        stubCall.addArgument(op2, X86::ecx);
        stubCall.call(result);
    } else
        compileBinaryArithOpSlowCase(op_add, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::compileFastArith_op_mul(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    // For now, only plant a fast int case if the constant operand is greater than zero.
    int32_t value;
    if (isOperandConstantImmediateInt(op1) && ((value = getConstantOperandImmediateInt(op1)) > 0)) {
        emitGetVirtualRegister(op2, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        addSlowCase(branchMul32(Overflow, Imm32(value), X86::eax, X86::eax));
        emitFastArithReTagImmediate(X86::eax, X86::eax);
    } else if (isOperandConstantImmediateInt(op2) && ((value = getConstantOperandImmediateInt(op2)) > 0)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        addSlowCase(branchMul32(Overflow, Imm32(value), X86::eax, X86::eax));
        emitFastArithReTagImmediate(X86::eax, X86::eax);
    } else
        compileBinaryArithOp(op_mul, result, op1, op2, types);

    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_mul(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if ((isOperandConstantImmediateInt(op1) && (getConstantOperandImmediateInt(op1) > 0))
        || (isOperandConstantImmediateInt(op2) && (getConstantOperandImmediateInt(op2) > 0))) {
        linkSlowCase(iter);
        linkSlowCase(iter);
        // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
        JITStubCall stubCall(this, JITStubs::cti_op_mul);
        stubCall.addArgument(op1, X86::ecx);
        stubCall.addArgument(op2, X86::ecx);
        stubCall.call(result);
    } else
        compileBinaryArithOpSlowCase(op_mul, iter, result, op1, op2, types);
}

void JIT::compileFastArith_op_sub(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    compileBinaryArithOp(op_sub, result, op1, op2, types);

    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_sub(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    compileBinaryArithOpSlowCase(op_sub, iter, result, op1, op2, types);
}

#else

typedef X86Assembler::JmpSrc JmpSrc;
typedef X86Assembler::JmpDst JmpDst;
typedef X86Assembler::XMMRegisterID XMMRegisterID;


void JIT::compileBinaryArithOp(OpcodeID opcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes types)
{
    Structure* numberStructure = m_globalData->numberStructure.get();
    JmpSrc wasJSNumberCell1;
    JmpSrc wasJSNumberCell2;

    emitGetVirtualRegisters(src1, X86::eax, src2, X86::edx);

    if (types.second().isReusable() && isSSE2Present()) {
        ASSERT(types.second().mightBeNumber());

        // Check op2 is a number
        __ testl_i32r(JSImmediate::TagTypeNumber, X86::edx);
        JmpSrc op2imm = __ jne();
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::edx, src2);
            __ cmpl_im(reinterpret_cast<unsigned>(numberStructure), FIELD_OFFSET(JSCell, m_structure), X86::edx);
            addSlowCase(__ jne());
        }

        // (1) In this case src2 is a reusable number cell.
        //     Slow case if src1 is not a number type.
        __ testl_i32r(JSImmediate::TagTypeNumber, X86::eax);
        JmpSrc op1imm = __ jne();
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::eax, src1);
            __ cmpl_im(reinterpret_cast<unsigned>(numberStructure), FIELD_OFFSET(JSCell, m_structure), X86::eax);
            addSlowCase(__ jne());
        }

        // (1a) if we get here, src1 is also a number cell
        __ movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::eax, X86::xmm0);
        JmpSrc loadedDouble = __ jmp();
        // (1b) if we get here, src1 is an immediate
        __ linkJump(op1imm, __ label());
        emitFastArithImmToInt(X86::eax);
        __ cvtsi2sd_rr(X86::eax, X86::xmm0);
        // (1c) 
        __ linkJump(loadedDouble, __ label());
        if (opcodeID == op_add)
            __ addsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        else if (opcodeID == op_sub)
            __ subsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        else {
            ASSERT(opcodeID == op_mul);
            __ mulsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        }

        // Store the result to the JSNumberCell and jump.
        __ movsd_rm(X86::xmm0, FIELD_OFFSET(JSNumberCell, m_value), X86::edx);
        __ movl_rr(X86::edx, X86::eax);
        emitPutVirtualRegister(dst);
        wasJSNumberCell2 = __ jmp();

        // (2) This handles cases where src2 is an immediate number.
        //     Two slow cases - either src1 isn't an immediate, or the subtract overflows.
        __ linkJump(op2imm, __ label());
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
    } else if (types.first().isReusable() && isSSE2Present()) {
        ASSERT(types.first().mightBeNumber());

        // Check op1 is a number
        __ testl_i32r(JSImmediate::TagTypeNumber, X86::eax);
        JmpSrc op1imm = __ jne();
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::eax, src1);
            __ cmpl_im(reinterpret_cast<unsigned>(numberStructure), FIELD_OFFSET(JSCell, m_structure), X86::eax);
            addSlowCase(__ jne());
        }

        // (1) In this case src1 is a reusable number cell.
        //     Slow case if src2 is not a number type.
        __ testl_i32r(JSImmediate::TagTypeNumber, X86::edx);
        JmpSrc op2imm = __ jne();
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::edx, src2);
            __ cmpl_im(reinterpret_cast<unsigned>(numberStructure), FIELD_OFFSET(JSCell, m_structure), X86::edx);
            addSlowCase(__ jne());
        }

        // (1a) if we get here, src2 is also a number cell
        __ movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm1);
        JmpSrc loadedDouble = __ jmp();
        // (1b) if we get here, src2 is an immediate
        __ linkJump(op2imm, __ label());
        emitFastArithImmToInt(X86::edx);
        __ cvtsi2sd_rr(X86::edx, X86::xmm1);
        // (1c) 
        __ linkJump(loadedDouble, __ label());
        __ movsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::eax, X86::xmm0);
        if (opcodeID == op_add)
            __ addsd_rr(X86::xmm1, X86::xmm0);
        else if (opcodeID == op_sub)
            __ subsd_rr(X86::xmm1, X86::xmm0);
        else {
            ASSERT(opcodeID == op_mul);
            __ mulsd_rr(X86::xmm1, X86::xmm0);
        }
        __ movsd_rm(X86::xmm0, FIELD_OFFSET(JSNumberCell, m_value), X86::eax);
        emitPutVirtualRegister(dst);

        // Store the result to the JSNumberCell and jump.
        __ movsd_rm(X86::xmm0, FIELD_OFFSET(JSNumberCell, m_value), X86::eax);
        emitPutVirtualRegister(dst);
        wasJSNumberCell1 = __ jmp();

        // (2) This handles cases where src1 is an immediate number.
        //     Two slow cases - either src2 isn't an immediate, or the subtract overflows.
        __ linkJump(op1imm, __ label());
        emitJumpSlowCaseIfNotImmediateInteger(X86::edx);
    } else
        emitJumpSlowCaseIfNotImmediateIntegers(X86::eax, X86::edx, X86::ecx);

    if (opcodeID == op_add) {
        emitFastArithDeTagImmediate(X86::eax);
        __ addl_rr(X86::edx, X86::eax);
        addSlowCase(__ jo());
    } else  if (opcodeID == op_sub) {
        __ subl_rr(X86::edx, X86::eax);
        addSlowCase(__ jo());
        signExtend32ToPtr(X86::eax, X86::eax);
        emitFastArithReTagImmediate(X86::eax, X86::eax);
    } else {
        ASSERT(opcodeID == op_mul);
        // convert eax & edx from JSImmediates to ints, and check if either are zero
        emitFastArithImmToInt(X86::edx);
        Jump op1Zero = emitFastArithDeTagImmediateJumpIfZero(X86::eax);
        __ testl_rr(X86::edx, X86::edx);
        JmpSrc op2NonZero = __ jne();
        op1Zero.link(this);
        // if either input is zero, add the two together, and check if the result is < 0.
        // If it is, we have a problem (N < 0), (N * 0) == -0, not representatble as a JSImmediate. 
        __ movl_rr(X86::eax, X86::ecx);
        __ addl_rr(X86::edx, X86::ecx);
        addSlowCase(__ js());
        // Skip the above check if neither input is zero
        __ linkJump(op2NonZero, __ label());
        __ imull_rr(X86::edx, X86::eax);
        addSlowCase(__ jo());
        signExtend32ToPtr(X86::eax, X86::eax);
        emitFastArithReTagImmediate(X86::eax, X86::eax);
    }
    emitPutVirtualRegister(dst);

    if (types.second().isReusable() && isSSE2Present()) {
        __ linkJump(wasJSNumberCell2, __ label());
    }
    else if (types.first().isReusable() && isSSE2Present()) {
        __ linkJump(wasJSNumberCell1, __ label());
    }
}

void JIT::compileBinaryArithOpSlowCase(OpcodeID opcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned dst, unsigned src1, unsigned src2, OperandTypes types)
{
    linkSlowCase(iter);
    if (types.second().isReusable() && isSSE2Present()) {
        if (!types.first().definitelyIsNumber()) {
            linkSlowCaseIfNotJSCell(iter, src1);
            linkSlowCase(iter);
        }
        if (!types.second().definitelyIsNumber()) {
            linkSlowCaseIfNotJSCell(iter, src2);
            linkSlowCase(iter);
        }
    } else if (types.first().isReusable() && isSSE2Present()) {
        if (!types.first().definitelyIsNumber()) {
            linkSlowCaseIfNotJSCell(iter, src1);
            linkSlowCase(iter);
        }
        if (!types.second().definitelyIsNumber()) {
            linkSlowCaseIfNotJSCell(iter, src2);
            linkSlowCase(iter);
        }
    }
    linkSlowCase(iter);

    // additional entry point to handle -0 cases.
    if (opcodeID == op_mul)
        linkSlowCase(iter);

    JITStubCall stubCall(this, opcodeID == op_add ? JITStubs::cti_op_add : opcodeID == op_sub ? JITStubs::cti_op_sub : JITStubs::cti_op_mul);
    stubCall.addArgument(src1, X86::ecx);
    stubCall.addArgument(src2, X86::ecx);
    stubCall.call(dst);
}

void JIT::compileFastArith_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
        emitPutVirtualRegister(result);
    } else {
        OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
        if (types.first().mightBeNumber() && types.second().mightBeNumber())
            compileBinaryArithOp(op_add, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
        else {
            JITStubCall stubCall(this, JITStubs::cti_op_add);
            stubCall.addArgument(op1, X86::ecx);
            stubCall.addArgument(op2, X86::ecx);
            stubCall.call(result);
        }
    }
}

void JIT::compileFastArithSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1)) {
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), X86::eax);
        notImm.link(this);
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(op1, X86::ecx);
        stubCall.addArgument(X86::eax);
        stubCall.call(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), X86::eax);
        notImm.link(this);
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(X86::eax);
        stubCall.addArgument(op2, X86::ecx);
        stubCall.call(result);
    } else {
        OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
        ASSERT(types.first().mightBeNumber() && types.second().mightBeNumber());
        compileBinaryArithOpSlowCase(op_add, iter, result, op1, op2, types);
    }
}

void JIT::compileFastArith_op_mul(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    // For now, only plant a fast int case if the constant operand is greater than zero.
    int32_t value;
    if (isOperandConstantImmediateInt(op1) && ((value = getConstantOperandImmediateInt(op1)) > 0)) {
        emitGetVirtualRegister(op2, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        emitFastArithDeTagImmediate(X86::eax);
        addSlowCase(branchMul32(Overflow, Imm32(value), X86::eax, X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
        emitFastArithReTagImmediate(X86::eax, X86::eax);
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2) && ((value = getConstantOperandImmediateInt(op2)) > 0)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmediateInteger(X86::eax);
        emitFastArithDeTagImmediate(X86::eax);
        addSlowCase(branchMul32(Overflow, Imm32(value), X86::eax, X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
        emitFastArithReTagImmediate(X86::eax, X86::eax);
        emitPutVirtualRegister(result);
    } else
        compileBinaryArithOp(op_mul, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}
void JIT::compileFastArithSlow_op_mul(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if ((isOperandConstantImmediateInt(op1) && (getConstantOperandImmediateInt(op1) > 0))
        || (isOperandConstantImmediateInt(op2) && (getConstantOperandImmediateInt(op2) > 0))) {
        linkSlowCase(iter);
        linkSlowCase(iter);
        // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
        JITStubCall stubCall(this, JITStubs::cti_op_mul);
        stubCall.addArgument(op1, X86::ecx);
        stubCall.addArgument(op2, X86::ecx);
        stubCall.call(result);
    } else
        compileBinaryArithOpSlowCase(op_mul, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::compileFastArith_op_sub(Instruction* currentInstruction)
{
    compileBinaryArithOp(op_sub, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
}
void JIT::compileFastArithSlow_op_sub(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileBinaryArithOpSlowCase(op_sub, iter, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
