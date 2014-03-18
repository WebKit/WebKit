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

#ifndef TypedElementDescendantIterator_h
#define TypedElementDescendantIterator_h

#include "ElementIterator.h"

namespace WebCore {

template <typename ElementType>
class TypedElementDescendantIterator : public ElementIterator<ElementType> {
public:
    TypedElementDescendantIterator(const ContainerNode& root);
    TypedElementDescendantIterator(const ContainerNode& root, ElementType* current);
    TypedElementDescendantIterator& operator++();
};

template <typename ElementType>
class TypedElementDescendantConstIterator : public ElementConstIterator<ElementType>  {
public:
    TypedElementDescendantConstIterator(const ContainerNode& root);
    TypedElementDescendantConstIterator(const ContainerNode& root, const ElementType* current);
    TypedElementDescendantConstIterator& operator++();
};

template <typename ElementType>
class TypedElementDescendantIteratorAdapter {
public:
    TypedElementDescendantIteratorAdapter(ContainerNode& root);
    TypedElementDescendantIterator<ElementType> begin();
    TypedElementDescendantIterator<ElementType> end();
    TypedElementDescendantIterator<ElementType> beginAt(ElementType&);
    TypedElementDescendantIterator<ElementType> from(Element&);

    ElementType* first();
    ElementType* last();

private:
    ContainerNode& m_root;
};

template <typename ElementType>
class TypedElementDescendantConstIteratorAdapter {
public:
    TypedElementDescendantConstIteratorAdapter(const ContainerNode& root);
    TypedElementDescendantConstIterator<ElementType> begin() const;
    TypedElementDescendantConstIterator<ElementType> end() const;
    TypedElementDescendantConstIterator<ElementType> beginAt(const ElementType&) const;
    TypedElementDescendantConstIterator<ElementType> from(const Element&) const;

    const ElementType* first() const;
    const ElementType* last() const;

private:
    const ContainerNode& m_root;
};

template <typename ElementType> TypedElementDescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode&);
template <typename ElementType> TypedElementDescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode&);

// TypedElementDescendantIterator

template <typename ElementType>
inline TypedElementDescendantIterator<ElementType>::TypedElementDescendantIterator(const ContainerNode& root)
    : ElementIterator<ElementType>(&root)
{
}

template <typename ElementType>
inline TypedElementDescendantIterator<ElementType>::TypedElementDescendantIterator(const ContainerNode& root, ElementType* current)
    : ElementIterator<ElementType>(&root, current)
{
}

template <typename ElementType>
inline TypedElementDescendantIterator<ElementType>& TypedElementDescendantIterator<ElementType>::operator++()
{
    return static_cast<TypedElementDescendantIterator<ElementType>&>(ElementIterator<ElementType>::traverseNext());
}

// TypedElementDescendantConstIterator

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType>::TypedElementDescendantConstIterator(const ContainerNode& root)
    : ElementConstIterator<ElementType>(&root)

{
}

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType>::TypedElementDescendantConstIterator(const ContainerNode& root, const ElementType* current)
    : ElementConstIterator<ElementType>(&root, current)
{
}

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType>& TypedElementDescendantConstIterator<ElementType>::operator++()
{
    return static_cast<TypedElementDescendantConstIterator<ElementType>&>(ElementConstIterator<ElementType>::traverseNext());
}

// TypedElementDescendantIteratorAdapter

template <typename ElementType>
inline TypedElementDescendantIteratorAdapter<ElementType>::TypedElementDescendantIteratorAdapter(ContainerNode& root)
    : m_root(root)
{
}

template <typename ElementType>
inline TypedElementDescendantIterator<ElementType> TypedElementDescendantIteratorAdapter<ElementType>::begin()
{
    return TypedElementDescendantIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(&m_root));
}

template <typename ElementType>
inline TypedElementDescendantIterator<ElementType> TypedElementDescendantIteratorAdapter<ElementType>::end()
{
    return TypedElementDescendantIterator<ElementType>(m_root);
}
    
template <typename ElementType>
inline TypedElementDescendantIterator<ElementType> TypedElementDescendantIteratorAdapter<ElementType>::beginAt(ElementType& descendant)
{
    ASSERT(descendant.isDescendantOf(&m_root));
    return TypedElementDescendantIterator<ElementType>(m_root, &descendant);
}

template <typename ElementType>
inline TypedElementDescendantIterator<ElementType> TypedElementDescendantIteratorAdapter<ElementType>::from(Element& descendant)
{
    ASSERT(descendant.isDescendantOf(&m_root));
    if (isElementOfType<const ElementType>(descendant))
        return TypedElementDescendantIterator<ElementType>(m_root, static_cast<ElementType*>(&descendant));
    ElementType* next = Traversal<ElementType>::next(&m_root, &descendant);
    return TypedElementDescendantIterator<ElementType>(m_root, next);
}

template <typename ElementType>
inline ElementType* TypedElementDescendantIteratorAdapter<ElementType>::first()
{
    return Traversal<ElementType>::firstWithin(&m_root);
}

template <typename ElementType>
inline ElementType* TypedElementDescendantIteratorAdapter<ElementType>::last()
{
    return Traversal<ElementType>::lastWithin(&m_root);
}

// TypedElementDescendantConstIteratorAdapter

template <typename ElementType>
inline TypedElementDescendantConstIteratorAdapter<ElementType>::TypedElementDescendantConstIteratorAdapter(const ContainerNode& root)
    : m_root(root)
{
}

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType> TypedElementDescendantConstIteratorAdapter<ElementType>::begin() const
{
    return TypedElementDescendantConstIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(&m_root));
}

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType> TypedElementDescendantConstIteratorAdapter<ElementType>::end() const
{
    return TypedElementDescendantConstIterator<ElementType>(m_root);
}

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType> TypedElementDescendantConstIteratorAdapter<ElementType>::beginAt(const ElementType& descendant) const
{
    ASSERT(descendant.isDescendantOf(&m_root));
    return TypedElementDescendantConstIterator<ElementType>(m_root, &descendant);
}

template <typename ElementType>
inline TypedElementDescendantConstIterator<ElementType> TypedElementDescendantConstIteratorAdapter<ElementType>::from(const Element& descendant) const
{
    ASSERT(descendant.isDescendantOf(&m_root));
    if (isElementOfType<const ElementType>(descendant))
        return TypedElementDescendantConstIterator<ElementType>(m_root, static_cast<const ElementType*>(&descendant));
    const ElementType* next = Traversal<ElementType>::next(&m_root, &descendant);
    return TypedElementDescendantConstIterator<ElementType>(m_root, next);
}

template <typename ElementType>
inline const ElementType* TypedElementDescendantConstIteratorAdapter<ElementType>::first() const
{
    return Traversal<ElementType>::firstWithin(&m_root);
}

template <typename ElementType>
inline const ElementType* TypedElementDescendantConstIteratorAdapter<ElementType>::last() const
{
    return Traversal<ElementType>::lastWithin(&m_root);
}

// Standalone functions

template <typename ElementType>
inline TypedElementDescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode& root)
{
    return TypedElementDescendantIteratorAdapter<ElementType>(root);
}

template <typename ElementType>
inline TypedElementDescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode& root)
{
    return TypedElementDescendantConstIteratorAdapter<ElementType>(root);
}

}

#endif
