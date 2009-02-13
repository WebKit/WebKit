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

using namespace std;

namespace JSC {

void JIT::unlinkCall(CallLinkInfo* callLinkInfo)
{
    // When the JSFunction is deleted the pointer embedded in the instruction stream will no longer be valid
    // (and, if a new JSFunction happened to be constructed at the same location, we could get a false positive
    // match).  Reset the check so it no longer matches.
    callLinkInfo->hotPathBegin.repatch(JSValuePtr::encode(jsImpossibleValue()));
}

void JIT::linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, JITCode ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount)
{
    // Currently we only link calls with the exact number of arguments.
    if (callerArgCount == calleeCodeBlock->m_numParameters) {
        ASSERT(!callLinkInfo->isLinked());
    
        calleeCodeBlock->addCaller(callLinkInfo);
    
        callLinkInfo->hotPathBegin.repatch(callee);
        callLinkInfo->hotPathOther.relink(ctiCode.addressForCall());
    }

    // patch the instruction that jumps out to the cold path, so that we only try to link once.
    callLinkInfo->hotPathBegin.jumpAtOffset(patchOffsetOpCallCompareToJump).relink(callLinkInfo->coldPathOther);
}

void JIT::compileOpCallInitializeCallFrame()
{
    store32(regT1, Address(callFrameRegister, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));

    loadPtr(Address(regT2, FIELD_OFFSET(JSFunction, m_scopeChain) + FIELD_OFFSET(ScopeChain, m_node)), regT1); // newScopeChain

    storePtr(ImmPtr(JSValuePtr::encode(noValue())), Address(callFrameRegister, RegisterFile::OptionalCalleeArguments * static_cast<int>(sizeof(Register))));
    storePtr(regT2, Address(callFrameRegister, RegisterFile::Callee * static_cast<int>(sizeof(Register))));
    storePtr(regT1, Address(callFrameRegister, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register))));
}

void JIT::compileOpCallSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutJITStubArg(regT2, 1);
    emitPutJITStubArgConstant(registerOffset, 2);
    emitPutJITStubArgConstant(argCount, 3);
}

void JIT::compileOpCallEvalSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutJITStubArg(regT2, 1);
    emitPutJITStubArgConstant(registerOffset, 2);
    emitPutJITStubArgConstant(argCount, 3);
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

#if !ENABLE(JIT_OPTIMIZE_CALL)

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // Handle eval
    Jump wasEval;
    if (opcodeID == op_call_eval) {
        emitGetVirtualRegister(callee, regT2);
        compileOpCallEvalSetupArgs(instruction);

        emitCTICall(Interpreter::cti_op_call_eval);
        wasEval = branchPtr(NotEqual, regT0, ImmPtr(JSValuePtr::encode(jsImpossibleValue())));
    }

    emitGetVirtualRegister(callee, regT2);
    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Check for JSFunctions.
    emitJumpSlowCaseIfNotJSCell(regT2);
    addSlowCase(branchPtr(NotEqual, Address(regT2), ImmPtr(m_interpreter->m_jsFunctionVptr)));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT2);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    emitNakedCall(m_interpreter->m_ctiVirtualCall);

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

    // This handles host functions
    emitCTICall(((opcodeID == op_construct) ? Interpreter::cti_op_construct_NotJSConstruct : Interpreter::cti_op_call_NotJSFunction));
    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutVirtualRegister(dst);

    sampleCodeBlock(m_codeBlock);
}

#else

static NO_RETURN void unreachable()
{
    ASSERT_NOT_REACHED();
    exit(1);
}

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned callLinkInfoIndex)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // Handle eval
    Jump wasEval;
    if (opcodeID == op_call_eval) {
        emitGetVirtualRegister(callee, regT2);
        compileOpCallEvalSetupArgs(instruction);

        emitCTICall(Interpreter::cti_op_call_eval);
        wasEval = branchPtr(NotEqual, regT0, ImmPtr(JSValuePtr::encode(jsImpossibleValue())));
    }

    // This plants a check for a cached JSFunction value, so we can plant a fast link to the callee.
    // This deliberately leaves the callee in ecx, used when setting up the stack frame below
    emitGetVirtualRegister(callee, regT2);
    DataLabelPtr addressOfLinkedFunctionCheck;
    Jump jumpToSlow = branchPtrWithPatch(NotEqual, regT2, addressOfLinkedFunctionCheck, ImmPtr(JSValuePtr::encode(jsImpossibleValue())));
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
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(thisRegister);
        emitGetVirtualRegister(callee, regT2);
    }

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    storePtr(ImmPtr(JSValuePtr::encode(noValue())), Address(callFrameRegister, (registerOffset + RegisterFile::OptionalCalleeArguments) * static_cast<int>(sizeof(Register))));
    storePtr(regT2, Address(callFrameRegister, (registerOffset + RegisterFile::Callee) * static_cast<int>(sizeof(Register))));
    loadPtr(Address(regT2, FIELD_OFFSET(JSFunction, m_scopeChain) + FIELD_OFFSET(ScopeChain, m_node)), regT1); // newScopeChain
    store32(Imm32(argCount), Address(callFrameRegister, (registerOffset + RegisterFile::ArgumentCount) * static_cast<int>(sizeof(Register))));
    storePtr(callFrameRegister, Address(callFrameRegister, (registerOffset + RegisterFile::CallerFrame) * static_cast<int>(sizeof(Register))));
    storePtr(regT1, Address(callFrameRegister, (registerOffset + RegisterFile::ScopeChain) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * sizeof(Register)), callFrameRegister);

    // Call to the callee
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall(reinterpret_cast<void*>(unreachable));
    
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
    Jump callLinkFailNotJSFunction = branchPtr(NotEqual, Address(regT2), ImmPtr(m_interpreter->m_jsFunctionVptr));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT2);
    }

    move(Imm32(argCount), regT1);

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);

    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation =
        emitNakedCall(m_interpreter->m_ctiVirtualCallPreLink);

    Jump storeResultForFirstRun = jump();

// FIXME: this label can be removed, since it is a fixed offset from 'callReturnLocation'.
    // This is the address for the cold path *after* the first run (which tries to link the call).
    m_callStructureStubCompilationInfo[callLinkInfoIndex].coldPathOther = MacroAssembler::Label(this);

    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Check for JSFunctions.
    Jump isNotObject = emitJumpIfNotJSCell(regT2);
    Jump isJSFunction = branchPtr(Equal, Address(regT2), ImmPtr(m_interpreter->m_jsFunctionVptr));

    // This handles host functions
    isNotObject.link(this);
    callLinkFailNotObject.link(this);
    callLinkFailNotJSFunction.link(this);
    emitCTICall(((opcodeID == op_construct) ? Interpreter::cti_op_construct_NotJSConstruct : Interpreter::cti_op_call_NotJSFunction));
    Jump wasNotJSFunction = jump();

    // Next, handle JSFunctions...
    isJSFunction.link(this);

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT2);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    emitNakedCall(m_interpreter->m_ctiVirtualCall);

    // Put the return value in dst. In the interpreter, op_ret does this.
    wasNotJSFunction.link(this);
    storeResultForFirstRun.link(this);
    emitPutVirtualRegister(dst);

    sampleCodeBlock(m_codeBlock);
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
