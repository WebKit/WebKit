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

#include "config.h"
#include "B3HeapRangeTree.h"

#if ENABLE(B3_JIT)

#include "B3AbstractHeap.h"
#include <algorithm>

namespace JSC { namespace B3 {

// Orders a range before every range it contains, so that a parent always precedes its children.
static bool rangeComesBefore(const HeapRange& a, const HeapRange& b)
{
    if (a.begin() != b.begin())
        return a.begin() < b.begin();
    return a.end() > b.end();
}

HeapRangeTree::HeapRangeTree(const AbstractHeap& heap)
    : m_rootRange(heap.range())
{
    for (const AbstractHeap* child : heap.children())
        appendNodes(*child, root);
    m_firstNodeAt.fill(0, m_rootRange.end());
    for (NodeID node = m_nodes.size(); node; --node)
        m_firstNodeAt[m_nodes[node - 1].range.begin()] = node;
}

void HeapRangeTree::appendNodes(const AbstractHeap& heap, NodeID parent)
{
    // A heap with a single child has the child's range, so the two share a node.
    NodeID node = parent;
    if (range(parent) != heap.range()) {
        m_nodes.append(Node { heap.range(), parent });
        node = m_nodes.size();
    }
    for (const AbstractHeap* child : heap.children())
        appendNodes(*child, node);
}

HeapRangeTree::NodeID HeapRangeTree::nodeForSlow(HeapRange range) const
{
    if (!range || m_nodes.isEmpty())
        return root;

    // nodeFor() already found a range that is a node. Node ids are one past their index, so this is
    // the last node ordered before range, which lies inside the smallest node containing range.
    NodeID node = std::ranges::partition_point(m_nodes, [&](const Node& candidate) {
        return rangeComesBefore(candidate.range, range);
    }) - m_nodes.begin();
    while (node != root) {
        const Node& candidate = m_nodes[node - 1];
        if (candidate.range.subsumes(range))
            break;
        node = candidate.parent;
    }
    return node;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
