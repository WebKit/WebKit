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

#include "RenderIterator.h"

namespace WebCore {

template <typename T>
class RenderChildIterator : public RenderIterator<T> {
public:
    RenderChildIterator(const RenderElement& parent);
    RenderChildIterator(const RenderElement& parent, T* current);
    RenderChildIterator& operator++();
};

template <typename T>
class RenderChildConstIterator : public RenderConstIterator<T> {
public:
    RenderChildConstIterator(const RenderElement& parent);
    RenderChildConstIterator(const RenderElement& parent, const T* current);
    RenderChildConstIterator& operator++();
};

template <typename T>
class RenderChildIteratorAdapter {
public:
    RenderChildIteratorAdapter(RenderElement& parent);
    RenderChildIterator<T> begin();
    RenderChildIterator<T> end();
    T* first();
    T* last();

private:
    RenderElement& m_parent;
};

template <typename T>
class RenderChildConstIteratorAdapter {
public:
    RenderChildConstIteratorAdapter(const RenderElement& parent);
    RenderChildConstIterator<T> begin() const;
    RenderChildConstIterator<T> end() const;
    const T* first() const;
    const T* last() const;

private:
    const RenderElement& m_parent;
};

template <typename T> RenderChildIteratorAdapter<T> childrenOfType(RenderElement&);
template <typename T> RenderChildConstIteratorAdapter<T> childrenOfType(const RenderElement&);

// RenderChildIterator

template <typename T>
inline RenderChildIterator<T>::RenderChildIterator(const RenderElement& parent)
    : RenderIterator<T>(&parent)
{
}

template <typename T>
inline RenderChildIterator<T>::RenderChildIterator(const RenderElement& parent, T* current)
    : RenderIterator<T>(&parent, current)
{
}

template <typename T>
inline RenderChildIterator<T>& RenderChildIterator<T>::operator++()
{
    return static_cast<RenderChildIterator<T>&>(RenderIterator<T>::traverseNextSibling());
}

// RenderChildConstIterator

template <typename T>
inline RenderChildConstIterator<T>::RenderChildConstIterator(const RenderElement& parent)
    : RenderConstIterator<T>(&parent)
{
}

template <typename T>
inline RenderChildConstIterator<T>::RenderChildConstIterator(const RenderElement& parent, const T* current)
    : RenderConstIterator<T>(&parent, current)
{
}

template <typename T>
inline RenderChildConstIterator<T>& RenderChildConstIterator<T>::operator++()
{
    return static_cast<RenderChildConstIterator<T>&>(RenderConstIterator<T>::traverseNextSibling());
}

// RenderChildIteratorAdapter

template <typename T>
inline RenderChildIteratorAdapter<T>::RenderChildIteratorAdapter(RenderElement& parent)
    : m_parent(parent)
{
}

template <typename T>
inline RenderChildIterator<T> RenderChildIteratorAdapter<T>::begin()
{
    return RenderChildIterator<T>(m_parent, RenderTraversal::firstChild<T>(m_parent));
}

template <typename T>
inline RenderChildIterator<T> RenderChildIteratorAdapter<T>::end()
{
    return RenderChildIterator<T>(m_parent);
}

template <typename T>
inline T* RenderChildIteratorAdapter<T>::first()
{
    return RenderTraversal::firstChild<T>(m_parent);
}

template <typename T>
inline T* RenderChildIteratorAdapter<T>::last()
{
    return RenderTraversal::lastChild<T>(m_parent);
}

// RenderChildConstIteratorAdapter

template <typename T>
inline RenderChildConstIteratorAdapter<T>::RenderChildConstIteratorAdapter(const RenderElement& parent)
    : m_parent(parent)
{
}

template <typename T>
inline RenderChildConstIterator<T> RenderChildConstIteratorAdapter<T>::begin() const
{
    return RenderChildConstIterator<T>(m_parent, RenderTraversal::firstChild<T>(m_parent));
}

template <typename T>
inline RenderChildConstIterator<T> RenderChildConstIteratorAdapter<T>::end() const
{
    return RenderChildConstIterator<T>(m_parent);
}

template <typename T>
inline const T* RenderChildConstIteratorAdapter<T>::first() const
{
    return RenderTraversal::firstChild<T>(m_parent);
}

template <typename T>
inline const T* RenderChildConstIteratorAdapter<T>::last() const
{
    return RenderTraversal::lastChild<T>(m_parent);
}

// Standalone functions

template <typename T>
inline RenderChildIteratorAdapter<T> childrenOfType(RenderElement& parent)
{
    return RenderChildIteratorAdapter<T>(parent);
}

template <typename T>
inline RenderChildConstIteratorAdapter<T> childrenOfType(const RenderElement& parent)
{
    return RenderChildConstIteratorAdapter<T>(parent);
}

} // namespace WebCore
