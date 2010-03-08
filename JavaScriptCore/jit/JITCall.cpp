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

void JIT::compileOpCallInitializeCallFrame()
{
    // regT0 holds callee, regT1 holds argCount
    store32(regT1, Address(callFrameRegister, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_data) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT1); // scopeChain

    emitStore(static_cast<unsigned>(RegisterFile::OptionalCalleeArguments), JSValue());
    storePtr(regT0, Address(callFrameRegister, RegisterFile::Callee * static_cast<int>(sizeof(Register)))); // callee
    storePtr(regT1, Address(callFrameRegister, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register)))); // scopeChain
}

void JIT::compileOpCallSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    emitPutJITStubArg(regT1, regT0, 0);
    emitPutJITStubArgConstant(registerOffset, 1);
    emitPutJITStubArgConstant(argCount, 2);
}
          
void JIT::compileOpConstructSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;
    int proto = instruction[5].u.operand;
    int thisRegister = instruction[6].u.operand;

    emitPutJITStubArg(regT1, regT0, 0);
    emitPutJITStubArgConstant(registerOffset, 1);
    emitPutJITStubArgConstant(argCount, 2);
    emitPutJITStubArgFromVirtualRegister(proto, 3, regT2, regT3);
    emitPutJITStubArgConstant(thisRegister, 4);
}

void JIT::compileOpCallVarargsSetupArgs(Instruction*)
{
    emitPutJITStubArg(regT1, regT0, 0);
    emitPutJITStubArg(regT3, 1); // registerOffset
    emitPutJITStubArg(regT2, 2); // argCount
}

void JIT::compileOpCallVarargs(Instruction* instruction)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCountRegister = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    emitLoad(callee, regT1, regT0);
    emitLoadPayload(argCountRegister, regT2); // argCount
    addPtr(Imm32(registerOffset), regT2, regT3); // registerOffset

    compileOpCallVarargsSetupArgs(instruction);

    emitJumpSlowCaseIfNotJSCell(callee, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr)));

    // Speculatively roll the callframe, assuming argCount will match the arity.
    mul32(Imm32(sizeof(Register)), regT3, regT3);
    addPtr(callFrameRegister, regT3);
    storePtr(callFrameRegister, Address(regT3, RegisterFile::CallerFrame * static_cast<int>(sizeof(Register))));
    move(regT3, callFrameRegister);

    move(regT2, regT1); // argCount

    emitNakedCall(m_globalData->jitStubs.ctiVirtualCall());

    emitStore(dst, regT1, regT0);
    
    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallVarargsSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;

    linkSlowCaseIfNotJSCell(iter, callee);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_call_NotJSFunction);
    stubCall.call(dst); // In the interpreter, the callee puts the return value in dst.

    map(m_bytecodeIndex + OPCODE_LENGTH(op_call_varargs), dst, regT1, regT0);
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

void JIT::emit_op_construct_verify(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;

    emitLoad(dst, regT1, regT0);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)));
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addSlowCase(branch8(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo) + OBJECT_OFFSETOF(TypeInfo, m_type)), Imm32(ObjectType)));
}

void JIT::emitSlow_op_construct_verify(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);
    emitLoad(src, regT1, regT0);
    emitStore(dst, regT1, regT0);
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

void JIT::emit_op_load_varargs(Instruction* currentInstruction)
{
    int argCountDst = currentInstruction[1].u.operand;
    int argsOffset = currentInstruction[2].u.operand;

    JITStubCall stubCall(this, cti_op_load_varargs);
    stubCall.addArgument(Imm32(argsOffset));
    stubCall.call();
    // Stores a naked int32 in the register file.
    store32(returnValueRegister, Address(callFrameRegister, argCountDst * sizeof(Register)));
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
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

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

    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    emitJumpSlowCaseIfNotJSCell(callee, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr)));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        JITStubCall(this, cti_op_construct_JSConstruct).call(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitLoad(callee, regT1, regT0);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    emitNakedCall(m_globalData->jitStubs.ctiVirtualCall());

    if (opcodeID == op_call_eval)
        wasEval.link(this);

    emitStore(dst, regT1, regT0);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned, OpcodeID opcodeID)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;

    linkSlowCaseIfNotJSCell(iter, callee);
    linkSlowCase(iter);

    JITStubCall stubCall(this, opcodeID == op_construct ? cti_op_construct_NotJSConstruct : cti_op_call_NotJSFunction);
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

    // In the case of OpConstruct, call out to a cti_ function to create the new object.
    if (opcodeID == op_construct) {
        int proto = instruction[5].u.operand;
        int thisRegister = instruction[6].u.operand;

        JITStubCall stubCall(this, cti_op_construct_JSConstruct);
        stubCall.addArgument(regT1, regT0);
        stubCall.addArgument(Imm32(0)); // FIXME: Remove this unused JITStub argument.
        stubCall.addArgument(Imm32(0)); // FIXME: Remove this unused JITStub argument.
        stubCall.addArgument(proto);
        stubCall.call(thisRegister);

        emitLoad(callee, regT1, regT0);
    }

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    emitStore(registerOffset + RegisterFile::OptionalCalleeArguments, JSValue());
    emitStore(registerOffset + RegisterFile::Callee, regT1, regT0);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_data) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT1); // newScopeChain
    store32(Imm32(argCount), Address(callFrameRegister, (registerOffset + RegisterFile::ArgumentCount) * static_cast<int>(sizeof(Register))));
    storePtr(callFrameRegister, Address(callFrameRegister, (registerOffset + RegisterFile::CallerFrame) * static_cast<int>(sizeof(Register))));
    storePtr(regT1, Address(callFrameRegister, (registerOffset + RegisterFile::ScopeChain) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * sizeof(Register)), callFrameRegister);

    // Call to the callee
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall();
    
    if (opcodeID == op_call_eval)
        wasEval.link(this);

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + opcodeLengths[opcodeID], dst, regT1, regT0);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex, OpcodeID opcodeID)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);

    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Fast check for JS function.
    Jump callLinkFailNotObject = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));
    Jump callLinkFailNotJSFunction = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        JITStubCall(this, cti_op_construct_JSConstruct).call(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitLoad(callee, regT1, regT0);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation = emitNakedCall(m_globalData->jitStubs.ctiVirtualCallLink());

    // Put the return value in dst.
    emitStore(dst, regT1, regT0);;
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
    JITStubCall(this, opcodeID == op_construct ? cti_op_construct_NotJSConstruct : cti_op_call_NotJSFunction).call();

    emitStore(dst, regT1, regT0);;
    sampleCodeBlock(m_codeBlock);
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_CALL)

#else // USE(JSVALUE32_64)

void JIT::compileOpCallInitializeCallFrame()
{
    store32(regT1, Address(callFrameRegister, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_data) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT1); // newScopeChain

    storePtr(ImmPtr(JSValue::encode(JSValue())), Address(callFrameRegister, RegisterFile::OptionalCalleeArguments * static_cast<int>(sizeof(Register))));
    storePtr(regT0, Address(callFrameRegister, RegisterFile::Callee * static_cast<int>(sizeof(Register))));
    storePtr(regT1, Address(callFrameRegister, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register))));
}

void JIT::compileOpCallSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutJITStubArg(regT0, 0);
    emitPutJITStubArgConstant(argCount, 2);
    emitPutJITStubArgConstant(registerOffset, 1);
}
          
void JIT::compileOpCallVarargsSetupArgs(Instruction* instruction)
{
    int registerOffset = instruction[4].u.operand;
    
    // ecx holds func
    emitPutJITStubArg(regT0, 0);
    emitPutJITStubArg(regT1, 2);
    addPtr(Imm32(registerOffset), regT1, regT2);
    emitPutJITStubArg(regT2, 1);
}

void JIT::compileOpConstructSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;
    int proto = instruction[5].u.operand;
    int thisRegister = instruction[6].u.operand;

    // ecx holds func
    emitPutJITStubArg(regT0, 0);
    emitPutJITStubArgConstant(registerOffset, 1);
    emitPutJITStubArgConstant(argCount, 2);
    emitPutJITStubArgFromVirtualRegister(proto, 3, regT2);
    emitPutJITStubArgConstant(thisRegister, 4);
}

void JIT::compileOpCallVarargs(Instruction* instruction)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCountRegister = instruction[3].u.operand;

    emitGetVirtualRegister(argCountRegister, regT1);
    emitGetVirtualRegister(callee, regT0);
    compileOpCallVarargsSetupArgs(instruction);

    // Check for JSFunctions.
    emitJumpSlowCaseIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr)));

    // Speculatively roll the callframe, assuming argCount will match the arity.
    mul32(Imm32(sizeof(Register)), regT2, regT2);
    intptr_t offset = (intptr_t)sizeof(Register) * (intptr_t)RegisterFile::CallerFrame;
    addPtr(Imm32((int32_t)offset), regT2, regT3);
    addPtr(callFrameRegister, regT3);
    storePtr(callFrameRegister, regT3);
    addPtr(regT2, callFrameRegister);
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
    JITStubCall stubCall(this, cti_op_call_NotJSFunction);
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
        JITStubCall stubCall(this, cti_op_call_eval);
        stubCall.addArgument(callee, regT0);
        stubCall.addArgument(JIT::Imm32(registerOffset));
        stubCall.addArgument(JIT::Imm32(argCount));
        stubCall.call();
        wasEval = branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(JSValue())));
    }

    emitGetVirtualRegister(callee, regT0);
    // The arguments have been set up on the hot path for op_call_eval
    if (opcodeID == op_call)
        compileOpCallSetupArgs(instruction);
    else if (opcodeID == op_construct)
        compileOpConstructSetupArgs(instruction);

    // Check for JSFunctions.
    emitJumpSlowCaseIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr)));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        JITStubCall(this, cti_op_construct_JSConstruct).call(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT0);
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
    JITStubCall stubCall(this, opcodeID == op_construct ? cti_op_construct_NotJSConstruct : cti_op_call_NotJSFunction);
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
        JITStubCall stubCall(this, cti_op_call_eval);
        stubCall.addArgument(callee, regT0);
        stubCall.addArgument(JIT::Imm32(registerOffset));
        stubCall.addArgument(JIT::Imm32(argCount));
        stubCall.call();
        wasEval = branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(JSValue())));
    }

    // This plants a check for a cached JSFunction value, so we can plant a fast link to the callee.
    // This deliberately leaves the callee in ecx, used when setting up the stack frame below
    emitGetVirtualRegister(callee, regT0);
    DataLabelPtr addressOfLinkedFunctionCheck;

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceOpCall);

    Jump jumpToSlow = branchPtrWithPatch(NotEqual, regT0, addressOfLinkedFunctionCheck, ImmPtr(JSValue::encode(JSValue())));

    END_UNINTERRUPTED_SEQUENCE(sequenceOpCall);

    addSlowCase(jumpToSlow);
    ASSERT_JIT_OFFSET(differenceBetween(addressOfLinkedFunctionCheck, jumpToSlow), patchOffsetOpCallCompareToJump);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;

    // The following is the fast case, only used whan a callee can be linked.

    // In the case of OpConstruct, call out to a cti_ function to create the new object.
    if (opcodeID == op_construct) {
        int proto = instruction[5].u.operand;
        int thisRegister = instruction[6].u.operand;

        emitPutJITStubArg(regT0, 0);
        emitPutJITStubArgFromVirtualRegister(proto, 3, regT2);
        JITStubCall stubCall(this, cti_op_construct_JSConstruct);
        stubCall.call(thisRegister);
        emitGetVirtualRegister(callee, regT0);
    }

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    storePtr(ImmPtr(JSValue::encode(JSValue())), Address(callFrameRegister, (registerOffset + RegisterFile::OptionalCalleeArguments) * static_cast<int>(sizeof(Register))));
    storePtr(regT0, Address(callFrameRegister, (registerOffset + RegisterFile::Callee) * static_cast<int>(sizeof(Register))));
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_data) + OBJECT_OFFSETOF(ScopeChain, m_node)), regT1); // newScopeChain
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
    Jump callLinkFailNotObject = emitJumpIfNotJSCell(regT0);
    Jump callLinkFailNotJSFunction = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsFunctionVPtr));

    // First, in the case of a construct, allocate the new object.
    if (opcodeID == op_construct) {
        JITStubCall(this, cti_op_construct_JSConstruct).call(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
        emitGetVirtualRegister(callee, regT0);
    }

    // Speculatively roll the callframe, assuming argCount will match the arity.
    storePtr(callFrameRegister, Address(callFrameRegister, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register))));
    addPtr(Imm32(registerOffset * static_cast<int>(sizeof(Register))), callFrameRegister);
    move(Imm32(argCount), regT1);

    move(regT0, regT2);

    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation = emitNakedCall(m_globalData->jitStubs.ctiVirtualCallLink());

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
    JITStubCall(this, opcodeID == op_construct ? cti_op_construct_NotJSConstruct : cti_op_call_NotJSFunction).call();

    emitPutVirtualRegister(dst);
    sampleCodeBlock(m_codeBlock);
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_CALL) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_CALL)

#endif // USE(JSVALUE32_64)

} // namespace JSC

#endif // ENABLE(JIT)
