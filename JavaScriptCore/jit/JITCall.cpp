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

void JIT::compileOpCallInitializeCallFrame()
{
    store32(regT1, Address(callFrameRegister, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));

    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_data) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT1); // newScopeChain

    storePtr(ImmPtr(JSValue::encode(JSValue())), Address(callFrameRegister, RegisterFile::OptionalCalleeArguments * static_cast<int>(sizeof(Register))));
    storePtr(regT2, Address(callFrameRegister, RegisterFile::Callee * static_cast<int>(sizeof(Register))));
    storePtr(regT1, Address(callFrameRegister, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register))));
}

void JIT::compileOpCallSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutJITStubArg(regT2, 1);
    emitPutJITStubArgConstant(argCount, 3);
    emitPutJITStubArgConstant(registerOffset, 2);
}
          
void JIT::compileOpCallVarargsSetupArgs(Instruction* instruction)
{
    int registerOffset = instruction[4].u.operand;
    
    // ecx holds func
    emitPutJITStubArg(regT2, 1);
    emitPutJITStubArg(regT1, 3);
    addPtr(Imm32(registerOffset), regT1, regT0);
    emitPutJITStubArg(regT0, 2);
}

void JIT::compileOpConstructSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;
    int proto = instruction[5].u.operand;
    int thisRegister = instruction[6].u.operand;

    // ecx holds func
    emitPutJITStubArg(regT2, 1);
    emitPutJITStubArgConstant(registerOffset, 2);
    emitPutJITStubArgConstant(argCount, 3);
    emitPutJITStubArgFromVirtualRegister(proto, 4, regT0);
    emitPutJITStubArgConstant(thisRegister, 5);
}

void JIT::compileOpCallVarargs(Instruction* instruction)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCountRegister = instruction[3].u.operand;

    emitGetVirtualRegister(argCountRegister, regT1);
    emitGetVirtualRegister(callee, regT2);
    compileOpCallVarargsSetupArgs(instruction);

    // Check for JSFunctions.
    emitJumpSlowCaseIfNotJSCell(regT2);
    addSlowCase(branchPtr(NotEqual, Address(regT2), ImmPtr(m_globalData->jsFunctionVPtr)));
    
    // Speculatively roll the callframe, assuming argCount will match the arity.
    mul32(Imm32(sizeof(Register)), regT0, regT0);
    intptr_t offset = (intptr_t)sizeof(Register) * (intptr_t)RegisterFile::CallerFrame;
    addPtr(Imm32((int32_t)offset), regT0, regT3);
    addPtr(callFrameRegister, regT3);
    storePtr(callFrameRegister, regT3);
    addPtr(regT0, callFrameRegister);
    emitNakedCall(m_globalData->jitStubs.ctiVirtualCall());

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutVirtualRegister(dst);
    
    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallVarargsSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = instruction[1].u.operand;
    
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, JITStubs::cti_op_call_NotJSFunction);
    stubCall.call(dst); // In the interpreter, the callee puts the return value in dst.
    
    sampleCodeBlock(m_codeBlock);
}
    
#if !ENABLE(JIT_OPTIMIZE_CALL)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // Handle eval
    Jump wasEval;
    if (opcodeID == op_call_eval) {
        CallEvalJITStub(this, instruction).call();
        wasEval = branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(JSValue())));
    }

    emitGetVirtualRegister(callee, regT2);
    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Check for JSFunctions.
    emitJumpSlowCaseIfNotJSCell(regT2);
    addSlowCase(branchPtr(NotEqual, Address(regT2), ImmPtr(m_globalData->jsFunctionVPtr)));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        JITStubCall(this, JITStubs::cti_op_construct_JSConstruct).call(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT2);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    emitNakedCall(m_globalData->jitStubs.ctiVirtualCall());

    if (opcodeID == op_call_eval)
        wasEval.link(this);

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutVirtualRegister(dst);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned, OpcodeID opcodeID)
{
    int dst = instruction[1].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, opcodeID == op_construct ? JITStubs::cti_op_construct_NotJSConstruct : JITStubs::cti_op_call_NotJSFunction);
    stubCall.call(dst); // In the interpreter, the callee puts the return value in dst.

    sampleCodeBlock(m_codeBlock);
}

#else // !ENABLE(JIT_OPTIMIZE_CALL)

/* ------------------------------ BEGIN: ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned callLinkInfoIndex)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // Handle eval
    Jump wasEval;
    if (opcodeID == op_call_eval) {
        CallEvalJITStub(this, instruction).call();
        wasEval = branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(JSValue())));
    }

    // This plants a check for a cached JSFunction value, so we can plant a fast link to the callee.
    // This deliberately leaves the callee in ecx, used when setting up the stack frame below
    emitGetVirtualRegister(callee, regT2);
    DataLabelPtr addressOfLinkedFunctionCheck;
    Jump jumpToSlow = branchPtrWithPatch(NotEqual, regT2, addressOfLinkedFunctionCheck, ImmPtr(JSValue::encode(JSValue())));
    addSlowCase(jumpToSlow);
    ASSERT(differenceBetween(addressOfLinkedFunctionCheck, jumpToSlow) == patchOffsetOpCallCompareToJump);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;

    // The following is the fast case, only used whan a callee can be linked.

    // In the case of OpConstruct, call out to a cti_ function to create the new object.
    if (opcodeID == op_construct) {
        int proto = instruction[5].u.operand;
        int thisRegister = instruction[6].u.operand;

        emitPutJITStubArg(regT2, 1);
        emitPutJITStubArgFromVirtualRegister(proto, 4, regT0);
        JITStubCall stubCall(this, JITStubs::cti_op_construct_JSConstruct);
        stubCall.call(thisRegister);
        emitGetVirtualRegister(callee, regT2);
    }

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    storePtr(ImmPtr(JSValue::encode(JSValue())), Address(callFrameRegister, (registerOffset + RegisterFile::OptionalCalleeArguments) * static_cast<int>(sizeof(Register))));
    storePtr(regT2, Address(callFrameRegister, (registerOffset + RegisterFile::Callee) * static_cast<int>(sizeof(Register))));
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_data) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT1); // newScopeChain
    store32(Imm32(argCount), Address(callFrameRegister, (registerOffset + RegisterFile::ArgumentCount) * static_cast<int>(sizeof(Register))));
    storePtr(callFrameRegister, Address(callFrameRegister, (registerOffset + RegisterFile::CallerFrame) * static_cast<int>(sizeof(Register))));
    storePtr(regT1, Address(callFrameRegister, (registerOffset + RegisterFile::ScopeChain) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * sizeof(Register)), callFrameRegister);

    // Call to the callee
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall();
    
    if (opcodeID == op_call_eval)
        wasEval.link(this);

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutVirtualRegister(dst);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex, OpcodeID opcodeID)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    linkSlowCase(iter);

    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Fast check for JS function.
    Jump callLinkFailNotObject = emitJumpIfNotJSCell(regT2);
    Jump callLinkFailNotJSFunction = branchPtr(NotEqual, Address(regT2), ImmPtr(m_globalData->jsFunctionVPtr));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        JITStubCall(this, JITStubs::cti_op_construct_JSConstruct).call(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT2);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation = emitNakedCall(m_globalData->jitStubs.ctiVirtualCallPreLink());

    // Put the return value in dst.
    emitPutVirtualRegister(dst);
    sampleCodeBlock(m_codeBlock);

    // If not, we need an extra case in the if below!
    ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_call_eval));

    // Done! - return back to the hot path.
    if (opcodeID == op_construct)
        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_construct));
    else
        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_call));

    // This handles host functions
    callLinkFailNotObject.link(this);
    callLinkFailNotJSFunction.link(this);
    JITStubCall(this, opcodeID == op_construct ? JITStubs::cti_op_construct_NotJSConstruct : JITStubs::cti_op_call_NotJSFunction).call();

    emitPutVirtualRegister(dst);
    sampleCodeBlock(m_codeBlock);
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_CALL)

} // namespace JSC

#endif // ENABLE(JIT)
