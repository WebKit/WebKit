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
#include "DFGStrengthReductionPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"

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
            handleCommutativity();

            if (m_node->child2()->isConstant()) {
                JSValue op2 = m_graph.valueOfJSConstant(m_node->child2().node());
                if (op2.isInt32() && !op2.asInt32()) {
                    convertToIdentityOverChild1();
                    break;
                }
            }
            break;
            
        case BitXor:
        case BitAnd:
            handleCommutativity();
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
            
        case ArithAdd:
            handleCommutativity();
            
            if (m_graph.isInt32Constant(m_node->child2().node())) {
                int32_t value = m_graph.valueOfInt32Constant(
                    m_node->child2().node());
                if (!value) {
                    convertToIdentityOverChild1();
                    break;
                }
            }
            break;
            
        case ArithMul:
            handleCommutativity();
            break;
            
        case ArithSub:
            if (m_graph.isInt32Constant(m_node->child2().node())
                && m_node->isBinaryUseKind(Int32Use)) {
                int32_t value = m_graph.valueOfInt32Constant(m_node->child2().node());
                if (-value != value) {
                    m_node->setOp(ArithAdd);
                    m_node->child2().setNode(
                        m_insertionSet.insertConstant(
                            m_nodeIndex, m_node->origin, jsNumber(-value)));
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
            
        case ValueRep:
        case Int52Rep:
        case DoubleRep: {
            // This short-circuits circuitous conversions, like ValueRep(DoubleRep(value)) or
            // even more complicated things. Like, it can handle a beast like
            // ValueRep(DoubleRep(Int52Rep(value))).
            
            // The only speculation that we would do beyond validating that we have a type that
            // can be represented a certain way is an Int32 check that would appear on Int52Rep
            // nodes. For now, if we see this and the final type we want is an Int52, we use it
            // as an excuse not to fold. The only thing we would need is a Int52RepInt32Use kind.
            bool hadInt32Check = false;
            if (m_node->op() == Int52Rep) {
                ASSERT(m_node->child1().useKind() == Int32Use);
                hadInt32Check = true;
            }
            for (Node* node = m_node->child1().node(); ; node = node->child1().node()) {
                if (canonicalResultRepresentation(node->result()) ==
                    canonicalResultRepresentation(m_node->result())) {
                    m_insertionSet.insertNode(
                        m_nodeIndex, SpecNone, Phantom, m_node->origin, m_node->child1());
                    if (hadInt32Check) {
                        // FIXME: Consider adding Int52RepInt32Use or even DoubleRepInt32Use,
                        // which would be super weird. The latter would only arise in some
                        // seriously circuitous conversions.
                        if (canonicalResultRepresentation(node->result()) != NodeResultJS)
                            break;
                        
                        m_insertionSet.insertNode(
                            m_nodeIndex, SpecNone, Phantom, m_node->origin,
                            Edge(node, Int32Use));
                    }
                    m_node->child1() = node->defaultEdge();
                    m_node->convertToIdentity();
                    m_changed = true;
                    break;
                }
                
                switch (node->op()) {
                case Int52Rep:
                    ASSERT(node->child1().useKind() == Int32Use);
                    hadInt32Check = true;
                    continue;
                    
                case DoubleRep:
                case ValueRep:
                    continue;
                    
                default:
                    break;
                }
                break;
            }
            break;
        }
            
        default:
            break;
        }
    }
            
    void convertToIdentityOverChild(unsigned childIndex)
    {
        m_insertionSet.insertNode(
            m_nodeIndex, SpecNone, Phantom, m_node->origin, m_node->children);
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
            m_nodeIndex, SpecNone, TypedArrayWatchpoint, m_node->origin,
            OpInfo(view));
        m_insertionSet.insertNode(
            m_nodeIndex, SpecNone, Phantom, m_node->origin, m_node->children);
    }
    
    void handleCommutativity()
    {
        // If the right side is a constant then there is nothing left to do.
        if (m_node->child2()->hasConstant())
            return;
        
        // This case ensures that optimizations that look for x + const don't also have
        // to look for const + x.
        if (m_node->child1()->hasConstant()) {
            std::swap(m_node->child1(), m_node->child2());
            m_changed = true;
            return;
        }
        
        // This case ensures that CSE is commutativity-aware.
        if (m_node->child1().node() > m_node->child2().node()) {
            std::swap(m_node->child1(), m_node->child2());
            m_changed = true;
            return;
        }
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

