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

#include <wtf/BitVector.h>
#include <wtf/IndexSparseSet.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// HEADS UP: The algorithm here is duplicated in AirRegLiveness.h. That one uses sets rather
// than fancy vectors, because that's better for register liveness analysis.
template<typename Adapter>
class Liveness : public Adapter {
public:
    typedef typename Adapter::CFG CFG;
    typedef typename Adapter::Thing Thing;
    typedef Vector<unsigned, 4, UnsafeVectorOverflow> IndexVector;
    typedef IndexSparseSet<unsigned, DefaultIndexSparseSetTraits<unsigned>, UnsafeVectorOverflow> Workset;
    
    template<typename... Arguments>
    Liveness(CFG& cfg, Arguments&&... arguments)
        : Adapter(std::forward<Arguments>(arguments)...)
        , m_cfg(cfg)
        , m_workset(Adapter::numIndices())
        , m_liveAtHead(cfg.template newMap<IndexVector>())
        , m_liveAtTail(cfg.template newMap<IndexVector>())
    {
    }
    
    // This calculator has to be run in reverse.
    class LocalCalc {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        LocalCalc(Liveness& liveness, typename CFG::Node block)
            : m_liveness(liveness)
            , m_block(block)
        {
            auto& workset = liveness.m_workset;
            workset.clear();
            IndexVector& liveAtTail = liveness.m_liveAtTail[block];
            for (unsigned index : liveAtTail)
                workset.add(index);
        }

        class Iterable {
            WTF_MAKE_FAST_ALLOCATED;
        public:
            Iterable(Liveness& liveness)
                : m_liveness(liveness)
            {
            }

            class iterator {
                WTF_MAKE_FAST_ALLOCATED;
            public:
                iterator(Adapter& adapter, Workset::const_iterator sparceSetIterator)
                    : m_adapter(adapter)
                    , m_sparceSetIterator(sparceSetIterator)
                {
                }

                iterator& operator++()
                {
                    ++m_sparceSetIterator;
                    return *this;
                }

                typename Adapter::Thing operator*() const
                {
                    return m_adapter.indexToValue(*m_sparceSetIterator);
                }

                bool operator==(const iterator& other) { return m_sparceSetIterator == other.m_sparceSetIterator; }
                bool operator!=(const iterator& other) { return m_sparceSetIterator != other.m_sparceSetIterator; }

            private:
                Adapter& m_adapter;
                Workset::const_iterator m_sparceSetIterator;
            };

            iterator begin() const { return iterator(m_liveness, m_liveness.m_workset.begin()); }
            iterator end() const { return iterator(m_liveness, m_liveness.m_workset.end()); }
            
            bool contains(const typename Adapter::Thing& thing) const
            {
                return m_liveness.m_workset.contains(m_liveness.valueToIndex(thing));
            }

        private:
            Liveness& m_liveness;
        };

        Iterable live() const
        {
            return Iterable(m_liveness);
        }

        bool isLive(const typename Adapter::Thing& thing) const
        {
            return live().contains(thing);
        }

        void execute(unsigned instIndex)
        {
            auto& workset = m_liveness.m_workset;

            // Want an easy example to help you visualize how this works?
            // Check out B3VariableLiveness.h.
            //
            // Want a hard example to help you understand the hard cases?
            // Check out AirLiveness.h.
            
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

    const IndexVector& rawLiveAtHead(typename CFG::Node block)
    {
        return m_liveAtHead[block];
    }

    template<typename UnderlyingIterable>
    class Iterable {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Iterable(Liveness& liveness, const UnderlyingIterable& iterable)
            : m_liveness(liveness)
            , m_iterable(iterable)
        {
        }

        class iterator {
            WTF_MAKE_FAST_ALLOCATED;
        public:
            iterator()
                : m_liveness(nullptr)
                , m_iter()
            {
            }
            
            iterator(Liveness& liveness, typename UnderlyingIterable::const_iterator iter)
                : m_liveness(&liveness)
                , m_iter(iter)
            {
            }

            typename Adapter::Thing operator*()
            {
                return m_liveness->indexToValue(*m_iter);
            }

            iterator& operator++()
            {
                ++m_iter;
                return *this;
            }

            bool operator==(const iterator& other) const
            {
                ASSERT(m_liveness == other.m_liveness);
                return m_iter == other.m_iter;
            }

            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }

        private:
            Liveness* m_liveness;
            typename UnderlyingIterable::const_iterator m_iter;
        };

        iterator begin() const { return iterator(m_liveness, m_iterable.begin()); }
        iterator end() const { return iterator(m_liveness, m_iterable.end()); }

        bool contains(const typename Adapter::Thing& thing) const
        {
            return m_liveness.m_workset.contains(m_liveness.valueToIndex(thing));
        }

    private:
        Liveness& m_liveness;
        const UnderlyingIterable& m_iterable;
    };

    Iterable<IndexVector> liveAtHead(typename CFG::Node block)
    {
        return Iterable<IndexVector>(*this, m_liveAtHead[block]);
    }

    Iterable<IndexVector> liveAtTail(typename CFG::Node block)
    {
        return Iterable<IndexVector>(*this, m_liveAtTail[block]);
    }

    Workset& workset() { return m_workset; }
    
    class LiveAtHead {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        LiveAtHead(Liveness& liveness)
            : m_liveness(liveness)
        {
            for (unsigned blockIndex = m_liveness.m_cfg.numNodes(); blockIndex--;) {
                typename CFG::Node block = m_liveness.m_cfg.node(blockIndex);
                if (!block)
                    continue;
                
                std::sort(m_liveness.m_liveAtHead[block].begin(), m_liveness.m_liveAtHead[block].end());
            }
        }
        
        bool isLiveAtHead(typename CFG::Node block, const typename Adapter::Thing& thing)
        {
            return !!tryBinarySearch<unsigned>(m_liveness.m_liveAtHead[block], m_liveness.m_liveAtHead[block].size(), m_liveness.valueToIndex(thing), [] (unsigned* value) { return *value; });
        }
        
    private:
        Liveness& m_liveness;
    };
    
    LiveAtHead liveAtHead() { return LiveAtHead(*this); }

protected:
    void compute()
    {
        Adapter::prepareToCompute();
        
        // The liveAtTail of each block automatically contains the LateUse's of the terminal.
        for (unsigned blockIndex = m_cfg.numNodes(); blockIndex--;) {
            typename CFG::Node block = m_cfg.node(blockIndex);
            if (!block)
                continue;
            
            IndexVector& liveAtTail = m_liveAtTail[block];

            Adapter::forEachUse(
                block, Adapter::blockSize(block),
                [&] (unsigned index) {
                    liveAtTail.append(index);
                });
            
            std::sort(liveAtTail.begin(), liveAtTail.end());
            removeRepeatedElements(liveAtTail);
        }

        // Blocks with new live values at tail.
        BitVector dirtyBlocks;
        for (size_t blockIndex = m_cfg.numNodes(); blockIndex--;)
            dirtyBlocks.set(blockIndex);
        
        IndexVector mergeBuffer;
        
        bool changed;
        do {
            changed = false;

            for (size_t blockIndex = m_cfg.numNodes(); blockIndex--;) {
                typename CFG::Node block = m_cfg.node(blockIndex);
                if (!block)
                    continue;

                if (!dirtyBlocks.quickClear(blockIndex))
                    continue;

                LocalCalc localCalc(*this, block);
                for (size_t instIndex = Adapter::blockSize(block); instIndex--;)
                    localCalc.execute(instIndex);

                // Handle the early def's of the first instruction.
                Adapter::forEachDef(
                    block, 0,
                    [&] (unsigned index) {
                        m_workset.remove(index);
                    });

                IndexVector& liveAtHead = m_liveAtHead[block];

                // We only care about Tmps that were discovered in this iteration. It is impossible
                // to remove a live value from the head.
                // We remove all the values we already knew about so that we only have to deal with
                // what is new in LiveAtHead.
                if (m_workset.size() == liveAtHead.size())
                    m_workset.clear();
                else {
                    for (unsigned liveIndexAtHead : liveAtHead)
                        m_workset.remove(liveIndexAtHead);
                }

                if (m_workset.isEmpty())
                    continue;

                liveAtHead.reserveCapacity(liveAtHead.size() + m_workset.size());
                for (unsigned newValue : m_workset)
                    liveAtHead.uncheckedAppend(newValue);
                
                m_workset.sort();
                
                for (typename CFG::Node predecessor : m_cfg.predecessors(block)) {
                    IndexVector& liveAtTail = m_liveAtTail[predecessor];
                    
                    if (liveAtTail.isEmpty())
                        liveAtTail = m_workset.values();
                    else {
                        mergeBuffer.resize(liveAtTail.size() + m_workset.size());
                        auto iter = mergeDeduplicatedSorted(
                            liveAtTail.begin(), liveAtTail.end(),
                            m_workset.begin(), m_workset.end(),
                            mergeBuffer.begin());
                        mergeBuffer.resize(iter - mergeBuffer.begin());
                        
                        if (mergeBuffer.size() == liveAtTail.size())
                            continue;
                    
                        RELEASE_ASSERT(mergeBuffer.size() > liveAtTail.size());
                        liveAtTail = mergeBuffer;
                    }
                    
                    dirtyBlocks.quickSet(predecessor->index());
                    changed = true;
                }
            }
        } while (changed);
    }

private:
    friend class LocalCalc;
    friend class LocalCalc::Iterable;

    CFG& m_cfg;
    Workset m_workset;
    typename CFG::template Map<IndexVector> m_liveAtHead;
    typename CFG::template Map<IndexVector> m_liveAtTail;
};

} // namespace WTF

