/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#if USE(JSVALUE32_64)
#include "JIT.h"

#include "BasicBlockLocation.h"
#include "BytecodeGenerator.h"
#include "BytecodeStructs.h"
#include "InterpreterInlines.h"
#include "JITInlines.h"
#include "JSFunction.h"
#include "SlowPathCall.h"
#include "TypeProfilerLog.h"
#include "VirtualRegister.h"

namespace JSC {

void JIT::compileOpEqCommon(VirtualRegister src1, VirtualRegister src2)
{
    emitGetVirtualRegister(src1, jsRegT10);
    emitGetVirtualRegister(src2, jsRegT32);
    addSlowCase(branch32(NotEqual, jsRegT10.tagGPR(), jsRegT32.tagGPR()));
    addSlowCase(branchIfCell(jsRegT10));
    addSlowCase(branch32(Below, jsRegT10.tagGPR(), TrustedImm32(JSValue::LowestTag)));
}

void JIT::compileOpEqSlowCommon(Vector<SlowCaseEntry>::iterator& iter)
{
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchIfNotString(jsRegT10.payloadGPR()));
    genericCase.append(branchIfNotString(jsRegT32.payloadGPR()));

    // String case.
    loadGlobalObject(regT4);
    callOperation(operationCompareStringEq, regT4, jsRegT10.payloadGPR(), jsRegT32.payloadGPR());
    Jump done = jump();

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    loadGlobalObject(regT4);
    callOperation(operationCompareEq, regT4, jsRegT10, jsRegT32);

    done.link(this);
}

void JIT::emit_op_eq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEq>();

    compileOpEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    compare32(Equal, jsRegT10.payloadGPR(), jsRegT32.payloadGPR(), regT0);
    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_neq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeq>();

    compileOpEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    compare32(NotEqual, jsRegT10.payloadGPR(), jsRegT32.payloadGPR(), regT0);
    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_jeq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    compileOpEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    addJump(branch32(Equal, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
}

void JIT::emit_op_jneq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    compileOpEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    addJump(branch32(NotEqual, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
}

void JIT::emitSlow_op_eq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpEq>();

    compileOpEqSlowCommon(iter);

    boxBoolean(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(bytecode.m_dst, returnValueJSR);
}

void JIT::emitSlow_op_neq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpNeq>();

    compileOpEqSlowCommon(iter);

    xor32(TrustedImm32(1), returnValueGPR);
    boxBoolean(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(bytecode.m_dst, returnValueJSR);
}

void JIT::emitSlow_op_jeq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    compileOpEqSlowCommon(iter);

    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jneq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    compileOpEqSlowCommon(iter);

    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

void JIT::compileOpStrictEqCommon(VirtualRegister src1,  VirtualRegister src2)
{
    emitGetVirtualRegister(src1, jsRegT10);
    emitGetVirtualRegister(src2, jsRegT32);

    // Bail if the tags differ, or are double.
    addSlowCase(branch32(NotEqual, jsRegT10.tagGPR(), jsRegT32.tagGPR()));
    addSlowCase(branch32(Below, jsRegT10.tagGPR(), TrustedImm32(JSValue::LowestTag)));

    // Jump to a slow case if both are strings or symbols (non object).
    Jump notCell = branchIfNotCell(jsRegT10);
    Jump firstIsObject = branchIfObject(jsRegT10.payloadGPR());
    addSlowCase(branchIfNotObject(jsRegT32.payloadGPR()));
    notCell.link(this);
    firstIsObject.link(this);
}

void JIT::emit_op_stricteq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpStricteq>();
    compileOpStrictEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    compare32(Equal, jsRegT10.payloadGPR(), jsRegT32.payloadGPR(), regT0);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_nstricteq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNstricteq>();
    compileOpStrictEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    compare32(NotEqual, jsRegT10.payloadGPR(), jsRegT32.payloadGPR(), regT0);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_jstricteq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJstricteq>();
    int target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    compileOpStrictEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    addJump(branch32(Equal, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
}

void JIT::emit_op_jnstricteq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJnstricteq>();
    int target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    compileOpStrictEqCommon(bytecode.m_lhs, bytecode.m_rhs);

    addJump(branch32(NotEqual, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
}

void JIT::emitSlow_op_jstricteq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT4);
    callOperation(operationCompareStrictEq, regT4, jsRegT10, jsRegT32);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jnstricteq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJnstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT4);
    callOperation(operationCompareStrictEq, regT4, jsRegT10, jsRegT32);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
