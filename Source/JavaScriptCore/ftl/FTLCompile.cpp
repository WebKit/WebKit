/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#if ENABLE(FTL_JIT)

#include "AirCode.h"
#include "AirDisassembler.h"
#include "B3Generate.h"
#include "B3ProcedureInlines.h"
#include "B3StackSlot.h"
#include "B3Value.h"
#include "B3ValueInlines.h"
#include "CodeBlockWithJITType.h"
#include "CCallHelpers.h"
#include "DFGCommon.h"
#include "DFGGraphSafepoint.h"
#include "DFGOperations.h"
#include "DataView.h"
#include "Disassembler.h"
#include "FTLJITCode.h"
#include "FTLThunks.h"
#include "JITSubGenerator.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "PCToCodeOriginMap.h"
#include "ScratchRegisterAllocator.h"
#include <wtf/RecursableLambda.h>

namespace JSC { namespace FTL {

const char* const tierName = "FTL ";

using namespace DFG;

void compile(State& state, Safepoint::Result& safepointResult)
{
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;
    VM& vm = graph.m_vm;

    if (shouldDumpDisassembly())
        state.proc->code().setDisassembler(makeUnique<B3::Air::Disassembler>());

    {
        GraphSafepoint safepoint(state.graph, safepointResult);

        B3::prepareForGeneration(*state.proc);
    }

    if (safepointResult.didGetCancelled())
        return;
    RELEASE_ASSERT(!state.graph.m_vm.heap.worldIsStopped());
    
    if (state.allocationFailed)
        return;
    
    std::unique_ptr<RegisterAtOffsetList> registerOffsets =
        makeUnique<RegisterAtOffsetList>(state.proc->calleeSaveRegisterAtOffsetList());
    if (shouldDumpDisassembly())
        dataLog(tierName, "Unwind info for ", CodeBlockWithJITType(codeBlock, JITType::FTLJIT), ": ", *registerOffsets, "\n");
    codeBlock->setCalleeSaveRegisters(WTFMove(registerOffsets));
    ASSERT(!(state.proc->frameSize() % sizeof(EncodedJSValue)));
    state.jitCode->common.frameRegisterCount = state.proc->frameSize() / sizeof(EncodedJSValue);

    int localsOffset =
        state.capturedValue->offsetFromFP() / sizeof(EncodedJSValue) + graph.m_nextMachineLocal;
    if (shouldDumpDisassembly()) {
        dataLog(tierName,
            "localsOffset = ", localsOffset, " for stack slot: ",
            pointerDump(state.capturedValue), " at ", RawPointer(state.capturedValue), "\n");
    }
    
    for (unsigned i = graph.m_inlineVariableData.size(); i--;) {
        InlineCallFrame* inlineCallFrame = graph.m_inlineVariableData[i].inlineCallFrame;
        
        if (inlineCallFrame->argumentCountRegister.isValid())
            inlineCallFrame->argumentCountRegister += localsOffset;
        
        for (unsigned argument = inlineCallFrame->argumentsWithFixup.size(); argument-- > 1;) {
            inlineCallFrame->argumentsWithFixup[argument] =
                inlineCallFrame->argumentsWithFixup[argument].withLocalsOffset(localsOffset);
        }
        
        if (inlineCallFrame->isClosureCall) {
            inlineCallFrame->calleeRecovery =
                inlineCallFrame->calleeRecovery.withLocalsOffset(localsOffset);
        }

    }

    // Note that the scope register could be invalid here if the original code had CallEval but it
    // got killed. That's because it takes the CallEval to cause the scope register to be kept alive
    // unless the debugger is also enabled.
    if (graph.needsScopeRegister() && codeBlock->scopeRegister().isValid())
        codeBlock->setScopeRegister(codeBlock->scopeRegister() + localsOffset);

    for (OSRExitDescriptor& descriptor : state.jitCode->osrExitDescriptors) {
        for (unsigned i = descriptor.m_values.size(); i--;)
            descriptor.m_values[i] = descriptor.m_values[i].withLocalsOffset(localsOffset);
        for (ExitTimeObjectMaterialization* materialization : descriptor.m_materializations)
            materialization->accountForLocalsOffset(localsOffset);
    }

    // We will add exception handlers while generating.
    codeBlock->clearExceptionHandlers();

    CCallHelpers jit(codeBlock);
    B3::generate(*state.proc, jit);

    // Emit the exception handler.
    *state.exceptionHandler = jit.label();
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
    jit.move(MacroAssembler::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
    jit.prepareCallOperation(vm);
    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.jumpToExceptionHandler(vm);
    jit.addLinkTask(
        [=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationLookupExceptionHandler));
        });

    state.finalizer->b3CodeLinkBuffer = makeUnique<LinkBuffer>(jit, codeBlock, JITCompilationCanFail);

    if (state.finalizer->b3CodeLinkBuffer->didFailToAllocate()) {
        state.allocationFailed = true;
        return;
    }
    
    B3::PCToOriginMap originMap = state.proc->releasePCToOriginMap();
    if (vm.shouldBuilderPCToCodeOriginMapping())
        codeBlock->setPCToCodeOriginMap(makeUnique<PCToCodeOriginMap>(PCToCodeOriginMapBuilder(vm, WTFMove(originMap)), *state.finalizer->b3CodeLinkBuffer));

    CodeLocationLabel<JSEntryPtrTag> label = state.finalizer->b3CodeLinkBuffer->locationOf<JSEntryPtrTag>(state.proc->code().entrypointLabel(0));
    state.generatedFunction = label;
    state.jitCode->initializeB3Byproducts(state.proc->releaseByproducts());

    for (auto pair : state.graph.m_entrypointIndexToCatchBytecodeIndex) {
        BytecodeIndex catchBytecodeIndex = pair.value;
        unsigned entrypointIndex = pair.key;
        Vector<FlushFormat> argumentFormats = state.graph.m_argumentFormats[entrypointIndex];
        state.jitCode->common.appendCatchEntrypoint(catchBytecodeIndex, state.finalizer->b3CodeLinkBuffer->locationOf<ExceptionHandlerPtrTag>(state.proc->code().entrypointLabel(entrypointIndex)), WTFMove(argumentFormats));
    }
    state.jitCode->common.finalizeCatchEntrypoints();

    if (B3::Air::Disassembler* disassembler = state.proc->code().disassembler()) {
        PrintStream& out = WTF::dataFile();

        out.print("Generated ", state.graph.m_plan.mode(), " code for ", CodeBlockWithJITType(state.graph.m_codeBlock, JITType::FTLJIT), ", instructions size = ", state.graph.m_codeBlock->instructionsSize(), ":\n");

        LinkBuffer& linkBuffer = *state.finalizer->b3CodeLinkBuffer;
        B3::Value* currentB3Value = nullptr;
        Node* currentDFGNode = nullptr;

        HashSet<B3::Value*> printedValues;
        HashSet<Node*> printedNodes;
        const char* dfgPrefix = "DFG " "    ";
        const char* b3Prefix  = "b3  " "          ";
        const char* airPrefix = "Air " "              ";
        const char* asmPrefix = "asm " "                ";

        auto printDFGNode = [&] (Node* node) {
            if (currentDFGNode == node)
                return;

            currentDFGNode = node;
            if (!currentDFGNode)
                return;

            HashSet<Node*> localPrintedNodes;
            WTF::Function<void(Node*)> printNodeRecursive = [&] (Node* node) {
                if (printedNodes.contains(node) || localPrintedNodes.contains(node))
                    return;

                localPrintedNodes.add(node);
                graph.doToChildren(node, [&] (Edge child) {
                    printNodeRecursive(child.node());
                });
                graph.dump(out, dfgPrefix, node);
            };
            printNodeRecursive(node);
            printedNodes.add(node);
        };

        auto printB3Value = [&] (B3::Value* value) {
            if (currentB3Value == value)
                return;

            currentB3Value = value;
            if (!currentB3Value)
                return;

            printDFGNode(bitwise_cast<Node*>(value->origin().data()));

            HashSet<B3::Value*> localPrintedValues;
            auto printValueRecursive = recursableLambda([&] (auto self, B3::Value* value) -> void {
                if (printedValues.contains(value) || localPrintedValues.contains(value))
                    return;

                localPrintedValues.add(value);
                for (unsigned i = 0; i < value->numChildren(); i++)
                    self(value->child(i));
                out.print(b3Prefix);
                value->deepDump(state.proc.get(), out);
                out.print("\n");
            });

            printValueRecursive(currentB3Value);
            printedValues.add(value);
        };

        auto forEachInst = scopedLambda<void(B3::Air::Inst&)>([&] (B3::Air::Inst& inst) {
            printB3Value(inst.origin);
        });

        disassembler->dump(state.proc->code(), out, linkBuffer, airPrefix, asmPrefix, forEachInst);
        linkBuffer.didAlreadyDisassemble();
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

