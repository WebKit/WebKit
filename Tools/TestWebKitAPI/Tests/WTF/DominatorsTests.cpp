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

#include <wtf/Dominators.h>
#include <wtf/IndexMap.h>
#include <wtf/IndexSet.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

namespace {

struct TestNode {
    unsigned index() const { return m_index; }

    unsigned m_index { 0 };
    Vector<TestNode*> successors;
    Vector<TestNode*> predecessors;
};

class TestGraph {
public:
    using Node = TestNode*;
    using Set = IndexSet<TestNode*>;
    template<typename T> using Map = IndexMap<TestNode*, T>;
    using List = Vector<TestNode*, 4>;

    explicit TestGraph(unsigned numNodes)
        : m_nodes(numNodes)
    {
        for (unsigned i = 0; i < numNodes; ++i)
            m_nodes[i].m_index = i;
    }

    void addEdge(unsigned from, unsigned to)
    {
        m_nodes[from].successors.append(&m_nodes[to]);
        m_nodes[to].predecessors.append(&m_nodes[from]);
    }

    Node root() { return &m_nodes[0]; }

    template<typename T>
    Map<T> newMap() { return Map<T>(m_nodes.size()); }

    const Vector<TestNode*>& successors(Node node) const { return node->successors; }
    const Vector<TestNode*>& predecessors(Node node) const { return node->predecessors; }

    unsigned index(Node node) const { return node->index(); }
    Node node(unsigned index) const { return const_cast<TestNode*>(&m_nodes[index]); }
    unsigned numNodes() const { return m_nodes.size(); }

    int dump(Node node) const { return node ? static_cast<int>(node->index()) : -1; }
    void dump(PrintStream& out) const { out.print("TestGraph(", m_nodes.size(), ")"); }

private:
    Vector<TestNode> m_nodes;
};

using TestDominators = WTF::Dominators<TestGraph>;

// Returns each node's position in enter order, after checking that every node is entered exactly once,
// that its immediate dominator is the innermost node still open, and that it is exited only after
// everything it dominates.
template<DominatorTreeChildOrder childOrder>
Vector<unsigned> checkPreOrderWalk(TestGraph& graph, const TestDominators& dominators)
{
    Vector<unsigned> enterPosition(FillWith { }, graph.numNodes(), UINT_MAX);
    Vector<TestNode*> open;
    unsigned enterCount = 0;
    dominators.forAllBlocksInDominatorTreePreOrder<childOrder>(
        [&](TestNode* node) {
            EXPECT_EQ(enterPosition[node->index()], UINT_MAX);
            enterPosition[node->index()] = enterCount++;
            if (open.isEmpty())
                EXPECT_EQ(node, graph.root());
            else
                EXPECT_EQ(open.last(), dominators.idom(node));
            open.append(node);
        },
        [&](TestNode* node) {
            EXPECT_FALSE(open.isEmpty());
            EXPECT_EQ(open.last(), node);
            open.removeLast();
        });
    EXPECT_TRUE(open.isEmpty());
    EXPECT_EQ(enterCount, graph.numNodes());
    return enterPosition;
}

} // anonymous namespace

TEST(WTF_Dominators, PreOrderWalkSingleNode)
{
    TestGraph graph(1);
    TestDominators dominators(graph);
    checkPreOrderWalk<DominatorTreeChildOrder::Any>(graph, dominators);
    checkPreOrderWalk<DominatorTreeChildOrder::ReversePostOrder>(graph, dominators);
}

TEST(WTF_Dominators, PreOrderWalkNesting)
{
    // 0 -> 1 -> 2 -> 3
    //      1 -> 4 -> 3
    // 0 -> 5
    TestGraph graph(6);
    graph.addEdge(0, 1);
    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addEdge(1, 4);
    graph.addEdge(4, 3);
    graph.addEdge(0, 5);
    TestDominators dominators(graph);

    EXPECT_EQ(dominators.idom(graph.node(3)), graph.node(1));
    checkPreOrderWalk<DominatorTreeChildOrder::Any>(graph, dominators);
    checkPreOrderWalk<DominatorTreeChildOrder::ReversePostOrder>(graph, dominators);
}

TEST(WTF_Dominators, PreOrderWalkReversePostOrder)
{
    // A join (4) dominated by the same node (0) as both its predecessors (1, 3), with a loop
    // (2 <-> 3) in one of them. The walk must enter 4 only after 1 and 3.
    //
    // 0 -> 1 -> 4
    // 0 -> 2 -> 3 -> 4
    //      2 <- 3
    TestGraph graph(5);
    graph.addEdge(0, 4);
    graph.addEdge(0, 2);
    graph.addEdge(0, 1);
    graph.addEdge(1, 4);
    graph.addEdge(2, 3);
    graph.addEdge(3, 2);
    graph.addEdge(3, 4);
    TestDominators dominators(graph);

    EXPECT_EQ(dominators.idom(graph.node(4)), graph.node(0));
    checkPreOrderWalk<DominatorTreeChildOrder::Any>(graph, dominators);
    Vector<unsigned> position = checkPreOrderWalk<DominatorTreeChildOrder::ReversePostOrder>(graph, dominators);

    for (unsigned from = 0; from < graph.numNodes(); ++from) {
        for (TestNode* to : graph.successors(graph.node(from))) {
            if (dominators.dominates(to, graph.node(from)))
                continue;
            EXPECT_LT(position[from], position[to->index()]);
        }
    }
}

} // namespace TestWebKitAPI
