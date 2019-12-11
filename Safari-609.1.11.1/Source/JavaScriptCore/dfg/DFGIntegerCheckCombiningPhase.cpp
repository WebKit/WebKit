/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#include "DFGIntegerCheckCombiningPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"
#include <wtf/HashMethod.h>
#include <wtf/StdUnorderedMap.h>

namespace JSC { namespace DFG {

namespace DFGIntegerCheckCombiningPhaseInternal {
static constexpr bool verbose = false;
}

class IntegerCheckCombiningPhase : public Phase {
public:
    enum RangeKind {
        InvalidRangeKind,
        
        // This means we did ArithAdd with CheckOverflow.
        Addition,
        
        // This means we did CheckInBounds on some length.
        ArrayBounds
    };

    struct RangeKey {
        static RangeKey addition(Edge edge)
        {
            RangeKey result;
            result.m_kind = Addition;
            result.m_source = edge.sanitized();
            result.m_key = 0;
            return result;
        }
        
        static RangeKey arrayBounds(Edge edge, Node* key)
        {
            RangeKey result;
            result.m_kind = ArrayBounds;
            result.m_source = edge.sanitized();
            result.m_key = key;
            return result;
        }
        
        bool operator!() const { return m_kind == InvalidRangeKind; }
        
        unsigned hash() const
        {
            return m_kind + m_source.hash() + PtrHash<Node*>::hash(m_key);
        }
        
        bool operator==(const RangeKey& other) const
        {
            return m_kind == other.m_kind
                && m_source == other.m_source
                && m_key == other.m_key;
        }
        
        void dump(PrintStream& out) const
        {
            switch (m_kind) {
            case InvalidRangeKind:
                out.print("InvalidRangeKind(");
                break;
            case Addition:
                out.print("Addition(");
                break;
            case ArrayBounds:
                out.print("ArrayBounds(");
                break;
            }
            if (m_source)
                out.print(m_source);
            else
                out.print("null");
            out.print(", ");
            if (m_key)
                out.print(m_key);
            else
                out.print("null");
            out.print(")");
        }
        
        RangeKind m_kind { InvalidRangeKind };
        Edge m_source;
        Node* m_key { nullptr };
    };

    struct RangeKeyAndAddend {
        RangeKeyAndAddend() = default;
        
        RangeKeyAndAddend(RangeKey key, int32_t addend)
            : m_key(key)
            , m_addend(addend)
        {
        }
        
        bool operator!() const { return !m_key && !m_addend; }
        
        void dump(PrintStream& out) const
        {
            out.print(m_key, " + ", m_addend);
        }
        
        RangeKey m_key;
        int32_t m_addend { 0 };
    };

    struct Range {
        void dump(PrintStream& out) const
        {
            out.print("(", m_minBound, " @", m_minOrigin, ") .. (", m_maxBound, " @", m_maxOrigin, "), count = ", m_count, ", hoisted = ", m_hoisted);
        }
        
        int32_t m_minBound { 0 };
        int32_t m_maxBound { 0 };
        CodeOrigin m_minOrigin;
        CodeOrigin m_maxOrigin;
        unsigned m_count { 0 }; // If this is zero then the bounds won't necessarily make sense.
        bool m_hoisted { false };
        Node* m_dependency { nullptr };
    };

    IntegerCheckCombiningPhase(Graph& graph)
        : Phase(graph, "integer check combining")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        m_changed = false;
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;)
            handleBlock(blockIndex);
        
        return m_changed;
    }

private:
    void handleBlock(BlockIndex blockIndex)
    {
        BasicBlock* block = m_graph.block(blockIndex);
        if (!block)
            return;
        
        m_map.clear();
        
        // First we collect Ranges. If operations within the range have enough redundancy,
        // we hoist. And then we remove additions and checks that fall within the max range.
        
        for (auto* node : *block) {
            RangeKeyAndAddend data = rangeKeyAndAddend(node);
            if (DFGIntegerCheckCombiningPhaseInternal::verbose)
                dataLog("For ", node, ": ", data, "\n");
            if (!data)
                continue;
            
            Range& range = m_map[data.m_key];
            if (DFGIntegerCheckCombiningPhaseInternal::verbose)
                dataLog("    Range: ", range, "\n");
            if (range.m_count) {
                if (data.m_addend > range.m_maxBound) {
                    range.m_maxBound = data.m_addend;
                    range.m_maxOrigin = node->origin.semantic;
                } else if (data.m_addend < range.m_minBound) {
                    range.m_minBound = data.m_addend;
                    range.m_minOrigin = node->origin.semantic;
                }
            } else {
                range.m_maxBound = data.m_addend;
                range.m_minBound = data.m_addend;
                range.m_minOrigin = node->origin.semantic;
                range.m_maxOrigin = node->origin.semantic;
            }
            range.m_count++;
            if (DFGIntegerCheckCombiningPhaseInternal::verbose)
                dataLog("    New range: ", range, "\n");
        }
        
        for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            RangeKeyAndAddend data = rangeKeyAndAddend(node);
            if (!data)
                continue;
            Range range = m_map[data.m_key];
            if (!isValid(data.m_key, range))
                continue;
            
            // Do the hoisting.
            if (!range.m_hoisted) {
                NodeOrigin minOrigin = node->origin.withSemantic(range.m_minOrigin);
                NodeOrigin maxOrigin = node->origin.withSemantic(range.m_maxOrigin);
                
                switch (data.m_key.m_kind) {
                case Addition: {
                    if (range.m_minBound < 0)
                        insertAdd(nodeIndex, minOrigin, data.m_key.m_source, range.m_minBound);
                    if (range.m_maxBound > 0)
                        insertAdd(nodeIndex, maxOrigin, data.m_key.m_source, range.m_maxBound);
                    break;
                }
                
                case ArrayBounds: {
                    Node* minNode;
                    Node* maxNode;
                    
                    if (!data.m_key.m_source) {
                        // data.m_key.m_source being null means that we're comparing against int32 constants (see rangeKeyAndAddend()).
                        // Since CheckInBounds does an unsigned comparison, if the minBound >= 0, it is also covered by the
                        // maxBound comparison. However, if minBound < 0, then CheckInBounds should always fail its speculation check.
                        // We'll force an OSR exit in that case.
                        minNode = nullptr;
                        if (range.m_minBound < 0)
                            m_insertionSet.insertNode(nodeIndex, SpecNone, ForceOSRExit, node->origin);
                        maxNode = m_insertionSet.insertConstant(
                            nodeIndex, maxOrigin, jsNumber(range.m_maxBound));
                    } else {
                        minNode = insertAdd(
                            nodeIndex, minOrigin, data.m_key.m_source, range.m_minBound,
                            Arith::Unchecked);
                        maxNode = insertAdd(
                            nodeIndex, maxOrigin, data.m_key.m_source, range.m_maxBound,
                            Arith::Unchecked);
                    }
                    
                    Node* minCheck = nullptr;
                    if (minNode) {
                        minCheck = m_insertionSet.insertNode(
                            nodeIndex, SpecNone, CheckInBounds, node->origin,
                            Edge(minNode, Int32Use), Edge(data.m_key.m_key, Int32Use));
                    }
                    m_map[data.m_key].m_dependency = m_insertionSet.insertNode(
                        nodeIndex, SpecNone, CheckInBounds, node->origin,
                        Edge(maxNode, Int32Use), Edge(data.m_key.m_key, Int32Use), Edge(minCheck, UntypedUse));
                    break;
                }
                
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                
                m_changed = true;
                m_map[data.m_key].m_hoisted = true;
            }
            
            // Do the elimination.
            switch (data.m_key.m_kind) {
            case Addition:
                node->setArithMode(Arith::Unchecked);
                m_changed = true;
                break;
                
            case ArrayBounds:
                node->convertToIdentityOn(m_map[data.m_key].m_dependency);
                m_changed = true;
                break;
                
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        
        m_insertionSet.execute(block);
    }
    
    RangeKeyAndAddend rangeKeyAndAddend(Node* node)
    {
        switch (node->op()) {
        case ArithAdd: {
            if (node->arithMode() != Arith::CheckOverflow
                && node->arithMode() != Arith::CheckOverflowAndNegativeZero)
                break;
            if (!node->child2()->isInt32Constant())
                break;
            return RangeKeyAndAddend(
                RangeKey::addition(node->child1()),
                node->child2()->asInt32());
        }
                
        case CheckInBounds: {
            Edge source;
            int32_t addend;
            Node* key = node->child2().node();
            
            Edge index = node->child1();
            
            if (index->isInt32Constant()) {
                source = Edge();
                addend = index->asInt32();
            } else if (
                index->op() == ArithAdd
                && index->isBinaryUseKind(Int32Use)
                && index->child2()->isInt32Constant()) {
                source = index->child1();
                addend = index->child2()->asInt32();
            } else {
                source = index;
                addend = 0;
            }
            
            return RangeKeyAndAddend(RangeKey::arrayBounds(source, key), addend);
        }
            
        default:
            break;
        }
        
        return RangeKeyAndAddend();
    }
    
    bool isValid(const RangeKey& key, const Range& range)
    {
        if (range.m_count < 2)
            return false;
        
        switch (key.m_kind) {
        case ArrayBounds: {
            // Have to do this carefully because C++ compilers are too smart. But all we're really doing is detecting if
            // the difference between the bounds is 2^31 or more. If it was, then we'd have to worry about wrap-around.
            // The way we'd like to write this expression is (range.m_maxBound - range.m_minBound) >= 0, but that is a
            // signed subtraction and compare, which allows the C++ compiler to do anything it wants in case of
            // wrap-around.
            uint32_t maxBound = range.m_maxBound;
            uint32_t minBound = range.m_minBound;
            uint32_t unsignedDifference = maxBound - minBound;
            return !(unsignedDifference >> 31);
        }
            
        default:
            return true;
        }
    }
    
    Node* insertAdd(
        unsigned nodeIndex, NodeOrigin origin, Edge source, int32_t addend,
        Arith::Mode arithMode = Arith::CheckOverflow)
    {
        if (!addend)
            return source.node();
        return m_insertionSet.insertNode(
            nodeIndex, source->prediction(), source->result(),
            ArithAdd, origin, OpInfo(arithMode), source,
            m_insertionSet.insertConstantForUse(
                nodeIndex, origin, jsNumber(addend), source.useKind()));
    }
    
    using RangeMap = StdUnorderedMap<RangeKey, Range, HashMethod<RangeKey>>;
    RangeMap m_map;
    
    InsertionSet m_insertionSet;
    bool m_changed;
};
    
bool performIntegerCheckCombining(Graph& graph)
{
    return runPhase<IntegerCheckCombiningPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

