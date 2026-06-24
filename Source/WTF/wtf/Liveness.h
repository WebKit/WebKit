/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <algorithm>
#include <bit>
#include <span>
#include <wtf/BitVector.h>
#include <wtf/GraphOrdering.h>
#include <wtf/SparseBitVector.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

// HEADS UP: The algorithm here is duplicated in AirRegLiveness.h. That one uses sets rather
// than fancy vectors, because that's better for register liveness analysis.
template<typename Adapter>
class Liveness : public Adapter {
public:
    typedef typename Adapter::CFG CFG;
    typedef typename Adapter::Thing Thing;

    template<typename... Arguments>
    Liveness(CFG& cfg, Arguments&&... arguments)
        : Adapter(std::forward<Arguments>(arguments)...)
        , m_cfg(cfg)
    {
    }

private:
    static constexpr unsigned wordBits = 64;
    enum class Storage : uint8_t { Dense, Sparse };

    static size_t wordsForBits(unsigned numBits)
    {
        return (numBits + wordBits - 1) / wordBits;
    }

    class Workset {
    public:
        void initialize(Storage storage, size_t wordsPerSet)
        {
            m_storage = storage;
            if (storage == Storage::Dense)
                m_dense.fill(0, wordsPerSet);
            else
                m_dense.clear();
            m_sparse.clear();
        }

        void clear()
        {
            if (m_storage == Storage::Dense)
                std::ranges::fill(m_dense, 0);
            else
                m_sparse.clear();
        }

        bool add(unsigned bit)
        {
            if (m_storage == Storage::Dense) {
                ASSERT(bit / wordBits < m_dense.size());
                uint64_t mask = static_cast<uint64_t>(1) << (bit % wordBits);
                uint64_t& word = m_dense[bit / wordBits];
                bool wasSet = word & mask;
                word |= mask;
                return !wasSet;
            }
            return m_sparse.set(bit);
        }

        bool remove(unsigned bit)
        {
            if (m_storage == Storage::Dense) {
                ASSERT(bit / wordBits < m_dense.size());
                uint64_t mask = static_cast<uint64_t>(1) << (bit % wordBits);
                uint64_t& word = m_dense[bit / wordBits];
                bool wasSet = word & mask;
                word &= ~mask;
                return wasSet;
            }
            return m_sparse.reset(bit);
        }

        bool contains(unsigned bit) const
        {
            if (m_storage == Storage::Dense) {
                if (bit / wordBits >= m_dense.size())
                    return false;
                return m_dense[bit / wordBits] & (static_cast<uint64_t>(1) << (bit % wordBits));
            }
            return m_sparse.contains(bit);
        }

        void copyFromDense(std::span<const uint64_t> source)
        {
            ASSERT(m_storage == Storage::Dense);
            ASSERT(source.size() == m_dense.size());
            memcpySpan(m_dense.mutableSpan(), source);
        }

        void copyFromSparse(const SparseBitVector<>& source)
        {
            ASSERT(m_storage == Storage::Sparse);
            m_sparse = source;
        }

        Storage storage() const { return m_storage; }
        std::span<const uint64_t> denseSpan() const LIFETIME_BOUND { return m_dense.span(); }
        const SparseBitVector<>& sparse() const LIFETIME_BOUND { return m_sparse; }

    private:
        Vector<uint64_t> m_dense;
        SparseBitVector<> m_sparse;
        Storage m_storage { Storage::Dense };
    };

    class Iterator {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Iterator);
    public:
        Iterator() = default;

        static Iterator makeBegin(Liveness& liveness, std::span<const uint64_t> dense, const SparseBitVector<>* sparse, Storage storage)
        {
            Iterator iter;
            iter.m_liveness = &liveness;
            iter.m_storage = storage;
            if (storage == Storage::Dense) {
                iter.m_denseSpan = dense;
                iter.m_wordIndex = 0;
                iter.m_currentWord = dense.empty() ? 0 : dense[0];
                iter.advanceDense();
            } else
                iter.m_sparseIter = sparse->begin();
            return iter;
        }

        static Iterator makeEnd(Liveness& liveness, std::span<const uint64_t> dense, const SparseBitVector<>* sparse, Storage storage)
        {
            Iterator iter;
            iter.m_liveness = &liveness;
            iter.m_storage = storage;
            if (storage == Storage::Dense) {
                iter.m_denseSpan = dense;
                iter.m_wordIndex = dense.size();
                iter.m_currentWord = 0;
            } else
                iter.m_sparseIter = sparse->end();
            return iter;
        }

        Thing operator*() const
        {
            if (m_storage == Storage::Dense) {
                unsigned bit = static_cast<unsigned>(m_wordIndex * wordBits + std::countr_zero(m_currentWord));
                return m_liveness->indexToValue(bit);
            }
            return m_liveness->indexToValue(*m_sparseIter);
        }

        Iterator& operator++()
        {
            if (m_storage == Storage::Dense) {
                m_currentWord &= m_currentWord - 1;
                advanceDense();
            } else
                ++m_sparseIter;
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            if (m_storage == Storage::Dense)
                return m_wordIndex == other.m_wordIndex && m_currentWord == other.m_currentWord;
            return m_sparseIter == other.m_sparseIter;
        }

    private:
        void advanceDense()
        {
            while (!m_currentWord && m_wordIndex + 1 < m_denseSpan.size()) {
                ++m_wordIndex;
                m_currentWord = m_denseSpan[m_wordIndex];
            }
            if (!m_currentWord)
                m_wordIndex = m_denseSpan.size();
        }

        Liveness* m_liveness { nullptr };
        Storage m_storage { Storage::Dense };
        std::span<const uint64_t> m_denseSpan;
        size_t m_wordIndex { 0 };
        uint64_t m_currentWord { 0 };
        typename SparseBitVector<>::iterator m_sparseIter;
    };

public:
    class Iterable {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Iterable);
    public:
        Iterable(Liveness& liveness, std::span<const uint64_t> dense, const SparseBitVector<>* sparse, Storage storage)
            : m_liveness(liveness)
            , m_dense(dense)
            , m_sparse(sparse)
            , m_storage(storage)
        {
        }

        Iterator begin() const LIFETIME_BOUND { return Iterator::makeBegin(m_liveness, m_dense, m_sparse, m_storage); }
        Iterator end() const LIFETIME_BOUND { return Iterator::makeEnd(m_liveness, m_dense, m_sparse, m_storage); }

        bool contains(const Thing& thing) const
        {
            unsigned bit = m_liveness.valueToIndex(thing);
            if (m_storage == Storage::Dense) {
                if (bit / wordBits >= m_dense.size())
                    return false;
                return m_dense[bit / wordBits] & (static_cast<uint64_t>(1) << (bit % wordBits));
            }
            return m_sparse->contains(bit);
        }

    private:
        Liveness& m_liveness;
        std::span<const uint64_t> m_dense;
        const SparseBitVector<>* m_sparse { nullptr };
        Storage m_storage { Storage::Dense };
    };

    // This calculator has to be run in reverse.
    class LocalCalc {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LocalCalc);
    public:
        LocalCalc(Liveness& liveness, typename CFG::Node block)
            : m_liveness(liveness)
            , m_block(block)
        {
            Workset& workset = liveness.m_workset;
            if (liveness.m_storage == Storage::Dense)
                workset.copyFromDense(liveness.denseTailSlice(block->index()));
            else
                workset.copyFromSparse(liveness.sparseTail(block->index()));
        }

        class Iterable {
            WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Iterable);
        public:
            Iterable(Liveness& liveness)
                : m_liveness(liveness)
            {
            }

            Iterator begin() const LIFETIME_BOUND
            {
                const Workset& workset = m_liveness.m_workset;
                return Iterator::makeBegin(m_liveness, workset.denseSpan(), &workset.sparse(), workset.storage());
            }
            Iterator end() const LIFETIME_BOUND
            {
                const Workset& workset = m_liveness.m_workset;
                return Iterator::makeEnd(m_liveness, workset.denseSpan(), &workset.sparse(), workset.storage());
            }

            bool contains(const Thing& thing) const
            {
                return m_liveness.m_workset.contains(m_liveness.valueToIndex(thing));
            }

        private:
            Liveness& m_liveness;
        };

        Iterable live() const LIFETIME_BOUND
        {
            return Iterable(m_liveness);
        }

        bool isLive(const Thing& thing) const
        {
            return m_liveness.m_workset.contains(m_liveness.valueToIndex(thing));
        }

        void execute(unsigned instIndex)
        {
            Workset& workset = m_liveness.m_workset;
            m_liveness.forEachDef(
                m_block, instIndex + 1,
                [&] (unsigned index) {
                    workset.remove(index);
                });
            m_liveness.forEachUse(
                m_block, instIndex,
                [&] (unsigned index) {
                    workset.add(index);
                });
        }

    private:
        Liveness& m_liveness;
        typename CFG::Node m_block;
    };

    Iterable liveAtHead(typename CFG::Node block) LIFETIME_BOUND
    {
        if (m_storage == Storage::Dense)
            return Iterable(*this, denseHeadSlice(block->index()), nullptr, Storage::Dense);
        return Iterable(*this, { }, &sparseHead(block->index()), Storage::Sparse);
    }

    Iterable liveAtTail(typename CFG::Node block) LIFETIME_BOUND
    {
        if (m_storage == Storage::Dense)
            return Iterable(*this, denseTailSlice(block->index()), nullptr, Storage::Dense);
        return Iterable(*this, { }, &sparseTail(block->index()), Storage::Sparse);
    }

    class LiveAtHead {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LiveAtHead);
    public:
        LiveAtHead(Liveness& liveness)
            : m_liveness(liveness)
        {
        }

        bool isLiveAtHead(typename CFG::Node block, const Thing& thing)
        {
            unsigned bit = m_liveness.valueToIndex(thing);
            if (m_liveness.m_storage == Storage::Dense) {
                std::span<const uint64_t> set = m_liveness.denseHeadSlice(block->index());
                if (bit / wordBits >= set.size())
                    return false;
                return set[bit / wordBits] & (static_cast<uint64_t>(1) << (bit % wordBits));
            }
            return m_liveness.sparseHead(block->index()).contains(bit);
        }

    private:
        Liveness& m_liveness;
    };

    LiveAtHead liveAtHead() LIFETIME_BOUND { return LiveAtHead(*this); }

protected:
    void compute()
    {
        uint64_t denseMatrixBits = static_cast<uint64_t>(m_cfg.numNodes()) * Adapter::numIndices();
        constexpr uint64_t denseMatrixBitBudget = 32 * 1024 * 1024; // 4 MB per live-set matrix.
        bool useSparse = denseMatrixBits > denseMatrixBitBudget;
#if ASSERT_ENABLED
        // Force the sparse storage on roughly half of the otherwise-dense functions in debug builds so
        // the sparse implementation gets coverage from the regular test suite.
        if (!useSparse && (m_cfg.numNodes() & 1))
            useSparse = true;
#endif
        if (useSparse)
            computeSparse();
        else
            computeDense();
    }

    void computeDense()
    {
        Adapter::prepareToCompute();

        unsigned numNodes = m_cfg.numNodes();
        unsigned numIndices = Adapter::numIndices();

        m_storage = Storage::Dense;
        m_wordsPerSet = wordsForBits(numIndices);

        size_t matrixWords = static_cast<size_t>(numNodes) * m_wordsPerSet;
        m_denseStore.fill(0, 2 * matrixWords);
        Vector<uint64_t> genStore(matrixWords);
        Vector<uint64_t> killStore(matrixWords);
        // Vector's sized constructor does not zero POD storage, and the dataflow ORs into these.
        std::ranges::fill(genStore, 0);
        std::ranges::fill(killStore, 0);
        std::span<uint64_t> store = m_denseStore.mutableSpan();
        std::span<uint64_t> liveInMatrix = store.subspan(0, matrixWords);
        std::span<uint64_t> liveOutMatrix = store.subspan(matrixWords, matrixWords);
        std::span<uint64_t> genMatrix = genStore.mutableSpan();
        std::span<uint64_t> killMatrix = killStore.mutableSpan();

        m_workset.initialize(Storage::Dense, m_wordsPerSet);

        auto setFor = [&] (std::span<uint64_t> matrix, unsigned blockIndex) -> std::span<uint64_t> {
            return matrix.subspan(static_cast<size_t>(blockIndex) * m_wordsPerSet, m_wordsPerSet);
        };
        auto setBit = [] (std::span<uint64_t> set, unsigned index) {
            set[index / wordBits] |= static_cast<uint64_t>(1) << (index % wordBits);
        };

        unsigned numActiveBlocks = 0;
        for (unsigned blockIndex = 0; blockIndex < numNodes; ++blockIndex) {
            auto block = m_cfg.node(blockIndex);
            if (!block)
                continue;

            auto killSet = setFor(killMatrix, blockIndex);
            auto genSet = setFor(genMatrix, blockIndex);
            auto liveOutSet = setFor(liveOutMatrix, blockIndex);

            for (size_t boundary = 0; boundary <= Adapter::blockSize(block); ++boundary) {
                Adapter::forEachDef(block, boundary, [&] (unsigned index) {
                    setBit(killSet, index);
                });
            }

            // gen = transfer function applied to an empty live-out: the uses exposed at the head.
            // The fixpoint below works purely on gen/kill/liveOut, so this is the only place that
            // walks instructions.
            m_workset.clear();
            for (size_t instIndex = Adapter::blockSize(block); instIndex--;) {
                Adapter::forEachDef(block, instIndex + 1, [&] (unsigned index) { m_workset.remove(index); });
                Adapter::forEachUse(block, instIndex, [&] (unsigned index) { m_workset.add(index); });
            }
            Adapter::forEachDef(block, 0, [&] (unsigned index) { m_workset.remove(index); });
            std::span<const uint64_t> worksetSpan = m_workset.denseSpan();
            for (size_t i = 0; i < m_wordsPerSet; ++i)
                genSet[i] = worksetSpan[i];

            // liveOut automatically contains the LateUse's of the terminal.
            Adapter::forEachUse(block, Adapter::blockSize(block), [&] (unsigned index) {
                setBit(liveOutSet, index);
            });

            ++numActiveBlocks;
        }

        // Reverse-post-order seed so the backward dataflow processes a block after its successors.
        Vector<unsigned, 64> worklist;
        worklist.reserveInitialCapacity(numActiveBlocks);
        BitVector dirtyBlocks(numNodes);
        appendNodeIndicesInOrder(m_cfg, GraphOrder::PostOrder, dirtyBlocks, worklist);
        worklist.reverse();
        // Active blocks the DFS did not reach (e.g. unreachable but not yet pruned).
        for (unsigned blockIndex = 0; blockIndex < numNodes; ++blockIndex) {
            if (m_cfg.node(blockIndex) && !dirtyBlocks.quickSet(blockIndex))
                worklist.append(blockIndex);
        }

        while (!worklist.isEmpty()) {
            unsigned blockIndex = worklist.takeLast();
            bool cleared = dirtyBlocks.quickClear(blockIndex);
            ASSERT_UNUSED(cleared, cleared);

            auto block = m_cfg.node(blockIndex);
            ASSERT(block);

            auto liveInSet = setFor(liveInMatrix, blockIndex);
            auto liveOutSet = setFor(liveOutMatrix, blockIndex);
            auto genSet = setFor(genMatrix, blockIndex);
            auto killSet = setFor(killMatrix, blockIndex);

            // liveIn = gen | (liveOut & ~kill)
            uint64_t changed = 0;
            for (size_t i = 0; i < m_wordsPerSet; ++i) {
                uint64_t newLiveIn = genSet[i] | (liveOutSet[i] & ~killSet[i]);
                uint64_t oldLiveIn = liveInSet[i];
                liveInSet[i] = newLiveIn;
                changed |= newLiveIn ^ oldLiveIn;
            }
            if (!changed)
                continue;

            for (auto predecessor : m_cfg.predecessors(block)) {
                auto predLiveOut = setFor(liveOutMatrix, predecessor->index());
                uint64_t predChanged = 0;
                for (size_t i = 0; i < m_wordsPerSet; ++i) {
                    uint64_t oldLiveOut = predLiveOut[i];
                    uint64_t newLiveOut = oldLiveOut | liveInSet[i];
                    predLiveOut[i] = newLiveOut;
                    predChanged |= newLiveOut ^ oldLiveOut;
                }
                if (predChanged && !dirtyBlocks.quickSet(predecessor->index()))
                    worklist.append(predecessor->index());
            }
        }
    }

    // Sparse fallback for functions whose dense matrices would exceed the budget.
    void computeSparse()
    {
        Adapter::prepareToCompute();

        unsigned numNodes = m_cfg.numNodes();

        m_storage = Storage::Sparse;
        m_wordsPerSet = 0;
        m_sparseStore.clear();
        m_sparseStore.grow(2 * numNodes);
        m_workset.initialize(Storage::Sparse, 0);

        BitVector dirtyBlocks(numNodes);
        Vector<unsigned, 64> worklist;
        for (unsigned blockIndex = 0; blockIndex < numNodes; ++blockIndex) {
            auto block = m_cfg.node(blockIndex);
            if (!block)
                continue;

            // liveAtTail automatically contains the LateUse's of the terminal.
            auto& liveAtTail = sparseTail(blockIndex);
            Adapter::forEachUse(
                block, Adapter::blockSize(block),
                [&] (unsigned index) {
                    liveAtTail.set(index);
                });

            dirtyBlocks.quickSet(blockIndex);
            worklist.append(blockIndex);
        }

        Vector<unsigned, 64> delta;
        while (!worklist.isEmpty()) {
            unsigned blockIndex = worklist.takeLast();
            bool cleared = dirtyBlocks.quickClear(blockIndex);
            ASSERT_UNUSED(cleared, cleared);

            auto block = m_cfg.node(blockIndex);
            ASSERT(block);

            m_workset.clear();
            sparseTail(blockIndex).forEachSetBit(
                [&] (unsigned index) {
                    m_workset.add(index);
                });
            for (size_t instIndex = Adapter::blockSize(block); instIndex--;) {
                Adapter::forEachDef(block, instIndex + 1, [&] (unsigned index) { m_workset.remove(index); });
                Adapter::forEachUse(block, instIndex, [&] (unsigned index) { m_workset.add(index); });
            }
            Adapter::forEachDef(block, 0, [&] (unsigned index) { m_workset.remove(index); });

            auto& liveAtHead = sparseHead(blockIndex);
            delta.shrink(0);
            m_workset.sparse().forEachSetBit([&] (unsigned index) {
                if (liveAtHead.set(index))
                    delta.append(index);
            });

            if (delta.isEmpty())
                continue;

            for (auto predecessor : m_cfg.predecessors(block)) {
                auto& predTail = sparseTail(predecessor->index());
                bool changed = false;
                for (unsigned index : delta) {
                    if (predTail.set(index))
                        changed = true;
                }
                if (changed && !dirtyBlocks.quickSet(predecessor->index()))
                    worklist.append(predecessor->index());
            }
        }
    }

private:
    friend class LocalCalc;
    friend class LocalCalc::Iterable;
    friend class Iterable;
    friend class LiveAtHead;

    size_t denseHalfWords() const { return m_denseStore.size() / 2; }
    std::span<const uint64_t> denseHeadSlice(unsigned blockIndex) const LIFETIME_BOUND
    {
        return m_denseStore.span().subspan(static_cast<size_t>(blockIndex) * m_wordsPerSet, m_wordsPerSet);
    }
    std::span<const uint64_t> denseTailSlice(unsigned blockIndex) const LIFETIME_BOUND
    {
        return m_denseStore.span().subspan(denseHalfWords() + static_cast<size_t>(blockIndex) * m_wordsPerSet, m_wordsPerSet);
    }

    size_t sparseHalfSize() const { return m_sparseStore.size() / 2; }
    SparseBitVector<>& sparseHead(unsigned blockIndex) LIFETIME_BOUND { return m_sparseStore[blockIndex]; }
    const SparseBitVector<>& sparseHead(unsigned blockIndex) const LIFETIME_BOUND { return m_sparseStore[blockIndex]; }
    SparseBitVector<>& sparseTail(unsigned blockIndex) LIFETIME_BOUND { return m_sparseStore[sparseHalfSize() + blockIndex]; }
    const SparseBitVector<>& sparseTail(unsigned blockIndex) const LIFETIME_BOUND { return m_sparseStore[sparseHalfSize() + blockIndex]; }

    CFG& m_cfg;
    Workset m_workset;
    Vector<uint64_t> m_denseStore;
    Vector<SparseBitVector<>> m_sparseStore;
    size_t m_wordsPerSet { 0 };
    Storage m_storage { Storage::Dense };
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
