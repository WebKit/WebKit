/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DFGCloneHelper.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"

namespace JSC { namespace DFG {

namespace {

enum class NodeCloneStatus {
    Common, // Use shared logic to clone this node
    Special, // Requires special handling for cloning
    PreCloned, // Cloned earlier (e.g. Phi), shouldn't reach cloneNodeImpl
    Unsupported // Not yet supported, future work
};

NodeCloneStatus nodeCloneStatusFor(NodeType op)
{
    switch (op) {
#define HANDLE_CASE(op, kind) \
    case op:                  \
        return NodeCloneStatus::kind;
        FOR_EACH_NODE_CLONE_STATUS(HANDLE_CASE)
#undef HANDLE_CASE
    default:
        return NodeCloneStatus::Unsupported;
    }
}

} // anonymous namespace

bool CloneHelper::isNodeCloneable(Graph& graph, HashSet<Node*>& cloneableCache, Node* node)
{
    if (cloneableCache.contains(node))
        return true;

    bool result = true;
    switch (nodeCloneStatusFor(node->op())) {
    case NodeCloneStatus::Common:
    case NodeCloneStatus::Special: {
        graph.doToChildrenWithCheck(node, [&](Edge& edge) {
            if (isNodeCloneable(graph, cloneableCache, edge.node()))
                return IterationStatus::Continue;
            result = false;
            return IterationStatus::Done;
        });
        return result;
    }
    case NodeCloneStatus::PreCloned:
        break;
    case NodeCloneStatus::Unsupported:
        result = false;
    }

    if (result)
        cloneableCache.add(node);
    return result;
}

Node* CloneHelper::cloneNode(BasicBlock* into, Node* node)
{
    ASSERT(node);
    auto iter = m_nodeClones.find(node);
    if (iter != m_nodeClones.end())
        return iter->value;
    Node* result = cloneNodeImpl(into, node);
    ASSERT(result);
    m_nodeClones.add(node, result);
    return result;
}

Node* CloneHelper::cloneNodeImpl(BasicBlock* into, Node* node)
{
#if ASSERT_ENABLED
    m_graph.doToChildren(node, [&](Edge& e) {
        ASSERT(e.node()->owner == node->owner);
    });
#endif

    auto cloneEdge = [&](Edge& edge) {
        return edge ? Edge(cloneNode(into, edge.node()), edge.useKind()) : Edge();
    };

    switch (nodeCloneStatusFor(node->op())) {
    case NodeCloneStatus::Common: {
        if (node->hasVarArgs()) {
            size_t firstChild = m_graph.m_varArgChildren.size();

            uint32_t validChildrenCount = 0;
            m_graph.doToChildren(node, [&](Edge& edge) {
                m_graph.m_varArgChildren.append(cloneEdge(edge));
                ++validChildrenCount;
            });

            uint32_t expectedCount = m_graph.numChildren(node);
            for (uint32_t i = validChildrenCount; i < expectedCount; ++i)
                m_graph.m_varArgChildren.append(Edge());

            Node* clone = into->cloneAndAppend(m_graph, node);
            clone->children.setFirstChild(firstChild);
            return clone;
        }

        Node* clone = into->cloneAndAppend(m_graph, node);
        clone->child1() = cloneEdge(node->child1());
        clone->child2() = cloneEdge(node->child2());
        clone->child3() = cloneEdge(node->child3());
        return clone;
    }

    case NodeCloneStatus::Special:
        switch (node->op()) {
        case Branch: {
            Node* clone = into->cloneAndAppend(m_graph, node);
            clone->setOpInfo(OpInfo(m_graph.m_branchData.add(WTFMove(*node->branchData()))));
            clone->child1() = cloneEdge(node->child1());
            return clone;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }

    case NodeCloneStatus::PreCloned:
        RELEASE_ASSERT_NOT_REACHED(); // e.g. Phi
        return nullptr;

    case NodeCloneStatus::Unsupported:
        dataLogLn("Node not cloneable: ", node->op());
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
