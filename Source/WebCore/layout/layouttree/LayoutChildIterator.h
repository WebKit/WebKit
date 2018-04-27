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
class LayoutChildtIterator : public LayoutIterator<T> {
public:
    LayoutChildtIterator(const Container& parent);
    LayoutChildtIterator(const Container& parent, const T* current);
    LayoutChildtIterator& operator++();
};

template <typename T>
class LayoutChildtIteratorAdapter {
public:
    LayoutChildtIteratorAdapter(const Container& parent);
    LayoutChildtIterator<T> begin() const;
    LayoutChildtIterator<T> end() const;
    const T* first() const;
    const T* last() const;

private:
    const Container& m_parent;
};

template <typename T> LayoutChildtIteratorAdapter<T> childrenOfType(const Container&);

// LayoutChildtIterator

template <typename T>
inline LayoutChildtIterator<T>::LayoutChildtIterator(const Container& parent)
    : LayoutIterator<T>(&parent)
{
}

template <typename T>
inline LayoutChildtIterator<T>::LayoutChildtIterator(const Container& parent, const T* current)
    : LayoutIterator<T>(&parent, current)
{
}

template <typename T>
inline LayoutChildtIterator<T>& LayoutChildtIterator<T>::operator++()
{
    return static_cast<LayoutChildtIterator<T>&>(LayoutIterator<T>::traverseNextSibling());
}

// LayoutChildtIteratorAdapter

template <typename T>
inline LayoutChildtIteratorAdapter<T>::LayoutChildtIteratorAdapter(const Container& parent)
    : m_parent(parent)
{
}

template <typename T>
inline LayoutChildtIterator<T> LayoutChildtIteratorAdapter<T>::begin() const
{
    return LayoutChildtIterator<T>(m_parent, Traversal::firstChild<T>(m_parent));
}

template <typename T>
inline LayoutChildtIterator<T> LayoutChildtIteratorAdapter<T>::end() const
{
    return LayoutChildtIterator<T>(m_parent);
}

template <typename T>
inline const T* LayoutChildtIteratorAdapter<T>::first() const
{
    return Traversal::firstChild<T>(m_parent);
}

template <typename T>
inline const T* LayoutChildtIteratorAdapter<T>::last() const
{
    return Traversal::lastChild<T>(m_parent);
}

template <typename T>
inline LayoutChildtIteratorAdapter<T> childrenOfType(const Container& parent)
{
    return LayoutChildtIteratorAdapter<T>(parent);
}

}
}
#endif
