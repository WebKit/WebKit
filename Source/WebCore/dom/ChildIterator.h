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

#ifndef ChildIterator_h
#define ChildIterator_h

#include "ElementTraversal.h"

#ifndef NDEBUG
#include "Document.h"
#endif

namespace WebCore {

template <typename ElementType>
class ChildIterator {
public:
    ChildIterator();
    ChildIterator(ElementType* current);
    ChildIterator& operator++();
    ElementType& operator*() { return *m_current; }
    ElementType* operator->() { return m_current; }
    bool operator!=(const ChildIterator& other) const;

private:
    ElementType* m_current;
#ifndef NDEBUG
    OwnPtr<NoEventDispatchAssertion> m_noEventDispatchAssertion;
    uint64_t m_initialDOMTreeVersion;
#endif
};

template <typename ElementType, typename ContainerType>
class ChildIteratorAdapter {
public:
    ChildIteratorAdapter(ContainerType* root);
    ChildIterator<ElementType> begin();
    ChildIterator<ElementType> end();

private:
    const ContainerType* m_root;
};

ChildIteratorAdapter<Element, ContainerNode> elementChildren(ContainerNode* root);
ChildIteratorAdapter<Element, Node> elementChildren(Node* root);
template <typename ElementType> ChildIteratorAdapter<ElementType, ContainerNode> childrenOfType(ContainerNode* root);
template <typename ElementType> ChildIteratorAdapter<ElementType, Node> childrenOfType(Node* root);

template <typename ElementType>
inline ChildIterator<ElementType>::ChildIterator()
    : m_current(nullptr)
#ifndef NDEBUG
    , m_initialDOMTreeVersion(0)
#endif
{
}

template <typename ElementType>
inline ChildIterator<ElementType>::ChildIterator(ElementType* current)
    : m_current(current)
#ifndef NDEBUG
    , m_noEventDispatchAssertion(adoptPtr(new NoEventDispatchAssertion))
    , m_initialDOMTreeVersion(m_current ? m_current->document()->domTreeVersion() : 0)
#endif
{
}

template <typename ElementType>
inline ChildIterator<ElementType>& ChildIterator<ElementType>::operator++()
{
    ASSERT(m_current);
    ASSERT(m_current->document()->domTreeVersion() == m_initialDOMTreeVersion);
    m_current = Traversal<ElementType>::nextSibling(m_current);
    return *this;
}

template <typename ElementType>
inline bool ChildIterator<ElementType>::operator!=(const ChildIterator& other) const
{
    return m_current != other.m_current;
}

template <typename ElementType, typename ContainerType>
inline ChildIteratorAdapter<ElementType, ContainerType>::ChildIteratorAdapter(ContainerType* root)
    : m_root(root)
{
}

template <typename ElementType, typename ContainerType>
inline ChildIterator<ElementType> ChildIteratorAdapter<ElementType, ContainerType>::begin()
{
    return ChildIterator<ElementType>(Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType, typename ContainerType>
inline ChildIterator<ElementType> ChildIteratorAdapter<ElementType, ContainerType>::end()
{
    return ChildIterator<ElementType>();
}

inline ChildIteratorAdapter<Element, ContainerNode> elementChildren(ContainerNode* root)
{
    return ChildIteratorAdapter<Element, ContainerNode>(root);
}

inline ChildIteratorAdapter<Element, Node> elementChildren(Node* root)
{
    return ChildIteratorAdapter<Element, Node>(root);
}

template <typename ElementType>
inline ChildIteratorAdapter<ElementType, ContainerNode> childrenOfType(ContainerNode* root)
{
    return ChildIteratorAdapter<ElementType, ContainerNode>(root);
}

template <typename ElementType>
inline ChildIteratorAdapter<ElementType, Node> childrenOfType(Node* root)
{
    return ChildIteratorAdapter<ElementType, Node>(root);
}

}

#endif
