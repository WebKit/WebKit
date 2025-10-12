/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "SourceAlpha.h"
#include "SourceGraphic.h"
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<typename NodeType>
class SVGFilterGraph {
public:
    using NodeVector = Vector<Ref<NodeType>>;

    virtual ~SVGFilterGraph() = default;

    virtual void addNamedNode(const AtomString& id, Ref<NodeType>&&) = 0;
    virtual RefPtr<NodeType> getNamedNode(const AtomString& id) const = 0;

    std::optional<NodeVector> getNamedNodes(std::span<const AtomString> names) const
    {
        NodeVector nodes;

        nodes.reserveInitialCapacity(names.size());

        for (auto& name : names) {
            if (auto node = getNamedNode(name))
                nodes.append(node.releaseNonNull());
            else if (!isSourceName(name))
                return std::nullopt;
        }

        return nodes;
    }

    void setNodeInputs(NodeType& node, NodeVector&& inputs)
    {
        m_nodeInputs.set({ node }, WTFMove(inputs));
    }

    NodeVector getNodeInputs(NodeType& node) const
    {
        return m_nodeInputs.get(node);
    }

    NodeVector nodes() const
    {
        return WTF::map(m_nodeInputs, [] (auto& pair) -> Ref<NodeType> {
            return pair.key;
        });
    }

    NodeType* lastNode() const { return m_lastNode.get(); }

    template<typename Callback>
    bool visit(NOESCAPE const Callback& callback)
    {
        RefPtr lastNode = m_lastNode;
        if (!lastNode)
            return false;

        Vector<Ref<NodeType>> stack;
        return visit(*lastNode, stack, 0, callback);
    }

protected:
    static bool isSourceName(const AtomString& name)
    {
        return name == SourceGraphic::effectName() || name == SourceAlpha::effectName();
    }

    template<typename Callback>
    bool visit(NodeType& node, Vector<Ref<NodeType>>& stack, unsigned level, NOESCAPE const Callback& callback)
    {
        // A cycle is detected.
        if (stack.containsIf([&](auto& item) { return item.ptr() == &node; }))
            return false;

        stack.append(node);

        callback(node, level);

        for (auto& input : getNodeInputs(node)) {
            if (!visit(input, stack, level + 1, callback))
                return false;
        }

        ASSERT(!stack.isEmpty());
        ASSERT(stack.last().ptr() == &node);

        stack.removeLast();
        return true;
    }

    HashMap<AtomString, Ref<NodeType>> m_namedNodes;
    HashMap<Ref<NodeType>, NodeVector> m_nodeInputs;
    RefPtr<NodeType> m_lastNode;
};

} // namespace WebCore
