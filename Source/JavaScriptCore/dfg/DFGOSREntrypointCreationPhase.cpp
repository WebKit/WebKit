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
#include "DFGOSREntrypointCreationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGBlockInsertionSet.h"
#include "DFGGraph.h"
#include "DFGLoopPreHeaderCreationPhase.h"
#include "DFGPhase.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class OSREntrypointCreationPhase : public Phase {
public:
    OSREntrypointCreationPhase(Graph& graph)
        : Phase(graph, "OSR entrypoint creation")
    {
    }
    
    bool run()
    {
        RELEASE_ASSERT(m_graph.m_plan.mode == FTLForOSREntryMode);
        RELEASE_ASSERT(m_graph.m_form == ThreadedCPS);
        
        unsigned bytecodeIndex = m_graph.m_plan.osrEntryBytecodeIndex;
        RELEASE_ASSERT(bytecodeIndex);
        RELEASE_ASSERT(bytecodeIndex != UINT_MAX);
        
        // Needed by createPreHeader().
        m_graph.m_dominators.computeIfNecessary(m_graph);
        
        CodeBlock* baseline = m_graph.m_profiledBlock;
        
        BasicBlock* target = 0;
        for (unsigned blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            Node* firstNode = block->at(0);
            if (firstNode->op() == LoopHint
                && firstNode->codeOrigin == CodeOrigin(bytecodeIndex)) {
                target = block;
                break;
            }
        }

        if (!target) {
            // This is a terrible outcome. It shouldn't often happen but it might
            // happen and so we should defend against it. If it happens, then this
            // compilation is a failure.
            return false;
        }
        
        BlockInsertionSet insertionSet(m_graph);
        
        BasicBlock* newRoot = insertionSet.insert(0);
        CodeOrigin codeOrigin = target->at(0)->codeOrigin;
        
        for (int argument = 0; argument < baseline->numParameters(); ++argument) {
            Node* oldNode = target->variablesAtHead.argument(argument);
            if (!oldNode) {
                // Just for sanity, always have a SetArgument even if it's not needed.
                oldNode = m_graph.m_arguments[argument];
            }
            Node* node = newRoot->appendNode(
                m_graph, SpecNone, SetArgument, codeOrigin,
                OpInfo(oldNode->variableAccessData()));
            m_graph.m_arguments[argument] = node;
        }
        Vector<Node*> locals(baseline->m_numCalleeRegisters);
        for (int local = 0; local < baseline->m_numCalleeRegisters; ++local) {
            Node* previousHead = target->variablesAtHead.local(local);
            if (!previousHead)
                continue;
            VariableAccessData* variable = previousHead->variableAccessData();
            locals[local] = newRoot->appendNode(
                m_graph, variable->prediction(), ExtractOSREntryLocal, codeOrigin,
                OpInfo(variable->local().offset()));
            
            // Create a MovHint. We can't use MovHint's directly at this stage of
            // compilation, so we cook one up by creating a new VariableAccessData
            // that isn't unified with any of the others. This ensures that this
            // SetLocal will turn into a MovHint and will not have any type checks.
            m_graph.m_variableAccessData.append(
                VariableAccessData(variable->local(), variable->isCaptured()));
            VariableAccessData* newVariable = &m_graph.m_variableAccessData.last();
            Node* setLocal = newRoot->appendNode(
                m_graph, SpecNone, SetLocal, codeOrigin, OpInfo(newVariable),
                Edge(locals[local]));
            setLocal->setSpeculationDirection(BackwardSpeculation);
        }
        for (int local = 0; local < baseline->m_numCalleeRegisters; ++local) {
            Node* previousHead = target->variablesAtHead.local(local);
            if (!previousHead)
                continue;
            VariableAccessData* variable = previousHead->variableAccessData();
            Node* node = locals[local];
            Node* setLocal = newRoot->appendNode(
                m_graph, SpecNone, SetLocal, codeOrigin, OpInfo(variable), Edge(node));
            setLocal->setSpeculationDirection(BackwardSpeculation);
        }
        
        newRoot->appendNode(
            m_graph, SpecNone, Jump, codeOrigin,
            OpInfo(createPreHeader(m_graph, insertionSet, target)));
        
        insertionSet.execute();
        m_graph.resetReachability();
        m_graph.killUnreachableBlocks();
        return true;
    }
};

bool performOSREntrypointCreation(Graph& graph)
{
    SamplingRegion samplingRegion("DFG OSR Entrypoint Creation");
    return runPhase<OSREntrypointCreationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


