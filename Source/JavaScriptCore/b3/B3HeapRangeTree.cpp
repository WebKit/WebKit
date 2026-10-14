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

#include <algorithm>

namespace JSC { namespace B3 {

void HeapRangeTree::build(Vector<HeapRange>&& ranges)
{
    m_nodes.clear();
    m_coverage.clear();
    m_collapseDepth = std::numeric_limits<unsigned>::max();

    m_coversEverything = ranges.contains(HeapRange::top());
    ranges.removeAllMatching([](const HeapRange& range) {
        return !range || range == HeapRange::top();
    });

    std::sort(ranges.begin(), ranges.end(), [](const HeapRange& a, const HeapRange& b) {
        if (a.begin() != b.begin())
            return a.begin() < b.begin();
        return a.end() > b.end();
    });

    size_t uniqueSize = 0;
    for (size_t i = 0; i < ranges.size(); ++i) {
        if (!uniqueSize || ranges[uniqueSize - 1] != ranges[i])
            ranges[uniqueSize++] = ranges[i];
    }
    ranges.shrink(uniqueSize);

    for (auto& range : ranges) {
        if (!m_coverage.isEmpty() && range.begin() <= m_coverage.last().end()) {
            m_coverage.last() = HeapRange(m_coverage.last().begin(), std::max(m_coverage.last().end(), range.end()));
            continue;
        }
        m_coverage.append(range);
    }

    Vector<unsigned> stack;
    stack.append(root);

    for (auto& range : ranges) {
        while (stack.size() > 1 && m_nodes[stack.last() - 1].range.end() <= range.begin())
            stack.removeLast();

        unsigned parentId = stack.last();
        if (parentId != root) {
            const HeapRange& parentRange = m_nodes[parentId - 1].range;
            if (!(parentRange.begin() <= range.begin() && range.end() <= parentRange.end()))
                continue;
        }

        unsigned nodeDepth = parentId == root ? 1 : m_nodes[parentId - 1].depth + 1;
        m_nodes.append(Node { range, parentId, nodeDepth });
        stack.append(m_nodes.size());
    }
}

size_t HeapRangeTree::firstCoverageEndingAfter(uint64_t position) const
{
    size_t low = 0;
    size_t high = m_coverage.size();
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (m_coverage[mid].end() <= position)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

bool HeapRangeTree::overlapsAnyBuiltRange(HeapRange range) const
{
    if (!range)
        return false;
    if (m_coversEverything)
        return true;

    size_t index = firstCoverageEndingAfter(range.begin());
    return index < m_coverage.size() && m_coverage[index].begin() < range.end();
}

bool HeapRangeTree::coversRange(HeapRange range) const
{
    if (!range || m_coversEverything)
        return true;

    size_t index = firstCoverageEndingAfter(range.begin());
    return index < m_coverage.size() && m_coverage[index].begin() <= range.begin() && range.end() <= m_coverage[index].end();
}

unsigned HeapRangeTree::nodeFor(HeapRange range) const
{
    if (!range || m_nodes.isEmpty())
        return root;

    unsigned low = 0;
    unsigned high = m_nodes.size();
    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        if (m_nodes[mid].range.begin() <= range.begin())
            low = mid + 1;
        else
            high = mid;
    }
    if (!low)
        return root;

    unsigned node = low;
    while (node != root) {
        const Node& candidate = m_nodes[node - 1];
        if (candidate.range.begin() <= range.begin() && range.end() <= candidate.range.end())
            break;
        node = candidate.parent;
    }

    while (node != root && m_nodes[node - 1].depth > m_collapseDepth)
        node = m_nodes[node - 1].parent;

    return node;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
