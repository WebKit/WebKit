/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "ElementRareData.h"
#include "HTMLSlotElement.h"
#include "PseudoElement.h"
#include "ShadowRoot.h"

namespace WebCore {

class HTMLSlotElement;

class ComposedTreeAncestorIterator {
public:
    ComposedTreeAncestorIterator();
    ComposedTreeAncestorIterator(Node& current);

    Element& operator*() { return get(); }
    Element* operator->() { return &get(); }

    friend bool operator==(ComposedTreeAncestorIterator, ComposedTreeAncestorIterator) = default;

    ComposedTreeAncestorIterator& operator++() { return traverseParent(); }

    Element& get() { return downcast<Element>(*m_current); }
    ComposedTreeAncestorIterator& traverseParent();

private:
    void traverseParentInShadowTree();

    Node* m_current { nullptr };
};

inline ComposedTreeAncestorIterator::ComposedTreeAncestorIterator()
{
}

inline ComposedTreeAncestorIterator::ComposedTreeAncestorIterator(Node& current)
    : m_current(&current)
{
    ASSERT(!is<ShadowRoot>(m_current));
}

inline ComposedTreeAncestorIterator& ComposedTreeAncestorIterator::traverseParent()
{
    auto* parent = m_current->parentNode();
    if (!parent) {
        m_current = nullptr;
        return *this;
    }
    if (auto shadowRoot = dynamicDowncast<ShadowRoot>(*parent)) {
        m_current = shadowRoot->host();
        return *this;
    }
    if (!is<Element>(*parent)) {
        m_current = nullptr;
        return *this;
    };

    if (auto* shadowRoot = parent->shadowRoot()) {
        m_current = shadowRoot->findAssignedSlot(*m_current);
        return *this;
    }

    m_current = parent;
    return *this;
}

class ComposedTreeAncestorAdapter {
public:
    using iterator = ComposedTreeAncestorIterator;

    ComposedTreeAncestorAdapter(Node& node)
        : m_node(node)
    { }

    iterator begin()
    {
        if (auto shadowRoot = dynamicDowncast<ShadowRoot>(m_node.get()))
            return iterator(*shadowRoot->host());
        if (auto pseudoElement = dynamicDowncast<PseudoElement>(m_node.get()))
            return iterator(*pseudoElement->hostElement());
        return iterator(m_node).traverseParent();
    }
    iterator end()
    {
        return iterator();
    }
    Element* first()
    {
        auto it = begin();
        if (it == end())
            return nullptr;
        return &it.get();
    }

private:
    Ref<Node> m_node;
};

// FIXME: We should have const versions too.
inline ComposedTreeAncestorAdapter composedTreeAncestors(Node& node)
{
    return ComposedTreeAncestorAdapter(node);
}

} // namespace WebCore
