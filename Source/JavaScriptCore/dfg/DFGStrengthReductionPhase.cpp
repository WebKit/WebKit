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

#include "DFGStrengthReductionPhase.h"

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class StrengthReductionPhase : public Phase {
public:
    StrengthReductionPhase(Graph& graph)
        : Phase(graph, "strength reduction")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_fixpointState == FixpointNotConverged);
        
        m_changed = false;
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            m_block = m_graph.block(blockIndex);
            if (!m_block)
                continue;
            for (m_nodeIndex = 0; m_nodeIndex < m_block->size(); ++m_nodeIndex) {
                m_node = m_block->at(m_nodeIndex);
                handleNode();
            }
            m_insertionSet.execute(m_block);
        }
        
        return m_changed;
    }

private:
    void handleNode()
    {
        switch (m_node->op()) {
        case BitOr:
            if (m_node->child1()->isConstant()) {
                JSValue op1 = m_graph.valueOfJSConstant(m_node->child1().node());
                if (op1.isInt32() && !op1.asInt32()) {
                    convertToIdentityOverChild2();
                    break;
                }
            }
            if (m_node->child2()->isConstant()) {
                JSValue op2 = m_graph.valueOfJSConstant(m_node->child2().node());
                if (op2.isInt32() && !op2.asInt32()) {
                    convertToIdentityOverChild1();
                    break;
                }
            }
            break;
            
        case BitLShift:
        case BitRShift:
        case BitURShift:
            if (m_node->child2()->isConstant()) {
                JSValue op2 = m_graph.valueOfJSConstant(m_node->child2().node());
                if (op2.isInt32() && !(op2.asInt32() & 0x1f)) {
                    convertToIdentityOverChild1();
                    break;
                }
            }
            break;
            
        case UInt32ToNumber:
            if (m_node->child1()->op() == BitURShift
                && m_node->child1()->child2()->isConstant()) {
                JSValue shiftAmount = m_graph.valueOfJSConstant(
                    m_node->child1()->child2().node());
                if (shiftAmount.isInt32() && (shiftAmount.asInt32() & 0x1f)) {
                    m_node->convertToIdentity();
                    m_changed = true;
                    break;
                }
            }
            break;
            
        case GetArrayLength:
            if (JSArrayBufferView* view = m_graph.tryGetFoldableViewForChild1(m_node))
                foldTypedArrayPropertyToConstant(view, jsNumber(view->length()));
            break;
            
        case GetTypedArrayByteOffset:
            if (JSArrayBufferView* view = m_graph.tryGetFoldableView(m_node->child1().node()))
                foldTypedArrayPropertyToConstant(view, jsNumber(view->byteOffset()));
            break;
            
        case GetIndexedPropertyStorage:
            if (JSArrayBufferView* view = m_graph.tryGetFoldableViewForChild1(m_node)) {
                if (view->mode() != FastTypedArray) {
                    prepareToFoldTypedArray(view);
                    m_node->convertToConstantStoragePointer(view->vector());
                    m_changed = true;
                    break;
                } else {
                    // FIXME: It would be awesome to be able to fold the property storage for
                    // these GC-allocated typed arrays. For now it doesn't matter because the
                    // most common use-cases for constant typed arrays involve large arrays with
                    // aliased buffer views.
                    // https://bugs.webkit.org/show_bug.cgi?id=125425
                }
            }
            break;
            
        default:
            break;
        }
    }
            
    void convertToIdentityOverChild(unsigned childIndex)
    {
        m_insertionSet.insertNode(
            m_nodeIndex, SpecNone, Phantom, m_node->codeOrigin, m_node->children);
        m_node->children.removeEdge(childIndex ^ 1);
        m_node->convertToIdentity();
        m_changed = true;
    }
    
    void convertToIdentityOverChild1()
    {
        convertToIdentityOverChild(0);
    }
    
    void convertToIdentityOverChild2()
    {
        convertToIdentityOverChild(1);
    }
    
    void foldTypedArrayPropertyToConstant(JSArrayBufferView* view, JSValue constant)
    {
        prepareToFoldTypedArray(view);
        m_graph.convertToConstant(m_node, constant);
        m_changed = true;
    }
    
    void prepareToFoldTypedArray(JSArrayBufferView* view)
    {
        m_insertionSet.insertNode(
            m_nodeIndex, SpecNone, TypedArrayWatchpoint, m_node->codeOrigin,
            OpInfo(view));
        m_insertionSet.insertNode(
            m_nodeIndex, SpecNone, Phantom, m_node->codeOrigin, m_node->children);
    }
    
    InsertionSet m_insertionSet;
    BasicBlock* m_block;
    unsigned m_nodeIndex;
    Node* m_node;
    bool m_changed;
};
    
bool performStrengthReduction(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Strength Reduction Phase");
    return runPhase<StrengthReductionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

