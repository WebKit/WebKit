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
#include "NodeTraversal.h"
#include "ShadowRoot.h"

#ifndef ComposedTreeIterator_h
#define ComposedTreeIterator_h

namespace WebCore {

class HTMLSlotElement;

class ComposedTreeIterator {
public:
    ComposedTreeIterator(ContainerNode& root);
    ComposedTreeIterator(ContainerNode& root, Node& current);

    Node& operator*() { return *m_current; }
    Node* operator->() { return m_current; }

    bool operator==(const ComposedTreeIterator& other) const { return m_current == other.m_current; }
    bool operator!=(const ComposedTreeIterator& other) const { return m_current != other.m_current; }

    ComposedTreeIterator& operator++() { return traverseNextSibling(); }

    ComposedTreeIterator& traverseNext();
    ComposedTreeIterator& traverseNextSibling();
    ComposedTreeIterator& traversePreviousSibling();
    ComposedTreeIterator& traverseParent();

private:
    void initializeShadowStack();
    void traverseNextInShadowTree();
    void traverseParentInShadowTree();
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    void traverseNextSiblingSlot();
    void traversePreviousSiblingSlot();
#endif

    ContainerNode& m_root;
    Node* m_current { 0 };

    struct ShadowContext {
        ShadowContext(Element* host)
            : host(host)
        { }

        Element* host;
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        HTMLSlotElement* currentSlot { nullptr };
        unsigned currentSlotNodeIndex { 0 };
#endif
    };
    Vector<ShadowContext, 4> m_shadowStack;
};

inline ComposedTreeIterator::ComposedTreeIterator(ContainerNode& root)
    : m_root(root)
{
    ASSERT(!is<ShadowRoot>(m_root));
}

inline ComposedTreeIterator::ComposedTreeIterator(ContainerNode& root, Node& current)
    : m_root(root)
    , m_current(&current)
{
    ASSERT(!is<ShadowRoot>(m_root));
    ASSERT(!is<ShadowRoot>(m_current));

    bool mayNeedShadowStack = m_root.shadowRoot() || (m_current != &m_root && current.parentNode() != &m_root);
    if (mayNeedShadowStack)
        initializeShadowStack();
}

inline ComposedTreeIterator& ComposedTreeIterator::traverseNext()
{
    if (auto* shadowRoot = m_current->shadowRoot()) {
        m_shadowStack.append(shadowRoot->host());
        m_current = shadowRoot;
    }

    if (m_shadowStack.isEmpty())
        m_current = NodeTraversal::next(*m_current, &m_root);
    else
        traverseNextInShadowTree();

    return *this;
}

inline ComposedTreeIterator& ComposedTreeIterator::traverseNextSibling()
{
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    bool isAssignedToSlot = !m_shadowStack.isEmpty() && m_current->parentNode()->shadowRoot();
    if (isAssignedToSlot) {
        traverseNextSiblingSlot();
        return *this;
    }
#endif
    m_current = m_current->nextSibling();
    return *this;
}

inline ComposedTreeIterator& ComposedTreeIterator::traversePreviousSibling()
{
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    bool isAssignedToSlot = !m_shadowStack.isEmpty() && m_current->parentNode()->shadowRoot();
    if (isAssignedToSlot) {
        traversePreviousSiblingSlot();
        return *this;
    }
#endif
    m_current = m_current->previousSibling();
    return *this;
}

inline ComposedTreeIterator& ComposedTreeIterator::traverseParent()
{
    if (m_shadowStack.isEmpty())
        m_current = m_current->parentNode();
    else
        traverseParentInShadowTree();

    return *this;
}

class ComposedTreeDescendantAdapter {
public:
    ComposedTreeDescendantAdapter(ContainerNode& parent)
        : m_parent(parent)
    { }

    ComposedTreeIterator begin() { return ComposedTreeIterator(m_parent, m_parent).traverseNext(); }
    ComposedTreeIterator end() { return ComposedTreeIterator(m_parent); }
    ComposedTreeIterator at(const Node& child) { return ComposedTreeIterator(m_parent, const_cast<Node&>(child)); }
    
private:
    ContainerNode& m_parent;
};

class ComposedTreeChildAdapter {
public:
    class Iterator : public ComposedTreeIterator {
    public:
        Iterator(ContainerNode& root)
            : ComposedTreeIterator(root)
        { }
        Iterator(ContainerNode& root, Node& current)
            : ComposedTreeIterator(root, current)
        { }

        Iterator& operator++() { return static_cast<Iterator&>(traverseNextSibling()); }
        Iterator& operator--() { return static_cast<Iterator&>(traversePreviousSibling()); }
    };

    ComposedTreeChildAdapter(ContainerNode& parent)
        : m_parent(parent)
    { }

    Iterator begin() { return static_cast<Iterator&>(Iterator(m_parent, m_parent).traverseNext()); }
    Iterator end() { return Iterator(m_parent); }
    Iterator at(const Node& child) { return Iterator(m_parent, const_cast<Node&>(child)); }

private:
    ContainerNode& m_parent;
};

// FIXME: We should have const versions too.
inline ComposedTreeDescendantAdapter composedTreeDescendants(ContainerNode& parent)
{
    return ComposedTreeDescendantAdapter(parent);
}

inline ComposedTreeChildAdapter composedTreeChildren(ContainerNode& parent)
{
    return ComposedTreeChildAdapter(parent);
}

}

#endif
