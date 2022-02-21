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

void JIT::emit_op_jless(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJless>(currentInstruction, LessThan);
}

void JIT::emit_op_jlesseq(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJlesseq>(currentInstruction, LessThanOrEqual);
}

void JIT::emit_op_jgreater(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJgreater>(currentInstruction, GreaterThan);
}

void JIT::emit_op_jgreatereq(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJgreatereq>(currentInstruction, GreaterThanOrEqual);
}

void JIT::emit_op_jnless(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJnless>(currentInstruction, GreaterThanOrEqual);
}

void JIT::emit_op_jnlesseq(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJnlesseq>(currentInstruction, GreaterThan);
}

void JIT::emit_op_jngreater(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJngreater>(currentInstruction, LessThanOrEqual);
}

void JIT::emit_op_jngreatereq(const Instruction* currentInstruction)
{
    emit_compareAndJump<OpJngreatereq>(currentInstruction, LessThan);
}

void JIT::emitSlow_op_jless(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJless>(currentInstruction, DoubleLessThanAndOrdered, operationCompareLess, false, iter);
}

void JIT::emitSlow_op_jlesseq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJlesseq>(currentInstruction, DoubleLessThanOrEqualAndOrdered, operationCompareLessEq, false, iter);
}

void JIT::emitSlow_op_jgreater(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJgreater>(currentInstruction, DoubleGreaterThanAndOrdered, operationCompareGreater, false, iter);
}

void JIT::emitSlow_op_jgreatereq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJgreatereq>(currentInstruction, DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq, false, iter);
}

void JIT::emitSlow_op_jnless(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJnless>(currentInstruction, DoubleGreaterThanOrEqualOrUnordered, operationCompareLess, true, iter);
}

void JIT::emitSlow_op_jnlesseq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJnlesseq>(currentInstruction, DoubleGreaterThanOrUnordered, operationCompareLessEq, true, iter);
}

void JIT::emitSlow_op_jngreater(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJngreater>(currentInstruction, DoubleLessThanOrEqualOrUnordered, operationCompareGreater, true, iter);
}

void JIT::emitSlow_op_jngreatereq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emit_compareAndJumpSlow<OpJngreatereq>(currentInstruction, DoubleLessThanOrUnordered, operationCompareGreaterEq, true, iter);
}

void JIT::emit_op_below(const Instruction* currentInstruction)
{
    emit_compareUnsigned<OpBelow>(currentInstruction, Below);
}

void JIT::emit_op_beloweq(const Instruction* currentInstruction)
{
    emit_compareUnsigned<OpBeloweq>(currentInstruction, BelowOrEqual);
}

void JIT::emit_op_jbelow(const Instruction* currentInstruction)
{
    emit_compareUnsignedAndJump<OpJbelow>(currentInstruction, Below);
}

void JIT::emit_op_jbeloweq(const Instruction* currentInstruction)
{
    emit_compareUnsignedAndJump<OpJbeloweq>(currentInstruction, BelowOrEqual);
}

void JIT::emit_op_unsigned(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpUnsigned>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_operand;
    
    emitGetVirtualRegister(op1, jsRegT10);
    emitJumpSlowCaseIfNotInt(jsRegT10);
    addSlowCase(branch32(LessThan, jsRegT10.payloadGPR(), TrustedImm32(0)));
    boxInt32(jsRegT10.payloadGPR(), jsRegT10);
    emitPutVirtualRegister(result, jsRegT10);
}

template<typename Op>
void JIT::emit_compareAndJump(const Instruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);
    emit_compareAndJumpImpl(op1, op2, target, condition);
}

void JIT::emit_compareAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition condition)
{
    // We generate inline code for the following cases in the fast path:
    // - int immediate to constant int immediate
    // - constant int immediate to int immediate
    // - int immediate to int immediate

    bool disallowAllocation = false;
    if (isOperandConstantChar(op1)) {
        emitGetVirtualRegister(op2, jsRegT10);
        addSlowCase(branchIfNotCell(jsRegT10));
        JumpList failures;
        emitLoadCharacterString(jsRegT10.payloadGPR(), regT0, failures);
        addSlowCase(failures);
        addJump(branch32(commute(condition), regT0, Imm32(asString(getConstantOperand(op1))->tryGetValue(disallowAllocation)[0])), target);
        return;
    }
    if (isOperandConstantChar(op2)) {
        emitGetVirtualRegister(op1, jsRegT10);
        addSlowCase(branchIfNotCell(jsRegT10));
        JumpList failures;
        emitLoadCharacterString(jsRegT10.payloadGPR(), regT0, failures);
        addSlowCase(failures);
        addJump(branch32(condition, regT0, Imm32(asString(getConstantOperand(op2))->tryGetValue(disallowAllocation)[0])), target);
        return;
    }
    if (isOperandConstantInt(op2)) {
        emitGetVirtualRegister(op1, jsRegT10);
        emitJumpSlowCaseIfNotInt(jsRegT10);
        int32_t op2imm = getOperandConstantInt(op2);
        addJump(branch32(condition, jsRegT10.payloadGPR(), Imm32(op2imm)), target);
        return;
    }
    if (isOperandConstantInt(op1)) {
        emitGetVirtualRegister(op2, jsRegT32);
        emitJumpSlowCaseIfNotInt(jsRegT32);
        int32_t op1imm = getOperandConstantInt(op1);
        addJump(branch32(commute(condition), jsRegT32.payloadGPR(), Imm32(op1imm)), target);
        return;
    }

    emitGetVirtualRegister(op1, jsRegT10);
    emitGetVirtualRegister(op2, jsRegT32);
    emitJumpSlowCaseIfNotInt(jsRegT10);
    emitJumpSlowCaseIfNotInt(jsRegT32);

    addJump(branch32(condition, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
}


template<typename Op>
void JIT::emit_compareUnsignedAndJump(const Instruction* instruction, RelationalCondition condition)
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
        emitGetVirtualRegisterPayload(op1, regT0);
        int32_t op2imm = getOperandConstantInt(op2);
        addJump(branch32(condition, regT0, Imm32(op2imm)), target);
    } else if (isOperandConstantInt(op1)) {
        emitGetVirtualRegisterPayload(op2, regT1);
        int32_t op1imm = getOperandConstantInt(op1);
        addJump(branch32(commute(condition), regT1, Imm32(op1imm)), target);
    } else {
        emitGetVirtualRegisterPayload(op1, regT0);
        emitGetVirtualRegisterPayload(op2, regT1);
        addJump(branch32(condition, regT0, regT1), target);
    }
}

template<typename Op>
void JIT::emit_compareUnsigned(const Instruction* instruction, RelationalCondition condition)
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
        emitGetVirtualRegisterPayload(op1, regT0);
        int32_t op2imm = getOperandConstantInt(op2);
        compare32(condition, regT0, Imm32(op2imm), regT0);
    } else if (isOperandConstantInt(op1)) {
        emitGetVirtualRegisterPayload(op2, regT0);
        int32_t op1imm = getOperandConstantInt(op1);
        compare32(commute(condition), regT0, Imm32(op1imm), regT0);
    } else {
        emitGetVirtualRegisterPayload(op1, regT0);
        emitGetVirtualRegisterPayload(op2, regT1);
        compare32(condition, regT0, regT1, regT0);
    }
    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

template<typename Op, typename SlowOperation>
void JIT::emit_compareAndJumpSlow(const Instruction* instruction, DoubleCondition condition, SlowOperation operation, bool invert, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);
    emit_compareAndJumpSlowImpl(op1, op2, target, instruction->size(), condition, operation, invert, iter);
}

template<typename SlowOperation>
void JIT::emit_compareAndJumpSlowImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, size_t instructionSize, DoubleCondition condition, SlowOperation operation, bool invert, Vector<SlowCaseEntry>::iterator& iter)
{

    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.
    if (isOperandConstantChar(op1) || isOperandConstantChar(op2)) {
        linkAllSlowCases(iter);

        constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
        constexpr JSValueRegs arg1JSR = preferredArgumentJSR<SlowOperation, 1>();
        constexpr JSValueRegs arg2JSR = preferredArgumentJSR<SlowOperation, 2>();

        emitGetVirtualRegister(op1, arg1JSR);
        emitGetVirtualRegister(op2, arg2JSR);
        loadGlobalObject(globalObjectGPR);
        callOperation(operation, globalObjectGPR, arg1JSR, arg2JSR);
        emitJumpSlowToHot(branchTest32(invert ? Zero : NonZero, returnValueGPR), target);
        return;
    }

    auto unboxDouble = [this](JSValueRegs src, FPRReg dst) {
#if USE(JSVALUE64)
        this->unboxDoubleWithoutAssertions(src.payloadGPR(), src.payloadGPR(), dst);
#elif USE(JSVALUE32_64)
        this->unboxDouble(src, dst);
#endif
    };

    if (isOperandConstantInt(op2)) {
        linkAllSlowCases(iter);

        if (supportsFloatingPoint()) {
            Jump fail1 = branchIfNotNumber(jsRegT10, regT4);
            unboxDouble(jsRegT10, fpRegT0);

            int32_t op2imm = getConstantOperand(op2).asInt32();

            move(Imm32(op2imm), regT2);
            convertInt32ToDouble(regT2, fpRegT1);

            emitJumpSlowToHot(branchDouble(condition, fpRegT0, fpRegT1), target);

            emitJumpSlowToHot(jump(), instructionSize);

            fail1.link(this);
        }

        emitGetVirtualRegister(op2, jsRegT32);
        loadGlobalObject(regT4);
        callOperation(operation, regT4, jsRegT10, jsRegT32);
        emitJumpSlowToHot(branchTest32(invert ? Zero : NonZero, returnValueGPR), target);
        return;
    }

    if (isOperandConstantInt(op1)) {
        linkAllSlowCases(iter);

        if (supportsFloatingPoint()) {
            Jump fail1 = branchIfNotNumber(jsRegT32, regT4);
            unboxDouble(jsRegT32, fpRegT1);

            int32_t op1imm = getConstantOperand(op1).asInt32();

            move(Imm32(op1imm), regT0);
            convertInt32ToDouble(regT0, fpRegT0);

            emitJumpSlowToHot(branchDouble(condition, fpRegT0, fpRegT1), target);

            emitJumpSlowToHot(jump(), instructionSize);

            fail1.link(this);
        }

        emitGetVirtualRegister(op1, jsRegT10);
        loadGlobalObject(regT4);
        callOperation(operation, regT4, jsRegT10, jsRegT32);
        emitJumpSlowToHot(branchTest32(invert ? Zero : NonZero, returnValueGPR), target);
        return;
    }

    linkSlowCase(iter); // LHS is not Int.

    if (supportsFloatingPoint()) {
        Jump fail1 = branchIfNotNumber(jsRegT10, regT4);
        Jump fail2 = branchIfNotNumber(jsRegT32, regT4);
        Jump fail3 = branchIfInt32(jsRegT32);
        unboxDouble(jsRegT10, fpRegT0);
        unboxDouble(jsRegT32, fpRegT1);

        emitJumpSlowToHot(branchDouble(condition, fpRegT0, fpRegT1), target);

        emitJumpSlowToHot(jump(), instructionSize);

        fail1.link(this);
        fail2.link(this);
        fail3.link(this);
    }

    linkSlowCase(iter); // RHS is not Int.
    loadGlobalObject(regT4);
    callOperation(operation, regT4, jsRegT10, jsRegT32);
    emitJumpSlowToHot(branchTest32(invert ? Zero : NonZero, returnValueGPR), target);
}

void JIT::emit_op_inc(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInc>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitGetVirtualRegister(srcDst, jsRegT10);
    emitJumpSlowCaseIfNotInt(jsRegT10);
    addSlowCase(branchAdd32(Overflow, TrustedImm32(1), jsRegT10.payloadGPR()));
    boxInt32(jsRegT10.payloadGPR(), jsRegT10);
    emitPutVirtualRegister(srcDst, jsRegT10);
}

void JIT::emit_op_dec(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDec>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitGetVirtualRegister(srcDst, jsRegT10);
    emitJumpSlowCaseIfNotInt(jsRegT10);
    addSlowCase(branchSub32(Overflow, TrustedImm32(1), jsRegT10.payloadGPR()));
    boxInt32(jsRegT10.payloadGPR(), jsRegT10);
    emitPutVirtualRegister(srcDst, jsRegT10);
}

/* ------------------------------ BEGIN: OP_MOD ------------------------------ */

#if CPU(X86_64)

void JIT::emit_op_mod(const Instruction* currentInstruction)
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
    addSlowCase(branch32(Equal, regT0, TrustedImm32(-2147483647-1)));
    denominatorNotNeg1.link(this);
    x86ConvertToDoubleWord32();
    x86Div32(ecx);
    Jump numeratorPositive = branch32(GreaterThanOrEqual, regT4, TrustedImm32(0));
    addSlowCase(branchTest32(Zero, edx));
    numeratorPositive.link(this);
    boxInt32(edx, jsRegT10);
    emitPutVirtualRegister(result, jsRegT10);
}

void JIT::emitSlow_op_mod(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_mod);
    slowPathCall.call();
}

#else // CPU(X86_64)

void JIT::emit_op_mod(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_mod);
    slowPathCall.call();
}

void JIT::emitSlow_op_mod(const Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    UNREACHABLE_FOR_PLATFORM();
}

#endif // CPU(X86_64)

/* ------------------------------ END: OP_MOD ------------------------------ */

void JIT::emit_op_pow(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPow>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr JSValueRegs leftRegs = jsRegT10;
    constexpr JSValueRegs rightRegs = jsRegT32;
    constexpr JSValueRegs resultRegs = leftRegs;
    constexpr GPRReg scratchGPR = regT4;

    emitGetVirtualRegister(op1, leftRegs);
    emitGetVirtualRegister(op2, rightRegs);
    emitJumpSlowCaseIfNotInt(rightRegs);

    addSlowCase(branch32(LessThan, rightRegs.payloadGPR(), TrustedImm32(0)));
    addSlowCase(branch32(GreaterThan, rightRegs.payloadGPR(), TrustedImm32(maxExponentForIntegerMathPow)));

    Jump lhsNotInt = branchIfNotInt32(leftRegs);
    convertInt32ToDouble(leftRegs.payloadGPR(), fpRegT0);
    Jump lhsReady = jump();
    lhsNotInt.link(this);
    addSlowCase(branchIfNotNumber(leftRegs, scratchGPR));
#if USE(JSVALUE64)
    unboxDouble(leftRegs.payloadGPR(), scratchGPR, fpRegT0);
#else
    unboxDouble(leftRegs, fpRegT0);
#endif
    lhsReady.link(this);

    move(TrustedImm32(1), scratchGPR);
    convertInt32ToDouble(scratchGPR, fpRegT1);

    Label loop = label();
    Jump exponentIsEven = branchTest32(Zero, rightRegs.payloadGPR(), TrustedImm32(1));
    mulDouble(fpRegT0, fpRegT1);
    exponentIsEven.link(this);
    mulDouble(fpRegT0, fpRegT0);
    rshift32(TrustedImm32(1), rightRegs.payloadGPR());
    branchTest32(NonZero, rightRegs.payloadGPR()).linkTo(loop, this);

    boxDouble(fpRegT1, resultRegs);
    emitPutVirtualRegister(result, resultRegs);
}

void JIT::emitSlow_op_pow(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_pow);
    slowPathCall.call();
}

void JIT::emit_op_negate(const Instruction* currentInstruction)
{
    UnaryArithProfile* arithProfile = &m_unlinkedCodeBlock->unaryArithProfile(currentInstruction->as<OpNegate>().m_profileIndex);
    JITNegIC* negateIC = m_mathICs.addJITNegIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, negateIC);
    // FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
    emitMathICFast<OpNegate>(negateIC, currentInstruction, operationArithNegateProfiled, operationArithNegate);
}

void JIT::emitSlow_op_negate(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITNegIC* negIC = bitwise_cast<JITNegIC*>(m_instructionToMathIC.get(currentInstruction));
    // FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
    emitMathICSlow<OpNegate>(negIC, currentInstruction, operationArithNegateProfiledOptimize, operationArithNegateProfiled, operationArithNegateOptimize);
}

template<typename Op, typename SnippetGenerator>
void JIT::emitBitBinaryOpFastPath(const Instruction* currentInstruction, ProfilingPolicy profilingPolicy)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr JSValueRegs leftRegs = jsRegT10;
    constexpr JSValueRegs rightRegs = jsRegT32;
    constexpr JSValueRegs resultRegs = leftRegs;
    constexpr GPRReg scratchGPR = regT4;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst())
        emitGetVirtualRegister(op1, leftRegs);
    if (!rightOperand.isConst())
        emitGetVirtualRegister(op2, rightRegs);

    SnippetGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, scratchGPR);

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);
    if (profilingPolicy == ProfilingPolicy::ShouldEmitProfiling)
        emitValueProfilingSiteIfProfiledOpcode(bytecode);
    emitPutVirtualRegister(result, resultRegs);

    addSlowCase(gen.slowPathJumpList());
}

void JIT::emit_op_bitnot(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpBitnot>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_operand;

    emitGetVirtualRegister(op1, jsRegT10);

    addSlowCase(branchIfNotInt32(jsRegT10));
    not32(jsRegT10.payloadGPR());
#if USE(JSVALUE64)
    boxInt32(jsRegT10.payloadGPR(), jsRegT10);
#endif

    emitValueProfilingSiteIfProfiledOpcode(bytecode);

    emitPutVirtualRegister(result, jsRegT10);
}

void JIT::emit_op_bitand(const Instruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitand, JITBitAndGenerator>(currentInstruction, ProfilingPolicy::ShouldEmitProfiling);
}

void JIT::emit_op_bitor(const Instruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitor, JITBitOrGenerator>(currentInstruction, ProfilingPolicy::ShouldEmitProfiling);
}

void JIT::emit_op_bitxor(const Instruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitxor, JITBitXorGenerator>(currentInstruction, ProfilingPolicy::ShouldEmitProfiling);
}

void JIT::emit_op_lshift(const Instruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpLshift, JITLeftShiftGenerator>(currentInstruction);
}

void JIT::emitRightShiftFastPath(const Instruction* currentInstruction, OpcodeID opcodeID)
{
    ASSERT(opcodeID == op_rshift || opcodeID == op_urshift);
    switch (opcodeID) {
    case op_rshift:
        emitRightShiftFastPath<OpRshift>(currentInstruction, JITRightShiftGenerator::SignedShift);
        break;
    case op_urshift:
        emitRightShiftFastPath<OpUrshift>(currentInstruction, JITRightShiftGenerator::UnsignedShift);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

template<typename Op>
void JIT::emitRightShiftFastPath(const Instruction* currentInstruction, JITRightShiftGenerator::ShiftType snippetShiftType)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr JSValueRegs leftRegs = jsRegT10;
    constexpr JSValueRegs rightRegs = jsRegT32;
    constexpr JSValueRegs resultRegs = leftRegs;
    constexpr GPRReg scratchGPR = regT4;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst())
        emitGetVirtualRegister(op1, leftRegs);
    if (!rightOperand.isConst())
        emitGetVirtualRegister(op2, rightRegs);

    JITRightShiftGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, fpRegT0, scratchGPR, snippetShiftType);

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);
    emitPutVirtualRegister(result, resultRegs);

    addSlowCase(gen.slowPathJumpList());
}

void JIT::emit_op_rshift(const Instruction* currentInstruction)
{
    emitRightShiftFastPath(currentInstruction, op_rshift);
}

void JIT::emit_op_urshift(const Instruction* currentInstruction)
{
    emitRightShiftFastPath(currentInstruction, op_urshift);
}

void JIT::emit_op_add(const Instruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpAdd>().m_profileIndex);
    JITAddIC* addIC = m_mathICs.addJITAddIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, addIC);
    emitMathICFast<OpAdd>(addIC, currentInstruction, operationValueAddProfiled, operationValueAdd);
}

void JIT::emitSlow_op_add(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITAddIC* addIC = bitwise_cast<JITAddIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpAdd>(addIC, currentInstruction, operationValueAddProfiledOptimize, operationValueAddProfiled, operationValueAddOptimize);
}

template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
void JIT::emitMathICFast(JITUnaryMathIC<Generator>* mathIC, const Instruction* currentInstruction, ProfiledFunction profiledFunction, NonProfiledFunction nonProfiledFunction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister operand = bytecode.m_operand;

    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr JSValueRegs srcRegs = preferredArgumentJSR<ProfiledFunction, 1>();
    // ArithNegate benefits from using the same register as src and dst.
    constexpr JSValueRegs resultRegs = srcRegs;
    constexpr GPRReg scratchGPR = globalObjectGPR;
    static_assert(noOverlap(srcRegs, scratchGPR));

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    mathIC->m_generator = Generator(resultRegs, srcRegs, scratchGPR);

    emitGetVirtualRegister(operand, srcRegs);

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.add(currentInstruction, makeUniqueRef<MathICGenerationState>()).iterator->value.get();

    bool generatedInlineCode = mathIC->generateInline(*this, mathICGenerationState);
    if (!generatedInlineCode) {
        UnaryArithProfile* arithProfile = mathIC->arithProfile();
        loadGlobalObject(globalObjectGPR);
        if (arithProfile && shouldEmitProfiling())
            callOperationWithResult(profiledFunction, resultRegs, globalObjectGPR, srcRegs, arithProfile);
        else
            callOperationWithResult(nonProfiledFunction, resultRegs, globalObjectGPR, srcRegs);
    } else
        addSlowCase(mathICGenerationState.slowPathJumps);

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(inlineEnd).executableAddress<char*>() - linkBuffer.locationOf(inlineStart).executableAddress<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultRegs);
}

template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
void JIT::emitMathICFast(JITBinaryMathIC<Generator>* mathIC, const Instruction* currentInstruction, ProfiledFunction profiledFunction, NonProfiledFunction nonProfiledFunction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr JSValueRegs leftRegs = preferredArgumentJSR<ProfiledFunction, 1>();
    constexpr JSValueRegs rightRegs = preferredArgumentJSR<ProfiledFunction, 2>();
    constexpr JSValueRegs resultRegs = returnValueJSR;
    constexpr GPRReg scratchGPR = regT5;
    static_assert(noOverlap(leftRegs, rightRegs, scratchGPR));
    static_assert(noOverlap(resultRegs, scratchGPR));

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    mathIC->m_generator = Generator(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, fpRegT0, fpRegT1, scratchGPR);
    
    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));
    
    if (!Generator::isLeftOperandValidConstant(leftOperand))
        emitGetVirtualRegister(op1, leftRegs);
    if (!Generator::isRightOperandValidConstant(rightOperand))
        emitGetVirtualRegister(op2, rightRegs);

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.add(currentInstruction, makeUniqueRef<MathICGenerationState>()).iterator->value.get();

    bool generatedInlineCode = mathIC->generateInline(*this, mathICGenerationState);
    if (!generatedInlineCode) {
        if (leftOperand.isConst())
            emitGetVirtualRegister(op1, leftRegs);
        else if (rightOperand.isConst())
            emitGetVirtualRegister(op2, rightRegs);
        BinaryArithProfile* arithProfile = mathIC->arithProfile();
        loadGlobalObject(globalObjectGPR);
        if (arithProfile && shouldEmitProfiling())
            callOperationWithResult(profiledFunction, resultRegs, globalObjectGPR, leftRegs, rightRegs, arithProfile);
        else
            callOperationWithResult(nonProfiledFunction, resultRegs, globalObjectGPR, leftRegs, rightRegs);
    } else
        addSlowCase(mathICGenerationState.slowPathJumps);

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(inlineEnd).executableAddress<char*>() - linkBuffer.locationOf(inlineStart).executableAddress<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultRegs);
}

template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
void JIT::emitMathICSlow(JITUnaryMathIC<Generator>* mathIC, const Instruction* currentInstruction, ProfiledRepatchFunction profiledRepatchFunction, ProfiledFunction profiledFunction, RepatchFunction repatchFunction)
{
    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
    mathICGenerationState.slowPathStart = label();

    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;

    constexpr GPRReg globalObjetGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr JSValueRegs srcRegs = preferredArgumentJSR<ProfiledFunction, 1>();
    constexpr JSValueRegs resultRegs = returnValueJSR;

#if ENABLE(MATH_IC_STATS)
    auto slowPathStart = label();
#endif

    UnaryArithProfile* arithProfile = mathIC->arithProfile();
    loadGlobalObject(globalObjetGPR);
    if (arithProfile && shouldEmitProfiling()) {
        if (mathICGenerationState.shouldSlowPathRepatch)
            mathICGenerationState.slowPathCall = callOperationWithResult(reinterpret_cast<J_JITOperation_GJMic>(profiledRepatchFunction), resultRegs, globalObjetGPR, srcRegs, TrustedImmPtr(mathIC));
        else
            mathICGenerationState.slowPathCall = callOperationWithResult(profiledFunction, resultRegs, globalObjetGPR, srcRegs, arithProfile);
    } else
        mathICGenerationState.slowPathCall = callOperationWithResult(reinterpret_cast<J_JITOperation_GJMic>(repatchFunction), resultRegs, globalObjetGPR, srcRegs, TrustedImmPtr(mathIC));

#if ENABLE(MATH_IC_STATS)
    auto slowPathEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(slowPathEnd).executableAddress<char*>() - linkBuffer.locationOf(slowPathStart).executableAddress<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultRegs);

    addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
        mathIC->finalizeInlineCode(mathICGenerationState, linkBuffer);
    });
}

template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
void JIT::emitMathICSlow(JITBinaryMathIC<Generator>* mathIC, const Instruction* currentInstruction, ProfiledRepatchFunction profiledRepatchFunction, ProfiledFunction profiledFunction, RepatchFunction repatchFunction)
{
    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
    mathICGenerationState.slowPathStart = label();

    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr GPRReg globalObjetGPR = preferredArgumentGPR<ProfiledFunction, 0>();
    constexpr JSValueRegs leftRegs = preferredArgumentJSR<ProfiledFunction, 1>();
    constexpr JSValueRegs rightRegs = preferredArgumentJSR<ProfiledFunction, 2>();
    constexpr JSValueRegs resultRegs = returnValueJSR;

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));

    if (Generator::isLeftOperandValidConstant(leftOperand))
        emitGetVirtualRegister(op1, leftRegs);
    else if (Generator::isRightOperandValidConstant(rightOperand))
        emitGetVirtualRegister(op2, rightRegs);

#if ENABLE(MATH_IC_STATS)
    auto slowPathStart = label();
#endif

    BinaryArithProfile* arithProfile = mathIC->arithProfile();
    loadGlobalObject(globalObjetGPR);
    if (arithProfile && shouldEmitProfiling()) {
        if (mathICGenerationState.shouldSlowPathRepatch)
            mathICGenerationState.slowPathCall = callOperationWithResult(bitwise_cast<J_JITOperation_GJJMic>(profiledRepatchFunction), resultRegs, globalObjetGPR, leftRegs, rightRegs, TrustedImmPtr(mathIC));
        else
            mathICGenerationState.slowPathCall = callOperationWithResult(profiledFunction, resultRegs, globalObjetGPR, leftRegs, rightRegs, arithProfile);
    } else
        mathICGenerationState.slowPathCall = callOperationWithResult(bitwise_cast<J_JITOperation_GJJMic>(repatchFunction), resultRegs, globalObjetGPR, leftRegs, rightRegs, TrustedImmPtr(mathIC));

#if ENABLE(MATH_IC_STATS)
    auto slowPathEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(slowPathEnd).executableAddress<char*>() - linkBuffer.locationOf(slowPathStart).executableAddress<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    emitPutVirtualRegister(result, resultRegs);

    addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
        mathIC->finalizeInlineCode(mathICGenerationState, linkBuffer);
    });
}

void JIT::emit_op_div(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDiv>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    constexpr JSValueRegs leftRegs = jsRegT10;
    constexpr JSValueRegs rightRegs = jsRegT32;
    constexpr JSValueRegs resultRegs = leftRegs;
    constexpr GPRReg scratchGPR = regT4;
    constexpr FPRReg scratchFPR = fpRegT2;

    BinaryArithProfile* arithProfile = nullptr;
    if (shouldEmitProfiling())
        arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpDiv>().m_profileIndex);

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
#if USE(JSVALUE64)
    else if (isOperandConstantDouble(op1))
        leftOperand.setConstDouble(getOperandConstantDouble(op1));
#endif
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));
#if USE(JSVALUE64)
    else if (isOperandConstantDouble(op2))
        rightOperand.setConstDouble(getOperandConstantDouble(op2));
#endif

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst())
        emitGetVirtualRegister(op1, leftRegs);
    if (!rightOperand.isConst())
        emitGetVirtualRegister(op2, rightRegs);

    JITDivGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs,
        fpRegT0, fpRegT1, scratchGPR, scratchFPR, arithProfile);

    gen.generateFastPath(*this);

    if (gen.didEmitFastPath()) {
        gen.endJumpList().link(this);
        emitPutVirtualRegister(result, resultRegs);

        addSlowCase(gen.slowPathJumpList());
    } else {
        ASSERT(gen.endJumpList().empty());
        ASSERT(gen.slowPathJumpList().empty());
        JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_div);
        slowPathCall.call();
    }
}

void JIT::emit_op_mul(const Instruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpMul>().m_profileIndex);
    JITMulIC* mulIC = m_mathICs.addJITMulIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, mulIC);
    emitMathICFast<OpMul>(mulIC, currentInstruction, operationValueMulProfiled, operationValueMul);
}

void JIT::emitSlow_op_mul(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITMulIC* mulIC = bitwise_cast<JITMulIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpMul>(mulIC, currentInstruction, operationValueMulProfiledOptimize, operationValueMulProfiled, operationValueMulOptimize);
}

void JIT::emit_op_sub(const Instruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpSub>().m_profileIndex);
    JITSubIC* subIC = m_mathICs.addJITSubIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, subIC);
    emitMathICFast<OpSub>(subIC, currentInstruction, operationValueSubProfiled, operationValueSub);
}

void JIT::emitSlow_op_sub(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITSubIC* subIC = bitwise_cast<JITSubIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpSub>(subIC, currentInstruction, operationValueSubProfiledOptimize, operationValueSubProfiled, operationValueSubOptimize);
}

} // namespace JSC

#endif // ENABLE(JIT)
