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
#include "FTLState.h"

#if ENABLE(FTL_JIT)

#include "AirCode.h"
#include "AirDisassembler.h"
#include "B3ValueInlines.h"
#include "CodeBlockWithJITType.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLJITCode.h"
#include "FTLJITFinalizer.h"
#include "FTLPatchpointExceptionHandle.h"

#include <wtf/RecursableLambda.h>

namespace JSC { namespace FTL {

using namespace B3;
using namespace DFG;

State::State(Graph& graph)
    : graph(graph)
{
    switch (graph.m_plan.mode()) {
    case JITCompilationMode::FTL: {
        jitCode = adoptRef(new JITCode());
        break;
    }
    case JITCompilationMode::FTLForOSREntry: {
        RefPtr<ForOSREntryJITCode> code = adoptRef(new ForOSREntryJITCode());
        code->initializeEntryBuffer(graph.m_vm, graph.m_profiledBlock->numCalleeLocals());
        code->setBytecodeIndex(graph.m_plan.osrEntryBytecodeIndex());
        jitCode = code;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    graph.m_plan.setFinalizer(makeUnique<JITFinalizer>(graph.m_plan));
    finalizer = static_cast<JITFinalizer*>(graph.m_plan.finalizer());

    proc = makeUnique<Procedure>();

    if (graph.m_vm.shouldBuilderPCToCodeOriginMapping())
        proc->setNeedsPCToOriginMap();

    proc->setOriginPrinter(
        [] (PrintStream& out, B3::Origin origin) {
            out.print(bitwise_cast<Node*>(origin.data()));
        });

    proc->setFrontendData(&graph);
}

void State::dumpDisassembly(PrintStream& out, const ScopedLambda<void(DFG::Node*)>& perDFGNodeCallback)
{
    B3::Air::Disassembler* disassembler = proc->code().disassembler();

    out.print("Generated ", graph.m_plan.mode(), " code for ", CodeBlockWithJITType(graph.m_codeBlock, JITType::FTLJIT), ", instructions size = ", graph.m_codeBlock->instructionsSize(), ":\n");

    LinkBuffer& linkBuffer = *finalizer->b3CodeLinkBuffer;
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

        perDFGNodeCallback(node);

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
            value->deepDump(proc.get(), out);
            out.print("\n");
        });

        printValueRecursive(currentB3Value);
        printedValues.add(value);
    };

    auto forEachInst = scopedLambda<void(B3::Air::Inst&)>([&] (B3::Air::Inst& inst) {
        printB3Value(inst.origin);
    });

    disassembler->dump(proc->code(), out, linkBuffer, airPrefix, asmPrefix, forEachInst);
    linkBuffer.didAlreadyDisassemble();
}

State::~State()
{
}

StructureStubInfo* State::addStructureStubInfo()
{
    ASSERT(!graph.m_plan.isUnlinked());
    auto* stubInfo = jitCode->common.m_stubInfos.add();
    stubInfo->useDataIC = Options::useDataICInFTL();
    return stubInfo;
}

OptimizingCallLinkInfo* State::addCallLinkInfo(CodeOrigin codeOrigin)
{
    return jitCode->common.m_callLinkInfos.add(codeOrigin, CallLinkInfo::UseDataIC::No);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

