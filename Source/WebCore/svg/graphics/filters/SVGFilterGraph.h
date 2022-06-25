/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

class FilterEffect;
class SVGFilterPrimitiveStandardAttributes;

template<typename NodeType>
class SVGFilterGraph {
public:
    using NodeVector = Vector<Ref<NodeType>>;

    SVGFilterGraph() = default;

    SVGFilterGraph(Ref<NodeType>&& sourceGraphic, Ref<NodeType>&& sourceAlpha)
    {
        m_builtinNodes.add(SourceGraphic::effectName(), WTFMove(sourceGraphic));
        m_builtinNodes.add(SourceAlpha::effectName(), WTFMove(sourceAlpha));

        setNodeInputs(*this->sourceAlpha(), NodeVector { *this->sourceGraphic() });
    }

    NodeType* sourceGraphic() const
    {
        return m_builtinNodes.get(FilterEffect::sourceGraphicName());
    }

    NodeType* sourceAlpha() const
    {
        return m_builtinNodes.get(FilterEffect::sourceAlphaName());
    }

    void addNamedNode(const AtomString& id, Ref<NodeType>&& node)
    {
        if (id.isEmpty()) {
            m_lastNode = WTFMove(node);
            return;
        }

        if (m_builtinNodes.contains(id))
            return;

        m_lastNode = WTFMove(node);
        m_namedNodes.set(id, Ref { *m_lastNode });
    }

    RefPtr<NodeType> getNamedNode(const AtomString& id) const
    {
        if (id.isEmpty()) {
            if (m_lastNode)
                return m_lastNode;

            return sourceGraphic();
        }

        if (m_builtinNodes.contains(id))
            return m_builtinNodes.get(id);

        return m_namedNodes.get(id);
    }

    std::optional<NodeVector> getNamedNodes(Span<const AtomString> names) const
    {
        NodeVector nodes;

        nodes.reserveInitialCapacity(names.size());

        for (auto& name : names) {
            auto node = getNamedNode(name);
            if (!node)
                return std::nullopt;

            nodes.uncheckedAppend(node.releaseNonNull());
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

    NodeType* lastNode() const { return m_lastNode.get(); }
    
    template<typename Callback>
    bool visit(Callback callback)
    {
        if (!lastNode())
            return false;

        Vector<Ref<NodeType>> stack;
        return visit(*lastNode(), stack, 0, callback);
    }

private:
    template<typename Callback>
    bool visit(NodeType& node, Vector<Ref<NodeType>>& stack, unsigned level, Callback callback)
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

    HashMap<AtomString, Ref<NodeType>> m_builtinNodes;
    HashMap<AtomString, Ref<NodeType>> m_namedNodes;
    HashMap<Ref<NodeType>, NodeVector> m_nodeInputs;
    RefPtr<NodeType> m_lastNode;
};

using SVGFilterEffectsGraph = SVGFilterGraph<FilterEffect>;
using SVGFilterPrimitivesGraph = SVGFilterGraph<SVGFilterPrimitiveStandardAttributes>;

} // namespace WebCore
