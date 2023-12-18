/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
#include "JITThunks.h"
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
    if (!graph.m_stringSearchTable8.isEmpty()) {
        FixedVector<std::unique_ptr<BoyerMooreHorspoolTable<uint8_t>>> tables(graph.m_stringSearchTable8.size());
        unsigned index = 0;
        for (auto& entry : graph.m_stringSearchTable8)
            tables[index++] = WTFMove(entry.value);
        state.jitCode->common.m_stringSearchTable8 = WTFMove(tables);
    }

    graph.registerFrozenValues();

    // Create the entrypoint. Note that we use this entrypoint totally differently
    // depending on whether we're doing OSR entry or not.
    CCallHelpers jit(codeBlock);
    
    std::unique_ptr<LinkBuffer> linkBuffer;

    CCallHelpers::Address frame = CCallHelpers::Address(
        CCallHelpers::stackPointerRegister, -static_cast<int32_t>(prologueStackPointerDelta()));
    
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

            unsigned numberOfParameters = codeBlock->numParameters();
            CCallHelpers::JumpList stackOverflow;
            jit.getArityPadding(vm, numberOfParameters, GPRInfo::regT1, GPRInfo::regT0, GPRInfo::regT2, GPRInfo::regT3, stackOverflow);

            jit.emitFunctionPrologue();
            jit.move(GPRInfo::regT0, GPRInfo::argumentGPR0);
            jit.nearCallThunk(CodeLocationLabel { vm.getCTIStub(CommonJITThunkID::ArityFixup).retaggedCode<NoPtrTag>() });
            jit.emitFunctionEpilogue();
            jit.untagReturnAddress();
            mainPathJumps.append(jit.jump());

            stackOverflow.link(&jit);
            jit.emitFunctionPrologue();
            jit.move(CCallHelpers::TrustedImmPtr(codeBlock), GPRInfo::argumentGPR0);
            jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);
            jit.callOperation<OperationPtrTag>(operationThrowStackOverflowError);
            jit.jumpThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleExceptionWithCallFrameRollback).retaggedCode<NoPtrTag>()));
            mainPathJumps.linkThunk(state.generatedFunction, &jit);

            linkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, LinkBuffer::Profile::FTL, JITCompilationCanFail);
            if (linkBuffer->didFailToAllocate()) {
                state.allocationFailed = true;
                return;
            }
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
        jit.jumpThunk(CodeLocationLabel { state.generatedFunction });
        
        linkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, LinkBuffer::Profile::FTL, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }

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
        state.jitCode->common.m_jumpReplacements = WTFMove(state.jumpReplacements);
    }

    state.finalizer->entrypointLinkBuffer = WTFMove(linkBuffer);
    state.finalizer->function = state.generatedFunction;
    state.finalizer->jitCode = state.jitCode;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

