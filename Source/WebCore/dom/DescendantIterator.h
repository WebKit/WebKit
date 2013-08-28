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

#if !ASSERT_DISABLED
#include "DescendantIteratorAssertions.h"
#endif

namespace WebCore {

template <typename ElementType>
class DescendantIterator {
public:
    DescendantIterator(const ContainerNode* root);
    DescendantIterator(const ContainerNode* root, ElementType* current);
    DescendantIterator& operator++();
    ElementType& operator*();
    ElementType* operator->();
    bool operator!=(const DescendantIterator& other) const;

private:
    const ContainerNode* m_root;
    ElementType* m_current;

#if !ASSERT_DISABLED
    DescendantIteratorAssertions m_assertions;
#endif
};

template <typename ElementType>
class DescendantConstIterator {
public:
    DescendantConstIterator(const ContainerNode* root);
    DescendantConstIterator(const ContainerNode* root, const ElementType* current);
    DescendantConstIterator& operator++();
    const ElementType& operator*() const;
    const ElementType* operator->() const;
    bool operator!=(const DescendantConstIterator& other) const;

private:
    const ContainerNode* m_root;
    const ElementType* m_current;

#if !ASSERT_DISABLED
    DescendantIteratorAssertions m_assertions;
#endif
};

template <typename ElementType>
class DescendantIteratorAdapter {
public:
    DescendantIteratorAdapter(ContainerNode* root);
    DescendantIterator<ElementType> begin();
    DescendantIterator<ElementType> end();

private:
    ContainerNode* m_root;
};

template <typename ElementType>
class DescendantConstIteratorAdapter {
public:
    DescendantConstIteratorAdapter(const ContainerNode* root);
    DescendantConstIterator<ElementType> begin() const;
    DescendantConstIterator<ElementType> end() const;

private:
    const ContainerNode* m_root;
};

DescendantIteratorAdapter<Element> elementDescendants(ContainerNode* root);
DescendantConstIteratorAdapter<Element> elementDescendants(const ContainerNode* root);
template <typename ElementType> DescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode* root);
template <typename ElementType> DescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode* root);

// DescendantIterator

template <typename ElementType>
inline DescendantIterator<ElementType>::DescendantIterator(const ContainerNode* root)
    : m_root(root)
    , m_current(nullptr)
{
}

template <typename ElementType>
inline DescendantIterator<ElementType>::DescendantIterator(const ContainerNode* root, ElementType* current)
    : m_root(root)
    , m_current(current)
#if !ASSERT_DISABLED
    , m_assertions(current)
#endif
{
}

template <typename ElementType>
inline DescendantIterator<ElementType>& DescendantIterator<ElementType>::operator++()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::next(m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementType& DescendantIterator<ElementType>::operator*()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

template <typename ElementType>
inline ElementType* DescendantIterator<ElementType>::operator->()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

template <typename ElementType>
inline bool DescendantIterator<ElementType>::operator!=(const DescendantIterator& other) const
{
    ASSERT(m_root == other.m_root);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current != other.m_current;
}

// DescendantConstIterator

template <typename ElementType>
inline DescendantConstIterator<ElementType>::DescendantConstIterator(const ContainerNode* root)
    : m_root(root)
    , m_current(nullptr)
{
}

template <typename ElementType>
inline DescendantConstIterator<ElementType>::DescendantConstIterator(const ContainerNode* root, const ElementType* current)
    : m_root(root)
    , m_current(current)
#if !ASSERT_DISABLED
    , m_assertions(current)
#endif
{
}

template <typename ElementType>
inline DescendantConstIterator<ElementType>& DescendantConstIterator<ElementType>::operator++()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::next(m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline const ElementType& DescendantConstIterator<ElementType>::operator*() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

template <typename ElementType>
inline const ElementType* DescendantConstIterator<ElementType>::operator->() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

template <typename ElementType>
inline bool DescendantConstIterator<ElementType>::operator!=(const DescendantConstIterator& other) const
{
    ASSERT(m_root == other.m_root);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current != other.m_current;
}

// DescendantIteratorAdapter

template <typename ElementType>
inline DescendantIteratorAdapter<ElementType>::DescendantIteratorAdapter(ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline DescendantIterator<ElementType> DescendantIteratorAdapter<ElementType>::begin()
{
    return DescendantIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(m_root));
}

template <typename ElementType>
inline DescendantIterator<ElementType> DescendantIteratorAdapter<ElementType>::end()
{
    return DescendantIterator<ElementType>(m_root);
}

// DescendantConstIteratorAdapter

template <typename ElementType>
inline DescendantConstIteratorAdapter<ElementType>::DescendantConstIteratorAdapter(const ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline DescendantConstIterator<ElementType> DescendantConstIteratorAdapter<ElementType>::begin() const
{
    return DescendantConstIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(m_root));
}

template <typename ElementType>
inline DescendantConstIterator<ElementType> DescendantConstIteratorAdapter<ElementType>::end() const
{
    return DescendantConstIterator<ElementType>(m_root);
}

// Standalone functions

inline DescendantIteratorAdapter<Element> elementDescendants(ContainerNode* root)
{
    return DescendantIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline DescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode* root)
{
    return DescendantIteratorAdapter<ElementType>(root);
}

inline DescendantConstIteratorAdapter<Element> elementDescendants(const ContainerNode* root)
{
    return DescendantConstIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline DescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode* root)
{
    return DescendantConstIteratorAdapter<ElementType>(root);
}

}

#endif
