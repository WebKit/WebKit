/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLLink.h"

#if ENABLE(FTL_JIT)

#include "CallFrameInlines.h"
#include "CodeBlockWithJITType.h"
#include "DFGCCallHelpers.h"
#include "DFGCommon.h"
#include "FTLJITCode.h"
#include "JITStubs.h"
#include "LinkBuffer.h"
#include <wtf/LLVMHeaders.h>

namespace JSC { namespace FTL {

using namespace DFG;

static void compileEntry(CCallHelpers& jit)
{
    jit.preserveReturnAddressAfterCall(GPRInfo::regT2);
    jit.emitPutToCallFrameHeader(GPRInfo::regT2, JSStack::ReturnPC);
    jit.emitPutImmediateToCallFrameHeader(jit.codeBlock(), JSStack::CodeBlock);
}

void link(State& state)
{
    CodeBlock* codeBlock = state.graph.m_codeBlock;
    
    // LLVM will create its own jump tables as needed.
    codeBlock->clearSwitchJumpTables();
    
    // Create the entrypoint. Note that we use this entrypoint totally differently
    // depending on whether we're doing OSR entry or not.
    // FIXME: Except for OSR entry, this is a total kludge - LLVM should just use our
    // calling convention.
    // https://bugs.webkit.org/show_bug.cgi?id=113621
    CCallHelpers jit(&state.graph.m_vm, codeBlock);
    
    OwnPtr<LinkBuffer> linkBuffer;
    CCallHelpers::Label arityCheck;
    switch (state.graph.m_plan.mode) {
    case FTLMode: {
        compileEntry(jit);
    
        // This part is only necessary for functions. We currently only compile functions.
        
        CCallHelpers::Label fromArityCheck = jit.label();
        
        // Plant a check that sufficient space is available in the JSStack.
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56291
        jit.addPtr(
            CCallHelpers::TrustedImm32(localToOperand(codeBlock->m_numCalleeRegisters) * sizeof(Register)),
            GPRInfo::callFrameRegister, GPRInfo::regT1);
        CCallHelpers::Jump stackCheck = jit.branchPtr(
            CCallHelpers::Above,
            CCallHelpers::AbsoluteAddress(state.graph.m_vm.interpreter->stack().addressOfEnd()),
            GPRInfo::regT1);
        CCallHelpers::Label fromStackCheck = jit.label();
        
        jit.setupArgumentsExecState();
        jit.move(
            CCallHelpers::TrustedImmPtr(reinterpret_cast<void*>(state.generatedFunction)),
            GPRInfo::nonArgGPR0);
        jit.call(GPRInfo::nonArgGPR0);
        jit.emitGetFromCallFrameHeaderPtr(JSStack::ReturnPC, GPRInfo::regT1);
        jit.emitGetFromCallFrameHeaderPtr(JSStack::CallerFrame, GPRInfo::callFrameRegister);
        jit.restoreReturnAddressBeforeReturn(GPRInfo::regT1);
        jit.ret();
        
        stackCheck.link(&jit);
        jit.move(CCallHelpers::stackPointerRegister, GPRInfo::argumentGPR0);
        jit.poke(
            GPRInfo::callFrameRegister,
            OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
        
        jit.store32(
            CCallHelpers::TrustedImm32(CallFrame::Location::encodeAsBytecodeOffset(0)),
            CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
        CCallHelpers::Call callStackCheck = jit.call();
        // FIXME: need to make this call register with exception handling somehow. This is
        // part of a bigger problem: FTL should be able to handle exceptions.
        // https://bugs.webkit.org/show_bug.cgi?id=113622
        jit.jump(fromStackCheck);
        
        arityCheck = jit.label();
        compileEntry(jit);
        jit.load32(
            CCallHelpers::payloadFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)),
            GPRInfo::regT1);
        jit.branch32(
            CCallHelpers::AboveOrEqual, GPRInfo::regT1,
            CCallHelpers::TrustedImm32(codeBlock->numParameters()))
            .linkTo(fromArityCheck, &jit);
        jit.move(CCallHelpers::stackPointerRegister, GPRInfo::argumentGPR0);
        jit.poke(
            GPRInfo::callFrameRegister,
            OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
        jit.store32(
            CCallHelpers::TrustedImm32(CallFrame::Location::encodeAsBytecodeOffset(0)),
            CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
        CCallHelpers::Call callArityCheck = jit.call();
        // FIXME: need to make this call register with exception handling somehow. This is
        // part of a bigger problem: FTL should be able to handle exceptions.
        // https://bugs.webkit.org/show_bug.cgi?id=113622
        jit.branchTest32(CCallHelpers::Zero, GPRInfo::regT0).linkTo(fromArityCheck, &jit);
        CCallHelpers::Call callArityFixup = jit.call();
        jit.jump(fromArityCheck);
        
        linkBuffer = adoptPtr(new LinkBuffer(state.graph.m_vm, &jit, codeBlock, JITCompilationMustSucceed));
        linkBuffer->link(callStackCheck, cti_stack_check);
        linkBuffer->link(callArityCheck, codeBlock->m_isConstructor ? cti_op_construct_arityCheck : cti_op_call_arityCheck);
        linkBuffer->link(callArityFixup, FunctionPtr((state.graph.m_vm.getCTIStub(arityFixup)).code().executableAddress()));
        break;
    }
        
    case FTLForOSREntryMode: {
        // We jump to here straight from DFG code, after having boxed up all of the
        // values into the scratch buffer. Everything should be good to go - at this
        // point we've even done the stack check. Basically we just have to make the
        // call to the LLVM-generated code.
        jit.setupArgumentsExecState();
        jit.move(
            CCallHelpers::TrustedImmPtr(reinterpret_cast<void*>(state.generatedFunction)),
            GPRInfo::nonArgGPR0);
        jit.call(GPRInfo::nonArgGPR0);
        jit.emitGetFromCallFrameHeaderPtr(JSStack::ReturnPC, GPRInfo::regT1);
        jit.emitGetFromCallFrameHeaderPtr(JSStack::CallerFrame, GPRInfo::callFrameRegister);
        jit.restoreReturnAddressBeforeReturn(GPRInfo::regT1);
        jit.ret();
        
        linkBuffer = adoptPtr(new LinkBuffer(
            state.graph.m_vm, &jit, codeBlock, JITCompilationMustSucceed));
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    state.finalizer->initializeEntrypointLinkBuffer(linkBuffer.release());
    state.finalizer->initializeFunction(state.generatedFunction);
    state.finalizer->initializeArityCheck(arityCheck);
    state.finalizer->initializeJITCode(state.jitCode);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

