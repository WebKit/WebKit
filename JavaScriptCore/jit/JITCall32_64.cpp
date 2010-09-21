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

#if ENABLE(JIT)
#if USE(JSVALUE32_64)
#include "JIT.h"

#include "CodeBlock.h"
#include "Interpreter.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

void JIT::compileOpCallInitializeCallFrame()
{
    // regT0 holds callee, regT1 holds argCount
    store32(regT1, Address(callFrameRegister, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT3); // scopeChain
    storePtr(regT0, Address(callFrameRegister, RegisterFile::Callee * static_cast<int>(sizeof(Register)))); // callee
    storePtr(regT3, Address(callFrameRegister, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register)))); // scopeChain
}

void JIT::emit_op_call_put_result(Instruction* instruction)
{
    int dst = instruction[1].u.operand;
    emitStore(dst, regT1, regT0);
}

void JIT::compileOpCallVarargs(Instruction* instruction)
{
    int callee = instruction[1].u.operand;
    int argCountRegister = instruction[2].u.operand;
    int registerOffset = instruction[3].u.operand;

    emitLoad(callee, regT1, regT0);
    emitLoadPayload(argCountRegister, regT2); // argCount
    addPtr(Imm32(registerOffset), regT2, regT3); // registerOffset

    emitJumpSlowCaseIfNotJSCell(callee, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr)));

    // Speculatively roll the callframe, assuming argCount will match the arity.
    mul32(Imm32(sizeof(Register)), regT3, regT3);
    addPtr(callFrameRegister, regT3);
    storePtr(callFrameRegister, Address(regT3, RegisterFile::CallerFrame * static_cast<int>(sizeof(Register))));
    move(regT3, callFrameRegister);

    move(regT2, regT1); // argCount

    emitNakedCall(m_globalData->jitStubs->ctiVirtualCall());

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallVarargsSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int callee = instruction[1].u.operand;

    linkSlowCaseIfNotJSCell(iter, callee);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_call_NotJSFunction);
    stubCall.addArgument(regT1, regT0);
    stubCall.addArgument(regT3);
    stubCall.addArgument(regT2);
    stubCall.call();

    sampleCodeBlock(m_codeBlock);
}

void JIT::emit_op_ret(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;

    // We could JIT generate the deref, only calling out to C when the refcount hits zero.
    if (m_codeBlock->needsFullScopeChain())
        JITStubCall(this, cti_op_ret_scopeChain).call();

    emitLoad(dst, regT1, regT0);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);

    restoreReturnAddressBeforeReturn(regT2);
    ret();
}

void JIT::emit_op_ret_object_or_this(Instruction* currentInstruction)
{
    unsigned result = currentInstruction[1].u.operand;
    unsigned thisReg = currentInstruction[2].u.operand;

    // We could JIT generate the deref, only calling out to C when the refcount hits zero.
    if (m_codeBlock->needsFullScopeChain())
        JITStubCall(this, cti_op_ret_scopeChain).call();

    emitLoad(result, regT1, regT0);
    Jump notJSCell = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    Jump notObject = branch8(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo) + OBJECT_OFFSETOF(TypeInfo, m_type)), Imm32(ObjectType));

    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);

    restoreReturnAddressBeforeReturn(regT2);
    ret();

    notJSCell.link(this);
    notObject.link(this);
    emitLoad(thisReg, regT1, regT0);

    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);

    restoreReturnAddressBeforeReturn(regT2);
    ret();
}

void JIT::emitSlow_op_call(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, op_call);
}

void JIT::emitSlow_op_call_eval(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, op_call_eval);
}

void JIT::emitSlow_op_call_varargs(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallVarargsSlowCase(currentInstruction, iter);
}

void JIT::emitSlow_op_construct(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, op_construct);
}

void JIT::emit_op_call(Instruction* currentInstruction)
{
    compileOpCall(op_call, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_call_eval(Instruction* currentInstruction)
{
    compileOpCall(op_call_eval, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_call_varargs(Instruction* currentInstruction)
{
    compileOpCallVarargs(currentInstruction);
}

void JIT::emit_op_construct(Instruction* currentInstruction)
{
    compileOpCall(op_construct, currentInstruction, m_callLinkInfoIndex++);
}

#if !ENABLE(JIT_OPTIMIZE_CALL)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned)
{
    int callee = instruction[1].u.operand;
    int argCount = instruction[2].u.operand;
    int registerOffset = instruction[3].u.operand;

    Jump wasEval;
    if (opcodeID == op_call_eval) {
        JITStubCall stubCall(this, cti_op_call_eval);
        stubCall.addArgument(callee);
        stubCall.addArgument(JIT::Imm32(registerOffset));
        stubCall.addArgument(JIT::Imm32(argCount));
        stubCall.call();
        wasEval = branch32(NotEqual, regT1, Imm32(JSValue::EmptyValueTag));
    }

    emitLoad(callee, regT1, regT0);

    emitJumpSlowCaseIfNotJSCell(callee, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr)));

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    emitNakedCall(opcodeID == op_construct ? m_globalData->jitStubs->ctiVirtualConstruct() : m_globalData->jitStubs->ctiVirtualCall());

    if (opcodeID == op_call_eval)
        wasEval.link(this);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned, OpcodeID opcodeID)
{
    int callee = instruction[1].u.operand;
    int argCount = instruction[2].u.operand;
    int registerOffset = instruction[3].u.operand;

    linkSlowCaseIfNotJSCell(iter, callee);
    linkSlowCase(iter);

    JITStubCall stubCall(this, opcodeID == op_construct ? cti_op_construct_NotJSConstruct : cti_op_call_NotJSFunction);
    stubCall.addArgument(callee);
    stubCall.addArgument(JIT::Imm32(registerOffset));
    stubCall.addArgument(JIT::Imm32(argCount));
    stubCall.call();

    sampleCodeBlock(m_codeBlock);
}

#else // !ENABLE(JIT_OPTIMIZE_CALL)

/* ------------------------------ BEGIN: ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned callLinkInfoIndex)
{
    int callee = instruction[1].u.operand;
    int argCount = instruction[2].u.operand;
    int registerOffset = instruction[3].u.operand;

    Jump wasEval;
    if (opcodeID == op_call_eval) {
        JITStubCall stubCall(this, cti_op_call_eval);
        stubCall.addArgument(callee);
        stubCall.addArgument(JIT::Imm32(registerOffset));
        stubCall.addArgument(JIT::Imm32(argCount));
        stubCall.call();
        wasEval = branch32(NotEqual, regT1, Imm32(JSValue::EmptyValueTag));
    }

    emitLoad(callee, regT1, regT0);

    DataLabelPtr addressOfLinkedFunctionCheck;

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceOpCall);

    Jump jumpToSlow = branchPtrWithPatch(NotEqual, regT0, addressOfLinkedFunctionCheck, ImmPtr(0));

    END_UNINTERRUPTED_SEQUENCE(sequenceOpCall);

    addSlowCase(jumpToSlow);
    ASSERT(differenceBetween(addressOfLinkedFunctionCheck, jumpToSlow) == patchOffsetOpCallCompareToJump);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;

    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)));

    // The following is the fast case, only used whan a callee can be linked.

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT2);

    store32(Imm32(argCount), Address(callFrameRegister, (registerOffset + RegisterFile::ArgumentCount) * static_cast<int>(sizeof(Register))));
    storePtr(callFrameRegister, Address(callFrameRegister, (registerOffset + RegisterFile::CallerFrame) * static_cast<int>(sizeof(Register))));
    emitStore(registerOffset + RegisterFile::Callee, regT1, regT0);
    storePtr(regT2, Address(callFrameRegister, (registerOffset + RegisterFile::ScopeChain) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * sizeof(Register)), callFrameRegister);

    // Call to the callee
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall();
    
    if (opcodeID == op_call_eval)
        wasEval.link(this);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex, OpcodeID opcodeID)
{
    int callee = instruction[1].u.operand;
    int argCount = instruction[2].u.operand;
    int registerOffset = instruction[3].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);

    // Fast check for JS function.
    Jump callLinkFailNotObject = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));
    Jump callLinkFailNotJSFunction = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr));

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation = emitNakedCall(opcodeID == op_construct ? m_globalData->jitStubs->ctiVirtualConstructLink() : m_globalData->jitStubs->ctiVirtualCallLink());

    // Done! - return back to the hot path.
    ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_call_eval));
    ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_construct));
    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_call));

    // This handles host functions
    callLinkFailNotObject.link(this);
    callLinkFailNotJSFunction.link(this);

    JITStubCall stubCall(this, opcodeID == op_construct ? cti_op_construct_NotJSConstruct : cti_op_call_NotJSFunction);
    stubCall.addArgument(callee);
    stubCall.addArgument(JIT::Imm32(registerOffset));
    stubCall.addArgument(JIT::Imm32(argCount));
    stubCall.call();

    sampleCodeBlock(m_codeBlock);
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_CALL)

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
