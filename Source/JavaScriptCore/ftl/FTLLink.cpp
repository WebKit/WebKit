/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "LLVMAPI.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "ProfilerCompilation.h"
#include "VirtualRegister.h"

namespace JSC { namespace FTL {

using namespace DFG;

void link(State& state)
{
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;
    VM& vm = graph.m_vm;
    
    // LLVM will create its own jump tables as needed.
    codeBlock->clearSwitchJumpTables();

#if !FTL_USES_B3
    // What LLVM's stackmaps call stackSizeForLocals and what we call frameRegisterCount have a simple
    // relationship, though it's not obvious from reading the code. The easiest way to understand them
    // is to look at stackOffset, i.e. what you have to add to FP to get SP. For LLVM that is just:
    //
    //     stackOffset == -state.jitCode->stackmaps.stackSizeForLocals()
    //
    // The way we define frameRegisterCount is that it satisfies this equality:
    //
    //     stackOffset == virtualRegisterForLocal(frameRegisterCount - 1).offset() * sizeof(Register)
    //
    // We can simplify this when we apply virtualRegisterForLocal():
    //
    //     stackOffset == (-1 - (frameRegisterCount - 1)) * sizeof(Register)
    //     stackOffset == (-1 - frameRegisterCount + 1) * sizeof(Register)
    //     stackOffset == -frameRegisterCount * sizeof(Register)
    //
    // Therefore we just have:
    //
    //     frameRegisterCount == -stackOffset / sizeof(Register)
    //
    // If we substitute what we have above, we get:
    //
    //     frameRegisterCount == -(-state.jitCode->stackmaps.stackSizeForLocals()) / sizeof(Register)
    //     frameRegisterCount == state.jitCode->stackmaps.stackSizeForLocals() / sizeof(Register)
    state.jitCode->common.frameRegisterCount = state.jitCode->stackmaps.stackSizeForLocals() / sizeof(void*);
#endif
    
    state.jitCode->common.requiredRegisterCountForExit = graph.requiredRegisterCountForExit();
    
    if (!graph.m_plan.inlineCallFrames->isEmpty())
        state.jitCode->common.inlineCallFrames = graph.m_plan.inlineCallFrames;
    
    graph.registerFrozenValues();

    // Create the entrypoint. Note that we use this entrypoint totally differently
    // depending on whether we're doing OSR entry or not.
    CCallHelpers jit(&vm, codeBlock);
    
    std::unique_ptr<LinkBuffer> linkBuffer;

    CCallHelpers::Address frame = CCallHelpers::Address(
        CCallHelpers::stackPointerRegister, -static_cast<int32_t>(AssemblyHelpers::prologueStackPointerDelta()));
    
    if (Profiler::Compilation* compilation = graph.compilation()) {
        compilation->addDescription(
            Profiler::OriginStack(),
            toCString("Generated FTL JIT code for ", CodeBlockWithJITType(codeBlock, JITCode::FTLJIT), ", instruction count = ", graph.m_codeBlock->instructionCount(), ":\n"));
        
        graph.ensureDominators();
        graph.ensureNaturalLoops();
        
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
#if FTL_USES_B3
        out.print("        <not implemented yet>\n");
#else
        for (unsigned i = 0; i < state.jitCode->handles().size(); ++i) {
            if (state.codeSectionNames[i] != SECTION_NAME("text"))
                continue;
            
            ExecutableMemoryHandle* handle = state.jitCode->handles()[i].get();
            disassemble(
                MacroAssemblerCodePtr(handle->start()), handle->sizeInBytes(),
                "      ", out, LLVMSubset);
        }
#endif
        compilation->addDescription(Profiler::OriginStack(), out.toCString());
        out.reset();
        
        state.jitCode->common.compilation = compilation;
    }
    
    switch (graph.m_plan.mode) {
    case FTLMode: {
        CCallHelpers::JumpList mainPathJumps;
    
        jit.load32(
            frame.withOffset(sizeof(Register) * JSStack::ArgumentCount),
            GPRInfo::regT1);
        mainPathJumps.append(jit.branch32(
            CCallHelpers::AboveOrEqual, GPRInfo::regT1,
            CCallHelpers::TrustedImm32(codeBlock->numParameters())));
        jit.emitFunctionPrologue();
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        jit.store32(
            CCallHelpers::TrustedImm32(CallSiteIndex(0).bits()),
            CCallHelpers::tagFor(JSStack::ArgumentCount));
        jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);
        CCallHelpers::Call callArityCheck = jit.call();
#if !ASSERT_DISABLED
        // FIXME: need to make this call register with exception handling somehow. This is
        // part of a bigger problem: FTL should be able to handle exceptions.
        // https://bugs.webkit.org/show_bug.cgi?id=113622
        // Until then, use a JIT ASSERT.
        jit.load64(vm.addressOfException(), GPRInfo::regT1);
        jit.jitAssertIsNull(GPRInfo::regT1);
#endif
        jit.move(GPRInfo::returnValueGPR, GPRInfo::argumentGPR0);
        jit.emitFunctionEpilogue();
        mainPathJumps.append(jit.branchTest32(CCallHelpers::Zero, GPRInfo::argumentGPR0));
        jit.emitFunctionPrologue();
        CCallHelpers::Call callArityFixup = jit.call();
        jit.emitFunctionEpilogue();
        mainPathJumps.append(jit.jump());

        linkBuffer = std::make_unique<LinkBuffer>(vm, jit, codeBlock, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        linkBuffer->link(callArityCheck, codeBlock->m_isConstructor ? operationConstructArityCheck : operationCallArityCheck);
        linkBuffer->link(callArityFixup, FunctionPtr((vm.getCTIStub(arityFixupGenerator)).code().executableAddress()));
        linkBuffer->link(mainPathJumps, CodeLocationLabel(bitwise_cast<void*>(state.generatedFunction)));

        state.jitCode->initializeAddressForCall(MacroAssemblerCodePtr(bitwise_cast<void*>(state.generatedFunction)));
        break;
    }
        
    case FTLForOSREntryMode: {
        // We jump to here straight from DFG code, after having boxed up all of the
        // values into the scratch buffer. Everything should be good to go - at this
        // point we've even done the stack check. Basically we just have to make the
        // call to the LLVM-generated code.
        CCallHelpers::Label start = jit.label();
        jit.emitFunctionEpilogue();
        CCallHelpers::Jump mainPathJump = jit.jump();
        
        linkBuffer = std::make_unique<LinkBuffer>(vm, jit, codeBlock, JITCompilationCanFail);
        if (linkBuffer->didFailToAllocate()) {
            state.allocationFailed = true;
            return;
        }
        linkBuffer->link(mainPathJump, CodeLocationLabel(bitwise_cast<void*>(state.generatedFunction)));

        state.jitCode->initializeAddressForCall(linkBuffer->locationOf(start));
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

