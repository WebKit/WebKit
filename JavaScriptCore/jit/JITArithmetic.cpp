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
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif


using namespace std;

namespace JSC {

void JIT::emit_op_lshift(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

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

void JIT::emitSlow_op_lshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

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

void JIT::emit_op_rshift(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op2)) {
        // isOperandConstantImmediateInt(op2) => 1 SlowCase
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
        if (supportsFloatingPointTruncate()) {
            Jump lhsIsInt = emitJumpIfImmediateInteger(regT0);
#if USE(ALTERNATE_JSIMMEDIATE)
            // supportsFloatingPoint() && USE(ALTERNATE_JSIMMEDIATE) => 3 SlowCases
            addSlowCase(emitJumpIfNotImmediateNumber(regT0));
            addPtr(tagTypeNumberRegister, regT0);
            movePtrToDouble(regT0, fpRegT0);
            addSlowCase(branchTruncateDoubleToInt32(fpRegT0, regT0));
#else
            // supportsFloatingPoint() && !USE(ALTERNATE_JSIMMEDIATE) => 5 SlowCases (of which 1 IfNotJSCell)
            emitJumpSlowCaseIfNotJSCell(regT0, op1);
            addSlowCase(checkStructure(regT0, m_globalData->numberStructure.get()));
            loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
            addSlowCase(branchTruncateDoubleToInt32(fpRegT0, regT0));
            addSlowCase(branchAdd32(Overflow, regT0, regT0));
#endif
            lhsIsInt.link(this);
            emitJumpSlowCaseIfNotImmediateInteger(regT2);
        } else {
            // !supportsFloatingPoint() => 2 SlowCases
            emitJumpSlowCaseIfNotImmediateInteger(regT0);
            emitJumpSlowCaseIfNotImmediateInteger(regT2);
        }
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

void JIT::emitSlow_op_rshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_rshift);

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
    } else {
        if (supportsFloatingPointTruncate()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
#else
            linkSlowCaseIfNotJSCell(iter, op1);
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
#endif
            // We're reloading op1 to regT0 as we can no longer guarantee that
            // we have not munged the operand.  It may have already been shifted
            // correctly, but it still will not have been tagged.
            stubCall.addArgument(op1, regT0);
            stubCall.addArgument(regT2);
        } else {
            linkSlowCase(iter);
            linkSlowCase(iter);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT2);
        }
    }

    stubCall.call(result);
}

void JIT::emit_op_jnless(Instruction* currentInstruction)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

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

void JIT::emitSlow_op_jnless(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);

        if (supportsFloatingPoint()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            addPtr(tagTypeNumberRegister, regT0);
            movePtrToDouble(regT0, fpRegT0);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2 = checkStructure(regT0, m_globalData->numberStructure.get());
            loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
#endif
            
            int32_t op2imm = getConstantOperand(op2).getInt32Fast();;
                    
            move(Imm32(op2imm), regT1);
            convertInt32ToDouble(regT1, fpRegT1);

            emitJumpSlowToHot(branchDouble(DoubleLessThanOrEqual, fpRegT1, fpRegT0), target + 3);

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

        if (supportsFloatingPoint()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT1);
            addPtr(tagTypeNumberRegister, regT1);
            movePtrToDouble(regT1, fpRegT1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1 = emitJumpIfNotJSCell(regT1);
            
            Jump fail2 = checkStructure(regT1, m_globalData->numberStructure.get());
            loadDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT1);
#endif
            
            int32_t op1imm = getConstantOperand(op1).getInt32Fast();;
                    
            move(Imm32(op1imm), regT0);
            convertInt32ToDouble(regT0, fpRegT0);

            emitJumpSlowToHot(branchDouble(DoubleLessThanOrEqual, fpRegT1, fpRegT0), target + 3);

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

        if (supportsFloatingPoint()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            Jump fail2 = emitJumpIfNotImmediateNumber(regT1);
            Jump fail3 = emitJumpIfImmediateInteger(regT1);
            addPtr(tagTypeNumberRegister, regT0);
            addPtr(tagTypeNumberRegister, regT1);
            movePtrToDouble(regT0, fpRegT0);
            movePtrToDouble(regT1, fpRegT1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail2 = emitJumpIfNotJSCell(regT1);

            Jump fail3 = checkStructure(regT0, m_globalData->numberStructure.get());
            Jump fail4 = checkStructure(regT1, m_globalData->numberStructure.get());
            loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
            loadDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT1);
#endif

            emitJumpSlowToHot(branchDouble(DoubleLessThanOrEqual, fpRegT1, fpRegT0), target + 3);

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

void JIT::emit_op_jnlesseq(Instruction* currentInstruction)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

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

void JIT::emitSlow_op_jnlesseq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);

        if (supportsFloatingPoint()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            addPtr(tagTypeNumberRegister, regT0);
            movePtrToDouble(regT0, fpRegT0);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2 = checkStructure(regT0, m_globalData->numberStructure.get());
            loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
#endif
            
            int32_t op2imm = getConstantOperand(op2).getInt32Fast();;
                    
            move(Imm32(op2imm), regT1);
            convertInt32ToDouble(regT1, fpRegT1);

            emitJumpSlowToHot(branchDouble(DoubleLessThan, fpRegT1, fpRegT0), target + 3);

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

        if (supportsFloatingPoint()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT1);
            addPtr(tagTypeNumberRegister, regT1);
            movePtrToDouble(regT1, fpRegT1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1 = emitJumpIfNotJSCell(regT1);
            
            Jump fail2 = checkStructure(regT1, m_globalData->numberStructure.get());
            loadDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT1);
#endif
            
            int32_t op1imm = getConstantOperand(op1).getInt32Fast();;
                    
            move(Imm32(op1imm), regT0);
            convertInt32ToDouble(regT0, fpRegT0);

            emitJumpSlowToHot(branchDouble(DoubleLessThan, fpRegT1, fpRegT0), target + 3);

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

        if (supportsFloatingPoint()) {
#if USE(ALTERNATE_JSIMMEDIATE)
            Jump fail1 = emitJumpIfNotImmediateNumber(regT0);
            Jump fail2 = emitJumpIfNotImmediateNumber(regT1);
            Jump fail3 = emitJumpIfImmediateInteger(regT1);
            addPtr(tagTypeNumberRegister, regT0);
            addPtr(tagTypeNumberRegister, regT1);
            movePtrToDouble(regT0, fpRegT0);
            movePtrToDouble(regT1, fpRegT1);
#else
            Jump fail1;
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1 = emitJumpIfNotJSCell(regT0);

            Jump fail2;
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail2 = emitJumpIfNotJSCell(regT1);

            Jump fail3 = checkStructure(regT0, m_globalData->numberStructure.get());
            Jump fail4 = checkStructure(regT1, m_globalData->numberStructure.get());
            loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
            loadDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT1);
#endif

            emitJumpSlowToHot(branchDouble(DoubleLessThan, fpRegT1, fpRegT0), target + 3);

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

void JIT::emit_op_bitand(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

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

void JIT::emitSlow_op_bitand(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

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

void JIT::emit_op_post_inc(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

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

void JIT::emitSlow_op_post_inc(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, JITStubs::cti_op_post_inc);
    stubCall.addArgument(regT0);
    stubCall.addArgument(Imm32(srcDst));
    stubCall.call(result);
}

void JIT::emit_op_post_dec(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

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

void JIT::emitSlow_op_post_dec(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, JITStubs::cti_op_post_dec);
    stubCall.addArgument(regT0);
    stubCall.addArgument(Imm32(srcDst));
    stubCall.call(result);
}

void JIT::emit_op_pre_inc(Instruction* currentInstruction)
{
    unsigned srcDst = currentInstruction[1].u.operand;

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

void JIT::emitSlow_op_pre_inc(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    Jump notImm = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(srcDst, regT0);
    notImm.link(this);
    JITStubCall stubCall(this, JITStubs::cti_op_pre_inc);
    stubCall.addArgument(regT0);
    stubCall.call(srcDst);
}

void JIT::emit_op_pre_dec(Instruction* currentInstruction)
{
    unsigned srcDst = currentInstruction[1].u.operand;

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

void JIT::emitSlow_op_pre_dec(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    Jump notImm = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(srcDst, regT0);
    notImm.link(this);
    JITStubCall stubCall(this, JITStubs::cti_op_pre_dec);
    stubCall.addArgument(regT0);
    stubCall.call(srcDst);
}

/* ------------------------------ BEGIN: OP_MOD ------------------------------ */

#if PLATFORM(X86) || PLATFORM(X86_64)

void JIT::emit_op_mod(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

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

void JIT::emitSlow_op_mod(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;

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

#else // PLATFORM(X86) || PLATFORM(X86_64)

void JIT::emit_op_mod(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_mod);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::emitSlow_op_mod(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

#endif // PLATFORM(X86) || PLATFORM(X86_64)

/* ------------------------------ END: OP_MOD ------------------------------ */

#if !ENABLE(JIT_OPTIMIZE_ARITHMETIC)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_ARITHMETIC) (OP_ADD, OP_SUB, OP_MUL) ------------------------------ */

void JIT::emit_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_add);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::emitSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_mul(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_mul);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::emitSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_sub(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, JITStubs::cti_op_sub);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
}

void JIT::emitSlow_op_sub(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

#elif USE(ALTERNATE_JSIMMEDIATE) // *AND* ENABLE(JIT_OPTIMIZE_ARITHMETIC)

/* ------------------------------ BEGIN: USE(ALTERNATE_JSIMMEDIATE) (OP_ADD, OP_SUB, OP_MUL) ------------------------------ */

void JIT::compileBinaryArithOp(OpcodeID opcodeID, unsigned, unsigned op1, unsigned op2, OperandTypes)
{
    emitGetVirtualRegisters(op1, regT0, op2, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);
    if (opcodeID == op_add)
        addSlowCase(branchAdd32(Overflow, regT1, regT0));
    else if (opcodeID == op_sub)
        addSlowCase(branchSub32(Overflow, regT1, regT0));
    else {
        ASSERT(opcodeID == op_mul);
        addSlowCase(branchMul32(Overflow, regT1, regT0));
        addSlowCase(branchTest32(Zero, regT0));
    }
    emitFastArithIntToImmNoCheck(regT0, regT0);
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
    emitGetVirtualRegister(op1, regT0);

    Label stubFunctionCall(this);
    JITStubCall stubCall(this, opcodeID == op_add ? JITStubs::cti_op_add : opcodeID == op_sub ? JITStubs::cti_op_sub : JITStubs::cti_op_mul);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call(result);
    Jump end = jump();

    // if we get here, eax is not an int32, edx not yet checked.
    notImm1.link(this);
    if (!types.first().definitelyIsNumber())
        emitJumpIfNotImmediateNumber(regT0).linkTo(stubFunctionCall, this);
    if (!types.second().definitelyIsNumber())
        emitJumpIfNotImmediateNumber(regT1).linkTo(stubFunctionCall, this);
    addPtr(tagTypeNumberRegister, regT0);
    movePtrToDouble(regT0, fpRegT1);
    Jump op2isDouble = emitJumpIfNotImmediateInteger(regT1);
    convertInt32ToDouble(regT1, fpRegT2);
    Jump op2wasInteger = jump();

    // if we get here, eax IS an int32, edx is not.
    notImm2.link(this);
    if (!types.second().definitelyIsNumber())
        emitJumpIfNotImmediateNumber(regT1).linkTo(stubFunctionCall, this);
    convertInt32ToDouble(regT0, fpRegT1);
    op2isDouble.link(this);
    addPtr(tagTypeNumberRegister, regT1);
    movePtrToDouble(regT1, fpRegT2);
    op2wasInteger.link(this);

    if (opcodeID == op_add)
        addDouble(fpRegT2, fpRegT1);
    else if (opcodeID == op_sub)
        subDouble(fpRegT2, fpRegT1);
    else {
        ASSERT(opcodeID == op_mul);
        mulDouble(fpRegT2, fpRegT1);
    }
    moveDoubleToPtr(fpRegT1, regT0);
    subPtr(tagTypeNumberRegister, regT0);
    emitPutVirtualRegister(result, regT0);

    end.link(this);
}

void JIT::emit_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!types.first().mightBeNumber() || !types.second().mightBeNumber()) {
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
        return;
    }

    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op1)), regT0));
        emitFastArithIntToImmNoCheck(regT0, regT0);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op2)), regT0));
        emitFastArithIntToImmNoCheck(regT0, regT0);
    } else
        compileBinaryArithOp(op_add, result, op1, op2, types);

    emitPutVirtualRegister(result);
}

void JIT::emitSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1) || isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);
        linkSlowCase(iter);
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
    } else
        compileBinaryArithOpSlowCase(op_add, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::emit_op_mul(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    // For now, only plant a fast int case if the constant operand is greater than zero.
    int32_t value;
    if (isOperandConstantImmediateInt(op1) && ((value = getConstantOperandImmediateInt(op1)) > 0)) {
        emitGetVirtualRegister(op2, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        addSlowCase(branchMul32(Overflow, Imm32(value), regT0, regT0));
        emitFastArithReTagImmediate(regT0, regT0);
    } else if (isOperandConstantImmediateInt(op2) && ((value = getConstantOperandImmediateInt(op2)) > 0)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        addSlowCase(branchMul32(Overflow, Imm32(value), regT0, regT0));
        emitFastArithReTagImmediate(regT0, regT0);
    } else
        compileBinaryArithOp(op_mul, result, op1, op2, types);

    emitPutVirtualRegister(result);
}

void JIT::emitSlow_op_mul(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
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
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
    } else
        compileBinaryArithOpSlowCase(op_mul, iter, result, op1, op2, types);
}

void JIT::emit_op_sub(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    compileBinaryArithOp(op_sub, result, op1, op2, types);

    emitPutVirtualRegister(result);
}

void JIT::emitSlow_op_sub(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    compileBinaryArithOpSlowCase(op_sub, iter, result, op1, op2, types);
}

#else // !ENABLE(JIT_OPTIMIZE_ARITHMETIC)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_ARITHMETIC) (OP_ADD, OP_SUB, OP_MUL) ------------------------------ */

void JIT::compileBinaryArithOp(OpcodeID opcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes types)
{
    Structure* numberStructure = m_globalData->numberStructure.get();
    Jump wasJSNumberCell1;
    Jump wasJSNumberCell2;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);

    if (types.second().isReusable() && supportsFloatingPoint()) {
        ASSERT(types.second().mightBeNumber());

        // Check op2 is a number
        Jump op2imm = emitJumpIfImmediateInteger(regT1);
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(regT1, src2);
            addSlowCase(checkStructure(regT1, numberStructure));
        }

        // (1) In this case src2 is a reusable number cell.
        //     Slow case if src1 is not a number type.
        Jump op1imm = emitJumpIfImmediateInteger(regT0);
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(regT0, src1);
            addSlowCase(checkStructure(regT0, numberStructure));
        }

        // (1a) if we get here, src1 is also a number cell
        loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
        Jump loadedDouble = jump();
        // (1b) if we get here, src1 is an immediate
        op1imm.link(this);
        emitFastArithImmToInt(regT0);
        convertInt32ToDouble(regT0, fpRegT0);
        // (1c) 
        loadedDouble.link(this);
        if (opcodeID == op_add)
            addDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
        else if (opcodeID == op_sub)
            subDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
        else {
            ASSERT(opcodeID == op_mul);
            mulDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
        }

        // Store the result to the JSNumberCell and jump.
        storeDouble(fpRegT0, Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)));
        move(regT1, regT0);
        emitPutVirtualRegister(dst);
        wasJSNumberCell2 = jump();

        // (2) This handles cases where src2 is an immediate number.
        //     Two slow cases - either src1 isn't an immediate, or the subtract overflows.
        op2imm.link(this);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
    } else if (types.first().isReusable() && supportsFloatingPoint()) {
        ASSERT(types.first().mightBeNumber());

        // Check op1 is a number
        Jump op1imm = emitJumpIfImmediateInteger(regT0);
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(regT0, src1);
            addSlowCase(checkStructure(regT0, numberStructure));
        }

        // (1) In this case src1 is a reusable number cell.
        //     Slow case if src2 is not a number type.
        Jump op2imm = emitJumpIfImmediateInteger(regT1);
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(regT1, src2);
            addSlowCase(checkStructure(regT1, numberStructure));
        }

        // (1a) if we get here, src2 is also a number cell
        loadDouble(Address(regT1, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT1);
        Jump loadedDouble = jump();
        // (1b) if we get here, src2 is an immediate
        op2imm.link(this);
        emitFastArithImmToInt(regT1);
        convertInt32ToDouble(regT1, fpRegT1);
        // (1c) 
        loadedDouble.link(this);
        loadDouble(Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)), fpRegT0);
        if (opcodeID == op_add)
            addDouble(fpRegT1, fpRegT0);
        else if (opcodeID == op_sub)
            subDouble(fpRegT1, fpRegT0);
        else {
            ASSERT(opcodeID == op_mul);
            mulDouble(fpRegT1, fpRegT0);
        }
        storeDouble(fpRegT0, Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)));
        emitPutVirtualRegister(dst);

        // Store the result to the JSNumberCell and jump.
        storeDouble(fpRegT0, Address(regT0, OBJECT_OFFSETOF(JSNumberCell, m_value)));
        emitPutVirtualRegister(dst);
        wasJSNumberCell1 = jump();

        // (2) This handles cases where src1 is an immediate number.
        //     Two slow cases - either src2 isn't an immediate, or the subtract overflows.
        op1imm.link(this);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);
    } else
        emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);

    if (opcodeID == op_add) {
        emitFastArithDeTagImmediate(regT0);
        addSlowCase(branchAdd32(Overflow, regT1, regT0));
    } else  if (opcodeID == op_sub) {
        addSlowCase(branchSub32(Overflow, regT1, regT0));
        signExtend32ToPtr(regT0, regT0);
        emitFastArithReTagImmediate(regT0, regT0);
    } else {
        ASSERT(opcodeID == op_mul);
        // convert eax & edx from JSImmediates to ints, and check if either are zero
        emitFastArithImmToInt(regT1);
        Jump op1Zero = emitFastArithDeTagImmediateJumpIfZero(regT0);
        Jump op2NonZero = branchTest32(NonZero, regT1);
        op1Zero.link(this);
        // if either input is zero, add the two together, and check if the result is < 0.
        // If it is, we have a problem (N < 0), (N * 0) == -0, not representatble as a JSImmediate. 
        move(regT0, regT2);
        addSlowCase(branchAdd32(Signed, regT1, regT2));
        // Skip the above check if neither input is zero
        op2NonZero.link(this);
        addSlowCase(branchMul32(Overflow, regT1, regT0));
        signExtend32ToPtr(regT0, regT0);
        emitFastArithReTagImmediate(regT0, regT0);
    }
    emitPutVirtualRegister(dst);

    if (types.second().isReusable() && supportsFloatingPoint())
        wasJSNumberCell2.link(this);
    else if (types.first().isReusable() && supportsFloatingPoint())
        wasJSNumberCell1.link(this);
}

void JIT::compileBinaryArithOpSlowCase(OpcodeID opcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned dst, unsigned src1, unsigned src2, OperandTypes types)
{
    linkSlowCase(iter);
    if (types.second().isReusable() && supportsFloatingPoint()) {
        if (!types.first().definitelyIsNumber()) {
            linkSlowCaseIfNotJSCell(iter, src1);
            linkSlowCase(iter);
        }
        if (!types.second().definitelyIsNumber()) {
            linkSlowCaseIfNotJSCell(iter, src2);
            linkSlowCase(iter);
        }
    } else if (types.first().isReusable() && supportsFloatingPoint()) {
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
    stubCall.addArgument(src1, regT2);
    stubCall.addArgument(src2, regT2);
    stubCall.call(dst);
}

void JIT::emit_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), regT0));
        signExtend32ToPtr(regT0, regT0);
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        addSlowCase(branchAdd32(Overflow, Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), regT0));
        signExtend32ToPtr(regT0, regT0);
        emitPutVirtualRegister(result);
    } else {
        OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
        if (types.first().mightBeNumber() && types.second().mightBeNumber())
            compileBinaryArithOp(op_add, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
        else {
            JITStubCall stubCall(this, JITStubs::cti_op_add);
            stubCall.addArgument(op1, regT2);
            stubCall.addArgument(op2, regT2);
            stubCall.call(result);
        }
    }
}

void JIT::emitSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1)) {
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), regT0);
        notImm.link(this);
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT0);
        stubCall.call(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), regT0);
        notImm.link(this);
        JITStubCall stubCall(this, JITStubs::cti_op_add);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
    } else {
        OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
        ASSERT(types.first().mightBeNumber() && types.second().mightBeNumber());
        compileBinaryArithOpSlowCase(op_add, iter, result, op1, op2, types);
    }
}

void JIT::emit_op_mul(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    // For now, only plant a fast int case if the constant operand is greater than zero.
    int32_t value;
    if (isOperandConstantImmediateInt(op1) && ((value = getConstantOperandImmediateInt(op1)) > 0)) {
        emitGetVirtualRegister(op2, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        emitFastArithDeTagImmediate(regT0);
        addSlowCase(branchMul32(Overflow, Imm32(value), regT0, regT0));
        signExtend32ToPtr(regT0, regT0);
        emitFastArithReTagImmediate(regT0, regT0);
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2) && ((value = getConstantOperandImmediateInt(op2)) > 0)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        emitFastArithDeTagImmediate(regT0);
        addSlowCase(branchMul32(Overflow, Imm32(value), regT0, regT0));
        signExtend32ToPtr(regT0, regT0);
        emitFastArithReTagImmediate(regT0, regT0);
        emitPutVirtualRegister(result);
    } else
        compileBinaryArithOp(op_mul, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::emitSlow_op_mul(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
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
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
    } else
        compileBinaryArithOpSlowCase(op_mul, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::emit_op_sub(Instruction* currentInstruction)
{
    compileBinaryArithOp(op_sub, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::emitSlow_op_sub(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileBinaryArithOpSlowCase(op_sub, iter, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

#endif // !ENABLE(JIT_OPTIMIZE_ARITHMETIC)

/* ------------------------------ END: OP_ADD, OP_SUB, OP_MUL ------------------------------ */

} // namespace JSC

#endif // ENABLE(JIT)
