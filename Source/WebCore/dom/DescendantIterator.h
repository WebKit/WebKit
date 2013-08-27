/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DescendantIterator_h
#define DescendantIterator_h

#include "ElementTraversal.h"

#ifndef NDEBUG
#include "Document.h"
#endif

namespace WebCore {

template <typename ElementType>
class DescendantIterator {
public:
    DescendantIterator(const Node* root);
    DescendantIterator(const Node* root, ElementType* current);
    DescendantIterator& operator++();
    ElementType& operator*() { return *m_current; }
    ElementType* operator->() { return m_current; }
    bool operator!=(const DescendantIterator& other) const;

private:
    const Node* m_root;
    ElementType* m_current;
#ifndef NDEBUG
    OwnPtr<NoEventDispatchAssertion> m_noEventDispatchAssertion;
    uint64_t m_initialDOMTreeVersion;
#endif
};

template <typename ElementType, typename ContainerType>
class DescendantIteratorAdapter {
public:
    DescendantIteratorAdapter(ContainerType* root);
    DescendantIterator<ElementType> begin();
    DescendantIterator<ElementType> end();

private:
    ContainerType* m_root;
};

DescendantIteratorAdapter<Element, ContainerNode> elementDescendants(ContainerNode* root);
DescendantIteratorAdapter<Element, Node> elementDescendants(Node* root);
template <typename ElementType> DescendantIteratorAdapter<ElementType, ContainerNode> descendantsOfType(ContainerNode* root);
template <typename ElementType> DescendantIteratorAdapter<ElementType, Node> descendantsOfType(Node* root);

template <typename ElementType>
inline DescendantIterator<ElementType>::DescendantIterator(const Node* root)
    : m_root(root)
    , m_current(nullptr)
#ifndef NDEBUG
    , m_initialDOMTreeVersion(0)
#endif
{
}

template <typename ElementType>
inline DescendantIterator<ElementType>::DescendantIterator(const Node* root, ElementType* current)
    : m_root(root)
    , m_current(current)
#ifndef NDEBUG
    , m_noEventDispatchAssertion(adoptPtr(new NoEventDispatchAssertion))
    , m_initialDOMTreeVersion(m_current ? m_current->document()->domTreeVersion() : 0)
#endif
{
}

template <typename ElementType>
inline DescendantIterator<ElementType>& DescendantIterator<ElementType>::operator++()
{
    ASSERT(m_current);
    ASSERT(m_current->document()->domTreeVersion() == m_initialDOMTreeVersion);
    m_current = Traversal<ElementType>::next(m_current, m_root);
    return *this;
}

template <typename ElementType>
inline bool DescendantIterator<ElementType>::operator!=(const DescendantIterator& other) const
{
    ASSERT(m_root == other.m_root);
    return m_current != other.m_current;
}

template <typename ElementType, typename ContainerType>
inline DescendantIteratorAdapter<ElementType, ContainerType>::DescendantIteratorAdapter(ContainerType* root)
    : m_root(root)
{
}

template <typename ElementType, typename ContainerType>
inline DescendantIterator<ElementType> DescendantIteratorAdapter<ElementType, ContainerType>::begin()
{
    return DescendantIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(m_root));
}

template <typename ElementType, typename ContainerType>
inline DescendantIterator<ElementType> DescendantIteratorAdapter<ElementType, ContainerType>::end()
{
    return DescendantIterator<ElementType>(m_root);
}

inline DescendantIteratorAdapter<Element, ContainerNode> elementDescendants(ContainerNode* root)
{
    return DescendantIteratorAdapter<Element, ContainerNode>(root);
}

inline DescendantIteratorAdapter<Element, Node> elementDescendants(Node* root)
{
    return DescendantIteratorAdapter<Element, Node>(root);
}

template <typename ElementType>
inline DescendantIteratorAdapter<ElementType, ContainerNode> descendantsOfType(ContainerNode* root)
{
    return DescendantIteratorAdapter<ElementType, ContainerNode>(root);
}

template <typename ElementType>
inline DescendantIteratorAdapter<ElementType, Node> descendantsOfType(Node* root)
{
    return DescendantIteratorAdapter<ElementType, Node>(root);
}

}

#endif
