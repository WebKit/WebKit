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

#if !ASSERT_DISABLED
#include "DescendantIteratorAssertions.h"
#endif

namespace WebCore {

template <typename ElementType>
class ChildIterator {
public:
    ChildIterator();
    ChildIterator(ElementType* current);
    ChildIterator& operator++();
    ElementType& operator*();
    ElementType* operator->();
    bool operator!=(const ChildIterator& other) const;

private:
    ElementType* m_current;

#if !ASSERT_DISABLED
    DescendantIteratorAssertions m_assertions;
#endif
};

template <typename ElementType>
class ChildConstIterator {
public:
    ChildConstIterator();
    ChildConstIterator(const ElementType* current);
    ChildConstIterator& operator++();
    const ElementType& operator*() const;
    const ElementType* operator->() const;
    bool operator!=(const ChildConstIterator& other) const;

private:
    const ElementType* m_current;

#if !ASSERT_DISABLED
    DescendantIteratorAssertions m_assertions;
#endif
};

template <typename ElementType>
class ChildIteratorAdapter {
public:
    ChildIteratorAdapter(ContainerNode* root);
    ChildIterator<ElementType> begin();
    ChildIterator<ElementType> end();

private:
    const ContainerNode* m_root;
};

template <typename ElementType>
class ChildConstIteratorAdapter {
public:
    ChildConstIteratorAdapter(const ContainerNode* root);
    ChildConstIterator<ElementType> begin() const;
    ChildConstIterator<ElementType> end() const;

private:
    const ContainerNode* m_root;
};

ChildIteratorAdapter<Element> elementChildren(ContainerNode* root);
ChildConstIteratorAdapter<Element> elementChildren(const ContainerNode* root);
template <typename ElementType> ChildIteratorAdapter<ElementType> childrenOfType(ContainerNode* root);
template <typename ElementType> ChildConstIteratorAdapter<ElementType> childrenOfType(const ContainerNode* root);

// ChildIterator

template <typename ElementType>
inline ChildIterator<ElementType>::ChildIterator()
    : m_current(nullptr)
{
}

template <typename ElementType>
inline ChildIterator<ElementType>::ChildIterator(ElementType* current)
    : m_current(current)
#if !ASSERT_DISABLED
    , m_assertions(current)
#endif
{
}

template <typename ElementType>
inline ChildIterator<ElementType>& ChildIterator<ElementType>::operator++()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::nextSibling(m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementType& ChildIterator<ElementType>::operator*()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

template <typename ElementType>
inline ElementType* ChildIterator<ElementType>::operator->()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

template <typename ElementType>
inline bool ChildIterator<ElementType>::operator!=(const ChildIterator& other) const
{
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current != other.m_current;
}

// ChildConstIterator

template <typename ElementType>
inline ChildConstIterator<ElementType>::ChildConstIterator()
    : m_current(nullptr)
{
}

template <typename ElementType>
inline ChildConstIterator<ElementType>::ChildConstIterator(const ElementType* current)
    : m_current(current)
#if !ASSERT_DISABLED
    , m_assertions(current)
#endif
{
}

template <typename ElementType>
inline ChildConstIterator<ElementType>& ChildConstIterator<ElementType>::operator++()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::nextSibling(m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline const ElementType& ChildConstIterator<ElementType>::operator*() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

template <typename ElementType>
inline const ElementType* ChildConstIterator<ElementType>::operator->() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

template <typename ElementType>
inline bool ChildConstIterator<ElementType>::operator!=(const ChildConstIterator& other) const
{
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current != other.m_current;
}

// ChildIteratorAdapter

template <typename ElementType>
inline ChildIteratorAdapter<ElementType>::ChildIteratorAdapter(ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline ChildIterator<ElementType> ChildIteratorAdapter<ElementType>::begin()
{
    return ChildIterator<ElementType>(Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType>
inline ChildIterator<ElementType> ChildIteratorAdapter<ElementType>::end()
{
    return ChildIterator<ElementType>();
}

// ChildConstIteratorAdapter

template <typename ElementType>
inline ChildConstIteratorAdapter<ElementType>::ChildConstIteratorAdapter(const ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline ChildConstIterator<ElementType> ChildConstIteratorAdapter<ElementType>::begin() const
{
    return ChildConstIterator<ElementType>(Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType>
inline ChildConstIterator<ElementType> ChildConstIteratorAdapter<ElementType>::end() const
{
    return ChildConstIterator<ElementType>();
}

// Standalone functions

inline ChildIteratorAdapter<Element> elementChildren(ContainerNode* root)
{
    return ChildIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline ChildIteratorAdapter<ElementType> childrenOfType(ContainerNode* root)
{
    return ChildIteratorAdapter<ElementType>(root);
}

inline ChildConstIteratorAdapter<Element> elementChildren(const ContainerNode* root)
{
    return ChildConstIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline ChildConstIteratorAdapter<ElementType> childrenOfType(const ContainerNode* root)
{
    return ChildConstIteratorAdapter<ElementType>(root);
}

}

#endif
