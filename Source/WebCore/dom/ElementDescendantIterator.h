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
    ElementDescendantIterator(const ContainerNode* root);
    ElementDescendantIterator(const ContainerNode* root, ElementType* current);
    ElementDescendantIterator& operator++();
};

template <typename ElementType>
class ElementDescendantConstIterator : public ElementConstIterator<ElementType>  {
public:
    ElementDescendantConstIterator(const ContainerNode* root);
    ElementDescendantConstIterator(const ContainerNode* root, const ElementType* current);
    ElementDescendantConstIterator& operator++();
};

template <typename ElementType>
class ElementDescendantIteratorAdapter {
public:
    ElementDescendantIteratorAdapter(ContainerNode* root);
    ElementDescendantIterator<ElementType> begin();
    ElementDescendantIterator<ElementType> end();

private:
    ContainerNode* m_root;
};

template <typename ElementType>
class ElementDescendantConstIteratorAdapter {
public:
    ElementDescendantConstIteratorAdapter(const ContainerNode* root);
    ElementDescendantConstIterator<ElementType> begin() const;
    ElementDescendantConstIterator<ElementType> end() const;

private:
    const ContainerNode* m_root;
};

ElementDescendantIteratorAdapter<Element> elementDescendants(ContainerNode* root);
ElementDescendantConstIteratorAdapter<Element> elementDescendants(const ContainerNode* root);
template <typename ElementType> ElementDescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode* root);
template <typename ElementType> ElementDescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode* root);

// ElementDescendantIterator

template <typename ElementType>
inline ElementDescendantIterator<ElementType>::ElementDescendantIterator(const ContainerNode* root)
    : ElementIterator<ElementType>(root)
{
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType>::ElementDescendantIterator(const ContainerNode* root, ElementType* current)
    : ElementIterator<ElementType>(root, current)
{
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType>& ElementDescendantIterator<ElementType>::operator++()
{
    return static_cast<ElementDescendantIterator<ElementType>&>(ElementIterator<ElementType>::traverseNext());
}

// ElementDescendantConstIterator

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType>::ElementDescendantConstIterator(const ContainerNode* root)
    : ElementConstIterator<ElementType>(root)

{
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType>::ElementDescendantConstIterator(const ContainerNode* root, const ElementType* current)
    : ElementConstIterator<ElementType>(root, current)
{
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType>& ElementDescendantConstIterator<ElementType>::operator++()
{
    return static_cast<ElementDescendantConstIterator<ElementType>&>(ElementConstIterator<ElementType>::traverseNext());
}

// ElementDescendantIteratorAdapter

template <typename ElementType>
inline ElementDescendantIteratorAdapter<ElementType>::ElementDescendantIteratorAdapter(ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType> ElementDescendantIteratorAdapter<ElementType>::begin()
{
    return ElementDescendantIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(m_root));
}

template <typename ElementType>
inline ElementDescendantIterator<ElementType> ElementDescendantIteratorAdapter<ElementType>::end()
{
    return ElementDescendantIterator<ElementType>(m_root);
}

// ElementDescendantConstIteratorAdapter

template <typename ElementType>
inline ElementDescendantConstIteratorAdapter<ElementType>::ElementDescendantConstIteratorAdapter(const ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType> ElementDescendantConstIteratorAdapter<ElementType>::begin() const
{
    return ElementDescendantConstIterator<ElementType>(m_root, Traversal<ElementType>::firstWithin(m_root));
}

template <typename ElementType>
inline ElementDescendantConstIterator<ElementType> ElementDescendantConstIteratorAdapter<ElementType>::end() const
{
    return ElementDescendantConstIterator<ElementType>(m_root);
}

// Standalone functions

inline ElementDescendantIteratorAdapter<Element> elementDescendants(ContainerNode* root)
{
    return ElementDescendantIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline ElementDescendantIteratorAdapter<ElementType> descendantsOfType(ContainerNode* root)
{
    return ElementDescendantIteratorAdapter<ElementType>(root);
}

inline ElementDescendantConstIteratorAdapter<Element> elementDescendants(const ContainerNode* root)
{
    return ElementDescendantConstIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline ElementDescendantConstIteratorAdapter<ElementType> descendantsOfType(const ContainerNode* root)
{
    return ElementDescendantConstIteratorAdapter<ElementType>(root);
}

}

#endif
