/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGArithNodeFlagsInferencePhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"

namespace JSC { namespace DFG {

class ArithNodeFlagsInferencePhase : public Phase {
public:
    ArithNodeFlagsInferencePhase(Graph& graph)
        : Phase(graph, "arithmetic node flags inference")
    {
    }
    
    void run()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        m_count = 0;
#endif
        do {
            m_changed = false;
            
            // Up here we start with a backward pass because we suspect that to be
            // more profitable.
            propagateBackward();
            if (!m_changed)
                break;
            
            m_changed = false;
            propagateForward();
        } while (m_changed);
    }
    
private:
    bool isNotNegZero(NodeIndex nodeIndex)
    {
        if (!m_graph.isNumberConstant(nodeIndex))
            return false;
        double value = m_graph.valueOfNumberConstant(nodeIndex);
        return !value && 1.0 / value < 0.0;
    }
    
    bool isNotZero(NodeIndex nodeIndex)
    {
        if (!m_graph.isNumberConstant(nodeIndex))
            return false;
        return !!m_graph.valueOfNumberConstant(nodeIndex);
    }
    
    void propagate(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = static_cast<NodeType>(node.op);
        NodeFlags flags = node.flags;
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("   %s @%u: %s ", Graph::opName(op), m_compileIndex, arithNodeFlagsAsString(flags));
#endif
        
        flags &= NodeUsedAsMask;
        
        bool changed = false;
        
        switch (op) {
        case ValueToInt32:
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitLShift:
        case BitRShift:
        case BitURShift: {
            // These operations are perfectly happy with truncated integers,
            // so we don't want to propagate anything.
            break;
        }
            
        case UInt32ToNumber: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithAdd:
        case ValueAdd: {
            if (isNotNegZero(node.child1().index()) || isNotNegZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithSub: {
            if (isNotZero(node.child1().index()) || isNotZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithNegate: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithMul:
        case ArithDiv: {
            // As soon as a multiply happens, we can easily end up in the part
            // of the double domain where the point at which you do truncation
            // can change the outcome. So, ArithMul always checks for overflow
            // no matter what, and always forces its inputs to check as well.
            
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithMin:
        case ArithMax: {
            flags |= NodeUsedAsNumber;
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithAbs: {
            flags &= ~NodeNeedsNegZero;
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            break;
        }
            
        case PutByVal: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags | NodeUsedAsNumber | NodeNeedsNegZero);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags | NodeUsedAsNumber);
            changed |= m_graph[node.child3()].mergeArithNodeFlags(flags | NodeUsedAsNumber | NodeNeedsNegZero);
            break;
        }
            
        case GetByVal: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags | NodeUsedAsNumber | NodeNeedsNegZero);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags | NodeUsedAsNumber);
            break;
        }
            
        default:
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            if (node.flags & NodeHasVarArgs) {
                for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
                    changed |= m_graph[m_graph.m_varArgChildren[childIdx]].mergeArithNodeFlags(flags);
            } else {
                if (!node.child1())
                    break;
                changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
                if (!node.child2())
                    break;
                changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
                if (!node.child3())
                    break;
                changed |= m_graph[node.child3()].mergeArithNodeFlags(flags);
            }
            break;
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("%s\n", changed ? "CHANGED" : "");
#endif
        
        m_changed |= changed;
    }
    
    void propagateForward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Propagating arithmetic node flags forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagate(m_graph[m_compileIndex]);
    }
    
    void propagateBackward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Propagating arithmetic node flags backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagate(m_graph[m_compileIndex]);
    }
    
    NodeIndex m_compileIndex;
    bool m_changed;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    unsigned m_count;
#endif
};

void performArithNodeFlagsInference(Graph& graph)
{
    runPhase<ArithNodeFlagsInferencePhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

