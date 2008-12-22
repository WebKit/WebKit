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
    DataLabelPtr::patch(callLinkInfo->hotPathBegin, JSImmediate::impossibleValue());
}

void JIT::linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, void* ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount)
{
    // Currently we only link calls with the exact number of arguments.
    if (callerArgCount == calleeCodeBlock->m_numParameters) {
        ASSERT(!callLinkInfo->isLinked());
    
        calleeCodeBlock->addCaller(callLinkInfo);
    
        DataLabelPtr::patch(callLinkInfo->hotPathBegin, callee);
        Jump::patch(callLinkInfo->hotPathOther, ctiCode);
    }

    // patch the instruction that jumps out to the cold path, so that we only try to link once.
    void* patchCheck = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(callLinkInfo->hotPathBegin) + patchOffsetOpCallCompareToJump);
    Jump::patch(patchCheck, callLinkInfo->coldPathOther);
}

void JIT::compileOpCallInitializeCallFrame()
{
    store32(X86::edx, Address(callFrameRegister, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));

    loadPtr(Address(X86::ecx, FIELD_OFFSET(JSFunction, m_scopeChain) + FIELD_OFFSET(ScopeChain, m_node)), X86::edx); // newScopeChain

    storePtr(ImmPtr(noValue()), Address(callFrameRegister, RegisterFile::OptionalCalleeArguments * static_cast<int>(sizeof(Register))));
    storePtr(X86::ecx, Address(callFrameRegister, RegisterFile::Callee * static_cast<int>(sizeof(Register))));
    storePtr(X86::edx, Address(callFrameRegister, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register))));
}

void JIT::compileOpCallSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutJITStubArg(X86::ecx, 1);
    emitPutJITStubArgConstant(registerOffset, 2);
    emitPutJITStubArgConstant(argCount, 3);
}

void JIT::compileOpCallEvalSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutJITStubArg(X86::ecx, 1);
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
    emitPutJITStubArg(X86::ecx, 1);
    emitPutJITStubArgConstant(registerOffset, 2);
    emitPutJITStubArgConstant(argCount, 3);
    emitPutJITStubArgFromVirtualRegister(proto, 4, X86::eax);
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
        emitGetVirtualRegister(callee, X86::ecx);
        compileOpCallEvalSetupArgs(instruction);

        emitCTICall(Interpreter::cti_op_call_eval);
        wasEval = jnePtr(X86::eax, ImmPtr(JSImmediate::impossibleValue()));
    }

    emitGetVirtualRegister(callee, X86::ecx);
    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Check for JSFunctions.
    emitJumpSlowCaseIfNotJSCell(X86::ecx);
    addSlowCase(jnePtr(Address(X86::ecx), ImmPtr(m_interpreter->m_jsFunctionVptr)));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, X86::ecx);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), X86::edx);

    emitNakedCall(m_interpreter->m_ctiVirtualCall);

    if (opcodeID == op_call_eval)
        wasEval.link(this);

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutVirtualRegister(dst);

#if ENABLE(CODEBLOCK_SAMPLING)
    storePtr(ImmPtr(m_codeBlock), m_interpreter->sampler()->codeBlockSlot());
#endif
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

#if ENABLE(CODEBLOCK_SAMPLING)
    storePtr(ImmPtr(m_codeBlock), m_interpreter->sampler()->codeBlockSlot());
#endif
}

#else

static void unreachable()
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
        emitGetVirtualRegister(callee, X86::ecx);
        compileOpCallEvalSetupArgs(instruction);

        emitCTICall(Interpreter::cti_op_call_eval);
        wasEval = jnePtr(X86::eax, ImmPtr(JSImmediate::impossibleValue()));
    }

    // This plants a check for a cached JSFunction value, so we can plant a fast link to the callee.
    // This deliberately leaves the callee in ecx, used when setting up the stack frame below
    emitGetVirtualRegister(callee, X86::ecx);
    DataLabelPtr addressOfLinkedFunctionCheck;
    Jump jumpToSlow = jnePtrWithPatch(X86::ecx, addressOfLinkedFunctionCheck, ImmPtr(JSImmediate::impossibleValue()));
    addSlowCase(jumpToSlow);
    ASSERT(differenceBetween(addressOfLinkedFunctionCheck, jumpToSlow) == patchOffsetOpCallCompareToJump);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;

    // The following is the fast case, only used whan a callee can be linked.

    // In the case of OpConstruct, call out to a cti_ function to create the new object.
    if (opcodeID == op_construct) {
        int proto = instruction[5].u.operand;
        int thisRegister = instruction[6].u.operand;

        emitPutJITStubArg(X86::ecx, 1);
        emitPutJITStubArgFromVirtualRegister(proto, 4, X86::eax);
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(thisRegister);
        emitGetVirtualRegister(callee, X86::ecx);
    }

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    storePtr(ImmPtr(noValue()), Address(callFrameRegister, (registerOffset + RegisterFile::OptionalCalleeArguments) * static_cast<int>(sizeof(Register))));
    storePtr(X86::ecx, Address(callFrameRegister, (registerOffset + RegisterFile::Callee) * static_cast<int>(sizeof(Register))));
    loadPtr(Address(X86::ecx, FIELD_OFFSET(JSFunction, m_scopeChain) + FIELD_OFFSET(ScopeChain, m_node)), X86::edx); // newScopeChain
    store32(Imm32(argCount), Address(callFrameRegister, (registerOffset + RegisterFile::ArgumentCount) * static_cast<int>(sizeof(Register))));
    storePtr(callFrameRegister, Address(callFrameRegister, (registerOffset + RegisterFile::CallerFrame) * static_cast<int>(sizeof(Register))));
    storePtr(X86::edx, Address(callFrameRegister, (registerOffset + RegisterFile::ScopeChain) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * sizeof(Register)), callFrameRegister);

    // Call to the callee
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall(reinterpret_cast<void*>(unreachable));
    
    if (opcodeID == op_call_eval)
        wasEval.link(this);

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutVirtualRegister(dst);

#if ENABLE(CODEBLOCK_SAMPLING)
        storePtr(ImmPtr(m_codeBlock), m_interpreter->sampler()->codeBlockSlot());
#endif
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
    Jump callLinkFailNotObject = jnz32(X86::ecx, Imm32(JSImmediate::TagMask));
    Jump callLinkFailNotJSFunction = jnePtr(Address(X86::ecx), ImmPtr(m_interpreter->m_jsFunctionVptr));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        emitCTICall(Interpreter::cti_op_construct_JSConstruct);
        emitPutVirtualRegister(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, X86::ecx);
    }

    move(Imm32(argCount), X86::edx);

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
    Jump isNotObject = jnzPtr(X86::ecx, Imm32(JSImmediate::TagMask));
    Jump isJSFunction = jePtr(Address(X86::ecx), ImmPtr(m_interpreter->m_jsFunctionVptr));

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
        emitGetVirtualRegister(callee, X86::ecx);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), X86::edx);

    emitNakedCall(m_interpreter->m_ctiVirtualCall);

    // Put the return value in dst. In the interpreter, op_ret does this.
    wasNotJSFunction.link(this);
    storeResultForFirstRun.link(this);
    emitPutVirtualRegister(dst);

#if ENABLE(CODEBLOCK_SAMPLING)
    storePtr(ImmPtr(m_codeBlock), m_interpreter->sampler()->codeBlockSlot());
#endif
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
