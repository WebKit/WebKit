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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(DFG_JIT)

#include "DFGStoreBarrierElisionPhase.h"

#include "DFGBasicBlock.h"
#include "DFGClobberSet.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCInlines.h"
#include <wtf/HashSet.h>

namespace JSC { namespace DFG {

class StoreBarrierElisionPhase : public Phase {
public:
    StoreBarrierElisionPhase(Graph& graph)
        : Phase(graph, "store barrier elision")
        , m_currentBlock(0)
        , m_currentIndex(0)
    {
        m_gcClobberSet.add(GCState);
    }

    bool run()
    {
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            m_currentBlock = m_graph.block(blockIndex);
            if (!m_currentBlock)
                continue;
            handleBlock(m_currentBlock);
        }
        return true;
    }

private:
    bool couldCauseGC(Node* node)
    {
        return writesOverlap(m_graph, node, m_gcClobberSet);
    }

    bool allocatesFreshObject(Node* node)
    {
        switch (node->op()) {
        case NewObject:
        case NewArray:
        case NewArrayWithSize:
        case NewArrayBuffer:
        case NewTypedArray:
        case NewRegexp:
            return true;
        default:
            return false;
        }
    }

    void noticeFreshObject(HashSet<Node*>& dontNeedBarriers, Node* node)
    {
        ASSERT(allocatesFreshObject(node));
        dontNeedBarriers.add(node);
    }

    Node* getBaseOfStore(Node* barrierNode)
    {
        ASSERT(barrierNode->isStoreBarrier());
        return barrierNode->child1().node();
    }

    bool shouldBeElided(HashSet<Node*>& dontNeedBarriers, Node* node)
    {
        ASSERT(node->isStoreBarrier());
        return dontNeedBarriers.contains(node->child1().node());
    }

    void elideBarrier(Node* node)
    {
        ASSERT(node->isStoreBarrier());
        node->convertToPhantom();
    }

    void handleNode(HashSet<Node*>& dontNeedBarriers, Node* node)
    {
        if (couldCauseGC(node))
            dontNeedBarriers.clear();

        if (allocatesFreshObject(node))
            noticeFreshObject(dontNeedBarriers, node);

        if (!node->isStoreBarrier())
            return;

        if (shouldBeElided(dontNeedBarriers, node)) {
            elideBarrier(node);
            return;
        }

        Node* base = getBaseOfStore(node);
        if (!base)
            return;

        if (dontNeedBarriers.contains(base))
            return;
        dontNeedBarriers.add(base);
    }

    bool handleBlock(BasicBlock* block)
    {
        HashSet<Node*> dontNeedBarriers;
        for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
            m_currentIndex = indexInBlock;
            Node* node = block->at(indexInBlock);
            handleNode(dontNeedBarriers, node);
        }
        return true;
    }

    ClobberSet m_gcClobberSet;
    BasicBlock* m_currentBlock;
    unsigned m_currentIndex;
};
    
bool performStoreBarrierElision(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Store Barrier Elision Phase");
    return runPhase<StoreBarrierElisionPhase>(graph);
}


} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
