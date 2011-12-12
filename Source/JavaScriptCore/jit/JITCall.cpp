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
#if USE(JSVALUE64)
#include "JIT.h"

#include "Arguments.h"
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

void JIT::emit_op_call_put_result(Instruction* instruction)
{
    int dst = instruction[1].u.operand;
    emitValueProfilingSite(FirstProfilingSite);
    emitPutVirtualRegister(dst);
}

void JIT::compileLoadVarargs(Instruction* instruction)
{
    int thisValue = instruction[2].u.operand;
    int arguments = instruction[3].u.operand;
    int firstFreeRegister = instruction[4].u.operand;

    killLastResultRegister();

    JumpList slowCase;
    JumpList end;
    if (m_codeBlock->usesArguments() && arguments == m_codeBlock->argumentsRegister()) {
        emitGetVirtualRegister(arguments, regT0);
        slowCase.append(branchPtr(NotEqual, regT0, TrustedImmPtr(JSValue::encode(JSValue()))));

        emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);
        slowCase.append(branch32(Above, regT0, TrustedImm32(Arguments::MaxArguments + 1)));
        // regT0: argumentCountIncludingThis

        move(regT0, regT1);
        add32(TrustedImm32(firstFreeRegister + RegisterFile::CallFrameHeaderSize), regT1);
        lshift32(TrustedImm32(3), regT1);
        addPtr(callFrameRegister, regT1);
        // regT1: newCallFrame

        slowCase.append(branchPtr(Below, AbsoluteAddress(m_globalData->interpreter->registerFile().addressOfEnd()), regT1));

        // Initialize ArgumentCount.
        emitFastArithReTagImmediate(regT0, regT2);
        storePtr(regT2, Address(regT1, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));

        // Initialize 'this'.
        emitGetVirtualRegister(thisValue, regT2);
        storePtr(regT2, Address(regT1, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));

        // Copy arguments.
        neg32(regT0);
        signExtend32ToPtr(regT0, regT0);
        end.append(branchAddPtr(Zero, Imm32(1), regT0));
        // regT0: -argumentCount

        Label copyLoop = label();
        loadPtr(BaseIndex(callFrameRegister, regT0, TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))), regT2);
        storePtr(regT2, BaseIndex(regT1, regT0, TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));
        branchAddPtr(NonZero, Imm32(1), regT0).linkTo(copyLoop, this);

        end.append(jump());
    }

    if (m_codeBlock->usesArguments() && arguments == m_codeBlock->argumentsRegister())
        slowCase.link(this);

    JITStubCall stubCall(this, cti_op_load_varargs);
    stubCall.addArgument(thisValue, regT0);
    stubCall.addArgument(arguments, regT0);
    stubCall.addArgument(Imm32(firstFreeRegister));
    stubCall.call(regT1);

    if (m_codeBlock->usesArguments() && arguments == m_codeBlock->argumentsRegister())
        end.link(this);
}

void JIT::compileCallEval()
{
    JITStubCall stubCall(this, cti_op_call_eval); // Initializes ScopeChain; ReturnPC; CodeBlock.
    stubCall.call();
    addSlowCase(branchPtr(Equal, regT0, TrustedImmPtr(JSValue::encode(JSValue()))));
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileCallEvalSlowCase(Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT0);
    emitNakedCall(m_globalData->jitStubs->ctiVirtualCall());

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned callLinkInfoIndex)
{
    int callee = instruction[1].u.operand;

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

    if (opcodeID == op_call_varargs)
        compileLoadVarargs(instruction);
    else {
        int argCount = instruction[2].u.operand;
        int registerOffset = instruction[3].u.operand;

        addPtr(TrustedImm32(registerOffset * sizeof(Register)), callFrameRegister, regT1);
        storePtr(TrustedImmPtr(JSValue::encode(jsNumber(argCount))), Address(regT1, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register))));
    } // regT1 holds newCallFrame with ArgumentCount initialized.
    emitGetVirtualRegister(callee, regT0); // regT0 holds callee.

    storePtr(callFrameRegister, Address(regT1, RegisterFile::CallerFrame * static_cast<int>(sizeof(Register))));
    storePtr(regT0, Address(regT1, RegisterFile::Callee * static_cast<int>(sizeof(Register))));
    move(regT1, callFrameRegister);

    if (opcodeID == op_call_eval) {
        compileCallEval();
        return;
    }

    DataLabelPtr addressOfLinkedFunctionCheck;
    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceOpCall);
    Jump slowCase = branchPtrWithPatch(NotEqual, regT0, addressOfLinkedFunctionCheck, TrustedImmPtr(JSValue::encode(JSValue())));
    END_UNINTERRUPTED_SEQUENCE(sequenceOpCall);
    addSlowCase(slowCase);

    ASSERT_JIT_OFFSET(differenceBetween(addressOfLinkedFunctionCheck, slowCase), patchOffsetOpCallCompareToJump);
    ASSERT(m_callStructureStubCompilationInfo.size() == callLinkInfoIndex);
    m_callStructureStubCompilationInfo.append(StructureStubCompilationInfo());
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;
    m_callStructureStubCompilationInfo[callLinkInfoIndex].callType = CallLinkInfo::callTypeFor(opcodeID);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].bytecodeIndex = m_bytecodeOffset;

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ScopeChain);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall();

    sampleCodeBlock(m_codeBlock);
}

void JIT::compileOpCallSlowCase(OpcodeID opcodeID, Instruction*, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex)
{
    if (opcodeID == op_call_eval) {
        compileCallEvalSlowCase(iter);
        return;
    }

    linkSlowCase(iter);
    
    m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation = emitNakedCall(opcodeID == op_construct ? m_globalData->jitStubs->ctiVirtualConstructLink() : m_globalData->jitStubs->ctiVirtualCallLink());

    sampleCodeBlock(m_codeBlock);
}

} // namespace JSC

#endif // USE(JSVALUE64)
#endif // ENABLE(JIT)
