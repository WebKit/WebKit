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

#include "ElementIterator.h"

namespace WebCore {

template <typename ElementType>
class ChildIterator : public ElementIterator<ElementType> {
public:
    ChildIterator(const ContainerNode* root);
    ChildIterator(const ContainerNode* root, ElementType* current);
    ChildIterator& operator++();
};

template <typename ElementType>
class ChildConstIterator : public ElementConstIterator<ElementType> {
public:
    ChildConstIterator(const ContainerNode* root);
    ChildConstIterator(const ContainerNode* root, const ElementType* current);
    ChildConstIterator& operator++();
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
inline ChildIterator<ElementType>::ChildIterator(const ContainerNode* root)
    : ElementIterator<ElementType>(root)
{
}

template <typename ElementType>
inline ChildIterator<ElementType>::ChildIterator(const ContainerNode* root, ElementType* current)
    : ElementIterator<ElementType>(root, current)
{
}

template <typename ElementType>
inline ChildIterator<ElementType>& ChildIterator<ElementType>::operator++()
{
    return static_cast<ChildIterator<ElementType>&>(ElementIterator<ElementType>::traverseNextSibling());
}

// ChildConstIterator

template <typename ElementType>
inline ChildConstIterator<ElementType>::ChildConstIterator(const ContainerNode* root)
    : ElementConstIterator<ElementType>(root)
{
}

template <typename ElementType>
inline ChildConstIterator<ElementType>::ChildConstIterator(const ContainerNode* root, const ElementType* current)
    : ElementConstIterator<ElementType>(root, current)
{
}

template <typename ElementType>
inline ChildConstIterator<ElementType>& ChildConstIterator<ElementType>::operator++()
{
    return static_cast<ChildConstIterator<ElementType>&>(ElementConstIterator<ElementType>::traverseNextSibling());
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
    return ChildIterator<ElementType>(m_root, Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType>
inline ChildIterator<ElementType> ChildIteratorAdapter<ElementType>::end()
{
    return ChildIterator<ElementType>(m_root);
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
    return ChildConstIterator<ElementType>(m_root, Traversal<ElementType>::firstChild(m_root));
}

template <typename ElementType>
inline ChildConstIterator<ElementType> ChildConstIteratorAdapter<ElementType>::end() const
{
    return ChildConstIterator<ElementType>(m_root);
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
