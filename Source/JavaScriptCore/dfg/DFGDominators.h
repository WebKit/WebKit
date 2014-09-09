/*
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGDominators_h
#define DFGDominators_h

#if ENABLE(DFG_JIT)

#include "DFGAnalysis.h"
#include "DFGBasicBlock.h"
#include "DFGBlockMap.h"
#include "DFGBlockSet.h"
#include "DFGCommon.h"

namespace JSC { namespace DFG {

class Graph;

class Dominators : public Analysis<Dominators> {
public:
    Dominators();
    ~Dominators();
    
    void compute(Graph&);
    
    bool strictlyDominates(BasicBlock* from, BasicBlock* to) const
    {
        ASSERT(isValid());
        return m_data[to].preNumber > m_data[from].preNumber
            && m_data[to].postNumber < m_data[from].postNumber;
    }
    
    bool dominates(BasicBlock* from, BasicBlock* to) const
    {
        return from == to || strictlyDominates(from, to);
    }
    
    BasicBlock* immediateDominatorOf(BasicBlock* block) const
    {
        return m_data[block].idomParent;
    }
    
    template<typename Functor>
    void forAllStrictDominatorsOf(BasicBlock* to, const Functor& functor) const
    {
        for (BasicBlock* block = m_data[to].idomParent; block; block = m_data[block].idomParent)
            functor(block);
    }
    
    template<typename Functor>
    void forAllDominatorsOf(BasicBlock* to, const Functor& functor) const
    {
        for (BasicBlock* block = to; block; block = m_data[block].idomParent)
            functor(block);
    }
    
    template<typename Functor>
    void forAllBlocksStrictlyDominatedBy(BasicBlock* from, const Functor& functor) const
    {
        Vector<BasicBlock*, 16> worklist;
        worklist.appendVector(m_data[from].idomKids);
        while (!worklist.isEmpty()) {
            BasicBlock* block = worklist.takeLast();
            functor(block);
            worklist.appendVector(m_data[block].idomKids);
        }
    }
    
    template<typename Functor>
    void forAllBlocksDominatedBy(BasicBlock* from, const Functor& functor) const
    {
        Vector<BasicBlock*, 16> worklist;
        worklist.append(from);
        while (!worklist.isEmpty()) {
            BasicBlock* block = worklist.takeLast();
            functor(block);
            worklist.appendVector(m_data[block].idomKids);
        }
    }
    
    BlockSet strictDominatorsOf(BasicBlock* to) const;
    BlockSet dominatorsOf(BasicBlock* to) const;
    BlockSet blocksStrictlyDominatedBy(BasicBlock* from) const;
    BlockSet blocksDominatedBy(BasicBlock* from) const;
    
    template<typename Functor>
    void forAllBlocksInDominanceFrontierOf(
        BasicBlock* from, const Functor& functor) const
    {
        BlockSet set;
        forAllBlocksInDominanceFrontierOfImpl(
            from,
            [&] (BasicBlock* block) {
                if (set.add(block))
                    functor(block);
            });
    }
    
    BlockSet dominanceFrontierOf(BasicBlock* from) const;
    
    template<typename Functor>
    void forAllBlocksInIteratedDominanceFrontierOf(
        const BlockList& from, const Functor& functor)
    {
        forAllBlocksInPrunedIteratedDominanceFrontierOf(
            from,
            [&] (BasicBlock* block) -> bool {
                functor(block);
                return true;
            });
    }
    
    // This is a close relative of forAllBlocksInIteratedDominanceFrontierOf(), which allows the
    // given functor to return false to indicate that we don't wish to consider the given block.
    // Useful for computing pruned SSA form.
    template<typename Functor>
    void forAllBlocksInPrunedIteratedDominanceFrontierOf(
        const BlockList& from, const Functor& functor)
    {
        BlockSet set;
        forAllBlocksInIteratedDominanceFrontierOfImpl(
            from,
            [&] (BasicBlock* block) -> bool {
                if (!set.add(block))
                    return false;
                return functor(block);
            });
    }
    
    BlockSet iteratedDominanceFrontierOf(const BlockList& from) const;
    
    void dump(PrintStream&) const;
    
private:
    bool naiveDominates(BasicBlock* from, BasicBlock* to) const;
    
    template<typename Functor>
    void forAllBlocksInDominanceFrontierOfImpl(
        BasicBlock* from, const Functor& functor) const
    {
        // Paraphrasing from http://en.wikipedia.org/wiki/Dominator_(graph_theory):
        //     "The dominance frontier of a block 'from' is the set of all blocks 'to' such that
        //     'from' dominates an immediate predecessor of 'to', but 'from' does not strictly
        //     dominate 'to'."
        //
        // A useful corner case to remember: a block may be in its own dominance frontier if it has
        // a loop edge to itself, since it dominates itself and so it dominates its own immediate
        // predecessor, and a block never strictly dominates itself.
        
        forAllBlocksDominatedBy(
            from,
            [&] (BasicBlock* block) {
                for (unsigned successorIndex = block->numSuccessors(); successorIndex--;) {
                    BasicBlock* to = block->successor(successorIndex);
                    if (!strictlyDominates(from, to))
                        functor(to);
                }
            });
    }
    
    template<typename Functor>
    void forAllBlocksInIteratedDominanceFrontierOfImpl(
        const BlockList& from, const Functor& functor) const
    {
        BlockList worklist = from;
        while (!worklist.isEmpty()) {
            BasicBlock* block = worklist.takeLast();
            forAllBlocksInDominanceFrontierOfImpl(
                block,
                [&] (BasicBlock* otherBlock) {
                    if (functor(otherBlock))
                        worklist.append(otherBlock);
                });
        }
    }
    
    struct BlockData {
        BlockData()
            : idomParent(nullptr)
            , preNumber(UINT_MAX)
            , postNumber(UINT_MAX)
        {
        }
        
        Vector<BasicBlock*> idomKids;
        BasicBlock* idomParent;
        
        unsigned preNumber;
        unsigned postNumber;
    };
    
    BlockMap<BlockData> m_data;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDominators_h
