/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)
#include "JIT.h"

#include "ArithProfile.h"
#include "BytecodeGenerator.h"
#include "CodeBlock.h"
#include "JITBitAndGenerator.h"
#include "JITBitOrGenerator.h"
#include "JITBitXorGenerator.h"
#include "JITDivGenerator.h"
#include "JITInlines.h"
#include "JITLeftShiftGenerator.h"
#include "JITMathIC.h"
#include "JITOperations.h"
#include "ResultType.h"
#include "SlowPathCall.h"

namespace JSC {

void JIT::emit_op_less(const JSInstruction* currentInstruction)
{
    emit_compare<OpLess>(currentInstruction, LessThan);
}

void JIT::emit_op_lesseq(const JSInstruction* currentInstruction)
{
    emit_compare<OpLesseq>(currentInstruction, LessThanOrEqual);
}

void JIT::emit_op_greater(const JSInstruction* currentInstruction)
{
    emit_compare<OpGreater>(currentInstruction, GreaterThan);
}

void JIT::emit_op_greatereq(const JSInstruction* currentInstruction)
{
    emit_compare<OpGreatereq>(currentInstruction, GreaterThanOrEqual);
}

void JIT::emit_op_jless(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJless>(currentInstruction, LessThan);
}

void JIT::emit_op_jlesseq(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJlesseq>(currentInstruction, LessThanOrEqual);
}

void JIT::emit_op_jgreater(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJgreater>(currentInstruction, GreaterThan);
}

void JIT::emit_op_jgreatereq(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJgreatereq>(currentInstruction, GreaterThanOrEqual);
}

void JIT::emit_op_jnless(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJnless>(currentInstruction, GreaterThanOrEqual);
}

void JIT::emit_op_jnlesseq(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJnlesseq>(currentInstruction, GreaterThan);
}

void JIT::emit_op_jngreater(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJngreater>(currentInstruction, LessThanOrEqual);
}

void JIT::emit_op_jngreatereq(const JSInstruction* currentInstruction)
{
    emit_compareAndJump<OpJngreatereq>(currentInstruction, LessThan);
}

void JIT::emitSlow_op_less(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareSlow<OpLess>(currentInstruction, DoubleLessThanAndOrdered, operationCompareLess, iter);
}

void JIT::emitSlow_op_lesseq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareSlow<OpLesseq>(currentInstruction, DoubleLessThanOrEqualAndOrdered, operationCompareLessEq, iter);
}

void JIT::emitSlow_op_greater(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareSlow<OpGreater>(currentInstruction, DoubleGreaterThanAndOrdered, operationCompareGreater, iter);
}

void JIT::emitSlow_op_greatereq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareSlow<OpGreatereq>(currentInstruction, DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq, iter);
}

void JIT::emitSlow_op_jless(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJless>(currentInstruction, DoubleLessThanAndOrdered, operationCompareLess, false, iter);
}

void JIT::emitSlow_op_jlesseq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJlesseq>(currentInstruction, DoubleLessThanOrEqualAndOrdered, operationCompareLessEq, false, iter);
}

void JIT::emitSlow_op_jgreater(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJgreater>(currentInstruction, DoubleGreaterThanAndOrdered, operationCompareGreater, false, iter);
}

void JIT::emitSlow_op_jgreatereq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJgreatereq>(currentInstruction, DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq, false, iter);
}

void JIT::emitSlow_op_jnless(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJnless>(currentInstruction, DoubleGreaterThanOrEqualOrUnordered, operationCompareLess, true, iter);
}

void JIT::emitSlow_op_jnlesseq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJnlesseq>(currentInstruction, DoubleGreaterThanOrUnordered, operationCompareLessEq, true, iter);
}

void JIT::emitSlow_op_jngreater(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJngreater>(currentInstruction, DoubleLessThanOrEqualOrUnordered, operationCompareGreater, true, iter);
}

void JIT::emitSlow_op_jngreatereq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJngreatereq>(currentInstruction, DoubleLessThanOrUnordered, operationCompareGreaterEq, true, iter);
}

void JIT::emit_op_below(const JSInstruction* currentInstruction)
{
    emit_compareUnsigned<OpBelow>(currentInstruction, Below);
}

void JIT::emit_op_beloweq(const JSInstruction* currentInstruction)
{
    emit_compareUnsigned<OpBeloweq>(currentInstruction, BelowOrEqual);
}

void JIT::emit_op_jbelow(const JSInstruction* currentInstruction)
{
    emit_compareUnsignedAndJump<OpJbelow>(currentInstruction, Below);
}

void JIT::emit_op_jbeloweq(const JSInstruction* currentInstruction)
{
    emit_compareUnsignedAndJump<OpJbeloweq>(currentInstruction, BelowOrEqual);
}

void JIT::emit_op_unsigned(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpUnsigned>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_operand;
    
    emitGetVirtualRegister(op1, regT0);
    emitJumpSlowCaseIfNotInt(regT0);
    addSlowCase(branch32(LessThan, regT0, TrustedImm32(0)));
    boxInt32(regT0, regT0);
    emitPutVirtualRegister(result, regT0);
}

template<typename Op>
void JIT::emit_compare(const JSInstruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    auto emitCompare = [&](RelationalCondition cond, GPRReg leftGPR, auto right) {
        compare32(cond, leftGPR, right, leftGPR);
        boxBoolean(leftGPR, leftGPR);
        emitPutVirtualRegister(dst, leftGPR);
    };
    emit_compareImpl(op1, op2, condition, emitCompare);
}

template<typename Op>
void JIT::emit_compareAndJump(const JSInstruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);
    auto emitCompareAndJump = [&](RelationalCondition cond, GPRReg leftGPR, auto right) {
        addJump(branch32(cond, leftGPR, right), target);
    };
    emit_compareImpl(op1, op2, condition, emitCompareAndJump);
}

template <typename EmitCompareFunctor>
ALWAYS_INLINE void JIT::emit_compareImpl(VirtualRegister op1, VirtualRegister op2, RelationalCondition condition, const EmitCompareFunctor& emitCompare)
{
    // We generate inline code for the following cases in the fast path:
    // - int immediate to constant int immediate
    // - constant int immediate to int immediate
    // - int immediate to int immediate

    auto handleConstantCharOperand = [&](VirtualRegister left, VirtualRegister right, RelationalCondition cond) {
        if (!isOperandConstantChar(left))
            return false;
        JSString* string = asString(getConstantOperand(left));
        RELEASE_ASSERT(!string->isRope());

        emitGetVirtualRegister(right, regT0);
        addSlowCase(branchIfNotCell(regT0));
        JumpList failures;
        emitLoadCharacterString(regT0, regT0, failures);
        addSlowCase(failures);
        emitCompare(commute(cond), regT0, Imm32(string->tryGetValueImpl()->at(0)));
        return true;
    };

    if (handleConstantCharOperand(op1, op2, condition))
        return;
    if (handleConstantCharOperand(op2, op1, commute(condition)))
        return;

    auto handleConstantIntOperand = [&](VirtualRegister left, VirtualRegister right, GPRReg rightGPR, RelationalCondition cond) {
        if (!isOperandConstantInt(left))
            return false;

        emitGetVirtualRegister(right, rightGPR);
        emitJumpSlowCaseIfNotInt(rightGPR);
        emitCompare(commute(cond), rightGPR, Imm32(getOperandConstantInt(left)));
        return true;
    };

    if (handleConstantIntOperand(op1, op2, regT2, condition))
        return;
    if (handleConstantIntOperand(op2, op1, regT0, commute(condition)))
        return;

    emitGetVirtualRegister(op1, regT0);
    emitGetVirtualRegister(op2, regT2);
    emitJumpSlowCaseIfNotInt(regT0);
    emitJumpSlowCaseIfNotInt(regT2);

    emitCompare(condition, regT0, regT2);
}

template<typename Op>
void JIT::emit_compareUnsignedAndJump(const JSInstruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);
    emit_compareUnsignedAndJumpImpl(op1, op2, target, condition);
}

void JIT::emit_compareUnsignedAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition condition)
{
    if (isOperandConstantInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        jitAssertIsJSInt32(regT0);
        int32_t op2imm = getOperandConstantInt(op2);
        addJump(branch32(condition, regT0, Imm32(op2imm)), target);
    } else if (isOperandConstantInt(op1)) {
        emitGetVirtualRegister(op2, regT1);
        jitAssertIsJSInt32(regT1);
        int32_t op1imm = getOperandConstantInt(op1);
        addJump(branch32(commute(condition), regT1, Imm32(op1imm)), target);
    } else {
        emitGetVirtualRegister(op1, regT0);
        emitGetVirtualRegister(op2, regT1);
        jitAssertIsJSInt32(regT0);
        jitAssertIsJSInt32(regT1);
        addJump(branch32(condition, regT0, regT1), target);
    }
}

template<typename Op>
void JIT::emit_compareUnsigned(const JSInstruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    emit_compareUnsignedImpl(dst, op1, op2, condition);
}

void JIT::emit_compareUnsignedImpl(VirtualRegister dst, VirtualRegister op1, VirtualRegister op2, RelationalCondition condition)
{
    if (isOperandConstantInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        int32_t op2imm = getOperandConstantInt(op2);
        compare32(condition, regT0, Imm32(op2imm), regT0);
    } else if (isOperandConstantInt(op1)) {
        emitGetVirtualRegister(op2, regT0);
        int32_t op1imm = getOperandConstantInt(op1);
        compare32(commute(condition), regT0, Imm32(op1imm), regT0);
    } else {
        emitGetVirtualRegister(op1, regT0);
        emitGetVirtualRegister(op2, regT1);
        compare32(condition, regT0, regT1, regT0);
    }
    boxBoolean(regT0, regT0);
    emitPutVirtualRegister(dst, regT0);
}

template<typename Op, typename SlowOperation>
void JIT::emit_compareSlow(const JSInstruction* instruction, DoubleCondition condition, SlowOperation operation, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    auto handleReturnValueGPR = [&]() {
        boxBoolean(returnValueGPR, regT0);
        emitPutVirtualRegister(dst, regT0);
    };
    auto emitDoubleCompare = [&](FPRReg left, FPRReg right) {
        compareDouble(condition, left, right, regT0);
        boxBoolean(regT0, regT0);
        emitPutVirtualRegister(dst, regT0);
    };
    emit_compareSlowImpl(op1, op2, instruction->size(), operation, iter, handleReturnValueGPR, emitDoubleCompare);
}

template<typename Op, typename SlowOperation>
void JIT::emit_compareAndJumpSlow(const JSInstruction* instruction, DoubleCondition condition, SlowOperation operation, bool invert, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);
    auto handleReturnValueGPR = [&]() {
        emitJumpSlowToHot(branchTest32(invert ? Zero : NonZero, returnValueGPR), target);
    };
    auto emitCompareAndJump = [&](FPRReg left, FPRReg right) {
        emitJumpSlowToHot(branchDouble(condition, left, right), target);
    };
    emit_compareSlowImpl(op1, op2, instruction->size(), operation, iter, handleReturnValueGPR, emitCompareAndJump);
}

template<typename SlowOperation, typename HandleReturnValueGPRFunctor, typename EmitDoubleCompareFunctor>
void JIT::emit_compareSlowImpl(VirtualRegister op1, VirtualRegister op2, size_t instructionSize, SlowOperation operation, Vector<SlowCaseEntry>::iterator& iter, const HandleReturnValueGPRFunctor& handleReturnValueGPR, const EmitDoubleCompareFunctor& emitDoubleCompare)
{

    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.
    if (isOperandConstantChar(op1) || isOperandConstantChar(op2)) {
        linkAllSlowCases(iter);

        constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
        constexpr GPRReg arg1GPR = preferredArgumentGPR<SlowOperation, 1>();
        constexpr GPRReg arg2GPR = preferredArgumentGPR<SlowOperation, 2>();

        emitGetVirtualRegister(op1, arg1GPR);
        emitGetVirtualRegister(op2, arg2GPR);
        loadGlobalObject(globalObjectGPR);
        callOperation(operation, globalObjectGPR, arg1GPR, arg2GPR);
        handleReturnValueGPR();
        return;
    }

    auto unboxDouble = [this](GPRReg src, FPRReg dst) {
        this->unboxDoubleWithoutAssertions(src, src, dst);
    };

    auto handleConstantIntOperandSlow = [&](VirtualRegister op, GPRReg gpr1, FPRReg fpReg1, GPRReg gpr2, FPRReg fpReg2) {
        if (!isOperandConstantInt(op))
            return false;
        linkAllSlowCases(iter);

        Jump fail1 = branchIfNotNumber(gpr2);
        unboxDouble(gpr2, fpReg2);

        move(Imm32(getConstantOperand(op).asInt32()), gpr1);
        convertInt32ToDouble(gpr1, fpReg1);

        emitDoubleCompare(fpRegT0, fpRegT1);

        emitJumpSlowToHot(jump(), instructionSize);

        fail1.link(this);

        emitGetVirtualRegister(op, gpr1);
        loadGlobalObject(regT4);
        callOperation(operation, regT4, regT0, regT2);
        handleReturnValueGPR();
        return true;
    };

    if (handleConstantIntOperandSlow(op1, regT0, fpRegT0, regT2, fpRegT1))
        return;
    if (handleConstantIntOperandSlow(op2, regT2, fpRegT1, regT0, fpRegT0))
        return;

    linkSlowCase(iter); // LHS is not Int.

    Jump fail1 = branchIfNotNumber(regT0);
    Jump fail2 = branchIfNotNumber(regT2);
    Jump fail3 = branchIfInt32(regT2);
    unboxDouble(regT0, fpRegT0);
    unboxDouble(regT2, fpRegT1);

    emitDoubleCompare(fpRegT0, fpRegT1);

    emitJumpSlowToHot(jump(), instructionSize);

    fail1.link(this);
    fail2.link(this);
    fail3.link(this);

    linkSlowCase(iter); // RHS is not Int.
    loadGlobalObject(regT4);
    callOperation(operation, regT4, regT0, regT2);
    handleReturnValueGPR();
}

void JIT::emit_op_inc(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInc>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitGetVirtualRegister(srcDst, regT0);
    emitJumpSlowCaseIfNotInt(regT0);
    addSlowCase(branchAdd32(Overflow, TrustedImm32(1), regT0));
    boxInt32(regT0, regT0);
    emitPutVirtualRegister(srcDst, regT0);
}

void JIT::emit_op_dec(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDec>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitGetVirtualRegister(srcDst, regT0);
    emitJumpSlowCaseIfNotInt(regT0);
    addSlowCase(branchSub32(Overflow, TrustedImm32(1), regT0));
    boxInt32(regT0, regT0);
    emitPutVirtualRegister(srcDst, regT0);
}

/* ------------------------------ BEGIN: OP_MOD ------------------------------ */

#if CPU(X86_64)

void JIT::emit_op_mod(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpMod>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    // Make sure registers are correct for x86 IDIV instructions.
    ASSERT(regT0 == X86Registers::eax);
    auto edx = X86Registers::edx;
    auto ecx = X86Registers::ecx;
    ASSERT(regT4 != edx);
    ASSERT(regT4 != ecx);

    emitGetVirtualRegister(op1, regT4);
    emitGetVirtualRegister(op2, ecx);
    emitJumpSlowCaseIfNotInt(regT4);
    emitJumpSlowCaseIfNotInt(ecx);

    move(regT4, regT0);
    addSlowCase(branchTest32(Zero, ecx));
    Jump denominatorNotNeg1 = branch32(NotEqual, ecx, TrustedImm32(-1));
    addSlowCase(branch32(Equal, regT0, TrustedImm32(INT32_MIN)));
    denominatorNotNeg1.link(this);
    x86ConvertToDoubleWord32();
    x86Div32(ecx);
    Jump numeratorPositive = branch32(GreaterThanOrEqual, regT4, TrustedImm32(0));
    addSlowCase(branchTest32(Zero, edx));
    numeratorPositive.link(this);
    boxInt32(edx, regT0);
    emitPutVirtualRegister(result, regT0);
}

void JIT::emitSlow_op_mod(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, slow_path_mod);
    slowPathCall.call();
}

#elif CPU(ARM64)

void JIT::emit_op_mod(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpMod>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    emitGetVirtualRegister(op1, regT0);
    emitGetVirtualRegister(op2, regT2);

    addSlowCase(branchIfNotInt32(regT0));
    addSlowCase(branchIfNotInt32(regT2));

    GPRReg dividendGPR = regT0;
    GPRReg divisorGPR = regT2;
    GPRReg quotientThenRemainderGPR = regT4;

    addSlowCase(branchTest32(Zero, divisorGPR));

    // This is doing: x - ((x / y) * y)
    div32(dividendGPR, divisorGPR, quotientThenRemainderGPR);
    // This should only overflow for INT32_MIN % -1 but that will end up with quotientThenRemainderGPR == 0 and finally yield -0.0 as expected.
    multiplySub32(quotientThenRemainderGPR, divisorGPR, dividendGPR, quotientThenRemainderGPR);

    // Make sure we're not accidentally producing a positive zero when it should be a negative zero.
    Jump numeratorPositive = branch32(GreaterThanOrEqual, dividendGPR, TrustedImm32(0));
    Jump nonZeroRemainder = branchTest32(NonZero, quotientThenRemainderGPR);
    moveValue(jsDoubleNumber(-0.0), regT0);
    Jump done = jump();

    numeratorPositive.link(this);
    nonZeroRemainder.link(this);

    boxInt32(quotientThenRemainderGPR, regT0);
    done.link(this);

    emitPutVirtualRegister(result, regT0);
}

void JIT::emitSlow_op_mod(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, slow_path_mod);
    slowPathCall.call();
}

#else // CPU(X86_64) || CPU(ARM64)

void JIT::emit_op_mod(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_mod);
    slowPathCall.call();
}

void JIT::emitSlow_op_mod(const JSInstruction*, Vector<SlowCaseEntry>::iterator&)
{
    UNREACHABLE_FOR_PLATFORM();
}

#endif // CPU(X86_64) || CPU(ARM64)

/* ------------------------------ END: OP_MOD ------------------------------ */

void JIT::emit_op_pow(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPow>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg leftGPR = regT0;
    constexpr GPRReg rightGPR = regT2;
    constexpr GPRReg resultGPR = leftGPR;
    constexpr GPRReg scratchGPR = regT4;

    emitGetVirtualRegister(op1, leftGPR);
    emitGetVirtualRegister(op2, rightGPR);
    emitJumpSlowCaseIfNotInt(rightGPR);

    addSlowCase(branch32(LessThan, rightGPR, TrustedImm32(0)));
    addSlowCase(branch32(GreaterThan, rightGPR, TrustedImm32(maxExponentForIntegerMathPow)));

    Jump lhsNotInt = branchIfNotInt32(leftGPR);
    convertInt32ToDouble(leftGPR, fpRegT0);
    Jump lhsReady = jump();
    lhsNotInt.link(this);
    addSlowCase(branchIfNotNumber(leftGPR));
    unboxDouble(leftGPR, scratchGPR, fpRegT0);
    lhsReady.link(this);

    move(TrustedImm32(1), scratchGPR);
    convertInt32ToDouble(scratchGPR, fpRegT1);

    Label loop = label();
    Jump exponentIsEven = branchTest32(Zero, rightGPR, TrustedImm32(1));
    mulDouble(fpRegT0, fpRegT1);
    exponentIsEven.link(this);
    mulDouble(fpRegT0, fpRegT0);
    rshift32(TrustedImm32(1), rightGPR);
    branchTest32(NonZero, rightGPR).linkTo(loop, this);

    boxDouble(fpRegT1, resultGPR);
    emitPutVirtualRegister(result, resultGPR);
}

void JIT::emitSlow_op_pow(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, slow_path_pow);
    slowPathCall.call();
}

void JIT::emit_op_negate(const JSInstruction* currentInstruction)
{
    UnaryArithProfile* arithProfile = &m_unlinkedCodeBlock->unaryArithProfile(currentInstruction->as<OpNegate>().m_profileIndex);
    JITNegIC* negateIC = m_mathICs.addJITNegIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, negateIC);
    // FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
    emitMathICFast<OpNegate>(negateIC, currentInstruction, operationArithNegateProfiled, operationArithNegate);
}

void JIT::emitSlow_op_negate(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITNegIC* negIC = std::bit_cast<JITNegIC*>(m_instructionToMathIC.get(currentInstruction));
    // FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
    emitMathICSlow<OpNegate>(negIC, currentInstruction, operationArithNegateProfiledOptimize, operationArithNegateProfiled, operationArithNegateOptimize);
}

template<typename Op, typename SnippetGenerator>
void JIT::emitBitBinaryOpFastPath(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg leftGPR = regT0;
    constexpr GPRReg rightGPR = regT2;
    constexpr GPRReg resultGPR = leftGPR;
    constexpr GPRReg scratchGPR = regT4;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if constexpr (Op::opcodeID == op_bitand || Op::opcodeID == op_bitor || Op::opcodeID == op_bitxor) {
        leftOperand = SnippetOperand(bytecode.m_operandTypes.first());
        rightOperand = SnippetOperand(bytecode.m_operandTypes.second());
    }

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst())
        emitGetVirtualRegister(op1, leftGPR);
    if (!rightOperand.isConst())
        emitGetVirtualRegister(op2, rightGPR);

    SnippetGenerator gen = [&] {
        if constexpr (SnippetGenerator::needsScratchGPR)
            return SnippetGenerator(leftOperand, rightOperand, resultGPR, leftGPR, rightGPR, scratchGPR);
        else {
            UNUSED_VARIABLE(scratchGPR);
            return SnippetGenerator(leftOperand, rightOperand, resultGPR, leftGPR, rightGPR);
        }
    }();

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);
    emitPutVirtualRegister(result, resultGPR);

    addSlowCase(gen.slowPathJumpList());
}

void JIT::emit_op_bitnot(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpBitnot>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_operand;

    emitGetVirtualRegister(op1, regT0);

    addSlowCase(branchIfNotInt32(regT0));
    not32(regT0);
    boxInt32(regT0, regT0);
    emitPutVirtualRegister(result, regT0);
}

void JIT::emit_op_bitand(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitand, JITBitAndGenerator>(currentInstruction);
}

void JIT::emit_op_bitor(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitor, JITBitOrGenerator>(currentInstruction);
}

void JIT::emit_op_bitxor(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitxor, JITBitXorGenerator>(currentInstruction);
}

void JIT::emit_op_lshift(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpLshift, JITLeftShiftGenerator>(currentInstruction);
}

template<typename Op>
void JIT::emitRightShiftFastPath(const JSInstruction* currentInstruction, JITRightShiftGenerator::ShiftType snippetShiftType)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg leftGPR = regT0;
    constexpr GPRReg rightGPR = regT2;
    constexpr GPRReg resultGPR = leftGPR;
    constexpr GPRReg scratchGPR = regT4;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst())
        emitGetVirtualRegister(op1, leftGPR);
    if (!rightOperand.isConst())
        emitGetVirtualRegister(op2, rightGPR);

    JITRightShiftGenerator gen(leftOperand, rightOperand, resultGPR, leftGPR, rightGPR, fpRegT0, scratchGPR, snippetShiftType);

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);
    emitPutVirtualRegister(result, resultGPR);

    addSlowCase(gen.slowPathJumpList());
}

void JIT::emit_op_rshift(const JSInstruction* currentInstruction)
{
    emitRightShiftFastPath<OpRshift>(currentInstruction, JITRightShiftGenerator::SignedShift);
}

void JIT::emit_op_urshift(const JSInstruction* currentInstruction)
{
    emitRightShiftFastPath<OpUrshift>(currentInstruction, JITRightShiftGenerator::UnsignedShift);
}

void JIT::emit_op_add(const JSInstruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpAdd>().m_profileIndex);
    JITAddIC* addIC = m_mathICs.addJITAddIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, addIC);
    emitMathICFast<OpAdd>(addIC, currentInstruction, operationValueAddProfiled, operationValueAdd);
}

void JIT::emitSlow_op_add(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITAddIC* addIC = std::bit_cast<JITAddIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpAdd>(addIC, currentInstruction, operationValueAddProfiledOptimize, operationValueAddProfiled, operationValueAddOptimize);
}

template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
void JIT::emitMathICFast(JITUnaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledFunction profiledFunction, NonProfiledFunction nonProfiledFunction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister operand = bytecode.m_operand;

    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr GPRReg srcGPR = preferredArgumentGPR<ProfiledFunction, 1>();
    // ArithNegate benefits from using the same register as src and dst.
    constexpr GPRReg resultGPR = srcGPR;
    constexpr GPRReg scratchGPR = globalObjectGPR;
    static_assert(noOverlap(srcGPR, scratchGPR));

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    mathIC->m_generator = Generator(resultGPR, srcGPR, scratchGPR);

    emitGetVirtualRegister(operand, srcGPR);

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.add(currentInstruction, makeUniqueRef<MathICGenerationState>()).iterator->value.get();

    bool generatedInlineCode = mathIC->generateInline(*this, mathICGenerationState);
    if (!generatedInlineCode) {
        UnaryArithProfile* arithProfile = mathIC->arithProfile();
        loadGlobalObject(globalObjectGPR);
        if (arithProfile && shouldEmitProfiling())
            callOperationWithResult(profiledFunction, resultGPR, globalObjectGPR, srcGPR, TrustedImmPtr(arithProfile));
        else
            callOperationWithResult(nonProfiledFunction, resultGPR, globalObjectGPR, srcGPR);
    } else
        addSlowCase(mathICGenerationState.slowPathJumps);

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(inlineEnd).taggedPtr<char*>() - linkBuffer.locationOf(inlineStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultGPR);
}

template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
void JIT::emitMathICFast(JITBinaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledFunction profiledFunction, NonProfiledFunction nonProfiledFunction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr GPRReg leftGPR = preferredArgumentGPR<ProfiledFunction, 1>();
    constexpr GPRReg rightGPR = preferredArgumentGPR<ProfiledFunction, 2>();
    constexpr GPRReg resultGPR = returnValueGPR;
    constexpr GPRReg scratchGPR = regT5;
    static_assert(noOverlap(leftGPR, rightGPR, scratchGPR));
    static_assert(noOverlap(resultGPR, scratchGPR));

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    mathIC->m_generator = Generator(leftOperand, rightOperand, resultGPR, leftGPR, rightGPR, fpRegT0, fpRegT1, scratchGPR);
    
    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));
    
    if (!Generator::isLeftOperandValidConstant(leftOperand))
        emitGetVirtualRegister(op1, leftGPR);
    if (!Generator::isRightOperandValidConstant(rightOperand))
        emitGetVirtualRegister(op2, rightGPR);

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.add(currentInstruction, makeUniqueRef<MathICGenerationState>()).iterator->value.get();

    bool generatedInlineCode = mathIC->generateInline(*this, mathICGenerationState);
    if (!generatedInlineCode) {
        if (leftOperand.isConst())
            emitGetVirtualRegister(op1, leftGPR);
        else if (rightOperand.isConst())
            emitGetVirtualRegister(op2, rightGPR);
        BinaryArithProfile* arithProfile = mathIC->arithProfile();
        loadGlobalObject(globalObjectGPR);
        if (arithProfile && shouldEmitProfiling())
            callOperationWithResult(profiledFunction, resultGPR, globalObjectGPR, leftGPR, rightGPR, TrustedImmPtr(arithProfile));
        else
            callOperationWithResult(nonProfiledFunction, resultGPR, globalObjectGPR, leftGPR, rightGPR);
    } else
        addSlowCase(mathICGenerationState.slowPathJumps);

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(inlineEnd).taggedPtr<char*>() - linkBuffer.locationOf(inlineStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultGPR);
}

template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
void JIT::emitMathICSlow(JITUnaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledRepatchFunction profiledRepatchFunction, ProfiledFunction profiledFunction, RepatchFunction repatchFunction)
{
    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
    mathICGenerationState.slowPathStart = label();

    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;

    constexpr GPRReg globalObjetGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr GPRReg srcGPR = preferredArgumentGPR<ProfiledFunction, 1>();
    constexpr GPRReg resultGPR = returnValueGPR;

#if ENABLE(MATH_IC_STATS)
    auto slowPathStart = label();
#endif

    UnaryArithProfile* arithProfile = mathIC->arithProfile();
    loadGlobalObject(globalObjetGPR);
    if (arithProfile && shouldEmitProfiling()) {
        if (mathICGenerationState.shouldSlowPathRepatch)
            mathICGenerationState.slowPathCall = callOperationWithResult(reinterpret_cast<J_JITOperation_GJMic>(profiledRepatchFunction), resultGPR, globalObjetGPR, srcGPR, TrustedImmPtr(mathIC));
        else
            mathICGenerationState.slowPathCall = callOperationWithResult(profiledFunction, resultGPR, globalObjetGPR, srcGPR, TrustedImmPtr(arithProfile));
    } else
        mathICGenerationState.slowPathCall = callOperationWithResult(reinterpret_cast<J_JITOperation_GJMic>(repatchFunction), resultGPR, globalObjetGPR, srcGPR, TrustedImmPtr(mathIC));

#if ENABLE(MATH_IC_STATS)
    auto slowPathEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(slowPathEnd).taggedPtr<char*>() - linkBuffer.locationOf(slowPathStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultGPR);

    addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
        mathIC->finalizeInlineCode(mathICGenerationState, linkBuffer);
    });
}

template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
void JIT::emitMathICSlow(JITBinaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledRepatchFunction profiledRepatchFunction, ProfiledFunction profiledFunction, RepatchFunction repatchFunction)
{
    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
    mathICGenerationState.slowPathStart = label();

    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg globalObjetGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr GPRReg leftGPR = preferredArgumentGPR<ProfiledFunction, 1>();
    constexpr GPRReg rightGPR = preferredArgumentGPR<ProfiledFunction, 2>();
    constexpr GPRReg resultGPR = returnValueGPR;

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));

    if (Generator::isLeftOperandValidConstant(leftOperand))
        emitGetVirtualRegister(op1, leftGPR);
    else if (Generator::isRightOperandValidConstant(rightOperand))
        emitGetVirtualRegister(op2, rightGPR);

#if ENABLE(MATH_IC_STATS)
    auto slowPathStart = label();
#endif

    BinaryArithProfile* arithProfile = mathIC->arithProfile();
    loadGlobalObject(globalObjetGPR);
    if (arithProfile && shouldEmitProfiling()) {
        if (mathICGenerationState.shouldSlowPathRepatch)
            mathICGenerationState.slowPathCall = callOperationWithResult(std::bit_cast<J_JITOperation_GJJMic>(profiledRepatchFunction), resultGPR, globalObjetGPR, leftGPR, rightGPR, TrustedImmPtr(mathIC));
        else
            mathICGenerationState.slowPathCall = callOperationWithResult(profiledFunction, resultGPR, globalObjetGPR, leftGPR, rightGPR, TrustedImmPtr(arithProfile));
    } else
        mathICGenerationState.slowPathCall = callOperationWithResult(std::bit_cast<J_JITOperation_GJJMic>(repatchFunction), resultGPR, globalObjetGPR, leftGPR, rightGPR, TrustedImmPtr(mathIC));

#if ENABLE(MATH_IC_STATS)
    auto slowPathEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(slowPathEnd).taggedPtr<char*>() - linkBuffer.locationOf(slowPathStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultGPR);

    addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
        mathIC->finalizeInlineCode(mathICGenerationState, linkBuffer);
    });
}

void JIT::emit_op_div(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDiv>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg leftGPR = regT0;
    constexpr GPRReg rightGPR = regT2;
    constexpr GPRReg resultGPR = leftGPR;
    constexpr GPRReg scratchGPR = regT4;
    constexpr FPRReg scratchFPR = fpRegT2;

    BinaryArithProfile* arithProfile = nullptr;
    if (shouldEmitProfiling())
        arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpDiv>().m_profileIndex);

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantDouble(op1))
        leftOperand.setConstDouble(getOperandConstantDouble(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));
    else if (isOperandConstantDouble(op2))
        rightOperand.setConstDouble(getOperandConstantDouble(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst())
        emitGetVirtualRegister(op1, leftGPR);
    if (!rightOperand.isConst())
        emitGetVirtualRegister(op2, rightGPR);

    JITDivGenerator gen(leftOperand, rightOperand, resultGPR, leftGPR, rightGPR,
        fpRegT0, fpRegT1, scratchGPR, scratchFPR, arithProfile);

    gen.generateFastPath(*this);

    if (gen.didEmitFastPath()) {
        gen.endJumpList().link(this);
        emitPutVirtualRegister(result, resultGPR);

        addSlowCase(gen.slowPathJumpList());
    } else {
        ASSERT(gen.endJumpList().empty());
        ASSERT(gen.slowPathJumpList().empty());
        JITSlowPathCall slowPathCall(this, slow_path_div);
        slowPathCall.call();
    }
}

void JIT::emit_op_mul(const JSInstruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpMul>().m_profileIndex);
    JITMulIC* mulIC = m_mathICs.addJITMulIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, mulIC);
    emitMathICFast<OpMul>(mulIC, currentInstruction, operationValueMulProfiled, operationValueMul);
}

void JIT::emitSlow_op_mul(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITMulIC* mulIC = std::bit_cast<JITMulIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpMul>(mulIC, currentInstruction, operationValueMulProfiledOptimize, operationValueMulProfiled, operationValueMulOptimize);
}

void JIT::emit_op_sub(const JSInstruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpSub>().m_profileIndex);
    JITSubIC* subIC = m_mathICs.addJITSubIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, subIC);
    emitMathICFast<OpSub>(subIC, currentInstruction, operationValueSubProfiled, operationValueSub);
}

void JIT::emitSlow_op_sub(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSubIC* subIC = std::bit_cast<JITSubIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpSub>(subIC, currentInstruction, operationValueSubProfiledOptimize, operationValueSubProfiled, operationValueSubOptimize);
}

} // namespace JSC

#endif // ENABLE(JIT)
