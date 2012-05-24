/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DFGArgumentsSimplificationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractState.h"
#include "DFGBasicBlock.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGValidate.h"

namespace JSC { namespace DFG {

class ArgumentsSimplificationPhase : public Phase {
public:
    ArgumentsSimplificationPhase(Graph& graph)
        : Phase(graph, "arguments simplification")
    {
    }
    
    bool run()
    {
        if (!m_graph.m_hasArguments)
            return false;
        
        bool changed = false;
        
        InsertionSet<NodeIndex> insertionSet;
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                NodeIndex nodeIndex = block->at(indexInBlock);
                Node& node = m_graph[nodeIndex];
                switch (node.op()) {
                case GetMyArgumentsLength: {
                    if (m_graph.m_executablesWhoseArgumentsEscaped.contains(
                            m_graph.executableFor(node.codeOrigin)))
                        break;
                    if (!node.codeOrigin.inlineCallFrame)
                        break;
                    
                    // We know exactly what this will return. But only after we have checked
                    // that nobody has escaped our arguments.
                    Node check(CheckArgumentsNotCreated, node.codeOrigin);
                    check.ref();
                    NodeIndex checkIndex = m_graph.size();
                    m_graph.append(check);
                    insertionSet.append(indexInBlock, checkIndex);
                    
                    m_graph.convertToConstant(
                        nodeIndex, jsNumber(node.codeOrigin.inlineCallFrame->arguments.size() - 1));
                    changed = true;
                    break;
                }
                    
                case GetMyArgumentByVal: {
                    if (m_graph.m_executablesWhoseArgumentsEscaped.contains(
                            m_graph.executableFor(node.codeOrigin)))
                        break;
                    if (!node.codeOrigin.inlineCallFrame)
                        break;
                    if (!m_graph[node.child1()].hasConstant())
                        break;
                    JSValue value = m_graph[node.child1()].valueOfJSConstant(codeBlock());
                    if (!value.isInt32())
                        break;
                    int32_t index = value.asInt32();
                    if (index < 0
                        || static_cast<size_t>(index + 1) >=
                            node.codeOrigin.inlineCallFrame->arguments.size())
                        break;
                    
                    // We know which argument this is accessing. But only after we have checked
                    // that nobody has escaped our arguments. We also need to ensure that the
                    // index is kept alive. That's somewhat pointless since it's a constant, but
                    // it's important because this is one of those invariants that we like to
                    // have in the DFG. Note finally that we use the GetLocalUnlinked opcode
                    // here, since this is being done _after_ the prediction propagation phase
                    // has run - therefore it makes little sense to link the GetLocal operation
                    // into the VariableAccessData and Phi graphs.

                    Node check(CheckArgumentsNotCreated, node.codeOrigin);
                    check.ref();
                    
                    Node phantom(Phantom, node.codeOrigin);
                    phantom.ref();
                    phantom.children = node.children;
                    
                    node.convertToGetLocalUnlinked(
                        static_cast<VirtualRegister>(
                            node.codeOrigin.inlineCallFrame->stackOffset +
                            argumentToOperand(index + 1)));
                    
                    NodeIndex checkNodeIndex = m_graph.size();
                    m_graph.append(check);
                    insertionSet.append(indexInBlock, checkNodeIndex);
                    NodeIndex phantomNodeIndex = m_graph.size();
                    m_graph.append(phantom);
                    insertionSet.append(indexInBlock, phantomNodeIndex);
                    
                    changed = true;
                    break;
                }
                    
                default:
                    break;
                }
            }
            insertionSet.execute(*block);
        }
        
        return changed;
    }
};

bool performArgumentsSimplification(Graph& graph)
{
    return runPhase<ArgumentsSimplificationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


