/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#include "DFGAbstractHeap.h"
#include "DFGClobberize.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"
#include <cstdlib>

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

            if (m_node->child1().useKind() != UntypedUse && m_node->child2()->isInt32Constant() && !m_node->child2()->asInt32()) {
                convertToIdentityOverChild1();
                break;
            }
            break;
            
        case BitXor:
        case BitAnd:
            handleCommutativity();
            break;
            
        case BitLShift:
        case BitRShift:
        case BitURShift:
            if (m_node->child1().useKind() != UntypedUse && m_node->child2()->isInt32Constant() && !(m_node->child2()->asInt32() & 0x1f)) {
                convertToIdentityOverChild1();
                break;
            }
            break;
            
        case UInt32ToNumber:
            if (m_node->child1()->op() == BitURShift
                && m_node->child1()->child2()->isInt32Constant()
                && (m_node->child1()->child2()->asInt32() & 0x1f)
                && m_node->arithMode() != Arith::DoOverflow) {
                m_node->convertToIdentity();
                m_changed = true;
                break;
            }
            break;
            
        case ArithAdd:
            handleCommutativity();
            
            if (m_node->child2()->isInt32Constant() && !m_node->child2()->asInt32()) {
                convertToIdentityOverChild1();
                break;
            }
            break;
            
        case ArithMul: {
            handleCommutativity();
            Edge& child2 = m_node->child2();
            if (child2->isNumberConstant() && child2->asNumber() == 2) {
                switch (m_node->binaryUseKind()) {
                case DoubleRepUse:
                    // It is always valuable to get rid of a double multiplication by 2.
                    // We won't have half-register dependencies issues on x86 and we won't have to load the constants.
                    m_node->setOp(ArithAdd);
                    child2.setNode(m_node->child1().node());
                    m_changed = true;
                    break;
#if USE(JSVALUE64)
                case Int52RepUse:
#endif
                case Int32Use:
                    // For integers, we can only convert compatible modes.
                    // ArithAdd does handle do negative zero check for example.
                    if (m_node->arithMode() == Arith::CheckOverflow || m_node->arithMode() == Arith::Unchecked) {
                        m_node->setOp(ArithAdd);
                        child2.setNode(m_node->child1().node());
                        m_changed = true;
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        }
        case ArithSub:
            if (m_node->child2()->isInt32Constant()
                && m_node->isBinaryUseKind(Int32Use)) {
                int32_t value = m_node->child2()->asInt32();
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

        case ArithPow:
            if (m_node->child2()->isNumberConstant()) {
                double yOperandValue = m_node->child2()->asNumber();
                if (yOperandValue == 1) {
                    convertToIdentityOverChild1();
                } else if (yOperandValue == 0.5) {
                    m_insertionSet.insertCheck(m_nodeIndex, m_node);
                    m_node->convertToArithSqrt();
                    m_changed = true;
                }
            }
            break;

        case ArithMod:
            // On Integers
            // In: ArithMod(ArithMod(x, const1), const2)
            // Out: Identity(ArithMod(x, const1))
            //     if const1 <= const2.
            if (m_node->binaryUseKind() == Int32Use
                && m_node->child2()->isInt32Constant()
                && m_node->child1()->op() == ArithMod
                && m_node->child1()->binaryUseKind() == Int32Use
                && m_node->child1()->child2()->isInt32Constant()
                && std::abs(m_node->child1()->child2()->asInt32()) <= std::abs(m_node->child2()->asInt32())) {
                    convertToIdentityOverChild1();
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
                if (m_node->child1().useKind() != Int32Use)
                    break;
                hadInt32Check = true;
            }
            for (Node* node = m_node->child1().node(); ; node = node->child1().node()) {
                if (canonicalResultRepresentation(node->result()) ==
                    canonicalResultRepresentation(m_node->result())) {
                    m_insertionSet.insertCheck(m_nodeIndex, m_node);
                    if (hadInt32Check) {
                        // FIXME: Consider adding Int52RepInt32Use or even DoubleRepInt32Use,
                        // which would be super weird. The latter would only arise in some
                        // seriously circuitous conversions.
                        if (canonicalResultRepresentation(node->result()) != NodeResultJS)
                            break;
                        
                        m_insertionSet.insertCheck(
                            m_nodeIndex, m_node->origin, Edge(node, Int32Use));
                    }
                    m_node->child1() = node->defaultEdge();
                    m_node->convertToIdentity();
                    m_changed = true;
                    break;
                }
                
                switch (node->op()) {
                case Int52Rep:
                    if (node->child1().useKind() != Int32Use)
                        break;
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
            
        case Flush: {
            ASSERT(m_graph.m_form != SSA);
            
            Node* setLocal = nullptr;
            VirtualRegister local = m_node->local();
            
            for (unsigned i = m_nodeIndex; i--;) {
                Node* node = m_block->at(i);
                if (node->op() == SetLocal && node->local() == local) {
                    setLocal = node;
                    break;
                }
                if (accessesOverlap(m_graph, node, AbstractHeap(Stack, local)))
                    break;
            }
            
            if (!setLocal)
                break;
            
            // The Flush should become a PhantomLocal at this point. This means that we want the
            // local's value during OSR, but we don't care if the value is stored to the stack. CPS
            // rethreading can canonicalize PhantomLocals for us.
            m_node->convertFlushToPhantomLocal();
            m_graph.dethread();
            m_changed = true;
            break;
        }

        // FIXME: we should probably do this in constant folding but this currently relies on an OSR exit rule.
        // https://bugs.webkit.org/show_bug.cgi?id=154832
        case OverridesHasInstance: {
            if (!m_node->child2().node()->isCellConstant())
                break;

            if (m_node->child2().node()->asCell() != m_graph.globalObjectFor(m_node->origin.semantic)->functionProtoHasInstanceSymbolFunction()) {
                m_graph.convertToConstant(m_node, jsBoolean(true));
                m_changed = true;

            } else if (!m_graph.hasExitSite(m_node->origin.semantic, BadTypeInfoFlags)) {
                // We optimistically assume that we will not see a function that has a custom instanceof operation as they should be rare.
                m_insertionSet.insertNode(m_nodeIndex, SpecNone, CheckTypeInfoFlags, m_node->origin, OpInfo(ImplementsDefaultHasInstance), Edge(m_node->child1().node(), CellUse));
                m_graph.convertToConstant(m_node, jsBoolean(false));
                m_changed = true;
            }

            break;
        }

        // FIXME: We have a lot of string constant-folding rules here. It would be great to
        // move these to the abstract interpreter once AbstractValue can support LazyJSValue.
        // https://bugs.webkit.org/show_bug.cgi?id=155204

        case MakeRope:
        case ValueAdd:
        case StrCat: {
            String leftString = m_node->child1()->tryGetString(m_graph);
            if (!leftString)
                break;
            String rightString = m_node->child2()->tryGetString(m_graph);
            if (!rightString)
                break;
            String extraString;
            if (m_node->child3()) {
                extraString = m_node->child3()->tryGetString(m_graph);
                if (!extraString)
                    break;
            }

            StringBuilder builder;
            builder.append(leftString);
            builder.append(rightString);
            if (!!extraString)
                builder.append(extraString);

            m_node->convertToLazyJSConstant(
                m_graph, LazyJSValue::newString(m_graph, builder.toString()));
            m_changed = true;
            break;
        }

        case GetArrayLength: {
            if (m_node->arrayMode().type() == Array::Generic
                || m_node->arrayMode().type() == Array::String) {
                String string = m_node->child1()->tryGetString(m_graph);
                if (!!string) {
                    m_graph.convertToConstant(m_node, jsNumber(string.length()));
                    m_changed = true;
                }
            }
            break;
        }

        default:
            break;
        }
    }
            
    void convertToIdentityOverChild(unsigned childIndex)
    {
        m_insertionSet.insertCheck(m_nodeIndex, m_node);
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
    return runPhase<StrengthReductionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

