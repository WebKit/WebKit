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
#include <limits>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

// Any two nodes' ranges are nested or disjoint: build() leaves out a range that partially overlaps
// one already stored. nodeFor resolves a range to the smallest node whose range fully contains it,
// or to root, which stands for HeapRange::top(), if none does.
class HeapRangeTree {
public:
    static constexpr unsigned root = 0;

    JS_EXPORT_PRIVATE void build(Vector<HeapRange>&&);
    JS_EXPORT_PRIVATE unsigned nodeFor(HeapRange) const;

    // Whether the range overlaps any range given to build(), including one that partially
    // overlaps a node and so resolves to a coarser one. A write for which this is false cannot
    // change what any of those ranges holds.
    JS_EXPORT_PRIVATE bool overlapsAnyBuiltRange(HeapRange) const;

    // Whether the range lies entirely within the union of the ranges given to build().
    JS_EXPORT_PRIVATE bool coversRange(HeapRange) const;

    unsigned parent(unsigned node) const
    {
        if (node == root)
            return root;
        return m_nodes[node - 1].parent;
    }

    unsigned depth(unsigned node) const
    {
        if (node == root)
            return 0;
        return m_nodes[node - 1].depth;
    }

    HeapRange range(unsigned node) const
    {
        if (node == root)
            return HeapRange::top();
        return m_nodes[node - 1].range;
    }

    unsigned size() const { return m_nodes.size() + 1; }

    // Makes nodeFor() answer with the ancestor at this depth in place of any deeper node.
    void collapseToDepth(unsigned maxDepth) { m_collapseDepth = maxDepth; }

private:
    struct Node {
        HeapRange range;
        unsigned parent;
        unsigned depth;
    };

    size_t firstCoverageEndingAfter(uint64_t position) const;

    Vector<Node> m_nodes;
    Vector<HeapRange> m_coverage; // Union of the built ranges, as disjoint non-adjacent ranges sorted by begin.
    bool m_coversEverything { false };
    unsigned m_collapseDepth { std::numeric_limits<unsigned>::max() };
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
