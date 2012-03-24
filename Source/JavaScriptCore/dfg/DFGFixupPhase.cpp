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
#include "DFGFixupPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"

namespace JSC { namespace DFG {

class FixupPhase : public Phase {
public:
    FixupPhase(Graph& graph)
        : Phase(graph, "fixup")
    {
    }
    
    void run()
    {
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            fixupNode(m_graph[m_compileIndex]);
    }

private:
    void fixupNode(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op();

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("   %s @%u: ", Graph::opName(op), m_compileIndex);
#endif
        
        switch (op) {
        case GetById: {
            if (!isInt32Prediction(m_graph[m_compileIndex].prediction()))
                break;
            if (codeBlock()->identifier(node.identifierNumber()) != globalData().propertyNames->length)
                break;
            bool isArray = isArrayPrediction(m_graph[node.child1()].prediction());
            bool isString = isStringPrediction(m_graph[node.child1()].prediction());
            bool isByteArray = m_graph[node.child1()].shouldSpeculateByteArray();
            bool isInt8Array = m_graph[node.child1()].shouldSpeculateInt8Array();
            bool isInt16Array = m_graph[node.child1()].shouldSpeculateInt16Array();
            bool isInt32Array = m_graph[node.child1()].shouldSpeculateInt32Array();
            bool isUint8Array = m_graph[node.child1()].shouldSpeculateUint8Array();
            bool isUint8ClampedArray = m_graph[node.child1()].shouldSpeculateUint8ClampedArray();
            bool isUint16Array = m_graph[node.child1()].shouldSpeculateUint16Array();
            bool isUint32Array = m_graph[node.child1()].shouldSpeculateUint32Array();
            bool isFloat32Array = m_graph[node.child1()].shouldSpeculateFloat32Array();
            bool isFloat64Array = m_graph[node.child1()].shouldSpeculateFloat64Array();
            if (!isArray && !isString && !isByteArray && !isInt8Array && !isInt16Array && !isInt32Array && !isUint8Array && !isUint8ClampedArray && !isUint16Array && !isUint32Array && !isFloat32Array && !isFloat64Array)
                break;
            
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("  @%u -> %s", m_compileIndex, isArray ? "GetArrayLength" : "GetStringLength");
#endif
            if (isArray)
                node.setOp(GetArrayLength);
            else if (isString)
                node.setOp(GetStringLength);
            else if (isByteArray)
                node.setOp(GetByteArrayLength);
            else if (isInt8Array)
                node.setOp(GetInt8ArrayLength);
            else if (isInt16Array)
                node.setOp(GetInt16ArrayLength);
            else if (isInt32Array)
                node.setOp(GetInt32ArrayLength);
            else if (isUint8Array)
                node.setOp(GetUint8ArrayLength);
            else if (isUint8ClampedArray)
                node.setOp(GetUint8ClampedArrayLength);
            else if (isUint16Array)
                node.setOp(GetUint16ArrayLength);
            else if (isUint32Array)
                node.setOp(GetUint32ArrayLength);
            else if (isFloat32Array)
                node.setOp(GetFloat32ArrayLength);
            else if (isFloat64Array)
                node.setOp(GetFloat64ArrayLength);
            else
                ASSERT_NOT_REACHED();
            // No longer MustGenerate
            ASSERT(node.flags() & NodeMustGenerate);
            node.clearFlags(NodeMustGenerate);
            m_graph.deref(m_compileIndex);
            break;
        }
        case GetIndexedPropertyStorage: {
            PredictedType basePrediction = m_graph[node.child2()].prediction();
            if (!(basePrediction & PredictInt32) && basePrediction) {
                node.setOpAndDefaultFlags(Nop);
                m_graph.clearAndDerefChild1(node);
                m_graph.clearAndDerefChild2(node);
                m_graph.clearAndDerefChild3(node);
                node.setRefCount(0);
            }
            break;
        }
        case GetByVal:
        case StringCharAt:
        case StringCharCodeAt: {
            if (!!node.child3() && m_graph[node.child3()].op() == Nop)
                node.children.child3() = Edge();
            break;
        }
            
        case ValueToInt32: {
            if (m_graph[node.child1()].shouldSpeculateNumber()) {
                node.clearFlags(NodeMustGenerate);
                m_graph.deref(m_compileIndex);
            }
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift: {
            fixIntEdge(node.children.child1());
            fixIntEdge(node.children.child2());
            break;
        }
            
        default:
            break;
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("\n");
#endif
    }
    
    void fixIntEdge(Edge& edge)
    {
        Node& node = m_graph[edge];
        if (node.op() != ValueToInt32)
            return;
        
        if (!m_graph[node.child1()].shouldSpeculateInteger())
            return;
        
        Edge oldEdge = edge;
        Edge newEdge = node.child1();
        
        m_graph.ref(newEdge);
        m_graph.deref(oldEdge);
        
        edge = newEdge;
    }
    
    NodeIndex m_compileIndex;
};
    
void performFixup(Graph& graph)
{
    runPhase<FixupPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

