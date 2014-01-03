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

#ifndef RenderAncestorIterator_h
#define RenderAncestorIterator_h

#include "RenderIterator.h"

namespace WebCore {

template <typename T>
class RenderAncestorIterator : public RenderIterator<T> {
public:
    RenderAncestorIterator();
    explicit RenderAncestorIterator(T* current);
    RenderAncestorIterator& operator++();
};

template <typename T>
class RenderAncestorConstIterator : public RenderConstIterator<T> {
public:
    RenderAncestorConstIterator();
    explicit RenderAncestorConstIterator(const T* current);
    RenderAncestorConstIterator& operator++();
};

template <typename T>
class RenderAncestorIteratorAdapter {
public:
    RenderAncestorIteratorAdapter(T* first);
    RenderAncestorIterator<T> begin();
    RenderAncestorIterator<T> end();
    T* first();

private:
    T* m_first;
};

template <typename T>
class RenderAncestorConstIteratorAdapter {
public:
    RenderAncestorConstIteratorAdapter(const T* first);
    RenderAncestorConstIterator<T> begin() const;
    RenderAncestorConstIterator<T> end() const;
    const T* first() const;

private:
    const T* m_first;
};

template <typename T> RenderAncestorIteratorAdapter<T> ancestorsOfType(RenderObject&);
template <typename T> RenderAncestorConstIteratorAdapter<T> ancestorsOfType(const RenderObject&);
template <typename T> RenderAncestorIteratorAdapter<T> lineageOfType(RenderObject&);
template <typename T> RenderAncestorConstIteratorAdapter<T> lineageOfType(const RenderObject&);

// RenderAncestorIterator

template <typename T>
inline RenderAncestorIterator<T>::RenderAncestorIterator()
    : RenderIterator<T>(nullptr)
{
}

template <typename T>
inline RenderAncestorIterator<T>::RenderAncestorIterator(T* current)
    : RenderIterator<T>(nullptr, current)
{
}

template <typename T>
inline RenderAncestorIterator<T>& RenderAncestorIterator<T>::operator++()
{
    return static_cast<RenderAncestorIterator<T>&>(RenderIterator<T>::traverseAncestor());
}

// RenderAncestorConstIterator

template <typename T>
inline RenderAncestorConstIterator<T>::RenderAncestorConstIterator()
    : RenderConstIterator<T>(nullptr)
{
}

template <typename T>
inline RenderAncestorConstIterator<T>::RenderAncestorConstIterator(const T* current)
    : RenderConstIterator<T>(nullptr, current)
{
}

template <typename T>
inline RenderAncestorConstIterator<T>& RenderAncestorConstIterator<T>::operator++()
{
    return static_cast<RenderAncestorConstIterator<T>&>(RenderConstIterator<T>::traverseAncestor());
}

// RenderAncestorIteratorAdapter

template <typename T>
inline RenderAncestorIteratorAdapter<T>::RenderAncestorIteratorAdapter(T* first)
    : m_first(first)
{
}

template <typename T>
inline RenderAncestorIterator<T> RenderAncestorIteratorAdapter<T>::begin()
{
    return RenderAncestorIterator<T>(m_first);
}

template <typename T>
inline RenderAncestorIterator<T> RenderAncestorIteratorAdapter<T>::end()
{
    return RenderAncestorIterator<T>();
}

template <typename T>
inline T* RenderAncestorIteratorAdapter<T>::first()
{
    return m_first;
}

// RenderAncestorConstIteratorAdapter

template <typename T>
inline RenderAncestorConstIteratorAdapter<T>::RenderAncestorConstIteratorAdapter(const T* first)
    : m_first(first)
{
}

template <typename T>
inline RenderAncestorConstIterator<T> RenderAncestorConstIteratorAdapter<T>::begin() const
{
    return RenderAncestorConstIterator<T>(m_first);
}

template <typename T>
inline RenderAncestorConstIterator<T> RenderAncestorConstIteratorAdapter<T>::end() const
{
    return RenderAncestorConstIterator<T>();
}

template <typename T>
inline const T* RenderAncestorConstIteratorAdapter<T>::first() const
{
    return m_first;
}

// Standalone functions

template <typename T>
inline RenderAncestorIteratorAdapter<T> ancestorsOfType(RenderObject& descendant)
{
    T* first = RenderTraversal::findAncestorOfType<T>(descendant);
    return RenderAncestorIteratorAdapter<T>(first);
}

template <typename T>
inline RenderAncestorConstIteratorAdapter<T> ancestorsOfType(const RenderObject& descendant)
{
    const T* first = RenderTraversal::findAncestorOfType<const T>(descendant);
    return RenderAncestorConstIteratorAdapter<T>(first);
}

template <typename T>
inline RenderAncestorIteratorAdapter<T> lineageOfType(RenderObject& first)
{
    if (isRendererOfType<const T>(first))
        return RenderAncestorIteratorAdapter<T>(static_cast<T*>(&first));
    return ancestorsOfType<T>(first);
}

template <typename T>
inline RenderAncestorConstIteratorAdapter<T> lineageOfType(const RenderObject& first)
{
    if (isRendererOfType<const T>(first))
        return RenderAncestorConstIteratorAdapter<T>(static_cast<const T*>(&first));
    return ancestorsOfType<T>(first);
}

}

#endif
