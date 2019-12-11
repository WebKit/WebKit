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

#include "ElementIterator.h"

namespace WebCore {

template <typename ElementType>
class ElementAncestorIterator : public ElementIterator<ElementType> {
public:
    ElementAncestorIterator();
    explicit ElementAncestorIterator(ElementType* current);
    ElementAncestorIterator& operator++();
};

template <typename ElementType>
class ElementAncestorConstIterator : public ElementConstIterator<ElementType> {
public:
    ElementAncestorConstIterator();
    explicit ElementAncestorConstIterator(const ElementType* current);
    ElementAncestorConstIterator& operator++();
};

template <typename ElementType>
class ElementAncestorIteratorAdapter {
public:
    explicit ElementAncestorIteratorAdapter(ElementType* first);
    ElementAncestorIterator<ElementType> begin();
    ElementAncestorIterator<ElementType> end();
    ElementType* first() { return m_first; }

private:
    ElementType* m_first;
};

template <typename ElementType>
class ElementAncestorConstIteratorAdapter {
public:
    explicit ElementAncestorConstIteratorAdapter(const ElementType* first);
    ElementAncestorConstIterator<ElementType> begin() const;
    ElementAncestorConstIterator<ElementType> end() const;
    const ElementType* first() const { return m_first; }

private:
    const ElementType* m_first;
};

ElementAncestorIteratorAdapter<Element> elementLineage(Element* first);
ElementAncestorConstIteratorAdapter<Element> elementLineage(const Element* first);
ElementAncestorIteratorAdapter<Element> elementAncestors(Element* descendant);
ElementAncestorConstIteratorAdapter<Element> elementAncestors(const Element* descendant);
template <typename ElementType> ElementAncestorIteratorAdapter<ElementType> lineageOfType(Element& first);
template <typename ElementType> ElementAncestorConstIteratorAdapter<ElementType> lineageOfType(const Element& first);
template <typename ElementType> ElementAncestorIteratorAdapter<ElementType> ancestorsOfType(Node& descendant);
template <typename ElementType> ElementAncestorConstIteratorAdapter<ElementType> ancestorsOfType(const Node& descendant);

// ElementAncestorIterator

template <typename ElementType>
inline ElementAncestorIterator<ElementType>::ElementAncestorIterator()
    : ElementIterator<ElementType>(nullptr)
{
}

template <typename ElementType>
inline ElementAncestorIterator<ElementType>::ElementAncestorIterator(ElementType* current)
    : ElementIterator<ElementType>(nullptr, current)
{
}

template <typename ElementType>
inline ElementAncestorIterator<ElementType>& ElementAncestorIterator<ElementType>::operator++()
{
    return static_cast<ElementAncestorIterator<ElementType>&>(ElementIterator<ElementType>::traverseAncestor());
}

// ElementAncestorConstIterator

template <typename ElementType>
inline ElementAncestorConstIterator<ElementType>::ElementAncestorConstIterator()
    : ElementConstIterator<ElementType>(nullptr)
{
}

template <typename ElementType>
inline ElementAncestorConstIterator<ElementType>::ElementAncestorConstIterator(const ElementType* current)
    : ElementConstIterator<ElementType>(nullptr, current)
{
}

template <typename ElementType>
inline ElementAncestorConstIterator<ElementType>& ElementAncestorConstIterator<ElementType>::operator++()
{
    return static_cast<ElementAncestorConstIterator<ElementType>&>(ElementConstIterator<ElementType>::traverseAncestor());
}

// ElementAncestorIteratorAdapter

template <typename ElementType>
inline ElementAncestorIteratorAdapter<ElementType>::ElementAncestorIteratorAdapter(ElementType* first)
    : m_first(first)
{
}

template <typename ElementType>
inline ElementAncestorIterator<ElementType> ElementAncestorIteratorAdapter<ElementType>::begin()
{
    return ElementAncestorIterator<ElementType>(m_first);
}

template <typename ElementType>
inline ElementAncestorIterator<ElementType> ElementAncestorIteratorAdapter<ElementType>::end()
{
    return ElementAncestorIterator<ElementType>();
}

// ElementAncestorConstIteratorAdapter

template <typename ElementType>
inline ElementAncestorConstIteratorAdapter<ElementType>::ElementAncestorConstIteratorAdapter(const ElementType* first)
    : m_first(first)
{
}

template <typename ElementType>
inline ElementAncestorConstIterator<ElementType> ElementAncestorConstIteratorAdapter<ElementType>::begin() const
{
    return ElementAncestorConstIterator<ElementType>(m_first);
}

template <typename ElementType>
inline ElementAncestorConstIterator<ElementType> ElementAncestorConstIteratorAdapter<ElementType>::end() const
{
    return ElementAncestorConstIterator<ElementType>();
}

// Standalone functions

inline ElementAncestorIteratorAdapter<Element> elementLineage(Element* first)
{
    return ElementAncestorIteratorAdapter<Element>(first);
}

inline ElementAncestorConstIteratorAdapter<Element> elementLineage(const Element* first)
{
    return ElementAncestorConstIteratorAdapter<Element>(first);
}

inline ElementAncestorIteratorAdapter<Element> elementAncestors(Element* descendant)
{
    return ElementAncestorIteratorAdapter<Element>(descendant->parentElement());
}

inline ElementAncestorConstIteratorAdapter<Element> elementAncestors(const Element* descendant)
{
    return ElementAncestorConstIteratorAdapter<Element>(descendant->parentElement());
}

template <typename ElementType>
inline ElementAncestorIteratorAdapter<ElementType> lineageOfType(Element& first)
{
    if (is<ElementType>(first))
        return ElementAncestorIteratorAdapter<ElementType>(static_cast<ElementType*>(&first));
    return ancestorsOfType<ElementType>(first);
}

template <typename ElementType>
inline ElementAncestorConstIteratorAdapter<ElementType> lineageOfType(const Element& first)
{
    if (is<ElementType>(first))
        return ElementAncestorConstIteratorAdapter<ElementType>(static_cast<const ElementType*>(&first));
    return ancestorsOfType<ElementType>(first);
}

template <typename ElementType>
inline ElementAncestorIteratorAdapter<ElementType> ancestorsOfType(Node& descendant)
{
    ElementType* first = findElementAncestorOfType<ElementType>(descendant);
    return ElementAncestorIteratorAdapter<ElementType>(first);
}

template <typename ElementType>
inline ElementAncestorConstIteratorAdapter<ElementType> ancestorsOfType(const Node& descendant)
{
    const ElementType* first = findElementAncestorOfType<const ElementType>(descendant);
    return ElementAncestorConstIteratorAdapter<ElementType>(first);
}

} // namespace WebCore
