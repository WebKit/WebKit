/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "DFGLivenessAnalysisPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGFlowIndexing.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"
#include <wtf/BitVector.h>
#include <wtf/IndexMap.h>
#include <wtf/IndexSparseSet.h>
#include <wtf/SparseBitVector.h>

namespace JSC { namespace DFG {

namespace {

using Workset = IndexSparseSet<unsigned, DefaultIndexSparseSetTraits<unsigned>, UnsafeVectorOverflow>;

class DenseTailStorage {
public:
    class Row {
    public:
        explicit Row(std::span<uint64_t> words)
            : m_words(words)
        { }

        bool add(unsigned bit)
        {
            uint64_t& word = m_words[bit / 64];
            uint64_t mask = static_cast<uint64_t>(1) << (bit % 64);
            bool wasSet = word & mask;
            word |= mask;
            return !wasSet;
        }

        template<typename Func>
        void forEachSetBit(const Func& func) const
        {
            WTF::forEachSetBit(std::span<const uint64_t> { m_words }, func);
        }

    private:
        std::span<uint64_t> m_words;
    };

    DenseTailStorage(Graph&, unsigned numBlocks, unsigned numIndices)
        : m_wordsPerSet((numIndices + 63) / 64)
        , m_store(static_cast<size_t>(numBlocks) * m_wordsPerSet)
    {
        std::ranges::fill(m_store, 0);
    }

    Row operator[](BlockIndex blockIndex)
    {
        return Row(m_store.mutableSpan().subspan(static_cast<size_t>(blockIndex) * m_wordsPerSet, m_wordsPerSet));
    }
    Row operator[](BasicBlock* block) { return (*this)[block->index()]; }

private:
    size_t m_wordsPerSet { 0 };
    Vector<uint64_t> m_store;
};

class SparseTailStorage {
public:
    SparseTailStorage(Graph& graph, unsigned, unsigned)
        : m_map(graph.numBlocks())
    { }

    SparseBitVector<>& operator[](BlockIndex blockIndex) LIFETIME_BOUND { return m_map[blockIndex]; }
    SparseBitVector<>& operator[](BasicBlock* block) LIFETIME_BOUND { return m_map[block]; }

private:
    IndexMap<BasicBlock*, SparseBitVector<>> m_map;
};

template<typename TailStorage>
class LivenessAnalysisPhase : public Phase {
public:
    LivenessAnalysisPhase(Graph& graph)
        : Phase(graph, "liveness analysis"_s)
        , m_dirtyBlocks(m_graph.numBlocks())
        , m_indexing(*m_graph.m_indexingCache)
        , m_liveAtHead(m_graph.numBlocks())
        , m_liveAtTail(m_graph, m_graph.numBlocks(), m_indexing.numIndices())
        , m_workset(m_indexing.numIndices())
    {
    }

    bool run()
    {
        BlockIndex numBlock = m_graph.numBlocks();
        m_dirtyBlocks.ensureSize(numBlock);
        m_worklist.reserveInitialCapacity(numBlock);
        for (BlockIndex blockIndex = 0; blockIndex < numBlock; ++blockIndex) {
            if (m_graph.block(blockIndex)) {
                m_dirtyBlocks.quickSet(blockIndex);
                m_worklist.append(blockIndex);
            }
        }

        // Fixpoint until we do not add any new live values at tail.
        while (!m_worklist.isEmpty()) {
            BlockIndex blockIndex = m_worklist.takeLast();
            bool cleared = m_dirtyBlocks.quickClear(blockIndex);
            ASSERT_UNUSED(cleared, cleared);
            processBlock(blockIndex);
        }

        // Update the per-block node list for live and tail.
        for (BlockIndex blockIndex = numBlock; blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;

            {
                block->ssa->liveAtHead = m_liveAtHead[blockIndex].map([this](auto index) {
                    return m_indexing.nodeProjection(index);
                });
            }
            {
                auto& liveAtTail = block->ssa->liveAtTail;
                liveAtTail.shrink(0);
                m_liveAtTail[blockIndex].forEachSetBit([&](size_t index) {
                    liveAtTail.append(m_indexing.nodeProjection(static_cast<unsigned>(index)));
                });
            }
        }

        return true;
    }

private:
    void processBlock(BlockIndex blockIndex)
    {
        BasicBlock* block = m_graph.block(blockIndex);
        ASSERT_WITH_MESSAGE(block, "Only dirty blocks needs updates. A null block should never be dirty.");

        m_workset.clear();
        m_liveAtTail[blockIndex].forEachSetBit([&](size_t index) {
            m_workset.add(static_cast<unsigned>(index));
        });

        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);

            auto handleEdge = [&] (Edge& edge) {
                bool newEntry = m_workset.add(m_indexing.index(edge.node()));
                edge.setKillStatus(newEntry ? DoesKill : DoesNotKill);
            };
            
            switch (node->op()) {
            case Upsilon: {
                ASSERT_WITH_MESSAGE(!m_workset.contains(node->index()), "Upsilon should not be used as defs by other nodes.");

                Node* phi = node->phi();
                m_workset.remove(m_indexing.shadowIndex(phi));
                handleEdge(node->child1());
                break;
            }
            case Phi: {
                m_workset.remove(m_indexing.index(node));
                m_workset.add(m_indexing.shadowIndex(node));
                break;
            }
            default:
                m_workset.remove(m_indexing.index(node));
                m_graph.doToChildren(node, handleEdge);
                break;
            }
        }

        // Update live at head.
        auto& liveAtHead = m_liveAtHead[blockIndex];
        if (m_workset.size() == liveAtHead.size())
            return;

        for (unsigned liveIndexAtHead : liveAtHead)
            m_workset.remove(liveIndexAtHead);
        ASSERT(!m_workset.isEmpty());

        liveAtHead.appendRange(m_workset.begin(), m_workset.end());

        for (BasicBlock* predecessor : block->predecessors) {
            auto&& liveAtTail = m_liveAtTail[predecessor];
            for (unsigned newValue : m_workset) {
                if (liveAtTail.add(newValue)) {
                    if (!m_dirtyBlocks.quickSet(predecessor->index()))
                        m_worklist.append(predecessor->index());
                }
            }
        }
    }

    // Blocks with new live values at tail.
    BitVector m_dirtyBlocks;

    FlowIndexing& m_indexing;

    // Live values per block edge.
    IndexMap<BasicBlock*, Vector<unsigned, 0, UnsafeVectorOverflow, 1>> m_liveAtHead;
    TailStorage m_liveAtTail;

    // Single sparse set allocated once and used by every basic block.
    Workset m_workset;

    Vector<BlockIndex, 64> m_worklist;
};

} // anonymous namespace

bool performGraphPackingAndLivenessAnalysis(Graph& graph)
{
    graph.packNodeIndices();
#ifndef NDEBUG
    graph.clearAbstractValues();
#endif
    graph.m_indexingCache->recompute();

    // Pick the live-at-tail representation by the same memory budget WTF::Liveness uses. The dense
    // bit-matrix is numBlocks * numIndices bits; once that would exceed the budget, fall back to the
    // SparseBitVector representation whose memory is proportional to the live values instead.
    uint64_t denseMatrixBits = static_cast<uint64_t>(graph.numBlocks()) * graph.m_indexingCache->numIndices();
    constexpr uint64_t denseMatrixBitBudget = 32 * 1024 * 1024; // 4 MB.
    bool useSparse = denseMatrixBits > denseMatrixBitBudget;
#if ASSERT_ENABLED
    // Most DFG functions are small enough that the dense path would always win. Force the sparse
    // path on roughly half of them in debug builds so the sparse implementation gets meaningful
    // coverage from the regular test suite.
    if (!useSparse && (graph.numBlocks() & 1))
        useSparse = true;
#endif
    if (useSparse)
        return runPhase<LivenessAnalysisPhase<SparseTailStorage>>(graph);
    return runPhase<LivenessAnalysisPhase<DenseTailStorage>>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

