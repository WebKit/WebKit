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

#pragma once

#include "ElementTraversal.h"

#if !ASSERT_DISABLED
#include "ElementIteratorAssertions.h"
#endif

namespace WebCore {

template <typename ElementType>
class ElementIterator {
public:
    ElementIterator(const ContainerNode* root);
    ElementIterator(const ContainerNode* root, ElementType* current);

    ElementType& operator*() const;
    ElementType* operator->() const;

    bool operator==(const ElementIterator& other) const;
    bool operator!=(const ElementIterator& other) const;

    ElementIterator& traverseNext();
    ElementIterator& traversePrevious();
    ElementIterator& traverseNextSibling();
    ElementIterator& traversePreviousSibling();
    ElementIterator& traverseNextSkippingChildren();
    ElementIterator& traverseAncestor();

    void dropAssertions();

private:
    const ContainerNode* m_root;
    ElementType* m_current;

#if !ASSERT_DISABLED
    ElementIteratorAssertions m_assertions;
#endif
};

template <typename ElementType>
class ElementConstIterator {
public:
    ElementConstIterator(const ContainerNode* root);
    ElementConstIterator(const ContainerNode* root, const ElementType* current);

    const ElementType& operator*() const;
    const ElementType* operator->() const;

    bool operator==(const ElementConstIterator& other) const;
    bool operator!=(const ElementConstIterator& other) const;

    ElementConstIterator& traverseNext();
    ElementConstIterator& traversePrevious();
    ElementConstIterator& traverseNextSibling();
    ElementConstIterator& traversePreviousSibling();
    ElementConstIterator& traverseNextSkippingChildren();
    ElementConstIterator& traverseAncestor();

    void dropAssertions();

private:
    const ContainerNode* m_root;
    const ElementType* m_current;

#if !ASSERT_DISABLED
    ElementIteratorAssertions m_assertions;
#endif
};

// ElementIterator

template <typename ElementType>
inline ElementIterator<ElementType>::ElementIterator(const ContainerNode* root)
    : m_root(root)
    , m_current(nullptr)
{
}

template <typename ElementType>
inline ElementIterator<ElementType>::ElementIterator(const ContainerNode* root, ElementType* current)
    : m_root(root)
    , m_current(current)
#if !ASSERT_DISABLED
    , m_assertions(current)
#endif
{
}

template <typename ElementType>
inline ElementIterator<ElementType>& ElementIterator<ElementType>::traverseNext()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::next(*m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementIterator<ElementType>& ElementIterator<ElementType>::traversePrevious()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::previous(*m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementIterator<ElementType>& ElementIterator<ElementType>::traverseNextSibling()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::nextSibling(*m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementIterator<ElementType>& ElementIterator<ElementType>::traversePreviousSibling()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::previousSibling(*m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementIterator<ElementType>& ElementIterator<ElementType>::traverseNextSkippingChildren()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::nextSkippingChildren(*m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline void ElementIterator<ElementType>::dropAssertions()
{
#if !ASSERT_DISABLED
    m_assertions.clear();
#endif
}

template <typename ElementType>
inline ElementType* findElementAncestorOfType(const Node& current)
{
    for (Element* ancestor = current.parentElement(); ancestor; ancestor = ancestor->parentElement()) {
        if (is<ElementType>(*ancestor))
            return downcast<ElementType>(ancestor);
    }
    return nullptr;
}

template <>
inline Element* findElementAncestorOfType<Element>(const Node& current)
{
    return current.parentElement();
}

template <typename ElementType>
inline ElementIterator<ElementType>& ElementIterator<ElementType>::traverseAncestor()
{
    ASSERT(m_current);
    ASSERT(m_current != m_root);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = findElementAncestorOfType<ElementType>(*m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementType& ElementIterator<ElementType>::operator*() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

template <typename ElementType>
inline ElementType* ElementIterator<ElementType>::operator->() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

template <typename ElementType>
inline bool ElementIterator<ElementType>::operator==(const ElementIterator& other) const
{
    ASSERT(m_root == other.m_root);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current == other.m_current;
}

template <typename ElementType>
inline bool ElementIterator<ElementType>::operator!=(const ElementIterator& other) const
{
    return !(*this == other);
}

// ElementConstIterator

template <typename ElementType>
inline ElementConstIterator<ElementType>::ElementConstIterator(const ContainerNode* root)
    : m_root(root)
    , m_current(nullptr)
{
}

template <typename ElementType>
inline ElementConstIterator<ElementType>::ElementConstIterator(const ContainerNode* root, const ElementType* current)
    : m_root(root)
    , m_current(current)
#if !ASSERT_DISABLED
    , m_assertions(current)
#endif
{
}

template <typename ElementType>
inline ElementConstIterator<ElementType>& ElementConstIterator<ElementType>::traverseNext()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::next(*m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementConstIterator<ElementType>& ElementConstIterator<ElementType>::traversePrevious()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::previous(*m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementConstIterator<ElementType>& ElementConstIterator<ElementType>::traverseNextSibling()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::nextSibling(*m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementConstIterator<ElementType>& ElementConstIterator<ElementType>::traversePreviousSibling()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::previousSibling(*m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementConstIterator<ElementType>& ElementConstIterator<ElementType>::traverseNextSkippingChildren()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = Traversal<ElementType>::nextSkippingChildren(*m_current, m_root);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline ElementConstIterator<ElementType>& ElementConstIterator<ElementType>::traverseAncestor()
{
    ASSERT(m_current);
    ASSERT(m_current != m_root);
    ASSERT(!m_assertions.domTreeHasMutated());
    m_current = findElementAncestorOfType<const ElementType>(*m_current);
#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif
    return *this;
}

template <typename ElementType>
inline void ElementConstIterator<ElementType>::dropAssertions()
{
#if !ASSERT_DISABLED
    m_assertions.clear();
#endif
}

template <typename ElementType>
inline const ElementType& ElementConstIterator<ElementType>::operator*() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

template <typename ElementType>
inline const ElementType* ElementConstIterator<ElementType>::operator->() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

template <typename ElementType>
inline bool ElementConstIterator<ElementType>::operator==(const ElementConstIterator& other) const
{
    ASSERT(m_root == other.m_root);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current == other.m_current;
}

template <typename ElementType>
inline bool ElementConstIterator<ElementType>::operator!=(const ElementConstIterator& other) const
{
    return !(*this == other);
}

} // namespace WebCore

#include "ElementAncestorIterator.h"
#include "ElementChildIterator.h"
#include "TypedElementDescendantIterator.h"
