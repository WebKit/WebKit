/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "B3HeapRange.h"
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

class AbstractHeap;

// The ranges of an AbstractHeap's descendants, any two of which are nested or disjoint. nodeFor
// resolves a range to the smallest node whose range fully contains it, or to root, which stands for
// the AbstractHeap itself and for every range it does not hold, such as HeapRange::top().
class HeapRangeTree {
public:
    using NodeID = unsigned;

    static constexpr NodeID root = 0;

    // The heaps' ranges must be computed already.
    JS_EXPORT_PRIVATE explicit HeapRangeTree(const AbstractHeap&);

    NodeID nodeFor(HeapRange range) const
    {
        if (range.begin() < m_firstNodeAt.size()) {
            // Nodes beginning at the same point are adjacent, outermost first.
            for (NodeID node = m_firstNodeAt[range.begin()]; node && node <= m_nodes.size(); ++node) {
                const HeapRange& candidate = m_nodes[node - 1].range;
                if (candidate.begin() != range.begin() || candidate.end() < range.end())
                    break;
                if (candidate.end() == range.end())
                    return node;
            }
        }
        return nodeForSlow(range);
    }

    NodeID parent(NodeID node) const
    {
        if (node == root)
            return root;
        return m_nodes[node - 1].parent;
    }

    HeapRange range(NodeID node) const
    {
        if (node == root)
            return m_rootRange;
        return m_nodes[node - 1].range;
    }

    unsigned size() const { return m_nodes.size() + 1; }

private:
    struct Node {
        HeapRange range;
        NodeID parent;
    };

    void appendNodes(const AbstractHeap&, NodeID parent);
    JS_EXPORT_PRIVATE NodeID nodeForSlow(HeapRange) const;

    HeapRange m_rootRange;
    Vector<Node> m_nodes; // In preorder, which puts a node before every node it contains.
    Vector<NodeID> m_firstNodeAt; // By begin, the outermost node beginning there, or 0.
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
