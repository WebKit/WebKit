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

#ifndef ElementDescendantIterator_h
#define ElementDescendantIterator_h

#include "ElementIterator.h"

namespace WebCore {

template <typename ElementType>
class ElementDescendantIterator : public ElementIterator<ElementType> {
public:
    ElementDescendantIterator(const ContainerNode& root);
    ElementDescendantIterator(const ContainerNode& root, ElementType* current);
    ElementDescendantIterator& operator++();
};

template <typename ElementType>
class ElementDescendantConstIterator : public ElementConstIterator<ElementType>  {
public:
    ElementDescendantConstIterator(const ContainerNode& root);
    ElementDescendantConstIterator(const ContainerNode& root, const ElementType* current);
    ElementDescendantConstIterator& operator++();
};

template <typename ElementType>
class ElementDescendantIteratorAdapter {
public:
    ElementDescendantIteratorAdapter(ContainerNode& root);
    ElementDescendantIterator<ElementType> begin();
    ElementDescendantIterator<ElementType> end();
    ElementDescendantIterator<ElementType> beginAt(ElementType&);
    ElementDescendantIterator<ElementType> from(Element&);

    ElementType* first();
    ElementType* last();

private:
    ContainerNode& m_root;
};

template <typename ElementType>
class ElementDescendantConstIteratorAdapter {
public:
    ElementDescendantConstIteratorAdapter(const ContainerNode& root);
    ElementDescendantConstIterator<ElementType> begin() const;
    ElementDescendantConstIterator<ElementType> end() const;
    ElementDescendantConstIterator<ElementType> beginAt(const ElementType&) const;
    ElementDescendantConstIterator<ElementType> from(const Element&) const;

    const ElementType* first() const;
    const ElementType* last() const;

private:
    const ContainerNode& m_root;
};

template <typename ElementType> ElementDescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode&);
template <typename ElementType> ElementDescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode&);

// ElementDescendantIterator

template <typename ElementType>
inline ElementDescendantIterator<ElementType>::ElementDescendantIterator(const ContainerNode& root)
    : ElementIterator<ElementType>(&root)
{
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType>::ElementDescendantIterator(const ContainerNode& root, ElementType* current)
    : ElementIterator<ElementType>(&root, current)
{
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType>& ElementDescendantIterator<ElementType>::operator++()
{
    return static_cast<ElementDescendantIterator<ElementType>&>(ElementIterator<ElementType>::traverseNext());
}

// ElementDescendantConstIterator

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType>::ElementDescendantConstIterator(const ContainerNode& root)
    : ElementConstIterator<ElementType>(&root)

{
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType>::ElementDescendantConstIterator(const ContainerNode& root, const ElementType* current)
    : ElementConstIterator<ElementType>(&root, current)
{
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType>& ElementDescendantConstIterator<ElementType>::operator++()
{
    return static_cast<ElementDescendantConstIterator<ElementType>&>(ElementConstIterator<ElementType>::traverseNext());
}

// ElementDescendantIteratorAdapter

template <typename ElementType>
inline ElementDescendantIteratorAdapter<ElementType>::ElementDescendantIteratorAdapter(ContainerNode& root)
    : m_root(root)
{
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType> ElementDescendantIteratorAdapter<ElementType>::begin()
{
    return ElementDescendantIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(&m_root));
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType> ElementDescendantIteratorAdapter<ElementType>::end()
{
    return ElementDescendantIterator<ElementType>(m_root);
}
    
template <typename ElementType>
inline ElementDescendantIterator<ElementType> ElementDescendantIteratorAdapter<ElementType>::beginAt(ElementType& descendant)
{
    ASSERT(descendant.isDescendantOf(&m_root));
    return ElementDescendantIterator<ElementType>(m_root, &descendant);
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType> ElementDescendantIteratorAdapter<ElementType>::from(Element& descendant)
{
    ASSERT(descendant.isDescendantOf(&m_root));
    if (isElementOfType<const ElementType>(descendant))
        return ElementDescendantIterator<ElementType>(m_root, static_cast<ElementType*>(&descendant));
    ElementType* next = Traversal<ElementType>::next(&m_root, &descendant);
    return ElementDescendantIterator<ElementType>(m_root, next);
}

template <typename ElementType>
inline ElementType* ElementDescendantIteratorAdapter<ElementType>::first()
{
    return Traversal<ElementType>::firstWithin(&m_root);
}

template <typename ElementType>
inline ElementType* ElementDescendantIteratorAdapter<ElementType>::last()
{
    return Traversal<ElementType>::lastWithin(&m_root);
}

// ElementDescendantConstIteratorAdapter

template <typename ElementType>
inline ElementDescendantConstIteratorAdapter<ElementType>::ElementDescendantConstIteratorAdapter(const ContainerNode& root)
    : m_root(root)
{
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType> ElementDescendantConstIteratorAdapter<ElementType>::begin() const
{
    return ElementDescendantConstIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(&m_root));
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType> ElementDescendantConstIteratorAdapter<ElementType>::end() const
{
    return ElementDescendantConstIterator<ElementType>(m_root);
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType> ElementDescendantConstIteratorAdapter<ElementType>::beginAt(const ElementType& descendant) const
{
    ASSERT(descendant.isDescendantOf(&m_root));
    return ElementDescendantConstIterator<ElementType>(m_root, &descendant);
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType> ElementDescendantConstIteratorAdapter<ElementType>::from(const Element& descendant) const
{
    ASSERT(descendant.isDescendantOf(&m_root));
    if (isElementOfType<const ElementType>(descendant))
        return ElementDescendantConstIterator<ElementType>(m_root, static_cast<const ElementType*>(&descendant));
    const ElementType* next = Traversal<ElementType>::next(&m_root, &descendant);
    return ElementDescendantConstIterator<ElementType>(m_root, next);
}

template <typename ElementType>
inline const ElementType* ElementDescendantConstIteratorAdapter<ElementType>::first() const
{
    return Traversal<ElementType>::firstWithin(&m_root);
}

template <typename ElementType>
inline const ElementType* ElementDescendantConstIteratorAdapter<ElementType>::last() const
{
    return Traversal<ElementType>::lastWithin(&m_root);
}

// Standalone functions

template <typename ElementType>
inline ElementDescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode& root)
{
    return ElementDescendantIteratorAdapter<ElementType>(root);
}

template <typename ElementType>
inline ElementDescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode& root)
{
    return ElementDescendantConstIteratorAdapter<ElementType>(root);
}

}

#endif
