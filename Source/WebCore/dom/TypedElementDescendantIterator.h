/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#include "ElementIterator.h"

namespace WebCore {

template<typename> class DoubleElementDescendantIterator;
template<typename> class DoubleElementDescendantRange;
template<typename> class ElementDescendantRange;
template<typename ElementType, bool(const ElementType&)> class FilteredElementDescendantRange;

// Range for iterating through descendant elements.
template<typename ElementType> ElementDescendantRange<ElementType> descendantsOfType(ContainerNode&);
template<typename ElementType> ElementDescendantRange<const ElementType> descendantsOfType(const ContainerNode&);

// Range that skips elements where the filter returns false.
template<typename ElementType, bool filter(const ElementType&)> FilteredElementDescendantRange<const ElementType, filter> filteredDescendants(const ContainerNode&);

// Range for use when both sets of descendants are known to be the same length.
// If they are different lengths, this will stop when the shorter one reaches the end, but also an assertion will fail.
template<typename ElementType> DoubleElementDescendantRange<ElementType> descendantsOfType(ContainerNode& firstRoot, ContainerNode& secondRoot);

template<typename ElementType> class ElementDescendantIterator : public ElementIterator<ElementType> {
public:
    ElementDescendantIterator() = default;
    ElementDescendantIterator(const ContainerNode& root, ElementType* current);
    ElementDescendantIterator& operator++();
    ElementDescendantIterator& operator--();
};

template<typename ElementType> class ElementDescendantRange {
public:
    ElementDescendantRange(const ContainerNode& root);
    ElementDescendantIterator<ElementType> begin() const;
    static constexpr std::nullptr_t end() { return nullptr; }
    ElementDescendantIterator<ElementType> beginAt(ElementType&) const;
    ElementDescendantIterator<ElementType> from(Element&) const;

    ElementType* first() const;
    ElementType* last() const;

private:
    const ContainerNode& m_root;
};

template<typename ElementType> class DoubleElementDescendantRange {
public:
    typedef ElementDescendantRange<ElementType> SingleAdapter;
    typedef DoubleElementDescendantIterator<ElementType> Iterator;

    DoubleElementDescendantRange(SingleAdapter&&, SingleAdapter&&);
    Iterator begin() const;
    static constexpr std::nullptr_t end() { return nullptr; }

private:
    std::pair<SingleAdapter, SingleAdapter> m_pair;
};

template<typename ElementType> class DoubleElementDescendantIterator {
public:
    typedef ElementDescendantIterator<ElementType> SingleIterator;
    typedef std::pair<ElementType&, ElementType&> ReferenceProxy;

    DoubleElementDescendantIterator(SingleIterator&&, SingleIterator&&);
    ReferenceProxy operator*() const;
    constexpr bool operator!=(std::nullptr_t) const;
    DoubleElementDescendantIterator& operator++();

private:
    std::pair<SingleIterator, SingleIterator> m_pair;
};

template<typename ElementType, bool filter(const ElementType&)> class FilteredElementDescendantIterator : public ElementIterator<ElementType> {
public:
    FilteredElementDescendantIterator(const ContainerNode&, ElementType* = nullptr);
    FilteredElementDescendantIterator& operator++();
};

template<typename ElementType, bool filter(const ElementType&)> class FilteredElementDescendantRange {
public:
    using Iterator = FilteredElementDescendantIterator<ElementType, filter>;

    FilteredElementDescendantRange(const ContainerNode&);
    Iterator begin() const;
    static constexpr std::nullptr_t end() { return nullptr; }

    ElementType* first() const;

private:
    const ContainerNode& m_root;
};

// ElementDescendantIterator

template<typename ElementType> ElementDescendantIterator<ElementType>::ElementDescendantIterator(const ContainerNode& root, ElementType* current)
    : ElementIterator<ElementType>(&root, current)
{
}

template<typename ElementType> ElementDescendantIterator<ElementType>& ElementDescendantIterator<ElementType>::operator++()
{
    ElementIterator<ElementType>::traverseNext();
    return *this;
}

template<typename ElementType> ElementDescendantIterator<ElementType>& ElementDescendantIterator<ElementType>::operator--()
{
    ElementIterator<ElementType>::traversePrevious();
    return *this;
}

// ElementDescendantRange

template<typename ElementType> ElementDescendantRange<ElementType>::ElementDescendantRange(const ContainerNode& root)
    : m_root(root)
{
}

template<typename ElementType> ElementDescendantIterator<ElementType> ElementDescendantRange<ElementType>::begin() const
{
    return ElementDescendantIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(m_root));
}

template<typename ElementType> ElementDescendantIterator<ElementType> ElementDescendantRange<ElementType>::beginAt(ElementType& descendant) const
{
    ASSERT(descendant.isDescendantOf(m_root));
    return ElementDescendantIterator<ElementType>(m_root, &descendant);
}

template<typename ElementType> ElementDescendantIterator<ElementType> ElementDescendantRange<ElementType>::from(Element& descendant) const
{
    ASSERT(descendant.isDescendantOf(m_root));
    if (is<ElementType>(descendant))
        return ElementDescendantIterator<ElementType>(m_root, downcast<ElementType>(&descendant));
    ElementType* next = Traversal<ElementType>::next(descendant, &m_root);
    return ElementDescendantIterator<ElementType>(m_root, next);
}

template<typename ElementType> ElementType* ElementDescendantRange<ElementType>::first() const
{
    return Traversal<ElementType>::firstWithin(m_root);
}

template<typename ElementType> ElementType* ElementDescendantRange<ElementType>::last() const
{
    return Traversal<ElementType>::lastWithin(m_root);
}

// DoubleElementDescendantRange

template<typename ElementType> DoubleElementDescendantRange<ElementType>::DoubleElementDescendantRange(SingleAdapter&& first, SingleAdapter&& second)
    : m_pair(WTFMove(first), WTFMove(second))
{
}

template<typename ElementType> auto DoubleElementDescendantRange<ElementType>::begin() const -> Iterator
{
    return Iterator(m_pair.first.begin(), m_pair.second.begin());
}

// DoubleElementDescendantIterator

template<typename ElementType> DoubleElementDescendantIterator<ElementType>::DoubleElementDescendantIterator(SingleIterator&& first, SingleIterator&& second)
    : m_pair(WTFMove(first), WTFMove(second))
{
}

template<typename ElementType> auto DoubleElementDescendantIterator<ElementType>::operator*() const -> ReferenceProxy
{
    return { *m_pair.first, *m_pair.second };
}

template<typename ElementType> constexpr bool DoubleElementDescendantIterator<ElementType>::operator!=(std::nullptr_t) const
{
    ASSERT(!m_pair.first == !m_pair.second);
    return m_pair.first;
}

template<typename ElementType> DoubleElementDescendantIterator<ElementType>& DoubleElementDescendantIterator<ElementType>::operator++()
{
    ++m_pair.first;
    ++m_pair.second;
    return *this;
}

// FilteredElementDescendantIterator

template<typename ElementType, bool filter(const ElementType&)> FilteredElementDescendantIterator<ElementType, filter>::FilteredElementDescendantIterator(const ContainerNode& root, ElementType* element)
    : ElementIterator<const ElementType> { &root, element }
{
}

template<typename ElementType, bool filter(const ElementType&)> FilteredElementDescendantIterator<ElementType, filter>& FilteredElementDescendantIterator<ElementType, filter>::operator++()
{
    do {
        ElementIterator<ElementType>::traverseNext();
    } while (*this && !filter(**this));
    return *this;
}

// FilteredElementDescendantRange

template<typename ElementType, bool filter(const ElementType&)> FilteredElementDescendantRange<ElementType, filter>::FilteredElementDescendantRange(const ContainerNode& root)
    : m_root { root }
{
}

template<typename ElementType, bool filter(const ElementType&)> auto FilteredElementDescendantRange<ElementType, filter>::begin() const -> Iterator
{
    return { m_root, first() };
}

template<typename ElementType, bool filter(const ElementType&)> ElementType* FilteredElementDescendantRange<ElementType, filter>::first() const
{
    for (auto* element = Traversal<ElementType>::firstWithin(m_root); element; element = Traversal<ElementType>::next(*element, &m_root)) {
        if (filter(*element))
            return element;
    }
    return nullptr;
}

// Standalone functions

template<typename ElementType> ElementDescendantRange<ElementType> descendantsOfType(ContainerNode& root)
{
    return ElementDescendantRange<ElementType>(root);
}

template<typename ElementType> ElementDescendantRange<const ElementType> descendantsOfType(const ContainerNode& root)
{
    return ElementDescendantRange<const ElementType>(root);
}

template<typename ElementType> DoubleElementDescendantRange<ElementType> descendantsOfType(ContainerNode& firstRoot, ContainerNode& secondRoot)
{
    return { descendantsOfType<ElementType>(firstRoot), descendantsOfType<ElementType>(secondRoot) };
}

template<typename ElementType, bool filter(const ElementType&)> FilteredElementDescendantRange<const ElementType, filter> filteredDescendants(const ContainerNode& root)
{
    return { root };
}

} // namespace WebCore
