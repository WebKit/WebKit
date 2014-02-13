/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "DFGIntegerCheckCombiningPhase.h"

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"
#include <unordered_map>
#include <wtf/HashMethod.h>

namespace JSC { namespace DFG {

namespace {

static const bool verbose = false;

enum RangeKind {
    InvalidRangeKind,
    
    // This means we did ArithAdd with CheckOverflow.
    Addition,
    
    // This means we did CheckInBounds on some length.
    ArrayBounds
};

struct RangeKey {
    RangeKey()
        : m_kind(InvalidRangeKind)
        , m_key(nullptr)
    {
    }
    
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
        out.print(m_source, ", ", m_key, ")");
    }
    
    RangeKind m_kind;
    Edge m_source;
    Node* m_key;
};

struct RangeKeyAndAddend {
    RangeKeyAndAddend()
        : m_addend(0)
    {
    }
    
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
    int32_t m_addend;
};

struct Range {
    Range()
        : m_minBound(0)
        , m_maxBound(0)
        , m_count(0)
        , m_hoisted(false)
    {
    }
    
    void dump(PrintStream& out) const
    {
        out.print("(", m_minBound, " @", m_minOrigin, ") .. (", m_maxBound, " @", m_maxOrigin, "), count = ", m_count, ", hoisted = ", m_hoisted);
    }
    
    int32_t m_minBound;
    CodeOrigin m_minOrigin;
    int32_t m_maxBound;
    CodeOrigin m_maxOrigin;
    unsigned m_count; // If this is zero then the bounds won't necessarily make sense.
    bool m_hoisted;
};

} // anonymous namespace

class IntegerCheckCombiningPhase : public Phase {
public:
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
        
        for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            RangeKeyAndAddend data = rangeKeyAndAddend(node);
            if (verbose)
                dataLog("For ", node, ": ", data, "\n");
            if (!data)
                continue;
            
            Range& range = m_map[data.m_key];
            if (verbose)
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
            if (verbose)
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
                switch (data.m_key.m_kind) {
                case Addition: {
                    if (range.m_minBound < 0) {
                        insertMustAdd(
                            nodeIndex, NodeOrigin(range.m_minOrigin, node->origin.forExit),
                            data.m_key.m_source, range.m_minBound);
                    }
                    if (range.m_maxBound > 0) {
                        insertMustAdd(
                            nodeIndex, NodeOrigin(range.m_maxOrigin, node->origin.forExit),
                            data.m_key.m_source, range.m_maxBound);
                    }
                    break;
                }
                
                case ArrayBounds: {
                    Node* minNode;
                    Node* maxNode;
                    
                    if (!data.m_key.m_source) {
                        minNode = 0;
                        maxNode = m_insertionSet.insertConstant(
                            nodeIndex, range.m_maxOrigin, jsNumber(range.m_maxBound));
                    } else {
                        minNode = insertAdd(
                            nodeIndex, NodeOrigin(range.m_minOrigin, node->origin.forExit),
                            data.m_key.m_source, range.m_minBound, Arith::Unchecked);
                        maxNode = insertAdd(
                            nodeIndex, NodeOrigin(range.m_maxOrigin, node->origin.forExit),
                            data.m_key.m_source, range.m_maxBound, Arith::Unchecked);
                    }
                    
                    if (minNode) {
                        m_insertionSet.insertNode(
                            nodeIndex, SpecNone, CheckInBounds, node->origin,
                            Edge(minNode, Int32Use), Edge(data.m_key.m_key, Int32Use));
                    }
                    m_insertionSet.insertNode(
                        nodeIndex, SpecNone, CheckInBounds, node->origin,
                        Edge(maxNode, Int32Use), Edge(data.m_key.m_key, Int32Use));
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
                node->convertToPhantom();
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
            if (!m_graph.isInt32Constant(node->child2().node()))
                break;
            return RangeKeyAndAddend(
                RangeKey::addition(node->child1()),
                m_graph.valueOfInt32Constant(node->child2().node()));
        }
                
        case CheckInBounds: {
            Edge source;
            int32_t addend;
            Node* key = node->child2().node();
            
            Edge index = node->child1();
            
            if (m_graph.isInt32Constant(index.node())) {
                source = Edge();
                addend = m_graph.valueOfInt32Constant(index.node());
            } else if (
                index->op() == ArithAdd
                && index->isBinaryUseKind(Int32Use)
                && m_graph.isInt32Constant(index->child2().node())) {
                source = index->child1();
                addend = m_graph.valueOfInt32Constant(index->child2().node());
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
        case ArrayBounds:
            return (range.m_maxBound - range.m_minBound) >= 0;
            
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
            nodeIndex, source->prediction(), ArithAdd, origin, OpInfo(arithMode),
            source, Edge(
                m_insertionSet.insertConstant(nodeIndex, origin, jsNumber(addend)),
                source.useKind()));
    }
    
    Node* insertMustAdd(
        unsigned nodeIndex, NodeOrigin origin, Edge source, int32_t addend)
    {
        Node* result = insertAdd(nodeIndex, origin, source, addend);
        m_insertionSet.insertNode(
            nodeIndex, SpecNone, HardPhantom, origin, Edge(result, UntypedUse));
        return result;
    }
    
    typedef std::unordered_map<RangeKey, Range, HashMethod<RangeKey>> RangeMap;
    RangeMap m_map;
    
    InsertionSet m_insertionSet;
    bool m_changed;
};
    
bool performIntegerCheckCombining(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Integer Check Combining Phase");
    return runPhase<IntegerCheckCombiningPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

