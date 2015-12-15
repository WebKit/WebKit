/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "FTLCompile.h"

#if ENABLE(FTL_JIT) && FTL_USES_B3

#include "B3Generate.h"
#include "B3ProcedureInlines.h"
#include "B3StackSlotValue.h"
#include "CodeBlockWithJITType.h"
#include "CCallHelpers.h"
#include "DFGCommon.h"
#include "DFGGraphSafepoint.h"
#include "DFGOperations.h"
#include "DataView.h"
#include "Disassembler.h"
#include "FTLExceptionHandlerManager.h"
#include "FTLExitThunkGenerator.h"
#include "FTLInlineCacheSize.h"
#include "FTLJITCode.h"
#include "FTLThunks.h"
#include "FTLUnwindInfo.h"
#include "JITSubGenerator.h"
#include "LinkBuffer.h"
#include "ScratchRegisterAllocator.h"

namespace JSC { namespace FTL {

using namespace DFG;

void compile(State& state, Safepoint::Result& safepointResult)
{
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;
    VM& vm = graph.m_vm;
    
    {
        GraphSafepoint safepoint(state.graph, safepointResult);

        B3::prepareForGeneration(*state.proc);
    }

    if (safepointResult.didGetCancelled())
        return;
    RELEASE_ASSERT(!state.graph.m_vm.heap.isCollecting());
    
    if (state.allocationFailed)
        return;
    
    std::unique_ptr<RegisterAtOffsetList> registerOffsets =
        std::make_unique<RegisterAtOffsetList>(state.proc->calleeSaveRegisters());
    if (shouldDumpDisassembly()) {
        dataLog("Unwind info for ", CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT), ":\n");
        dataLog("    ", *registerOffsets, "\n");
    }
    state.graph.m_codeBlock->setCalleeSaveRegisters(WTF::move(registerOffsets));
    ASSERT(!(state.proc->frameSize() % sizeof(EncodedJSValue)));
    state.jitCode->common.frameRegisterCount = state.proc->frameSize() / sizeof(EncodedJSValue);

    int localsOffset =
        state.capturedValue->offsetFromFP() / sizeof(EncodedJSValue) + graph.m_nextMachineLocal;
    for (unsigned i = graph.m_inlineVariableData.size(); i--;) {
        InlineCallFrame* inlineCallFrame = graph.m_inlineVariableData[i].inlineCallFrame;
        
        if (inlineCallFrame->argumentCountRegister.isValid())
            inlineCallFrame->argumentCountRegister += localsOffset;
        
        for (unsigned argument = inlineCallFrame->arguments.size(); argument-- > 1;) {
            inlineCallFrame->arguments[argument] =
                inlineCallFrame->arguments[argument].withLocalsOffset(localsOffset);
        }
        
        if (inlineCallFrame->isClosureCall) {
            inlineCallFrame->calleeRecovery =
                inlineCallFrame->calleeRecovery.withLocalsOffset(localsOffset);
        }

        if (graph.hasDebuggerEnabled())
            codeBlock->setScopeRegister(codeBlock->scopeRegister() + localsOffset);
    }
    for (OSRExitDescriptor& descriptor : state.jitCode->osrExitDescriptors) {
        for (unsigned i = descriptor.m_values.size(); i--;)
            descriptor.m_values[i] = descriptor.m_values[i].withLocalsOffset(localsOffset);
        for (ExitTimeObjectMaterialization* materialization : descriptor.m_materializations)
            materialization->accountForLocalsOffset(localsOffset);
    }

    CCallHelpers jit(&vm, codeBlock);
    B3::generate(*state.proc, jit);

    state.finalizer->b3CodeLinkBuffer = std::make_unique<LinkBuffer>(
        vm, jit, codeBlock, JITCompilationCanFail);
    if (state.finalizer->b3CodeLinkBuffer->didFailToAllocate()) {
        state.allocationFailed = true;
        return;
    }

    state.generatedFunction = bitwise_cast<GeneratedFunction>(
        state.finalizer->b3CodeLinkBuffer->entrypoint().executableAddress());
    state.jitCode->initializeB3Byproducts(state.proc->releaseByproducts());
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && FTL_USES_B3

