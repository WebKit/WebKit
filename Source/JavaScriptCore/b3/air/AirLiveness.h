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

#if ENABLE(B3_JIT)

#include "AirBasicBlock.h"
#include "AirCode.h"
#include "AirInstInlines.h"
#include "AirStackSlot.h"
#include "AirTmpInlines.h"
#include "B3TimingScope.h"
#include <wtf/IndexMap.h>
#include <wtf/IndexSparseSet.h>
#include <wtf/ListDump.h>

namespace JSC { namespace B3 { namespace Air {

template<Bank adapterBank, Arg::Temperature minimumTemperature = Arg::Cold>
struct TmpLivenessAdapter {
    static constexpr const char* name = "TmpLiveness";
    typedef Tmp Thing;

    TmpLivenessAdapter(Code&) { }

    static unsigned numIndices(Code& code)
    {
        unsigned numTmps = code.numTmps(adapterBank);
        return AbsoluteTmpMapper<adapterBank>::absoluteIndex(numTmps);
    }
    static bool acceptsBank(Bank bank) { return bank == adapterBank; }
    static bool acceptsRole(Arg::Role role) { return Arg::temperature(role) >= minimumTemperature; }
    static unsigned valueToIndex(Tmp tmp) { return AbsoluteTmpMapper<adapterBank>::absoluteIndex(tmp); }
    static Tmp indexToValue(unsigned index) { return AbsoluteTmpMapper<adapterBank>::tmpFromAbsoluteIndex(index); }
};

struct StackSlotLivenessAdapter {
    static constexpr const char* name = "StackSlotLiveness";
    typedef StackSlot* Thing;

    StackSlotLivenessAdapter(Code& code)
        : m_code(code)
    {
    }

    static unsigned numIndices(Code& code)
    {
        return code.stackSlots().size();
    }
    static bool acceptsBank(Bank) { return true; }
    static bool acceptsRole(Arg::Role) { return true; }
    static unsigned valueToIndex(StackSlot* stackSlot) { return stackSlot->index(); }
    StackSlot* indexToValue(unsigned index) { return m_code.stackSlots()[index]; }

private:
    Code& m_code;
};

// HEADS UP: The algorithm here is duplicated in AirRegLiveness.h.
template<typename Adapter>
class Liveness : public Adapter {
    struct Workset;
public:
    typedef typename Adapter::Thing Thing;
    typedef Vector<unsigned, 4, UnsafeVectorOverflow> IndexVector;
    
    Liveness(Code& code)
        : Adapter(code)
        , m_workset(Adapter::numIndices(code))
        , m_liveAtHead(code.size())
        , m_liveAtTail(code.size())
    {
        TimingScope timingScope(Adapter::name);
        
        // The liveAtTail of each block automatically contains the LateUse's of the terminal.
        for (BasicBlock* block : code) {
            IndexVector& liveAtTail = m_liveAtTail[block];

            block->last().forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isLateUse(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        liveAtTail.append(Adapter::valueToIndex(thing));
                });
            
            std::sort(liveAtTail.begin(), liveAtTail.end());
            removeRepeatedElements(liveAtTail);
        }

        // Blocks with new live values at tail.
        BitVector dirtyBlocks;
        for (size_t blockIndex = code.size(); blockIndex--;)
            dirtyBlocks.set(blockIndex);
        
        IndexVector mergeBuffer;
        
        bool changed;
        do {
            changed = false;

            for (size_t blockIndex = code.size(); blockIndex--;) {
                BasicBlock* block = code[blockIndex];
                if (!block)
                    continue;

                if (!dirtyBlocks.quickClear(blockIndex))
                    continue;

                LocalCalc localCalc(*this, block);
                for (size_t instIndex = block->size(); instIndex--;)
                    localCalc.execute(instIndex);

                // Handle the early def's of the first instruction.
                block->at(0).forEach<typename Adapter::Thing>(
                    [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                        if (Arg::isEarlyDef(role)
                            && Adapter::acceptsBank(bank)
                            && Adapter::acceptsRole(role))
                            m_workset.remove(Adapter::valueToIndex(thing));
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
                
                for (BasicBlock* predecessor : block->predecessors()) {
                    IndexVector& liveAtTail = m_liveAtTail[predecessor];
                    
                    if (liveAtTail.isEmpty())
                        liveAtTail = m_workset.values();
                    else {
                        mergeBuffer.resize(0);
                        mergeBuffer.reserveCapacity(liveAtTail.size() + m_workset.size());
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

    // This calculator has to be run in reverse.
    class LocalCalc {
    public:
        LocalCalc(Liveness& liveness, BasicBlock* block)
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
        public:
            Iterable(Liveness& liveness)
                : m_liveness(liveness)
            {
            }

            class iterator {
            public:
                iterator(Adapter& adapter, IndexSparseSet<UnsafeVectorOverflow>::const_iterator sparceSetIterator)
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
                IndexSparseSet<UnsafeVectorOverflow>::const_iterator m_sparceSetIterator;
            };

            iterator begin() const { return iterator(m_liveness, m_liveness.m_workset.begin()); }
            iterator end() const { return iterator(m_liveness, m_liveness.m_workset.end()); }
            
            bool contains(const typename Adapter::Thing& thing) const
            {
                return m_liveness.m_workset.contains(Adapter::valueToIndex(thing));
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
            Inst& inst = m_block->at(instIndex);
            auto& workset = m_liveness.m_workset;

            // First handle the early def's of the next instruction.
            if (instIndex + 1 < m_block->size()) {
                Inst& nextInst = m_block->at(instIndex + 1);
                nextInst.forEach<typename Adapter::Thing>(
                    [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                        if (Arg::isEarlyDef(role)
                            && Adapter::acceptsBank(bank)
                            && Adapter::acceptsRole(role))
                            workset.remove(Adapter::valueToIndex(thing));
                    });
            }
            
            // Then handle def's.
            inst.forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isLateDef(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        workset.remove(Adapter::valueToIndex(thing));
                });

            // Then handle use's.
            inst.forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isEarlyUse(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        workset.add(Adapter::valueToIndex(thing));
                });

            // And finally, handle the late use's of the previous instruction.
            if (instIndex) {
                Inst& prevInst = m_block->at(instIndex - 1);
                prevInst.forEach<typename Adapter::Thing>(
                    [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                        if (Arg::isLateUse(role)
                            && Adapter::acceptsBank(bank)
                            && Adapter::acceptsRole(role))
                            workset.add(Adapter::valueToIndex(thing));
                    });
            }
        }
        
    private:
        Liveness& m_liveness;
        BasicBlock* m_block;
    };

    const IndexVector& rawLiveAtHead(BasicBlock* block)
    {
        return m_liveAtHead[block];
    }

    template<typename UnderlyingIterable>
    class Iterable {
    public:
        Iterable(Liveness& liveness, const UnderlyingIterable& iterable)
            : m_liveness(liveness)
            , m_iterable(iterable)
        {
        }

        class iterator {
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
            return m_liveness.m_workset.contains(Adapter::valueToIndex(thing));
        }

    private:
        Liveness& m_liveness;
        const UnderlyingIterable& m_iterable;
    };

    Iterable<IndexVector> liveAtHead(BasicBlock* block)
    {
        return Iterable<IndexVector>(*this, m_liveAtHead[block]);
    }

    Iterable<IndexVector> liveAtTail(BasicBlock* block)
    {
        return Iterable<IndexVector>(*this, m_liveAtTail[block]);
    }

    IndexSparseSet<UnsafeVectorOverflow>& workset() { return m_workset; }

private:
    friend class LocalCalc;
    friend class LocalCalc::Iterable;

    IndexSparseSet<UnsafeVectorOverflow> m_workset;
    IndexMap<BasicBlock, IndexVector> m_liveAtHead;
    IndexMap<BasicBlock, IndexVector> m_liveAtTail;
};

template<Bank bank, Arg::Temperature minimumTemperature = Arg::Cold>
using TmpLiveness = Liveness<TmpLivenessAdapter<bank, minimumTemperature>>;

typedef Liveness<TmpLivenessAdapter<GP>> GPLiveness;
typedef Liveness<TmpLivenessAdapter<FP>> FPLiveness;
typedef Liveness<StackSlotLivenessAdapter> StackSlotLiveness;

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
