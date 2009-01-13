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

namespace JSC {

void JIT::compileFastArith_op_lshift(unsigned result, unsigned op1, unsigned op2)
{
    emitGetVirtualRegisters(op1, X86::eax, op2, X86::ecx);
    // FIXME: would we be better using 'emitJumpSlowCaseIfNotImmNums'? - we *probably* ought to be consistent.
    emitJumpSlowCaseIfNotImmNum(X86::eax);
    emitJumpSlowCaseIfNotImmNum(X86::ecx);
    emitFastArithImmToInt(X86::eax);
    emitFastArithImmToInt(X86::ecx);
#if !PLATFORM(X86)
    // Mask with 0x1f as per ecma-262 11.7.2 step 7.
    // On 32-bit x86 this is not necessary, since the shift anount is implicitly masked in the instruction.
    and32(Imm32(0x1f), X86::ecx);
#endif
    lshift32(X86::ecx, X86::eax);
#if !USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(joAdd32(X86::eax, X86::eax));
    signExtend32ToPtr(X86::eax, X86::eax);
#endif
    emitFastArithReTagImmediate(X86::eax, X86::eax);
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
    emitGetVirtualRegisters(op1, X86::eax, op2, X86::ecx);
    notImm1.link(this);
    notImm2.link(this);
#endif
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArg(X86::ecx, 2);
    emitCTICall(Interpreter::cti_op_lshift);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArith_op_rshift(unsigned result, unsigned op1, unsigned op2)
{
    if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
        // Mask with 0x1f as per ecma-262 11.7.2 step 7.
#if USE(ALTERNATE_JSIMMEDIATE)
        rshift32(Imm32(getConstantOperandImmediateInt(op2) & 0x1f), X86::eax);
#else
        rshiftPtr(Imm32(getConstantOperandImmediateInt(op2) & 0x1f), X86::eax);
#endif
    } else {
        emitGetVirtualRegisters(op1, X86::eax, op2, X86::ecx);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::ecx);
        emitFastArithImmToInt(X86::ecx);
#if !PLATFORM(X86)
        // Mask with 0x1f as per ecma-262 11.7.2 step 7.
        // On 32-bit x86 this is not necessary, since the shift anount is implicitly masked in the instruction.
        and32(Imm32(0x1f), X86::ecx);
#endif
#if USE(ALTERNATE_JSIMMEDIATE)
        rshift32(X86::ecx, X86::eax);
#else
        rshiftPtr(X86::ecx, X86::eax);
#endif
    }
#if USE(ALTERNATE_JSIMMEDIATE)
    emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
    orPtr(Imm32(JSImmediate::TagTypeInteger), X86::eax);
#endif
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_rshift(unsigned result, unsigned, unsigned op2, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    if (isOperandConstantImmediateInt(op2))
        emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
    else {
        linkSlowCase(iter);
        emitPutJITStubArg(X86::ecx, 2);
    }

    emitPutJITStubArg(X86::eax, 1);
    emitCTICall(Interpreter::cti_op_rshift);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArith_op_bitand(unsigned result, unsigned op1, unsigned op2)
{
    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t imm = getConstantOperandImmediateInt(op1);
        andPtr(Imm32(imm), X86::eax);
        if (imm >= 0)
            emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
        andPtr(Imm32(static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op1)))), X86::eax);
#endif
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
        int32_t imm = getConstantOperandImmediateInt(op2);
        andPtr(Imm32(imm), X86::eax);
        if (imm >= 0)
            emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
        andPtr(Imm32(static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)))), X86::eax);
#endif
    } else {
        emitGetVirtualRegisters(op1, X86::eax, op2, X86::edx);
        andPtr(X86::edx, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
    }
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_bitand(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    if (isOperandConstantImmediateInt(op1)) {
        emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
        emitPutJITStubArg(X86::eax, 2);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitPutJITStubArg(X86::eax, 1);
        emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
    } else {
        emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
        emitPutJITStubArg(X86::edx, 2);
    }
    emitCTICall(Interpreter::cti_op_bitand);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArith_op_mod(unsigned result, unsigned op1, unsigned op2)
{
    emitGetVirtualRegisters(op1, X86::eax, op2, X86::ecx);
    emitJumpSlowCaseIfNotImmNum(X86::eax);
    emitJumpSlowCaseIfNotImmNum(X86::ecx);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(jePtr(X86::ecx, ImmPtr(JSValuePtr::encode(js0()))));
    mod32(X86::ecx, X86::eax, X86::edx);
#else
    emitFastArithDeTagImmediate(X86::eax);
    addSlowCase(emitFastArithDeTagImmediateJumpIfZero(X86::ecx));
    mod32(X86::ecx, X86::eax, X86::edx);
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
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArg(X86::ecx, 2);
    emitCTICall(Interpreter::cti_op_mod);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArith_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
        // FIXME: investigate performing a 31-bit add here (can we preserve upper bit & detect overflow from low word to high?)
        //        (or, detect carry? - if const is positive, will only carry when overflowing from negative to positive?)
        addSlowCase(joAdd32(Imm32(getConstantOperandImmediateInt(op1)), X86::eax));
        emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
        addSlowCase(joAdd32(Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
#endif
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
        emitFastArithImmToInt(X86::eax);
        addSlowCase(joAdd32(Imm32(getConstantOperandImmediateInt(op2)), X86::eax));
        emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
        addSlowCase(joAdd32(Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
#endif
        emitPutVirtualRegister(result);
    } else {
        OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
        if (types.first().mightBeNumber() && types.second().mightBeNumber())
            compileBinaryArithOp(op_add, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
        else {
            emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
            emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
            emitCTICall(Interpreter::cti_op_add);
            emitPutVirtualRegister(result);
        }
    }
}
void JIT::compileFastArithSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op1)) {
#if USE(ALTERNATE_JSIMMEDIATE)
        linkSlowCase(iter);
        linkSlowCase(iter);
        emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
        emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
#else
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), X86::eax);
        notImm.link(this);
        emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
        emitPutJITStubArg(X86::eax, 2);
#endif
        emitCTICall(Interpreter::cti_op_add);
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2)) {
#if USE(ALTERNATE_JSIMMEDIATE)
        linkSlowCase(iter);
        linkSlowCase(iter);
        emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
        emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
#else
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), X86::eax);
        notImm.link(this);
        emitPutJITStubArg(X86::eax, 1);
        emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
#endif
        emitCTICall(Interpreter::cti_op_add);
        emitPutVirtualRegister(result);
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
        emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
        addSlowCase(joMul32(Imm32(value), X86::eax, X86::eax));
#else
        emitFastArithDeTagImmediate(X86::eax);
        addSlowCase(joMul32(Imm32(value), X86::eax, X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
#endif
        emitFastArithReTagImmediate(X86::eax, X86::eax);
        emitPutVirtualRegister(result);
    } else if (isOperandConstantImmediateInt(op2) && ((value = getConstantOperandImmediateInt(op2)) > 0)) {
        emitGetVirtualRegister(op1, X86::eax);
        emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
        addSlowCase(joMul32(Imm32(value), X86::eax, X86::eax));
#else
        emitFastArithDeTagImmediate(X86::eax);
        addSlowCase(joMul32(Imm32(value), X86::eax, X86::eax));
        signExtend32ToPtr(X86::eax, X86::eax);
#endif
        emitFastArithReTagImmediate(X86::eax, X86::eax);
        emitPutVirtualRegister(result);
    } else
        compileBinaryArithOp(op_mul, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}
void JIT::compileFastArithSlow_op_mul(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int result = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    if ((isOperandConstantImmediateInt(op1) && (getConstantOperandImmediateInt(op1) > 0))
        || (isOperandConstantImmediateInt(op2) && (getConstantOperandImmediateInt(op2) > 0))) {
        linkSlowCase(iter);
        linkSlowCase(iter);
        // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
        emitPutJITStubArgFromVirtualRegister(op1, 1, X86::ecx);
        emitPutJITStubArgFromVirtualRegister(op2, 2, X86::ecx);
        emitCTICall(Interpreter::cti_op_mul);
        emitPutVirtualRegister(result);
    } else
        compileBinaryArithOpSlowCase(op_mul, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
}

void JIT::compileFastArith_op_post_inc(unsigned result, unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, X86::eax);
    move(X86::eax, X86::edx);
    emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(joAdd32(Imm32(1), X86::edx));
    emitFastArithIntToImmNoCheck(X86::edx, X86::edx);
#else
    addSlowCase(joAdd32(Imm32(1 << JSImmediate::IntegerPayloadShift), X86::edx));
    signExtend32ToPtr(X86::edx, X86::edx);
#endif
    emitPutVirtualRegister(srcDst, X86::edx);
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_post_inc(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    emitPutJITStubArg(X86::eax, 1);
    emitCTICall(Interpreter::cti_op_post_inc);
    emitPutVirtualRegister(srcDst, X86::edx);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArith_op_post_dec(unsigned result, unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, X86::eax);
    move(X86::eax, X86::edx);
    emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(joSub32(Imm32(1), X86::edx));
    emitFastArithIntToImmNoCheck(X86::edx, X86::edx);
#else
    addSlowCase(joSub32(Imm32(1 << JSImmediate::IntegerPayloadShift), X86::edx));
    signExtend32ToPtr(X86::edx, X86::edx);
#endif
    emitPutVirtualRegister(srcDst, X86::edx);
    emitPutVirtualRegister(result);
}
void JIT::compileFastArithSlow_op_post_dec(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    emitPutJITStubArg(X86::eax, 1);
    emitCTICall(Interpreter::cti_op_post_dec);
    emitPutVirtualRegister(srcDst, X86::edx);
    emitPutVirtualRegister(result);
}

void JIT::compileFastArith_op_pre_inc(unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, X86::eax);
    emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
    // FIXME: Could add ptr & specify int64; no need to re-sign-extend?
    addSlowCase(joAdd32(Imm32(1), X86::eax));
    emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
    addSlowCase(joAdd32(Imm32(1 << JSImmediate::IntegerPayloadShift), X86::eax));
    signExtend32ToPtr(X86::eax, X86::eax);
#endif
    emitPutVirtualRegister(srcDst);
}
void JIT::compileFastArithSlow_op_pre_inc(unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    Jump notImm = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(srcDst, X86::eax);
    notImm.link(this);
    emitPutJITStubArg(X86::eax, 1);
    emitCTICall(Interpreter::cti_op_pre_inc);
    emitPutVirtualRegister(srcDst);
}

void JIT::compileFastArith_op_pre_dec(unsigned srcDst)
{
    emitGetVirtualRegister(srcDst, X86::eax);
    emitJumpSlowCaseIfNotImmNum(X86::eax);
#if USE(ALTERNATE_JSIMMEDIATE)
    addSlowCase(joSub32(Imm32(1), X86::eax));
    emitFastArithIntToImmNoCheck(X86::eax, X86::eax);
#else
    addSlowCase(joSub32(Imm32(1 << JSImmediate::IntegerPayloadShift), X86::eax));
    signExtend32ToPtr(X86::eax, X86::eax);
#endif
    emitPutVirtualRegister(srcDst);
}
void JIT::compileFastArithSlow_op_pre_dec(unsigned srcDst, Vector<SlowCaseEntry>::iterator& iter)
{
    Jump notImm = getSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(srcDst, X86::eax);
    notImm.link(this);
    emitPutJITStubArg(X86::eax, 1);
    emitCTICall(Interpreter::cti_op_pre_dec);
    emitPutVirtualRegister(srcDst);
}


#if !ENABLE(JIT_OPTIMIZE_ARITHMETIC)

void JIT::compileBinaryArithOp(OpcodeID opcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes)
{
    emitPutJITStubArgFromVirtualRegister(src1, 1, X86::ecx);
    emitPutJITStubArgFromVirtualRegister(src2, 2, X86::ecx);
    if (opcodeID == op_add)
        emitCTICall(Interpreter::cti_op_add);
    else if (opcodeID == op_sub)
        emitCTICall(Interpreter::cti_op_sub);
    else {
        ASSERT(opcodeID == op_mul);
        emitCTICall(Interpreter::cti_op_mul);
    }
    emitPutVirtualRegister(dst);
}

void JIT::compileBinaryArithOpSlowCase(OpcodeID, Vector<SlowCaseEntry>::iterator&, unsigned, unsigned, unsigned, OperandTypes)
{
    ASSERT_NOT_REACHED();
}

#else

typedef X86Assembler::JmpSrc JmpSrc;
typedef X86Assembler::JmpDst JmpDst;
typedef X86Assembler::XMMRegisterID XMMRegisterID;

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
#else
            flags = 0;
            // FIXME: Add GCC code to do above asm
#endif
            present = (flags & SSE2FeatureBit) != 0;
        }
        bool present;
    };
    static SSE2Check check;
    return check.present;
}

#endif

/*
  This is required since number representation is canonical - values representable as a JSImmediate should not be stored in a JSNumberCell.
  
  In the common case, the double value from 'xmmSource' is written to the reusable JSNumberCell pointed to by 'jsNumberCell', then 'jsNumberCell'
  is written to the output SF Register 'dst', and then a jump is planted (stored into *wroteJSNumberCell).
  
  However if the value from xmmSource is representable as a JSImmediate, then the JSImmediate value will be written to the output, and flow
  control will fall through from the code planted.
*/
void JIT::putDoubleResultToJSNumberCellOrJSImmediate(X86::XMMRegisterID xmmSource, X86::RegisterID jsNumberCell, unsigned dst, JmpSrc* wroteJSNumberCell,  X86::XMMRegisterID tempXmm, X86::RegisterID tempReg1, X86::RegisterID tempReg2)
{
    // convert (double -> JSImmediate -> double), and check if the value is unchanged - in which case the value is representable as a JSImmediate.
    __ cvttsd2si_rr(xmmSource, tempReg1);
    __ addl_rr(tempReg1, tempReg1);
    __ sarl_i8r(1, tempReg1);
    __ cvtsi2sd_rr(tempReg1, tempXmm);
    // Compare & branch if immediate. 
    __ ucomis_rr(tempXmm, xmmSource);
    JmpSrc resultIsImm = __ je();
    JmpDst resultLookedLikeImmButActuallyIsnt = __ label();
    
    // Store the result to the JSNumberCell and jump.
    __ movsd_rm(xmmSource, FIELD_OFFSET(JSNumberCell, m_value), jsNumberCell);
    if (jsNumberCell != X86::eax)
        __ movl_rr(jsNumberCell, X86::eax);
    emitPutVirtualRegister(dst);
    *wroteJSNumberCell = __ jmp();

    __ link(resultIsImm, __ label());
    // value == (double)(JSImmediate)value... or at least, it looks that way...
    // ucomi will report that (0 == -0), and will report true if either input in NaN (result is unordered).
    __ link(__ jp(), resultLookedLikeImmButActuallyIsnt); // Actually was a NaN
    __ pextrw_irr(3, xmmSource, tempReg2);
    __ cmpl_ir(0x8000, tempReg2);
    __ link(__ je(), resultLookedLikeImmButActuallyIsnt); // Actually was -0
    // Yes it really really really is representable as a JSImmediate.
    emitFastArithIntToImmNoCheck(tempReg1, X86::eax);
    emitPutVirtualRegister(dst);
}

void JIT::compileBinaryArithOp(OpcodeID opcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes types)
{
    Structure* numberStructure = m_globalData->numberStructure.get();
    JmpSrc wasJSNumberCell1;
    JmpSrc wasJSNumberCell1b;
    JmpSrc wasJSNumberCell2;
    JmpSrc wasJSNumberCell2b;

    emitGetVirtualRegisters(src1, X86::eax, src2, X86::edx);

    if (types.second().isReusable() && isSSE2Present()) {
        ASSERT(types.second().mightBeNumber());

        // Check op2 is a number
        __ testl_i32r(JSImmediate::TagTypeInteger, X86::edx);
        JmpSrc op2imm = __ jne();
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::edx, src2);
            __ cmpl_im(reinterpret_cast<unsigned>(numberStructure), FIELD_OFFSET(JSCell, m_structure), X86::edx);
            addSlowCase(__ jne());
        }

        // (1) In this case src2 is a reusable number cell.
        //     Slow case if src1 is not a number type.
        __ testl_i32r(JSImmediate::TagTypeInteger, X86::eax);
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
        __ link(op1imm, __ label());
        emitFastArithImmToInt(X86::eax);
        __ cvtsi2sd_rr(X86::eax, X86::xmm0);
        // (1c) 
        __ link(loadedDouble, __ label());
        if (opcodeID == op_add)
            __ addsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        else if (opcodeID == op_sub)
            __ subsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        else {
            ASSERT(opcodeID == op_mul);
            __ mulsd_mr(FIELD_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        }

        putDoubleResultToJSNumberCellOrJSImmediate(X86::xmm0, X86::edx, dst, &wasJSNumberCell2, X86::xmm1, X86::ecx, X86::eax);
        wasJSNumberCell2b = __ jmp();

        // (2) This handles cases where src2 is an immediate number.
        //     Two slow cases - either src1 isn't an immediate, or the subtract overflows.
        __ link(op2imm, __ label());
        emitJumpSlowCaseIfNotImmNum(X86::eax);
    } else if (types.first().isReusable() && isSSE2Present()) {
        ASSERT(types.first().mightBeNumber());

        // Check op1 is a number
        __ testl_i32r(JSImmediate::TagTypeInteger, X86::eax);
        JmpSrc op1imm = __ jne();
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::eax, src1);
            __ cmpl_im(reinterpret_cast<unsigned>(numberStructure), FIELD_OFFSET(JSCell, m_structure), X86::eax);
            addSlowCase(__ jne());
        }

        // (1) In this case src1 is a reusable number cell.
        //     Slow case if src2 is not a number type.
        __ testl_i32r(JSImmediate::TagTypeInteger, X86::edx);
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
        __ link(op2imm, __ label());
        emitFastArithImmToInt(X86::edx);
        __ cvtsi2sd_rr(X86::edx, X86::xmm1);
        // (1c) 
        __ link(loadedDouble, __ label());
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

        putDoubleResultToJSNumberCellOrJSImmediate(X86::xmm0, X86::eax, dst, &wasJSNumberCell1, X86::xmm1, X86::ecx, X86::edx);
        wasJSNumberCell1b = __ jmp();

        // (2) This handles cases where src1 is an immediate number.
        //     Two slow cases - either src2 isn't an immediate, or the subtract overflows.
        __ link(op1imm, __ label());
        emitJumpSlowCaseIfNotImmNum(X86::edx);
    } else
        emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, X86::ecx);

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
        JmpSrc op1Zero = emitFastArithDeTagImmediateJumpIfZero(X86::eax);
        __ testl_rr(X86::edx, X86::edx);
        JmpSrc op2NonZero = __ jne();
        __ link(op1Zero, __ label());
        // if either input is zero, add the two together, and check if the result is < 0.
        // If it is, we have a problem (N < 0), (N * 0) == -0, not representatble as a JSImmediate. 
        __ movl_rr(X86::eax, X86::ecx);
        __ addl_rr(X86::edx, X86::ecx);
        addSlowCase(__ js());
        // Skip the above check if neither input is zero
        __ link(op2NonZero, __ label());
        __ imull_rr(X86::edx, X86::eax);
        addSlowCase(__ jo());
        signExtend32ToPtr(X86::eax, X86::eax);
        emitFastArithReTagImmediate(X86::eax, X86::eax);
    }
    emitPutVirtualRegister(dst);

    if (types.second().isReusable() && isSSE2Present()) {
        __ link(wasJSNumberCell2, __ label());
        __ link(wasJSNumberCell2b, __ label());
    }
    else if (types.first().isReusable() && isSSE2Present()) {
        __ link(wasJSNumberCell1, __ label());
        __ link(wasJSNumberCell1b, __ label());
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

    emitPutJITStubArgFromVirtualRegister(src1, 1, X86::ecx);
    emitPutJITStubArgFromVirtualRegister(src2, 2, X86::ecx);
    if (opcodeID == op_add)
        emitCTICall(Interpreter::cti_op_add);
    else if (opcodeID == op_sub)
        emitCTICall(Interpreter::cti_op_sub);
    else {
        ASSERT(opcodeID == op_mul);
        emitCTICall(Interpreter::cti_op_mul);
    }
    emitPutVirtualRegister(dst);
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
