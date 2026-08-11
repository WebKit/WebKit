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

#include <algorithm>
#include <array>
#include <wtf/BitVector.h>
#include <wtf/Vector.h>

namespace WTF {

enum class GraphOrder : uint8_t {
    PreOrder,
    PostOrder,
};

// Most general form: for every node reachable from `roots`, appends projection(index) into
// `output` in the requested order. `visited` must be sized to graph.numNodes(); it is set for
// every node this call appends (callers may reuse it afterwards). `roots` is any iterable of
// graph nodes; falsy roots (e.g. null Node) are skipped.
template<typename Graph, typename Roots, typename Container, typename Projection>
void appendNodeOrderFromRoots(Graph& graph, const Roots& roots, GraphOrder order, BitVector& visited, Container& output, NOESCAPE const Projection& projection)
{
    constexpr unsigned revisitFlag = 1u << 31;
    ASSERT(static_cast<unsigned>(graph.numNodes()) < revisitFlag);

    Vector<unsigned, 32> stack;
    for (auto root : roots) {
        if (root)
            stack.append(graph.index(root));
    }

    while (!stack.isEmpty()) {
        unsigned entry = stack.takeLast();

        if (entry & revisitFlag) {
            // Post-visit: all successors have been emitted.
            output.append(projection(entry & ~revisitFlag));
            continue;
        }

        if (visited.quickGet(entry))
            continue;
        visited.quickSet(entry);

        if (order == GraphOrder::PreOrder)
            output.append(projection(entry));
        else
            stack.append(entry | revisitFlag);

        for (auto successor : graph.successors(graph.node(entry))) {
            unsigned successorIndex = graph.index(successor);
            if (!visited.quickGet(successorIndex))
                stack.append(successorIndex);
        }
    }
}

template<typename Graph, typename Container, typename Projection>
void appendNodeOrder(Graph& graph, GraphOrder order, BitVector& visited, Container& output, NOESCAPE const Projection& projection)
{
    std::array<typename Graph::Node, 1> roots { graph.root() };
    appendNodeOrderFromRoots(graph, roots, order, visited, output, projection);
}

// Appends node indices (as held by the graph) into `output`. Used by index-based dataflow such as
// WTF::Liveness and WTF::Dominators. `visited` is caller-owned.
template<typename Graph, typename Container>
void appendNodeIndicesInOrder(Graph& graph, GraphOrder order, BitVector& visited, Container& output)
{
    appendNodeOrder(graph, order, visited, output, [](unsigned index) { return index; });
}

template<typename Graph, typename Roots, typename Container>
void appendNodeIndicesInOrder(Graph& graph, const Roots& roots, GraphOrder order, BitVector& visited, Container& output)
{
    appendNodeOrderFromRoots(graph, roots, order, visited, output, [](unsigned index) { return index; });
}

// Appends graph nodes into `output`. Used by B3/Air/DFG which produce block lists. The visited set
// is allocated internally.
template<typename Graph, typename Container>
void appendNodesInOrder(Graph& graph, GraphOrder order, Container& output)
{
    BitVector visited(graph.numNodes());
    appendNodeOrder(graph, order, visited, output, [&](unsigned index) { return graph.node(index); });
}

template<typename Graph, typename Roots, typename Container>
void appendNodesInOrder(Graph& graph, const Roots& roots, GraphOrder order, Container& output)
{
    BitVector visited(graph.numNodes());
    appendNodeOrderFromRoots(graph, roots, order, visited, output, [&](unsigned index) { return graph.node(index); });
}

} // namespace WTF

using WTF::GraphOrder;
using WTF::appendNodeOrder;
using WTF::appendNodeOrderFromRoots;
using WTF::appendNodeIndicesInOrder;
using WTF::appendNodesInOrder;
