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

#if USE(JSVALUE32_64)

void JIT::emit_op_negate(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump srcNotInt = branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag));
    addSlowCase(branch32(Equal, regT0, Imm32(0)));

    neg32(regT0);
    emitStoreInt32(dst, regT0, (dst == src));

    Jump end = jump();

    srcNotInt.link(this);
    addSlowCase(branch32(Above, regT1, Imm32(JSValue::LowestTag)));

    xor32(Imm32(1 << 31), regT1);
    store32(regT1, tagFor(dst));
    if (dst != src)
        store32(regT0, payloadFor(dst));

    end.link(this);
}

void JIT::emitSlow_op_negate(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;

    linkSlowCase(iter); // 0 check
    linkSlowCase(iter); // double check

    JITStubCall stubCall(this, cti_op_negate);
    stubCall.addArgument(regT1, regT0);
    stubCall.call(dst);
}

void JIT::emit_op_jnless(Instruction* currentInstruction)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    JumpList notInt32Op1;
    JumpList notInt32Op2;

    // Int32 less.
    if (isOperandConstantImmediateInt(op1)) {
        emitLoad(op2, regT3, regT2);
        notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
        addJump(branch32(LessThanOrEqual, regT2, Imm32(getConstantOperand(op1).asInt32())), target + 3);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitLoad(op1, regT1, regT0);
        notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        addJump(branch32(GreaterThanOrEqual, regT0, Imm32(getConstantOperand(op2).asInt32())), target + 3);
    } else {
        emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
        notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
        addJump(branch32(GreaterThanOrEqual, regT0, regT2), target + 3);
    }

    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32Op1);
        addSlowCase(notInt32Op2);
        return;
    }
    Jump end = jump();

    // Double less.
    emitBinaryDoubleOp(op_jnless, target, op1, op2, OperandTypes(), notInt32Op1, notInt32Op2, !isOperandConstantImmediateInt(op1), isOperandConstantImmediateInt(op1) || !isOperandConstantImmediateInt(op2));
    end.link(this);
}

void JIT::emitSlow_op_jnless(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    if (!supportsFloatingPoint()) {
        if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
            linkSlowCase(iter); // int32 check
        linkSlowCase(iter); // int32 check
    } else {
        if (!isOperandConstantImmediateInt(op1)) {
            linkSlowCase(iter); // double check
            linkSlowCase(iter); // int32 check
        }
        if (isOperandConstantImmediateInt(op1) || !isOperandConstantImmediateInt(op2))
            linkSlowCase(iter); // double check
    }

    JITStubCall stubCall(this, cti_op_jless);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);
}

void JIT::emit_op_jnlesseq(Instruction* currentInstruction)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    JumpList notInt32Op1;
    JumpList notInt32Op2;

    // Int32 less.
    if (isOperandConstantImmediateInt(op1)) {
        emitLoad(op2, regT3, regT2);
        notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
        addJump(branch32(LessThan, regT2, Imm32(getConstantOperand(op1).asInt32())), target + 3);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitLoad(op1, regT1, regT0);
        notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        addJump(branch32(GreaterThan, regT0, Imm32(getConstantOperand(op2).asInt32())), target + 3);
    } else {
        emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
        notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
        addJump(branch32(GreaterThan, regT0, regT2), target + 3);
    }

    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32Op1);
        addSlowCase(notInt32Op2);
        return;
    }
    Jump end = jump();

    // Double less.
    emitBinaryDoubleOp(op_jnlesseq, target, op1, op2, OperandTypes(), notInt32Op1, notInt32Op2, !isOperandConstantImmediateInt(op1), isOperandConstantImmediateInt(op1) || !isOperandConstantImmediateInt(op2));
    end.link(this);
}

void JIT::emitSlow_op_jnlesseq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    if (!supportsFloatingPoint()) {
        if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
            linkSlowCase(iter); // int32 check
        linkSlowCase(iter); // int32 check
    } else {
        if (!isOperandConstantImmediateInt(op1)) {
            linkSlowCase(iter); // double check
            linkSlowCase(iter); // int32 check
        }
        if (isOperandConstantImmediateInt(op1) || !isOperandConstantImmediateInt(op2))
            linkSlowCase(iter); // double check
    }

    JITStubCall stubCall(this, cti_op_jlesseq);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);
}

// LeftShift (<<)

void JIT::emit_op_lshift(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op2)) {
        emitLoad(op1, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        lshift32(Imm32(getConstantOperand(op2).asInt32()), regT0);
        emitStoreInt32(dst, regT0, dst == op1);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    if (!isOperandConstantImmediateInt(op1))
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    lshift32(regT2, regT0);
    emitStoreInt32(dst, regT0, dst == op1 || dst == op2);
}

void JIT::emitSlow_op_lshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_lshift);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// RightShift (>>)

void JIT::emit_op_rshift(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op2)) {
        emitLoad(op1, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        rshift32(Imm32(getConstantOperand(op2).asInt32()), regT0);
        emitStoreInt32(dst, regT0, dst == op1);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    if (!isOperandConstantImmediateInt(op1))
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    rshift32(regT2, regT0);
    emitStoreInt32(dst, regT0, dst == op1 || dst == op2);
}

void JIT::emitSlow_op_rshift(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_rshift);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// BitAnd (&)

void JIT::emit_op_bitand(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    unsigned op;
    int32_t constant;
    if (getOperandConstantImmediateInt(op1, op2, op, constant)) {
        emitLoad(op, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        and32(Imm32(constant), regT0);
        emitStoreInt32(dst, regT0, (op == dst));
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    and32(regT2, regT0);
    emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));
}

void JIT::emitSlow_op_bitand(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_bitand);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// BitOr (|)

void JIT::emit_op_bitor(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    unsigned op;
    int32_t constant;
    if (getOperandConstantImmediateInt(op1, op2, op, constant)) {
        emitLoad(op, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        or32(Imm32(constant), regT0);
        emitStoreInt32(dst, regT0, (op == dst));
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    or32(regT2, regT0);
    emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));
}

void JIT::emitSlow_op_bitor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_bitor);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// BitXor (^)

void JIT::emit_op_bitxor(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    unsigned op;
    int32_t constant;
    if (getOperandConstantImmediateInt(op1, op2, op, constant)) {
        emitLoad(op, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        xor32(Imm32(constant), regT0);
        emitStoreInt32(dst, regT0, (op == dst));
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    xor32(regT2, regT0);
    emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));
}

void JIT::emitSlow_op_bitxor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_bitxor);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// BitNot (~)

void JIT::emit_op_bitnot(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));

    not32(regT0);
    emitStoreInt32(dst, regT0, (dst == src));
}

void JIT::emitSlow_op_bitnot(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;

    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_bitnot);
    stubCall.addArgument(regT1, regT0);
    stubCall.call(dst);
}

// PostInc (i++)

void JIT::emit_op_post_inc(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;
    
    emitLoad(srcDst, regT1, regT0);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));

    if (dst == srcDst) // x = x++ is a noop for ints.
        return;

    emitStoreInt32(dst, regT0);

    addSlowCase(branchAdd32(Overflow, Imm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emitSlow_op_post_inc(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

    linkSlowCase(iter); // int32 check
    if (dst != srcDst)
        linkSlowCase(iter); // overflow check

    JITStubCall stubCall(this, cti_op_post_inc);
    stubCall.addArgument(srcDst);
    stubCall.addArgument(Imm32(srcDst));
    stubCall.call(dst);
}

// PostDec (i--)

void JIT::emit_op_post_dec(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

    emitLoad(srcDst, regT1, regT0);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));

    if (dst == srcDst) // x = x-- is a noop for ints.
        return;

    emitStoreInt32(dst, regT0);

    addSlowCase(branchSub32(Overflow, Imm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emitSlow_op_post_dec(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned srcDst = currentInstruction[2].u.operand;

    linkSlowCase(iter); // int32 check
    if (dst != srcDst)
        linkSlowCase(iter); // overflow check

    JITStubCall stubCall(this, cti_op_post_dec);
    stubCall.addArgument(srcDst);
    stubCall.addArgument(Imm32(srcDst));
    stubCall.call(dst);
}

// PreInc (++i)

void JIT::emit_op_pre_inc(Instruction* currentInstruction)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    emitLoad(srcDst, regT1, regT0);

    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branchAdd32(Overflow, Imm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emitSlow_op_pre_inc(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // overflow check

    JITStubCall stubCall(this, cti_op_pre_inc);
    stubCall.addArgument(srcDst);
    stubCall.call(srcDst);
}

// PreDec (--i)

void JIT::emit_op_pre_dec(Instruction* currentInstruction)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    emitLoad(srcDst, regT1, regT0);

    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branchSub32(Overflow, Imm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emitSlow_op_pre_dec(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // overflow check

    JITStubCall stubCall(this, cti_op_pre_dec);
    stubCall.addArgument(srcDst);
    stubCall.call(srcDst);
}

// Addition (+)

void JIT::emit_op_add(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!types.first().mightBeNumber() || !types.second().mightBeNumber()) {
        JITStubCall stubCall(this, cti_op_add);
        stubCall.addArgument(op1);
        stubCall.addArgument(op2);
        stubCall.call(dst);
        return;
    }

    JumpList notInt32Op1;
    JumpList notInt32Op2;

    unsigned op;
    int32_t constant;
    if (getOperandConstantImmediateInt(op1, op2, op, constant)) {
        emitAdd32Constant(dst, op, constant, op == op1 ? types.first() : types.second());
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));

    // Int32 case.
    addSlowCase(branchAdd32(Overflow, regT2, regT0));
    emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));

    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32Op1);
        addSlowCase(notInt32Op2);
        return;
    }
    Jump end = jump();

    // Double case.
    emitBinaryDoubleOp(op_add, dst, op1, op2, types, notInt32Op1, notInt32Op2);
    end.link(this);
}

void JIT::emitAdd32Constant(unsigned dst, unsigned op, int32_t constant, ResultType opType)
{
    // Int32 case.
    emitLoad(op, regT1, regT0);
    Jump notInt32 = branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag));
    addSlowCase(branchAdd32(Overflow, Imm32(constant), regT0));
    emitStoreInt32(dst, regT0, (op == dst));

    // Double case.
    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32);
        return;
    }
    Jump end = jump();

    notInt32.link(this);
    if (!opType.definitelyIsNumber())
        addSlowCase(branch32(Above, regT1, Imm32(JSValue::LowestTag)));
    move(Imm32(constant), regT2);
    convertInt32ToDouble(regT2, fpRegT0);
    emitLoadDouble(op, fpRegT1);
    addDouble(fpRegT1, fpRegT0);
    emitStoreDouble(dst, fpRegT0);

    end.link(this);
}

void JIT::emitSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!types.first().mightBeNumber() || !types.second().mightBeNumber())
        return;

    unsigned op;
    int32_t constant;
    if (getOperandConstantImmediateInt(op1, op2, op, constant)) {
        linkSlowCase(iter); // overflow check

        if (!supportsFloatingPoint())
            linkSlowCase(iter); // non-sse case
        else {
            ResultType opType = op == op1 ? types.first() : types.second();
            if (!opType.definitelyIsNumber())
                linkSlowCase(iter); // double check
        }
    } else {
        linkSlowCase(iter); // overflow check

        if (!supportsFloatingPoint()) {
            linkSlowCase(iter); // int32 check
            linkSlowCase(iter); // int32 check
        } else {
            if (!types.first().definitelyIsNumber())
                linkSlowCase(iter); // double check

            if (!types.second().definitelyIsNumber()) {
                linkSlowCase(iter); // int32 check
                linkSlowCase(iter); // double check
            }
        }
    }

    JITStubCall stubCall(this, cti_op_add);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// Subtraction (-)

void JIT::emit_op_sub(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    JumpList notInt32Op1;
    JumpList notInt32Op2;

    if (isOperandConstantImmediateInt(op2)) {
        emitSub32Constant(dst, op1, getConstantOperand(op2).asInt32(), types.first());
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));

    // Int32 case.
    addSlowCase(branchSub32(Overflow, regT2, regT0));
    emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));

    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32Op1);
        addSlowCase(notInt32Op2);
        return;
    }
    Jump end = jump();

    // Double case.
    emitBinaryDoubleOp(op_sub, dst, op1, op2, types, notInt32Op1, notInt32Op2);
    end.link(this);
}

void JIT::emitSub32Constant(unsigned dst, unsigned op, int32_t constant, ResultType opType)
{
    // Int32 case.
    emitLoad(op, regT1, regT0);
    Jump notInt32 = branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag));
    addSlowCase(branchSub32(Overflow, Imm32(constant), regT0));
    emitStoreInt32(dst, regT0, (op == dst));

    // Double case.
    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32);
        return;
    }
    Jump end = jump();

    notInt32.link(this);
    if (!opType.definitelyIsNumber())
        addSlowCase(branch32(Above, regT1, Imm32(JSValue::LowestTag)));
    move(Imm32(constant), regT2);
    convertInt32ToDouble(regT2, fpRegT0);
    emitLoadDouble(op, fpRegT1);
    subDouble(fpRegT0, fpRegT1);
    emitStoreDouble(dst, fpRegT1);

    end.link(this);
}

void JIT::emitSlow_op_sub(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter); // overflow check

        if (!supportsFloatingPoint() || !types.first().definitelyIsNumber())
            linkSlowCase(iter); // int32 or double check
    } else {
        linkSlowCase(iter); // overflow check

        if (!supportsFloatingPoint()) {
            linkSlowCase(iter); // int32 check
            linkSlowCase(iter); // int32 check
        } else {
            if (!types.first().definitelyIsNumber())
                linkSlowCase(iter); // double check

            if (!types.second().definitelyIsNumber()) {
                linkSlowCase(iter); // int32 check
                linkSlowCase(iter); // double check
            }
        }
    }

    JITStubCall stubCall(this, cti_op_sub);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

void JIT::emitBinaryDoubleOp(OpcodeID opcodeID, unsigned dst, unsigned op1, unsigned op2, OperandTypes types, JumpList& notInt32Op1, JumpList& notInt32Op2, bool op1IsInRegisters, bool op2IsInRegisters)
{
    JumpList end;
    
    if (!notInt32Op1.empty()) {
        // Double case 1: Op1 is not int32; Op2 is unknown.
        notInt32Op1.link(this);

        ASSERT(op1IsInRegisters);

        // Verify Op1 is double.
        if (!types.first().definitelyIsNumber())
            addSlowCase(branch32(Above, regT1, Imm32(JSValue::LowestTag)));

        if (!op2IsInRegisters)
            emitLoad(op2, regT3, regT2);

        Jump doubleOp2 = branch32(Below, regT3, Imm32(JSValue::LowestTag));

        if (!types.second().definitelyIsNumber())
            addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));

        convertInt32ToDouble(regT2, fpRegT0);
        Jump doTheMath = jump();

        // Load Op2 as double into double register.
        doubleOp2.link(this);
        emitLoadDouble(op2, fpRegT0);

        // Do the math.
        doTheMath.link(this);
        switch (opcodeID) {
            case op_mul:
                emitLoadDouble(op1, fpRegT2);
                mulDouble(fpRegT2, fpRegT0);
                emitStoreDouble(dst, fpRegT0);
                break;
            case op_add:
                emitLoadDouble(op1, fpRegT2);
                addDouble(fpRegT2, fpRegT0);
                emitStoreDouble(dst, fpRegT0);
                break;
            case op_sub:
                emitLoadDouble(op1, fpRegT1);
                subDouble(fpRegT0, fpRegT1);
                emitStoreDouble(dst, fpRegT1);
                break;
            case op_div:
                emitLoadDouble(op1, fpRegT1);
                divDouble(fpRegT0, fpRegT1);
                emitStoreDouble(dst, fpRegT1);
                break;
            case op_jnless:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleLessThanOrEqual, fpRegT0, fpRegT2), dst + 3);
                break;
            case op_jnlesseq:
                emitLoadDouble(op1, fpRegT2);
                addJump(branchDouble(DoubleLessThan, fpRegT0, fpRegT2), dst + 3);
                break;
            default:
                ASSERT_NOT_REACHED();
        }

        if (!notInt32Op2.empty())
            end.append(jump());
    }

    if (!notInt32Op2.empty()) {
        // Double case 2: Op1 is int32; Op2 is not int32.
        notInt32Op2.link(this);

        ASSERT(op2IsInRegisters);

        if (!op1IsInRegisters)
            emitLoadPayload(op1, regT0);

        convertInt32ToDouble(regT0, fpRegT0);

        // Verify op2 is double.
        if (!types.second().definitelyIsNumber())
            addSlowCase(branch32(Above, regT3, Imm32(JSValue::LowestTag)));

        // Do the math.
        switch (opcodeID) {
            case op_mul:
                emitLoadDouble(op2, fpRegT2);
                mulDouble(fpRegT2, fpRegT0);
                emitStoreDouble(dst, fpRegT0);
                break;
            case op_add:
                emitLoadDouble(op2, fpRegT2);
                addDouble(fpRegT2, fpRegT0);
                emitStoreDouble(dst, fpRegT0);
                break;
            case op_sub:
                emitLoadDouble(op2, fpRegT2);
                subDouble(fpRegT2, fpRegT0);
                emitStoreDouble(dst, fpRegT0);
                break;
            case op_div:
                emitLoadDouble(op2, fpRegT2);
                divDouble(fpRegT2, fpRegT0);
                emitStoreDouble(dst, fpRegT0);
                break;
            case op_jnless:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleLessThanOrEqual, fpRegT1, fpRegT0), dst + 3);
                break;
            case op_jnlesseq:
                emitLoadDouble(op2, fpRegT1);
                addJump(branchDouble(DoubleLessThan, fpRegT1, fpRegT0), dst + 3);
                break;
            default:
                ASSERT_NOT_REACHED();
        }
    }

    end.link(this);
}

// Multiplication (*)

void JIT::emit_op_mul(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    JumpList notInt32Op1;
    JumpList notInt32Op2;

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));

    // Int32 case.
    move(regT0, regT3);
    addSlowCase(branchMul32(Overflow, regT2, regT0));
    addSlowCase(branchTest32(Zero, regT0));
    emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));

    if (!supportsFloatingPoint()) {
        addSlowCase(notInt32Op1);
        addSlowCase(notInt32Op2);
        return;
    }
    Jump end = jump();

    // Double case.
    emitBinaryDoubleOp(op_mul, dst, op1, op2, types, notInt32Op1, notInt32Op2);
    end.link(this);
}

void JIT::emitSlow_op_mul(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    Jump overflow = getSlowCase(iter); // overflow check
    linkSlowCase(iter); // zero result check

    Jump negZero = branchOr32(Signed, regT2, regT3);
    emitStoreInt32(dst, Imm32(0), (op1 == dst || op2 == dst));

    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_mul));

    negZero.link(this);
    overflow.link(this);

    if (!supportsFloatingPoint()) {
        linkSlowCase(iter); // int32 check
        linkSlowCase(iter); // int32 check
    }

    if (supportsFloatingPoint()) {
        if (!types.first().definitelyIsNumber())
            linkSlowCase(iter); // double check

        if (!types.second().definitelyIsNumber()) {
            linkSlowCase(iter); // int32 check
            linkSlowCase(iter); // double check
        }
    }

    Label jitStubCall(this);
    JITStubCall stubCall(this, cti_op_mul);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// Division (/)

void JIT::emit_op_div(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!supportsFloatingPoint()) {
        addSlowCase(jump());
        return;
    }

    // Int32 divide.
    JumpList notInt32Op1;
    JumpList notInt32Op2;

    JumpList end;

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);

    notInt32Op1.append(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    notInt32Op2.append(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));

    convertInt32ToDouble(regT0, fpRegT0);
    convertInt32ToDouble(regT2, fpRegT1);
    divDouble(fpRegT1, fpRegT0);

    JumpList doubleResult;
    if (!isOperandConstantImmediateInt(op1) || getConstantOperand(op1).asInt32() > 1) {
        m_assembler.cvttsd2si_rr(fpRegT0, regT0);
        convertInt32ToDouble(regT0, fpRegT1);
        m_assembler.ucomisd_rr(fpRegT1, fpRegT0);

        doubleResult.append(m_assembler.jne());
        doubleResult.append(m_assembler.jp());
        
        doubleResult.append(branchTest32(Zero, regT0));

        // Int32 result.
        emitStoreInt32(dst, regT0, (op1 == dst || op2 == dst));
        end.append(jump());
    }

    // Double result.
    doubleResult.link(this);
    emitStoreDouble(dst, fpRegT0);
    end.append(jump());

    // Double divide.
    emitBinaryDoubleOp(op_div, dst, op1, op2, types, notInt32Op1, notInt32Op2);
    end.link(this);
}

void JIT::emitSlow_op_div(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!supportsFloatingPoint())
        linkSlowCase(iter);
    else {
        if (!types.first().definitelyIsNumber())
            linkSlowCase(iter); // double check

        if (!types.second().definitelyIsNumber()) {
            linkSlowCase(iter); // int32 check
            linkSlowCase(iter); // double check
        }
    }

    JITStubCall stubCall(this, cti_op_div);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

// Mod (%)

/* ------------------------------ BEGIN: OP_MOD ------------------------------ */

#if PLATFORM(X86) || PLATFORM(X86_64)

void JIT::emit_op_mod(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op2) && getConstantOperand(op2).asInt32() != 0) {
        emitLoad(op1, X86Registers::edx, X86Registers::eax);
        move(Imm32(getConstantOperand(op2).asInt32()), X86Registers::ecx);
        addSlowCase(branch32(NotEqual, X86Registers::edx, Imm32(JSValue::Int32Tag)));
        if (getConstantOperand(op2).asInt32() == -1)
            addSlowCase(branch32(Equal, X86Registers::eax, Imm32(0x80000000))); // -2147483648 / -1 => EXC_ARITHMETIC
    } else {
        emitLoad2(op1, X86Registers::edx, X86Registers::eax, op2, X86Registers::ebx, X86Registers::ecx);
        addSlowCase(branch32(NotEqual, X86Registers::edx, Imm32(JSValue::Int32Tag)));
        addSlowCase(branch32(NotEqual, X86Registers::ebx, Imm32(JSValue::Int32Tag)));

        addSlowCase(branch32(Equal, X86Registers::eax, Imm32(0x80000000))); // -2147483648 / -1 => EXC_ARITHMETIC
        addSlowCase(branch32(Equal, X86Registers::ecx, Imm32(0))); // divide by 0
    }

    move(X86Registers::eax, X86Registers::ebx); // Save dividend payload, in case of 0.
    m_assembler.cdq();
    m_assembler.idivl_r(X86Registers::ecx);
    
    // If the remainder is zero and the dividend is negative, the result is -0.
    Jump storeResult1 = branchTest32(NonZero, X86Registers::edx);
    Jump storeResult2 = branchTest32(Zero, X86Registers::ebx, Imm32(0x80000000)); // not negative
    emitStore(dst, jsNumber(m_globalData, -0.0));
    Jump end = jump();

    storeResult1.link(this);
    storeResult2.link(this);
    emitStoreInt32(dst, X86Registers::edx, (op1 == dst || op2 == dst));
    end.link(this);
}

void JIT::emitSlow_op_mod(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    if (isOperandConstantImmediateInt(op2) && getConstantOperand(op2).asInt32() != 0) {
        linkSlowCase(iter); // int32 check
        if (getConstantOperand(op2).asInt32() == -1)
            linkSlowCase(iter); // 0x80000000 check
    } else {
        linkSlowCase(iter); // int32 check
        linkSlowCase(iter); // int32 check
        linkSlowCase(iter); // 0 check
        linkSlowCase(iter); // 0x80000000 check
    }

    JITStubCall stubCall(this, cti_op_mod);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

#else // PLATFORM(X86) || PLATFORM(X86_64)

void JIT::emit_op_mod(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_mod);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call(dst);
}

void JIT::emitSlow_op_mod(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
}

#endif // PLATFORM(X86) || PLATFORM(X86_64)

/* ------------------------------ END: OP_MOD ------------------------------ */

#else // USE(JSVALUE32_64)

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
#if !USE(JSVALUE64)
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

#if USE(JSVALUE64)
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
    JITStubCall stubCall(this, cti_op_lshift);
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
#if USE(JSVALUE64)
        rshift32(Imm32(getConstantOperandImmediateInt(op2) & 0x1f), regT0);
#else
        rshiftPtr(Imm32(getConstantOperandImmediateInt(op2) & 0x1f), regT0);
#endif
    } else {
        emitGetVirtualRegisters(op1, regT0, op2, regT2);
        if (supportsFloatingPointTruncate()) {
            Jump lhsIsInt = emitJumpIfImmediateInteger(regT0);
#if USE(JSVALUE64)
            // supportsFloatingPoint() && USE(JSVALUE64) => 3 SlowCases
            addSlowCase(emitJumpIfNotImmediateNumber(regT0));
            addPtr(tagTypeNumberRegister, regT0);
            movePtrToDouble(regT0, fpRegT0);
            addSlowCase(branchTruncateDoubleToInt32(fpRegT0, regT0));
#else
            // supportsFloatingPoint() && !USE(JSVALUE64) => 5 SlowCases (of which 1 IfNotJSCell)
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
#if USE(JSVALUE64)
        rshift32(regT2, regT0);
#else
        rshiftPtr(regT2, regT0);
#endif
    }
#if USE(JSVALUE64)
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

    JITStubCall stubCall(this, cti_op_rshift);

    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
    } else {
        if (supportsFloatingPointTruncate()) {
#if USE(JSVALUE64)
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
#if USE(JSVALUE64)
        int32_t op2imm = getConstantOperandImmediateInt(op2);
#else
        int32_t op2imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)));
#endif
        addJump(branch32(GreaterThanOrEqual, regT0, Imm32(op2imm)), target + 3);
    } else if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(JSVALUE64)
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
#if USE(JSVALUE64)
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
            
            int32_t op2imm = getConstantOperand(op2).asInt32();;
                    
            move(Imm32(op2imm), regT1);
            convertInt32ToDouble(regT1, fpRegT1);

            emitJumpSlowToHot(branchDouble(DoubleLessThanOrEqual, fpRegT1, fpRegT0), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnless));

#if USE(JSVALUE64)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, cti_op_jless);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else if (isOperandConstantImmediateInt(op1)) {
        linkSlowCase(iter);

        if (supportsFloatingPoint()) {
#if USE(JSVALUE64)
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
            
            int32_t op1imm = getConstantOperand(op1).asInt32();;
                    
            move(Imm32(op1imm), regT0);
            convertInt32ToDouble(regT0, fpRegT0);

            emitJumpSlowToHot(branchDouble(DoubleLessThanOrEqual, fpRegT1, fpRegT0), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnless));

#if USE(JSVALUE64)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, cti_op_jless);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else {
        linkSlowCase(iter);

        if (supportsFloatingPoint()) {
#if USE(JSVALUE64)
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

#if USE(JSVALUE64)
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
        JITStubCall stubCall(this, cti_op_jless);
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
#if USE(JSVALUE64)
        int32_t op2imm = getConstantOperandImmediateInt(op2);
#else
        int32_t op2imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)));
#endif
        addJump(branch32(GreaterThan, regT0, Imm32(op2imm)), target + 3);
    } else if (isOperandConstantImmediateInt(op1)) {
        emitGetVirtualRegister(op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(JSVALUE64)
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
#if USE(JSVALUE64)
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
            
            int32_t op2imm = getConstantOperand(op2).asInt32();;
                    
            move(Imm32(op2imm), regT1);
            convertInt32ToDouble(regT1, fpRegT1);

            emitJumpSlowToHot(branchDouble(DoubleLessThan, fpRegT1, fpRegT0), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnlesseq));

#if USE(JSVALUE64)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op1))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, cti_op_jlesseq);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else if (isOperandConstantImmediateInt(op1)) {
        linkSlowCase(iter);

        if (supportsFloatingPoint()) {
#if USE(JSVALUE64)
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
            
            int32_t op1imm = getConstantOperand(op1).asInt32();;
                    
            move(Imm32(op1imm), regT0);
            convertInt32ToDouble(regT0, fpRegT0);

            emitJumpSlowToHot(branchDouble(DoubleLessThan, fpRegT1, fpRegT0), target + 3);

            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jnlesseq));

#if USE(JSVALUE64)
            fail1.link(this);
#else
            if (!m_codeBlock->isKnownNotImmediate(op2))
                fail1.link(this);
            fail2.link(this);
#endif
        }

        JITStubCall stubCall(this, cti_op_jlesseq);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(Zero, regT0), target + 3);

    } else {
        linkSlowCase(iter);

        if (supportsFloatingPoint()) {
#if USE(JSVALUE64)
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

#if USE(JSVALUE64)
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
        JITStubCall stubCall(this, cti_op_jlesseq);
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
#if USE(JSVALUE64)
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
#if USE(JSVALUE64)
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
        JITStubCall stubCall(this, cti_op_bitand);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT0);
        stubCall.call(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        JITStubCall stubCall(this, cti_op_bitand);
        stubCall.addArgument(regT0);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
    } else {
        JITStubCall stubCall(this, cti_op_bitand);
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
#if USE(JSVALUE64)
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
    JITStubCall stubCall(this, cti_op_post_inc);
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
#if USE(JSVALUE64)
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
    JITStubCall stubCall(this, cti_op_post_dec);
    stubCall.addArgument(regT0);
    stubCall.addArgument(Imm32(srcDst));
    stubCall.call(result);
}

void JIT::emit_op_pre_inc(Instruction* currentInstruction)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    emitGetVirtualRegister(srcDst, regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(JSVALUE64)
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
    JITStubCall stubCall(this, cti_op_pre_inc);
    stubCall.addArgument(regT0);
    stubCall.call(srcDst);
}

void JIT::emit_op_pre_dec(Instruction* currentInstruction)
{
    unsigned srcDst = currentInstruction[1].u.operand;

    emitGetVirtualRegister(srcDst, regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(JSVALUE64)
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
    JITStubCall stubCall(this, cti_op_pre_dec);
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

    emitGetVirtualRegisters(op1, X86Registers::eax, op2, X86Registers::ecx);
    emitJumpSlowCaseIfNotImmediateInteger(X86Registers::eax);
    emitJumpSlowCaseIfNotImmediateInteger(X86Registers::ecx);
#if USE(JSVALUE64)
    addSlowCase(branchPtr(Equal, X86Registers::ecx, ImmPtr(JSValue::encode(jsNumber(m_globalData, 0)))));
    m_assembler.cdq();
    m_assembler.idivl_r(X86Registers::ecx);
#else
    emitFastArithDeTagImmediate(X86Registers::eax);
    addSlowCase(emitFastArithDeTagImmediateJumpIfZero(X86Registers::ecx));
    m_assembler.cdq();
    m_assembler.idivl_r(X86Registers::ecx);
    signExtend32ToPtr(X86Registers::edx, X86Registers::edx);
#endif
    emitFastArithReTagImmediate(X86Registers::edx, X86Registers::eax);
    emitPutVirtualRegister(result);
}

void JIT::emitSlow_op_mod(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;

#if USE(JSVALUE64)
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
#else
    Jump notImm1 = getSlowCase(iter);
    Jump notImm2 = getSlowCase(iter);
    linkSlowCase(iter);
    emitFastArithReTagImmediate(X86Registers::eax, X86Registers::eax);
    emitFastArithReTagImmediate(X86Registers::ecx, X86Registers::ecx);
    notImm1.link(this);
    notImm2.link(this);
#endif
    JITStubCall stubCall(this, cti_op_mod);
    stubCall.addArgument(X86Registers::eax);
    stubCall.addArgument(X86Registers::ecx);
    stubCall.call(result);
}

#else // PLATFORM(X86) || PLATFORM(X86_64)

void JIT::emit_op_mod(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_mod);
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

#if USE(JSVALUE64)

/* ------------------------------ BEGIN: USE(JSVALUE64) (OP_ADD, OP_SUB, OP_MUL) ------------------------------ */

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

void JIT::compileBinaryArithOpSlowCase(OpcodeID opcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned result, unsigned op1, unsigned op2, OperandTypes types, bool op1HasImmediateIntFastCase, bool op2HasImmediateIntFastCase)
{
    // We assume that subtracting TagTypeNumber is equivalent to adding DoubleEncodeOffset.
    COMPILE_ASSERT(((JSImmediate::TagTypeNumber + JSImmediate::DoubleEncodeOffset) == 0), TagTypeNumber_PLUS_DoubleEncodeOffset_EQUALS_0);
    
    Jump notImm1;
    Jump notImm2;
    if (op1HasImmediateIntFastCase) {
        notImm2 = getSlowCase(iter);
    } else if (op2HasImmediateIntFastCase) {
        notImm1 = getSlowCase(iter);
    } else {
        notImm1 = getSlowCase(iter);
        notImm2 = getSlowCase(iter);
    }

    linkSlowCase(iter); // Integer overflow case - we could handle this in JIT code, but this is likely rare.
    if (opcodeID == op_mul && !op1HasImmediateIntFastCase && !op2HasImmediateIntFastCase) // op_mul has an extra slow case to handle 0 * negative number.
        linkSlowCase(iter);
    emitGetVirtualRegister(op1, regT0);

    Label stubFunctionCall(this);
    JITStubCall stubCall(this, opcodeID == op_add ? cti_op_add : opcodeID == op_sub ? cti_op_sub : cti_op_mul);
    if (op1HasImmediateIntFastCase || op2HasImmediateIntFastCase) {
        emitGetVirtualRegister(op1, regT0);
        emitGetVirtualRegister(op2, regT1);
    }
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call(result);
    Jump end = jump();

    if (op1HasImmediateIntFastCase) {
        notImm2.link(this);
        if (!types.second().definitelyIsNumber())
            emitJumpIfNotImmediateNumber(regT0).linkTo(stubFunctionCall, this);
        emitGetVirtualRegister(op1, regT1);
        convertInt32ToDouble(regT1, fpRegT1);
        addPtr(tagTypeNumberRegister, regT0);
        movePtrToDouble(regT0, fpRegT2);
    } else if (op2HasImmediateIntFastCase) {
        notImm1.link(this);
        if (!types.first().definitelyIsNumber())
            emitJumpIfNotImmediateNumber(regT0).linkTo(stubFunctionCall, this);
        emitGetVirtualRegister(op2, regT1);
        convertInt32ToDouble(regT1, fpRegT1);
        addPtr(tagTypeNumberRegister, regT0);
        movePtrToDouble(regT0, fpRegT2);
    } else {
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
    }

    if (opcodeID == op_add)
        addDouble(fpRegT2, fpRegT1);
    else if (opcodeID == op_sub)
        subDouble(fpRegT2, fpRegT1);
    else if (opcodeID == op_mul)
        mulDouble(fpRegT2, fpRegT1);
    else {
        ASSERT(opcodeID == op_div);
        divDouble(fpRegT2, fpRegT1);
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
        JITStubCall stubCall(this, cti_op_add);
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
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!types.first().mightBeNumber() || !types.second().mightBeNumber())
        return;

    bool op1HasImmediateIntFastCase = isOperandConstantImmediateInt(op1);
    bool op2HasImmediateIntFastCase = !op1HasImmediateIntFastCase && isOperandConstantImmediateInt(op2);
    compileBinaryArithOpSlowCase(op_add, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand), op1HasImmediateIntFastCase, op2HasImmediateIntFastCase);
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

    bool op1HasImmediateIntFastCase = isOperandConstantImmediateInt(op1) && getConstantOperandImmediateInt(op1) > 0;
    bool op2HasImmediateIntFastCase = !op1HasImmediateIntFastCase && isOperandConstantImmediateInt(op2) && getConstantOperandImmediateInt(op2) > 0;
    compileBinaryArithOpSlowCase(op_mul, iter, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand), op1HasImmediateIntFastCase, op2HasImmediateIntFastCase);
}

void JIT::emit_op_div(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (isOperandConstantImmediateDouble(op1)) {
        emitGetVirtualRegister(op1, regT0);
        addPtr(tagTypeNumberRegister, regT0);
        movePtrToDouble(regT0, fpRegT0);
    } else if (isOperandConstantImmediateInt(op1)) {
        emitLoadInt32ToDouble(op1, fpRegT0);
    } else {
        emitGetVirtualRegister(op1, regT0);
        if (!types.first().definitelyIsNumber())
            emitJumpSlowCaseIfNotImmediateNumber(regT0);
        Jump notInt = emitJumpIfNotImmediateInteger(regT0);
        convertInt32ToDouble(regT0, fpRegT0);
        Jump skipDoubleLoad = jump();
        notInt.link(this);
        addPtr(tagTypeNumberRegister, regT0);
        movePtrToDouble(regT0, fpRegT0);
        skipDoubleLoad.link(this);
    }
    
    if (isOperandConstantImmediateDouble(op2)) {
        emitGetVirtualRegister(op2, regT1);
        addPtr(tagTypeNumberRegister, regT1);
        movePtrToDouble(regT1, fpRegT1);
    } else if (isOperandConstantImmediateInt(op2)) {
        emitLoadInt32ToDouble(op2, fpRegT1);
    } else {
        emitGetVirtualRegister(op2, regT1);
        if (!types.second().definitelyIsNumber())
            emitJumpSlowCaseIfNotImmediateNumber(regT1);
        Jump notInt = emitJumpIfNotImmediateInteger(regT1);
        convertInt32ToDouble(regT1, fpRegT1);
        Jump skipDoubleLoad = jump();
        notInt.link(this);
        addPtr(tagTypeNumberRegister, regT1);
        movePtrToDouble(regT1, fpRegT1);
        skipDoubleLoad.link(this);
    }
    divDouble(fpRegT1, fpRegT0);

    JumpList doubleResult;
    Jump end;
    bool attemptIntConversion = (!isOperandConstantImmediateInt(op1) || getConstantOperand(op1).asInt32() > 1) && isOperandConstantImmediateInt(op2);
    if (attemptIntConversion) {
        m_assembler.cvttsd2si_rr(fpRegT0, regT0);
        doubleResult.append(branchTest32(Zero, regT0));
        m_assembler.ucomisd_rr(fpRegT1, fpRegT0);
        
        doubleResult.append(m_assembler.jne());
        doubleResult.append(m_assembler.jp());
        emitFastArithIntToImmNoCheck(regT0, regT0);
        end = jump();
    }

    // Double result.
    doubleResult.link(this);
    moveDoubleToPtr(fpRegT0, regT0);
    subPtr(tagTypeNumberRegister, regT0);

    if (attemptIntConversion)
        end.link(this);
    emitPutVirtualRegister(dst, regT0);
}

void JIT::emitSlow_op_div(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
    if (types.first().definitelyIsNumber() && types.second().definitelyIsNumber()) {
#ifndef NDEBUG
        breakpoint();
#endif
        return;
    }
    if (!isOperandConstantImmediateDouble(op1) && !isOperandConstantImmediateInt(op1)) {
        if (!types.first().definitelyIsNumber())
            linkSlowCase(iter);
    }
    if (!isOperandConstantImmediateDouble(op2) && !isOperandConstantImmediateInt(op2)) {
        if (!types.second().definitelyIsNumber())
            linkSlowCase(iter);
    }
    // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
    JITStubCall stubCall(this, cti_op_div);
    stubCall.addArgument(op1, regT2);
    stubCall.addArgument(op2, regT2);
    stubCall.call(result);
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

    compileBinaryArithOpSlowCase(op_sub, iter, result, op1, op2, types, false, false);
}

#else // USE(JSVALUE64)

/* ------------------------------ BEGIN: !USE(JSVALUE64) (OP_ADD, OP_SUB, OP_MUL) ------------------------------ */

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

    JITStubCall stubCall(this, opcodeID == op_add ? cti_op_add : opcodeID == op_sub ? cti_op_sub : cti_op_mul);
    stubCall.addArgument(src1, regT2);
    stubCall.addArgument(src2, regT2);
    stubCall.call(dst);
}

void JIT::emit_op_add(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;
    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);

    if (!types.first().mightBeNumber() || !types.second().mightBeNumber()) {
        JITStubCall stubCall(this, cti_op_add);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(op2, regT2);
        stubCall.call(result);
        return;
    }

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
        compileBinaryArithOp(op_add, result, op1, op2, OperandTypes::fromInt(currentInstruction[4].u.operand));
    }
}

void JIT::emitSlow_op_add(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
    if (!types.first().mightBeNumber() || !types.second().mightBeNumber())
        return;

    if (isOperandConstantImmediateInt(op1)) {
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op1) << JSImmediate::IntegerPayloadShift), regT0);
        notImm.link(this);
        JITStubCall stubCall(this, cti_op_add);
        stubCall.addArgument(op1, regT2);
        stubCall.addArgument(regT0);
        stubCall.call(result);
    } else if (isOperandConstantImmediateInt(op2)) {
        Jump notImm = getSlowCase(iter);
        linkSlowCase(iter);
        sub32(Imm32(getConstantOperandImmediateInt(op2) << JSImmediate::IntegerPayloadShift), regT0);
        notImm.link(this);
        JITStubCall stubCall(this, cti_op_add);
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
        JITStubCall stubCall(this, cti_op_mul);
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

#endif // USE(JSVALUE64)

/* ------------------------------ END: OP_ADD, OP_SUB, OP_MUL ------------------------------ */

#endif // USE(JSVALUE32_64)

} // namespace JSC

#endif // ENABLE(JIT)
