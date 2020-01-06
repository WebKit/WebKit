/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "DFGCommon.h"
#include "FTLJITCode.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "ProfilerCompilation.h"
#include "ThunkGenerators.h"
#include "VirtualRegister.h"

namespace JSC { namespace FTL {

void link(State& state)
{
    using namespace DFG;
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;
    VM& vm = graph.m_vm;
    
    // B3 will create its own jump tables as needed.
    codeBlock->clearSwitchJumpTables();

    state.jitCode->common.requiredRegisterCountForExit = graph.requiredRegisterCountForExit();
    
    if (!graph.m_plan.inlineCallFrames()->isEmpty())
        state.jitCode->common.inlineCallFrames = graph.m_plan.inlineCallFrames();

    graph.registerFrozenValues();

    // Create the entrypoint. Note that we use this entrypoint totally differently
    // depending on whether we're doing OSR entry or not.
    CCallHelpers jit(codeBlock);
    
    std::unique_ptr<LinkBuffer> linkBuffer;

    CCallHelpers::Address frame = CCallHelpers::Address(
        CCallHelpers::stackPointerRegister, -static_cast<int32_t>(AssemblyHelpers::prologueStackPointerDelta()));
    
    Profiler::Compilation* compilation = graph.compilation();
    if (UNLIKELY(compilation)) {
        compilation->addDescription(
            Profiler::OriginStack(),
            toCString("Generated FTL JIT code for ", CodeBlockWithJITType(codeBlock, JITType::FTLJIT), ", instructions size = ", graph.m_codeBlock->instructionsSize(), ":\n"));
        
        graph.ensureSSADominators();
        graph.ensureSSANaturalLoops();
        
        const char* prefix = "    ";
        
        DumpContext dumpContext;
        StringPrintStream out;
        Node* lastNode = 0;
        for (size_t blockIndex = 0; blockIndex < graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = graph.block(blockIndex);
            if (!block)
                continue;
            
            graph.dumpBlockHeader(out, prefix, block, Graph::DumpLivePhisOnly, &dumpContext);
            compilation->addDescription(Profiler::OriginStack(), out.toCString());
            out.reset();
            
            for (size_t nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                Profiler::OriginStack stack;
                
                if (node->origin.semantic.isSet()) {
                    stack = Profiler::OriginStack(
                        *vm.m_perBytecodeProfiler, codeBlock, node->origin.semantic);
                }
                
                if (graph.dumpCodeOrigin(out, prefix, lastNode, node, &dumpContext)) {
                    compilation->addDescription(stack, out.toCString());
                    out.reset();
                }
                
                graph.dump(out, prefix, node, &dumpContext);
                compilation->addDescription(stack, out.toCString());
                out.reset();
                
                if (node->origin.semantic.isSet())
                    lastNode = node;
            }
        }
        
        dumpContext.dump(out, prefix);
        compilation->addDescription(Profiler::OriginStack(), out.toCString());
        out.reset();

        out.print("    Disassembly:\n");
        out.print("        <not implemented yet>\n");
        compilation->addDescription(Profiler::OriginStack(), out.toCString());
        out.reset();
        
        state.jitCode->common.compilation = compilation;
    }

    switch (graph.m_plan.mode()) {
    case FTLMode: {
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

            auto noException = jit.branch32(CCallHelpers::GreaterThanOrEqual, GPRInfo::returnValueGPR, CCallHelpers::TrustedImm32(0));
            jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
            jit.move(CCallHelpers::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
            jit.prepareCallOperation(vm);
            CCallHelpers::Call callLookupExceptionHandlerFromCallerFrame = jit.call(OperationPtrTag);
            jit.jumpToExceptionHandler(vm);
            noException.link(&jit);

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

            linkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, JITCompilationCanFail);
            if (linkBuffer->didFailToAllocate()) {
                state.allocationFailed = true;
                return;
            }
            linkBuffer->link(callArityCheck, FunctionPtr<OperationPtrTag>(codeBlock->isConstructor() ? operationConstructArityCheck : operationCallArityCheck));
            linkBuffer->link(callLookupExceptionHandlerFromCallerFrame, FunctionPtr<OperationPtrTag>(operationLookupExceptionHandlerFromCallerFrame));
            linkBuffer->link(callArityFixup, FunctionPtr<JITThunkPtrTag>(vm.getCTIStub(arityFixupGenerator).code()));
            linkBuffer->link(mainPathJumps, state.generatedFunction);
        }

        state.jitCode->initializeAddressForCall(state.generatedFunction);
        break;
    }
        
    case FTLForOSREntryMode: {
        // We jump to here straight from DFG code, after having boxed up all of the
        // values into the scratch buffer. Everything should be good to go - at this
        // point we've even done the stack check. Basically we just have to make the
        // call to the B3-generated code.
        CCallHelpers::Label start = jit.label();
        jit.emitFunctionEpilogue();
        jit.untagReturnAddress();
        CCallHelpers::Jump mainPathJump = jit.jump();
        
        linkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, JITCompilationCanFail);
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
    
    state.finalizer->entrypointLinkBuffer = WTFMove(linkBuffer);
    state.finalizer->function = state.generatedFunction;
    state.finalizer->jitCode = state.jitCode;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

