/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "CCallHelpers.h"
#include "CodeBlockWithJITType.h"
#include "FTLJITCode.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "ProfilerCompilation.h"
#include "ThunkGenerators.h"

namespace JSC { namespace FTL {

void link(State& state)
{
    using namespace DFG;
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;
    VM& vm = graph.m_vm;
    
    state.jitCode->common.requiredRegisterCountForExit = graph.requiredRegisterCountForExit();
    
    if (!graph.m_plan.inlineCallFrames()->isEmpty())
        state.jitCode->common.inlineCallFrames = graph.m_plan.inlineCallFrames();

    graph.registerFrozenValues();

#if ASSERT_ENABLED
    {
        ConcurrentJSLocker locker(codeBlock->m_lock);
        ASSERT(codeBlock->ensureJITData(locker).m_stringSwitchJumpTables.isEmpty());
        ASSERT(codeBlock->ensureJITData(locker).m_switchJumpTables.isEmpty());
    }
#endif

    // Create the entrypoint. Note that we use this entrypoint totally differently
    // depending on whether we're doing OSR entry or not.
    CCallHelpers jit(codeBlock);
    
    std::unique_ptr<LinkBuffer> linkBuffer;

    CCallHelpers::Address frame = CCallHelpers::Address(
        CCallHelpers::stackPointerRegister, -static_cast<int32_t>(AssemblyHelpers::prologueStackPointerDelta()));
    
    switch (graph.m_plan.mode()) {
    case JITCompilationMode::FTL: {
        bool requiresArityFixup = codeBlock->numParameters() != 1;
        if (codeBlock->codeType() == FunctionCode && requiresArityFixup) {
            CCallHelpers::JumpList mainPathJumps;
    
            jit.load32(
                frame.withOffset(sizeof(Register) * CallFrameSlot::argumentCountIncludingThis),
                GPRInfo::regT1);
            mainPathJumps.append(jit.branch32(
                                     CCallHelpers::AboveOrEqual, GPRInfo::regT1,
                                     CCallHelpers::TrustedImm32(codeBlock->numParameters())));
            jit.emitFunctionPrologue();
            jit.move(CCallHelpers::TrustedImmPtr(codeBlock->globalObject()), GPRInfo::argumentGPR0);
            jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);
            CCallHelpers::Call callArityCheck = jit.call(OperationPtrTag);

#if ENABLE(EXTRA_CTI_THUNKS)
            auto jumpToExceptionHandler = jit.branch32(CCallHelpers::LessThan, GPRInfo::returnValueGPR, CCallHelpers::TrustedImm32(0));
#else
            auto noException = jit.branch32(CCallHelpers::GreaterThanOrEqual, GPRInfo::returnValueGPR, CCallHelpers::TrustedImm32(0));
            jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
            jit.move(CCallHelpers::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
            jit.prepareCallOperation(vm);
            CCallHelpers::Call callLookupExceptionHandlerFromCallerFrame = jit.call(OperationPtrTag);
            jit.jumpToExceptionHandler(vm);
            noException.link(&jit);
#endif // ENABLE(EXTRA_CTI_THUNKS)

            if (ASSERT_ENABLED) {
                jit.load64(vm.addressOfException(), GPRInfo::regT1);
                jit.jitAssertIsNull(GPRInfo::regT1);
            }

            jit.move(GPRInfo::returnValueGPR, GPRInfo::argumentGPR0);
            jit.emitFunctionEpilogue();
            jit.untagReturnAddress();
            mainPathJumps.append(jit.branchTest32(CCallHelpers::Zero, GPRInfo::argumentGPR0));
            jit.emitFunctionPrologue();
            CCallHelpers::Call callArityFixup = jit.nearCall();
            jit.emitFunctionEpilogue();
            jit.untagReturnAddress();
            mainPathJumps.append(jit.jump());

            linkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, LinkBuffer::Profile::FTL, JITCompilationCanFail);
            if (linkBuffer->didFailToAllocate()) {
                state.allocationFailed = true;
                return;
            }
            linkBuffer->link(callArityCheck, FunctionPtr<OperationPtrTag>(codeBlock->isConstructor() ? operationConstructArityCheck : operationCallArityCheck));
#if ENABLE(EXTRA_CTI_THUNKS)
            linkBuffer->link(jumpToExceptionHandler, CodeLocationLabel(vm.getCTIStub(handleExceptionWithCallFrameRollbackGenerator).retaggedCode<NoPtrTag>()));
#else
            linkBuffer->link(callLookupExceptionHandlerFromCallerFrame, FunctionPtr<OperationPtrTag>(operationLookupExceptionHandlerFromCallerFrame));
#endif
            linkBuffer->link(callArityFixup, FunctionPtr<JITThunkPtrTag>(vm.getCTIStub(arityFixupGenerator).code()));
            linkBuffer->link(mainPathJumps, state.generatedFunction);
        }

        state.jitCode->initializeAddressForCall(state.generatedFunction);
        break;
    }
        
    case JITCompilationMode::FTLForOSREntry: {
        // We jump to here straight from DFG code, after having boxed up all of the
        // values into the scratch buffer. Everything should be good to go - at this
        // point we've even done the stack check. Basically we just have to make the
        // call to the B3-generated code.
        CCallHelpers::Label start = jit.label();
        jit.emitFunctionEpilogue();
        jit.untagReturnAddress();
        CCallHelpers::Jump mainPathJump = jit.jump();
        
        linkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, LinkBuffer::Profile::FTL, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        linkBuffer->link(mainPathJump, state.generatedFunction);

        state.jitCode->initializeAddressForCall(linkBuffer->locationOf<JSEntryPtrTag>(start));
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    {
        bool dumpDisassembly = shouldDumpDisassembly() || Options::asyncDisassembly();

        MacroAssemblerCodeRef<JSEntryPtrTag> b3CodeRef =
            FINALIZE_CODE_IF(dumpDisassembly, *state.finalizer->b3CodeLinkBuffer, JSEntryPtrTag,
                "FTL B3 code for %s", toCString(CodeBlockWithJITType(codeBlock, JITType::FTLJIT)).data());

        MacroAssemblerCodeRef<JSEntryPtrTag> arityCheckCodeRef = linkBuffer
            ? FINALIZE_CODE_IF(dumpDisassembly, *linkBuffer, JSEntryPtrTag,
                "FTL entrypoint thunk for %s with B3 generated code at %p", toCString(CodeBlockWithJITType(codeBlock, JITType::FTLJIT)).data(), state.generatedFunction)
            : MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(b3CodeRef.code());

        state.jitCode->initializeB3Code(b3CodeRef);
        state.jitCode->initializeArityCheckEntrypoint(arityCheckCodeRef);
    }

    state.finalizer->entrypointLinkBuffer = WTFMove(linkBuffer);
    state.finalizer->function = state.generatedFunction;
    state.finalizer->jitCode = state.jitCode;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

