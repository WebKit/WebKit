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

#include "HTMLSlotElement.h"
#include "PseudoElement.h"
#include "ShadowRoot.h"

#ifndef ComposedTreeAncestorIterator_h
#define ComposedTreeAncestorIterator_h

namespace WebCore {

class HTMLSlotElement;

class ComposedTreeAncestorIterator {
public:
    ComposedTreeAncestorIterator(ContainerNode& root);
    ComposedTreeAncestorIterator(ContainerNode& root, Node& current);

    ContainerNode& operator*() { return get(); }
    ContainerNode* operator->() { return &get(); }

    bool operator==(const ComposedTreeAncestorIterator& other) const { return m_current == other.m_current; }
    bool operator!=(const ComposedTreeAncestorIterator& other) const { return m_current != other.m_current; }

    ComposedTreeAncestorIterator& operator++() { return traverseParent(); }

    ContainerNode& get() { return downcast<ContainerNode>(*m_current); }
    ComposedTreeAncestorIterator& traverseParent();

private:
    void traverseParentInShadowTree();

    ContainerNode& m_root;
    Node* m_current { 0 };
};

inline ComposedTreeAncestorIterator::ComposedTreeAncestorIterator(ContainerNode& root)
    : m_root(root)
{
    ASSERT(!is<ShadowRoot>(m_root));
}

inline ComposedTreeAncestorIterator::ComposedTreeAncestorIterator(ContainerNode& root, Node& current)
    : m_root(root)
    , m_current(&current)
{
    ASSERT(!is<ShadowRoot>(m_root));
    ASSERT(!is<ShadowRoot>(m_current));
}

inline ComposedTreeAncestorIterator& ComposedTreeAncestorIterator::traverseParent()
{
    if (m_current == &m_root) {
        m_current = nullptr;
        return *this;
    }

    auto* parent = m_current->parentNode();
    if (!parent) {
        m_current = nullptr;
        return *this;
    }
    if (is<ShadowRoot>(*parent)) {
        m_current = downcast<ShadowRoot>(*parent).host();
        return *this;
    }

    if (auto* shadowRoot = parent->shadowRoot()) {
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        m_current = shadowRoot->findAssignedSlot(*m_current);
#else
        UNUSED_PARAM(shadowRoot);
        m_current = nullptr;
#endif
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
        if (is<ShadowRoot>(m_node))
            return iterator(m_node.document(), *downcast<ShadowRoot>(m_node).host());
        if (is<PseudoElement>(m_node))
            return iterator(m_node.document(), *downcast<PseudoElement>(m_node).hostElement());
        return iterator(m_node.document(), m_node).traverseParent();
    }
    iterator end()
    {
        return iterator(m_node.document());
    }
    ContainerNode* first()
    {
        auto it = begin();
        if (it == end())
            return nullptr;
        return &it.get();
    }

private:
    Node& m_node;
};

// FIXME: We should have const versions too.
inline ComposedTreeAncestorAdapter composedTreeAncestors(Node& node)
{
    return ComposedTreeAncestorAdapter(node);
}

}

#endif
