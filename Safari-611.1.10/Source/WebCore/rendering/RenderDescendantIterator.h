/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "RenderIterator.h"

namespace WebCore {

template <typename T>
class RenderDescendantIterator : public RenderIterator<T> {
public:
    RenderDescendantIterator(const RenderElement& root);
    RenderDescendantIterator(const RenderElement& root, T* current);
    RenderDescendantIterator& operator++();
};

template <typename T>
class RenderDescendantConstIterator : public RenderConstIterator<T> {
public:
    RenderDescendantConstIterator(const RenderElement& root);
    RenderDescendantConstIterator(const RenderElement& root, const T* current);
    RenderDescendantConstIterator& operator++();
};

template <typename T>
class RenderDescendantIteratorAdapter {
public:
    RenderDescendantIteratorAdapter(RenderElement& root);
    RenderDescendantIterator<T> begin();
    RenderDescendantIterator<T> end();
    RenderDescendantIterator<T> at(T&);

private:
    RenderElement& m_root;
};

template <typename T>
class RenderDescendantConstIteratorAdapter {
public:
    RenderDescendantConstIteratorAdapter(const RenderElement& root);
    RenderDescendantConstIterator<T> begin() const;
    RenderDescendantConstIterator<T> end() const;
    RenderDescendantConstIterator<T> at(const T&) const;

private:
    const RenderElement& m_root;
};

template <typename T> RenderDescendantIteratorAdapter<T> descendantsOfType(RenderElement&);
template <typename T> RenderDescendantConstIteratorAdapter<T> descendantsOfType(const RenderElement&);

// RenderDescendantIterator

template <typename T>
inline RenderDescendantIterator<T>::RenderDescendantIterator(const RenderElement& root)
    : RenderIterator<T>(&root)
{
}

template <typename T>
inline RenderDescendantIterator<T>::RenderDescendantIterator(const RenderElement& root, T* current)
    : RenderIterator<T>(&root, current)
{
}

template <typename T>
inline RenderDescendantIterator<T>& RenderDescendantIterator<T>::operator++()
{
    return static_cast<RenderDescendantIterator<T>&>(RenderIterator<T>::traverseNext());
}

// RenderDescendantConstIterator

template <typename T>
inline RenderDescendantConstIterator<T>::RenderDescendantConstIterator(const RenderElement& root)
    : RenderConstIterator<T>(&root)
{
}

template <typename T>
inline RenderDescendantConstIterator<T>::RenderDescendantConstIterator(const RenderElement& root, const T* current)
    : RenderConstIterator<T>(&root, current)
{
}

template <typename T>
inline RenderDescendantConstIterator<T>& RenderDescendantConstIterator<T>::operator++()
{
    return static_cast<RenderDescendantConstIterator<T>&>(RenderConstIterator<T>::traverseNext());
}

// RenderDescendantIteratorAdapter

template <typename T>
inline RenderDescendantIteratorAdapter<T>::RenderDescendantIteratorAdapter(RenderElement& root)
    : m_root(root)
{
}

template <typename T>
inline RenderDescendantIterator<T> RenderDescendantIteratorAdapter<T>::begin()
{
    return RenderDescendantIterator<T>(m_root, RenderTraversal::firstWithin<T>(m_root));
}

template <typename T>
inline RenderDescendantIterator<T> RenderDescendantIteratorAdapter<T>::end()
{
    return RenderDescendantIterator<T>(m_root);
}

template <typename T>
inline RenderDescendantIterator<T> RenderDescendantIteratorAdapter<T>::at(T& current)
{
    return RenderDescendantIterator<T>(m_root, &current);
}

// RenderDescendantConstIteratorAdapter

template <typename T>
inline RenderDescendantConstIteratorAdapter<T>::RenderDescendantConstIteratorAdapter(const RenderElement& root)
    : m_root(root)
{
}

template <typename T>
inline RenderDescendantConstIterator<T> RenderDescendantConstIteratorAdapter<T>::begin() const
{
    return RenderDescendantConstIterator<T>(m_root, RenderTraversal::firstWithin<T>(m_root));
}

template <typename T>
inline RenderDescendantConstIterator<T> RenderDescendantConstIteratorAdapter<T>::end() const
{
    return RenderDescendantConstIterator<T>(m_root);
}

template <typename T>
inline RenderDescendantConstIterator<T> RenderDescendantConstIteratorAdapter<T>::at(const T& current) const
{
    return RenderDescendantConstIterator<T>(m_root, &current);
}

// Standalone functions

template <typename T>
inline RenderDescendantIteratorAdapter<T> descendantsOfType(RenderElement& root)
{
    return RenderDescendantIteratorAdapter<T>(root);
}

template <typename T>
inline RenderDescendantConstIteratorAdapter<T> descendantsOfType(const RenderElement& root)
{
    return RenderDescendantConstIteratorAdapter<T>(root);
}

} // namespace WebCore
