/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayoutIterator.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

template <typename T>
class LayoutAncestorIterator : public LayoutIterator<T> {
public:
    LayoutAncestorIterator();
    explicit LayoutAncestorIterator(const T* current);
    LayoutAncestorIterator& operator++();
};

template <typename T>
class LayoutAncestorIteratorAdapter {
public:
    LayoutAncestorIteratorAdapter(const T* first);
    LayoutAncestorIterator<T> begin() const;
    LayoutAncestorIterator<T> end() const;
    const T* first() const;

private:
    const T* m_first;
};

template <typename T> LayoutAncestorIteratorAdapter<T> ancestorsOfType(const Box&);
template <typename T> LayoutAncestorIteratorAdapter<T> lineageOfType(const Box&);

// LayoutAncestorIterator

template <typename T>
inline LayoutAncestorIterator<T>::LayoutAncestorIterator()
    : LayoutIterator<T>(nullptr)
{
}

template <typename T>
inline LayoutAncestorIterator<T>::LayoutAncestorIterator(const T* current)
    : LayoutIterator<T>(nullptr, current)
{
}

template <typename T>
inline LayoutAncestorIterator<T>& LayoutAncestorIterator<T>::operator++()
{
    return static_cast<LayoutAncestorIterator<T>&>(LayoutIterator<T>::traverseAncestor());
}

// LayoutAncestorIteratorAdapter

template <typename T>
inline LayoutAncestorIteratorAdapter<T>::LayoutAncestorIteratorAdapter(const T* first)
    : m_first(first)
{
}

template <typename T>
inline LayoutAncestorIterator<T> LayoutAncestorIteratorAdapter<T>::begin() const
{
    return LayoutAncestorIterator<T>(m_first);
}

template <typename T>
inline LayoutAncestorIterator<T> LayoutAncestorIteratorAdapter<T>::end() const
{
    return LayoutAncestorIterator<T>();
}

template <typename T>
inline const T* LayoutAncestorIteratorAdapter<T>::first() const
{
    return m_first;
}

// Standalone functions

template <typename T>
inline LayoutAncestorIteratorAdapter<T> ancestorsOfType(const Box& descendant)
{
    const T* first = Traversal::findAncestorOfType<const T>(descendant);
    return LayoutAncestorIteratorAdapter<T>(first);
}

template <typename T>
inline LayoutAncestorIteratorAdapter<T> lineageOfType(const Box& first)
{
    if (isLayoutBoxOfType<T>(first))
        return LayoutAncestorIteratorAdapter<T>(static_cast<const T*>(&first));
    return ancestorsOfType<T>(first);
}

}
}
#endif
