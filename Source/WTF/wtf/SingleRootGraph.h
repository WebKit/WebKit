/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>
#include <wtf/GraphNodeWorklist.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template <typename Graph>
class SingleRootGraphNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // We use "#root" to refer to the synthetic root we have created.
    static const char* rootName() { return "#root"; };

    SingleRootGraphNode(typename Graph::Node node = typename Graph::Node())
        : m_node(node)
    {
    }

    static SingleRootGraphNode root()
    {
        SingleRootGraphNode result;
        result.m_node = 0;
        result.m_isRoot = true;
        return result;
    }

    bool operator==(const SingleRootGraphNode& other) const
    {
        return m_node == other.m_node
            && m_isRoot == other.m_isRoot;
    }

    bool operator!=(const SingleRootGraphNode& other) const
    {
        return !(*this == other);
    }

    explicit operator bool() const { return *this != SingleRootGraphNode(); }

    bool isRoot() const
    {
        return m_isRoot;
    }

    typename Graph::Node node() const
    {
        ASSERT(!m_isRoot);
        return m_node;
    }

private:
    typename Graph::Node m_node;
    bool m_isRoot { false };
};

template <typename Graph>
class SingleRootGraphSet {
    WTF_MAKE_FAST_ALLOCATED;
    using Node = SingleRootGraphNode<Graph>;
public:
    SingleRootGraphSet() = default;
    
    bool add(const Node& node)
    {
        if (node.isRoot())
            return checkAndSet(m_hasRoot, true);
        return m_set.add(node.node());
    }

    bool remove(const Node& node)
    {
        if (node.isRoot())
            return checkAndSet(m_hasRoot, false);
        return m_set.remove(node.node());
    }

    bool contains(const Node& node)
    {
        if (node.isRoot())
            return m_hasRoot;
        return m_set.contains(node.node());
    }

    void dump(PrintStream& out) const
    {
        if (m_hasRoot)
            out.print(Node::rootName(), " ");
        out.print(m_set);
    }
    
private:
    typename Graph::Set m_set;
    bool m_hasRoot { false };
};

template<typename T, typename Graph>
class SingleRootMap {
    WTF_MAKE_FAST_ALLOCATED;
    using Node = SingleRootGraphNode<Graph>;
public:
    SingleRootMap(Graph& graph)
        : m_map(graph.template newMap<T>())
    {
    }

    SingleRootMap(SingleRootMap&& other)
        : m_map(WTFMove(other.m_map))
        , m_root(WTFMove(other.m_root))
    {
    }

    void clear()
    {
        m_map.clear();
        m_root = T();
    }

    size_t size() const { return m_map.size() + 1; }

    T& operator[](size_t index)
    {
        if (!index)
            return m_root;
        return m_map[index - 1];
    }

    const T& operator[](size_t index) const
    {
        return (*const_cast<SingleRootMap*>(this))[index];
    }

    T& operator[](const Node& node)
    {
        if (node.isRoot())
            return m_root;
        return m_map[node.node()];
    }

    const T& operator[](const Node& node) const
    {
        return (*const_cast<SingleRootMap*>(this))[node];
    }
    
private:
    typename Graph::template Map<T> m_map;
    T m_root;
};

template<typename Graph>
class SingleRootGraph {
    WTF_MAKE_NONCOPYABLE(SingleRootGraph);
    WTF_MAKE_FAST_ALLOCATED;
public:

    using Node = SingleRootGraphNode<Graph>;
    using Set = SingleRootGraphSet<Graph>;
    template <typename T> using Map = SingleRootMap<T, Graph>;
    using List = Vector<Node, 4>;

    SingleRootGraph(Graph& graph)
        : m_graph(graph)
    {
        for (typename Graph::Node realRoot : m_graph.roots()) {
            ASSERT(m_graph.predecessors(realRoot).isEmpty());
            m_rootSuccessorList.append(realRoot);
            m_rootSuccessorSet.add(realRoot);
        }
        ASSERT(m_rootSuccessorList.size());
    }

    Node root() const { return Node::root(); }

    template<typename T>
    Map<T> newMap() { return Map<T>(m_graph); }

    List successors(const Node& node) const
    {
        assertIsConsistent();

        if (node.isRoot())
            return m_rootSuccessorList;
        List result;
        for (typename Graph::Node successor : m_graph.successors(node.node()))
            result.append(successor);
        return result;
    }

    List predecessors(const Node& node) const
    {
        assertIsConsistent();

        if (node.isRoot())
            return List { };

        if (m_rootSuccessorSet.contains(node.node())) {
            ASSERT(m_graph.predecessors(node.node()).isEmpty());
            return List { root() };
        }

        List result;
        for (typename Graph::Node predecessor : m_graph.predecessors(node.node()))
            result.append(predecessor);
        return result;
    }

    unsigned index(const Node& node) const
    {
        if (node.isRoot())
            return 0;
        return m_graph.index(node.node()) + 1;
    }

    Node node(unsigned index) const
    {
        if (!index)
            return root();
        return m_graph.node(index - 1);
    }

    unsigned numNodes() const
    {
        return m_graph.numNodes() + 1;
    }

    CString dump(Node node) const
    {
        StringPrintStream out;
        if (!node)
            out.print("<null>");
        else if (node.isRoot())
            out.print(Node::rootName());
        else
            out.print(m_graph.dump(node.node()));
        return out.toCString();
    }

    void dump(PrintStream& out) const
    {
        for (unsigned i = 0; i < numNodes(); ++i) {
            Node node = this->node(i);
            if (!node)
                continue;
            out.print(dump(node), ":\n");
            out.print("    Preds: ");
            CommaPrinter comma;
            for (Node predecessor : predecessors(node))
                out.print(comma, dump(predecessor));
            out.print("\n");
            out.print("    Succs: ");
            comma = CommaPrinter();
            for (Node successor : successors(node))
                out.print(comma, dump(successor));
            out.print("\n");
        }
    }

private:
    ALWAYS_INLINE void assertIsConsistent() const
    {
#if !ASSERT_DISABLED
        // We expect the roots() function to be idempotent while we're alive so we can cache
        // the result in the constructor. If a user of this changes the result of its roots()
        // function, it's expected that the user will create a new instance of this class.
        List rootSuccessorList;
        for (typename Graph::Node realRoot : m_graph.roots()) {
            ASSERT(m_graph.predecessors(realRoot).isEmpty());
            rootSuccessorList.append(realRoot);
        }
        ASSERT(rootSuccessorList.size());
        ASSERT(rootSuccessorList == m_rootSuccessorList);
#endif
    }

    Graph& m_graph;
    List m_rootSuccessorList;
    typename Graph::Set m_rootSuccessorSet;
};

} // namespace WTF

using WTF::SingleRootGraph;
