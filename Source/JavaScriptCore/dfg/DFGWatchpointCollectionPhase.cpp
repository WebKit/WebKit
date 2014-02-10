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

#if ENABLE(DFG_JIT)

#include "DFGWatchpointCollectionPhase.h"

#include "ArrayPrototype.h"
#include "DFGClobberize.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class WatchpointCollectionPhase : public Phase {
    static const bool verbose = false;
    
public:
    WatchpointCollectionPhase(Graph& graph)
        : Phase(graph, "watchpoint collection")
    {
    }
    
    bool run()
    {
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                m_node = block->at(nodeIndex);
                handle();
            }
        }
        
        return true;
    }

private:
    void handle()
    {
        DFG_NODE_DO_TO_CHILDREN(m_graph, m_node, handleEdge);
        
        switch (m_node->op()) {
        case CompareEqConstant:
        case IsUndefined:
            handleMasqueradesAsUndefined();
            break;
            
        case CompareEq:
            if (m_node->isBinaryUseKind(ObjectUse)
                || (m_node->child1().useKind() == ObjectUse && m_node->child2().useKind() == ObjectOrOtherUse)
                || (m_node->child1().useKind() == ObjectOrOtherUse && m_node->child2().useKind() == ObjectUse))
                handleMasqueradesAsUndefined();
            break;
            
        case LogicalNot:
        case Branch:
            if (m_node->child1().useKind() == ObjectOrOtherUse)
                handleMasqueradesAsUndefined();
            break;
            
        case GetByVal:
            if (m_node->arrayMode().type() == Array::Double
                && m_node->arrayMode().isSaneChain()) {
                addLazily(globalObject()->arrayPrototype()->structure()->transitionWatchpointSet());
                addLazily(globalObject()->objectPrototype()->structure()->transitionWatchpointSet());
            }
            
            if (m_node->arrayMode().type() == Array::String)
                handleStringGetByVal();

            if (JSArrayBufferView* view = m_graph.tryGetFoldableViewForChild1(m_node))
                addLazily(view);
            break;
            
        case PutByVal:
            if (JSArrayBufferView* view = m_graph.tryGetFoldableViewForChild1(m_node))
                addLazily(view);
            break;
            
        case StringCharAt:
            handleStringGetByVal();
            break;
            
        case NewArray:
        case NewArrayWithSize:
        case NewArrayBuffer:
            if (!globalObject()->isHavingABadTime() && !hasArrayStorage(m_node->indexingType()))
                addLazily(globalObject()->havingABadTimeWatchpoint());
            break;
            
        case AllocationProfileWatchpoint:
            addLazily(jsCast<JSFunction*>(m_node->function())->allocationProfileWatchpointSet());
            break;
            
        case StructureTransitionWatchpoint:
            m_graph.watchpoints().addLazily(
                m_node->codeOrigin,
                m_node->child1()->op() == WeakJSConstant ? BadWeakConstantCacheWatchpoint : BadCacheWatchpoint,
                m_node->structure()->transitionWatchpointSet());
            break;
            
        case VariableWatchpoint:
            addLazily(m_node->variableWatchpointSet());
            break;
            
        case VarInjectionWatchpoint:
            addLazily(globalObject()->varInjectionWatchpoint());
            break;
            
        case FunctionReentryWatchpoint:
            addLazily(m_node->symbolTable()->m_functionEnteredOnce);
            break;
            
        case TypedArrayWatchpoint:
            addLazily(m_node->typedArray());
            break;
            
        default:
            break;
        }
    }
    
    void handleEdge(Node*, Edge edge)
    {
        switch (edge.useKind()) {
        case StringObjectUse:
        case StringOrStringObjectUse: {
            Structure* stringObjectStructure = globalObject()->stringObjectStructure();
            Structure* stringPrototypeStructure = stringObjectStructure->storedPrototype().asCell()->structure();
            ASSERT(m_graph.watchpoints().isValidOrMixed(stringPrototypeStructure->transitionWatchpointSet()));
            
            m_graph.watchpoints().addLazily(
                m_node->codeOrigin, NotStringObject,
                stringPrototypeStructure->transitionWatchpointSet());
            break;
        }
            
        default:
            break;
        }
    }
    
    void handleMasqueradesAsUndefined()
    {
        if (m_graph.masqueradesAsUndefinedWatchpointIsStillValid(m_node->codeOrigin))
            addLazily(globalObject()->masqueradesAsUndefinedWatchpoint());
    }
    
    void handleStringGetByVal()
    {
        if (!m_node->arrayMode().isOutOfBounds())
            return;
        if (!globalObject()->stringPrototypeChainIsSane())
            return;
        addLazily(globalObject()->stringPrototype()->structure()->transitionWatchpointSet());
        addLazily(globalObject()->objectPrototype()->structure()->transitionWatchpointSet());
    }

    void addLazily(WatchpointSet* set)
    {
        m_graph.watchpoints().addLazily(set);
    }
    void addLazily(InlineWatchpointSet& set)
    {
        m_graph.watchpoints().addLazily(set);
    }
    void addLazily(JSArrayBufferView* view)
    {
        m_graph.watchpoints().addLazily(view);
    }
    
    JSGlobalObject* globalObject()
    {
        return m_graph.globalObjectFor(m_node->codeOrigin);
    }
    
    Node* m_node;
};

bool performWatchpointCollection(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Watchpoint Collection Phase");
    return runPhase<WatchpointCollectionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

