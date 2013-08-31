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

#ifndef ElementChildIterator_h
#define ElementChildIterator_h

#include "ElementIterator.h"

namespace WebCore {

template <typename ElementType>
class ElementChildIterator : public ElementIterator<ElementType> {
public:
    ElementChildIterator(const ContainerNode* root);
    ElementChildIterator(const ContainerNode* root, ElementType* current);
    ElementChildIterator& operator++();
};

template <typename ElementType>
class ElementChildConstIterator : public ElementConstIterator<ElementType> {
public:
    ElementChildConstIterator(const ContainerNode* root);
    ElementChildConstIterator(const ContainerNode* root, const ElementType* current);
    ElementChildConstIterator& operator++();
};

template <typename ElementType>
class ElementChildIteratorAdapter {
public:
    ElementChildIteratorAdapter(ContainerNode* root);
    ElementChildIterator<ElementType> begin();
    ElementChildIterator<ElementType> end();

private:
    const ContainerNode* m_root;
};

template <typename ElementType>
class ElementChildConstIteratorAdapter {
public:
    ElementChildConstIteratorAdapter(const ContainerNode* root);
    ElementChildConstIterator<ElementType> begin() const;
    ElementChildConstIterator<ElementType> end() const;

private:
    const ContainerNode* m_root;
};

ElementChildIteratorAdapter<Element> elementChildren(ContainerNode* root);
ElementChildConstIteratorAdapter<Element> elementChildren(const ContainerNode* root);
template <typename ElementType> ElementChildIteratorAdapter<ElementType> childrenOfType(ContainerNode* root);
template <typename ElementType> ElementChildConstIteratorAdapter<ElementType> childrenOfType(const ContainerNode* root);

// ElementChildIterator

template <typename ElementType>
inline ElementChildIterator<ElementType>::ElementChildIterator(const ContainerNode* root)
    : ElementIterator<ElementType>(root)
{
}

template <typename ElementType>
inline ElementChildIterator<ElementType>::ElementChildIterator(const ContainerNode* root, ElementType* current)
    : ElementIterator<ElementType>(root, current)
{
}

template <typename ElementType>
inline ElementChildIterator<ElementType>& ElementChildIterator<ElementType>::operator++()
{
    return static_cast<ElementChildIterator<ElementType>&>(ElementIterator<ElementType>::traverseNextSibling());
}

// ElementChildConstIterator

template <typename ElementType>
inline ElementChildConstIterator<ElementType>::ElementChildConstIterator(const ContainerNode* root)
    : ElementConstIterator<ElementType>(root)
{
}

template <typename ElementType>
inline ElementChildConstIterator<ElementType>::ElementChildConstIterator(const ContainerNode* root, const ElementType* current)
    : ElementConstIterator<ElementType>(root, current)
{
}

template <typename ElementType>
inline ElementChildConstIterator<ElementType>& ElementChildConstIterator<ElementType>::operator++()
{
    return static_cast<ElementChildConstIterator<ElementType>&>(ElementConstIterator<ElementType>::traverseNextSibling());
}

// ElementChildIteratorAdapter

template <typename ElementType>
inline ElementChildIteratorAdapter<ElementType>::ElementChildIteratorAdapter(ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline ElementChildIterator<ElementType> ElementChildIteratorAdapter<ElementType>::begin()
{
    return ElementChildIterator<ElementType>(m_root, Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType>
inline ElementChildIterator<ElementType> ElementChildIteratorAdapter<ElementType>::end()
{
    return ElementChildIterator<ElementType>(m_root);
}

// ElementChildConstIteratorAdapter

template <typename ElementType>
inline ElementChildConstIteratorAdapter<ElementType>::ElementChildConstIteratorAdapter(const ContainerNode* root)
    : m_root(root)
{
}

template <typename ElementType>
inline ElementChildConstIterator<ElementType> ElementChildConstIteratorAdapter<ElementType>::begin() const
{
    return ElementChildConstIterator<ElementType>(m_root, Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType>
inline ElementChildConstIterator<ElementType> ElementChildConstIteratorAdapter<ElementType>::end() const
{
    return ElementChildConstIterator<ElementType>(m_root);
}

// Standalone functions

inline ElementChildIteratorAdapter<Element> elementChildren(ContainerNode* root)
{
    return ElementChildIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline ElementChildIteratorAdapter<ElementType> childrenOfType(ContainerNode* root)
{
    return ElementChildIteratorAdapter<ElementType>(root);
}

inline ElementChildConstIteratorAdapter<Element> elementChildren(const ContainerNode* root)
{
    return ElementChildConstIteratorAdapter<Element>(root);
}

template <typename ElementType>
inline ElementChildConstIteratorAdapter<ElementType> childrenOfType(const ContainerNode* root)
{
    return ElementChildConstIteratorAdapter<ElementType>(root);
}

}

#endif
