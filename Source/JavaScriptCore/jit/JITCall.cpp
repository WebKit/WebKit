/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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
#if USE(JSVALUE64)
#include "JIT.h"

#include "Arguments.h"
#include "CodeBlock.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "RepatchBuffer.h"
#include "ResultType.h"
#include "SamplingTool.h"
#include "StackAlignment.h"
#include "ThunkGenerators.h"
#include <wtf/StringPrintStream.h>


namespace JSC {

void JIT::emitPutCallResult(Instruction* instruction)
{
    int dst = instruction[1].u.operand;
    emitValueProfilingSite();
    emitPutVirtualRegister(dst);
}

void JIT::compileLoadVarargs(Instruction* instruction)
{
    int thisValue = instruction[3].u.operand;
    int arguments = instruction[4].u.operand;
    int firstFreeRegister = instruction[5].u.operand;
    int firstVarArgOffset = instruction[6].u.operand;

    JumpList slowCase;
    JumpList end;
    bool canOptimize = m_codeBlock->usesArguments()
        && arguments == m_codeBlock->argumentsRegister().offset()
        && !m_codeBlock->symbolTable()->slowArguments();

    if (canOptimize) {
        emitGetVirtualRegister(arguments, regT0);
        slowCase.append(branch64(NotEqual, regT0, TrustedImm64(JSValue::encode(JSValue()))));

        emitGetFromCallFrameHeader32(JSStack::ArgumentCount, regT0);
        if (firstVarArgOffset) {
            Jump sufficientArguments = branch32(GreaterThan, regT0, TrustedImm32(firstVarArgOffset + 1));
            move(TrustedImm32(1), regT0);
            Jump endVarArgs = jump();
            sufficientArguments.link(this);
            sub32(TrustedImm32(firstVarArgOffset), regT0);
            endVarArgs.link(this);
        }
        slowCase.append(branch32(Above, regT0, TrustedImm32(Arguments::MaxArguments + 1)));
        // regT0: argumentCountIncludingThis
        move(regT0, regT1);
        add64(TrustedImm32(-firstFreeRegister + JSStack::CallFrameHeaderSize), regT1);
        // regT1 now has the required frame size in Register units
        // Round regT1 to next multiple of stackAlignmentRegisters()
        add64(TrustedImm32(stackAlignmentRegisters() - 1), regT1);
        and64(TrustedImm32(~(stackAlignmentRegisters() - 1)), regT1);

        neg64(regT1);
        lshift64(TrustedImm32(3), regT1);
        addPtr(callFrameRegister, regT1);
        // regT1: newCallFrame

        slowCase.append(branchPtr(Above, AbsoluteAddress(m_vm->addressOfStackLimit()), regT1));

        // Initialize ArgumentCount.
        store32(regT0, Address(regT1, JSStack::ArgumentCount * static_cast<int>(sizeof(Register)) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));

        // Initialize 'this'.
        emitGetVirtualRegister(thisValue, regT2);
        store64(regT2, Address(regT1, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));

        // Copy arguments.
        signExtend32ToPtr(regT0, regT0);
        end.append(branchSub64(Zero, TrustedImm32(1), regT0));
        // regT0: argumentCount

        Label copyLoop = label();
        load64(BaseIndex(callFrameRegister, regT0, TimesEight, (CallFrame::thisArgumentOffset() + firstVarArgOffset) * static_cast<int>(sizeof(Register))), regT2);
        store64(regT2, BaseIndex(regT1, regT0, TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));
        branchSub64(NonZero, TrustedImm32(1), regT0).linkTo(copyLoop, this);

        end.append(jump());
    }

    if (canOptimize)
        slowCase.link(this);

    emitGetVirtualRegister(arguments, regT1);
    callOperation(operationSizeFrameForVarargs, regT1, firstFreeRegister, firstVarArgOffset);
    move(returnValueGPR, stackPointerRegister);
    emitGetVirtualRegister(thisValue, regT1);
    emitGetVirtualRegister(arguments, regT2);
    callOperation(operationLoadVarargs, returnValueGPR, regT1, regT2, firstVarArgOffset);
    move(returnValueGPR, regT1);

    if (canOptimize)
        end.link(this);
    
    addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), regT1, stackPointerRegister);
}

void JIT::compileCallEval(Instruction* instruction)
{
    addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, regT1);
    callOperationNoExceptionCheck(operationCallEval, regT1);

    Jump noException = emitExceptionCheck(InvertedExceptionCheck);
    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);    
    exceptionCheck(jump());

    noException.link(this);
    addSlowCase(branch64(Equal, regT0, TrustedImm64(JSValue::encode(JSValue()))));

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
    emitPutCallResult(instruction);
}

void JIT::compileCallEvalSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    load64(Address(stackPointerRegister, sizeof(Register) * JSStack::Callee - sizeof(CallerFrameAndPC)), regT0);
    emitNakedCall(m_vm->getCTIStub(virtualCallThunkGenerator).code());
    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
    emitPutCallResult(instruction);
}

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned callLinkInfoIndex)
{
    int callee = instruction[2].u.operand;

    /* Caller always:
        - Updates callFrameRegister to callee callFrame.
        - Initializes ArgumentCount; CallerFrame; Callee.

       For a JS call:
        - Caller initializes ScopeChain.
        - Callee initializes ReturnPC; CodeBlock.
        - Callee restores callFrameRegister before return.

       For a non-JS call:
        - Caller initializes ScopeChain; ReturnPC; CodeBlock.
        - Caller restores callFrameRegister after return.
    */
    COMPILE_ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_construct), call_and_construct_opcodes_must_be_same_length);
    COMPILE_ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_call_varargs), call_and_call_varargs_opcodes_must_be_same_length);
    if (opcodeID == op_call_varargs)
        compileLoadVarargs(instruction);
    else {
        int argCount = instruction[3].u.operand;
        int registerOffset = -instruction[4].u.operand;

        if (opcodeID == op_call && shouldEmitProfiling()) {
            emitGetVirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0), regT0);
            Jump done = emitJumpIfNotJSCell(regT0);
            load32(Address(regT0, JSCell::structureIDOffset()), regT0);
            store32(regT0, instruction[OPCODE_LENGTH(op_call) - 2].u.arrayProfile->addressOfLastSeenStructureID());
            done.link(this);
        }
    
        addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);
        store32(TrustedImm32(argCount), Address(stackPointerRegister, JSStack::ArgumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
    } // SP holds newCallFrame + sizeof(CallerFrameAndPC), with ArgumentCount initialized.
    
    uint32_t bytecodeOffset = instruction - m_codeBlock->instructions().begin();
    uint32_t locationBits = CallFrame::Location::encodeAsBytecodeOffset(bytecodeOffset);
    store32(TrustedImm32(locationBits), Address(callFrameRegister, JSStack::ArgumentCount * static_cast<int>(sizeof(Register)) + TagOffset));
    emitGetVirtualRegister(callee, regT0); // regT0 holds callee.

    store64(regT0, Address(stackPointerRegister, JSStack::Callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC)));

    if (opcodeID == op_call_eval) {
        compileCallEval(instruction);
        return;
    }

    DataLabelPtr addressOfLinkedFunctionCheck;
    Jump slowCase = branchPtrWithPatch(NotEqual, regT0, addressOfLinkedFunctionCheck, TrustedImmPtr(0));
    addSlowCase(slowCase);

    ASSERT(m_callStructureStubCompilationInfo.size() == callLinkInfoIndex);
    m_callStructureStubCompilationInfo.append(StructureStubCompilationInfo());
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;
    m_callStructureStubCompilationInfo[callLinkInfoIndex].callType = CallLinkInfo::callTypeFor(opcodeID);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].bytecodeIndex = m_bytecodeOffset;

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scope)), regT2);
    store64(regT2, Address(MacroAssembler::stackPointerRegister, JSStack::ScopeChain * sizeof(Register) - sizeof(CallerFrameAndPC)));

    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall();

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
    emitPutCallResult(instruction);
}

void JIT::compileOpCallSlowCase(OpcodeID opcodeID, Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex)
{
    if (opcodeID == op_call_eval) {
        compileCallEvalSlowCase(instruction, iter);
        return;
    }

    linkSlowCase(iter);

    ThunkGenerator generator = linkThunkGeneratorFor(
        opcodeID == op_construct ? CodeForConstruct : CodeForCall,
        RegisterPreservationNotRequired);
    
    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation = emitNakedCall(m_vm->getCTIStub(generator).code());

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
    emitPutCallResult(instruction);
}

void JIT::privateCompileClosureCall(CallLinkInfo* callLinkInfo, CodeBlock* calleeCodeBlock, Structure* expectedStructure, ExecutableBase* expectedExecutable, MacroAssemblerCodePtr codePtr)
{
    JumpList slowCases;

    slowCases.append(branchTestPtr(NonZero, regT0, tagMaskRegister));
    slowCases.append(branchStructure(NotEqual, Address(regT0, JSCell::structureIDOffset()), expectedStructure));
    slowCases.append(branchPtr(NotEqual, Address(regT0, JSFunction::offsetOfExecutable()), TrustedImmPtr(expectedExecutable)));
    
    loadPtr(Address(regT0, JSFunction::offsetOfScopeChain()), regT1);
    emitPutToCallFrameHeader(regT1, JSStack::ScopeChain);
    
    Call call = nearCall();
    Jump done = jump();
    
    slowCases.link(this);
    move(TrustedImmPtr(callLinkInfo->callReturnLocation.executableAddress()), regT2);
    restoreReturnAddressBeforeReturn(regT2);
    Jump slow = jump();
    
    LinkBuffer patchBuffer(*m_vm, this, m_codeBlock);
    
    patchBuffer.link(call, FunctionPtr(codePtr.executableAddress()));
    patchBuffer.link(done, callLinkInfo->hotPathOther.labelAtOffset(0));
    patchBuffer.link(slow, CodeLocationLabel(m_vm->getCTIStub(virtualCallThunkGenerator).code()));
    
    RefPtr<ClosureCallStubRoutine> stubRoutine = adoptRef(new ClosureCallStubRoutine(
        FINALIZE_CODE(
            patchBuffer,
            ("Baseline closure call stub for %s, return point %p, target %p (%s)",
                toCString(*m_codeBlock).data(),
                callLinkInfo->hotPathOther.labelAtOffset(0).executableAddress(),
                codePtr.executableAddress(),
                toCString(pointerDump(calleeCodeBlock)).data())),
        *m_vm, m_codeBlock->ownerExecutable(), expectedStructure, expectedExecutable,
        callLinkInfo->codeOrigin));
    
    RepatchBuffer repatchBuffer(m_codeBlock);
    
    repatchBuffer.replaceWithJump(
        RepatchBuffer::startOfBranchPtrWithPatchOnRegister(callLinkInfo->hotPathBegin),
        CodeLocationLabel(stubRoutine->code().code()));
    repatchBuffer.relink(callLinkInfo->callReturnLocation, m_vm->getCTIStub(virtualCallThunkGenerator).code());

    callLinkInfo->stub = stubRoutine.release();
}

void JIT::emit_op_call(Instruction* currentInstruction)
{
    compileOpCall(op_call, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_call_eval(Instruction* currentInstruction)
{
    compileOpCall(op_call_eval, currentInstruction, m_callLinkInfoIndex);
}

void JIT::emit_op_call_varargs(Instruction* currentInstruction)
{
    compileOpCall(op_call_varargs, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_construct(Instruction* currentInstruction)
{
    compileOpCall(op_construct, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_call(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(op_call, currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_call_eval(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(op_call_eval, currentInstruction, iter, m_callLinkInfoIndex);
}
 
void JIT::emitSlow_op_call_varargs(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(op_call_varargs, currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_construct(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(op_construct, currentInstruction, iter, m_callLinkInfoIndex++);
}

} // namespace JSC

#endif // USE(JSVALUE64)
#endif // ENABLE(JIT)
