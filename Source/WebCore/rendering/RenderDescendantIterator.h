/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Igalia S.L.
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

template <typename T>
class RenderDescendantPostOrderIterator : public RenderPostOrderIterator<T> {
public:
    RenderDescendantPostOrderIterator(const RenderElement& root);
    RenderDescendantPostOrderIterator(const RenderElement& root, T* current);
    RenderDescendantPostOrderIterator& operator++();
};

template <typename T>
class RenderDescendantPostOrderConstIterator : public RenderPostOrderConstIterator<T> {
public:
    RenderDescendantPostOrderConstIterator(const RenderElement& root);
    RenderDescendantPostOrderConstIterator(const RenderElement& root, const T* current);
    RenderDescendantPostOrderConstIterator& operator++();
};

template <typename T>
class RenderDescendantPostOrderIteratorAdapter {
public:
    RenderDescendantPostOrderIteratorAdapter(RenderElement& root);
    RenderDescendantPostOrderIterator<T> begin();
    RenderDescendantPostOrderIterator<T> end();
    RenderDescendantPostOrderIterator<T> at(T&);

private:
    RenderElement& m_root;
};

template <typename T>
class RenderDescendantPostOrderConstIteratorAdapter {
public:
    RenderDescendantPostOrderConstIteratorAdapter(const RenderElement& root);
    RenderDescendantPostOrderConstIterator<T> begin() const;
    RenderDescendantPostOrderConstIterator<T> end() const;
    RenderDescendantPostOrderConstIterator<T> at(const T&) const;

private:
    const RenderElement& m_root;
};

template <typename T> RenderDescendantIteratorAdapter<T> descendantsOfType(RenderElement&);
template <typename T> RenderDescendantConstIteratorAdapter<T> descendantsOfType(const RenderElement&);

template <typename T> RenderDescendantPostOrderIteratorAdapter<T> descendantsOfTypePostOrder(RenderElement&);
template <typename T> RenderDescendantPostOrderConstIteratorAdapter<T> descendantsOfTypePostOrder(const RenderElement&);

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

// RenderDescendantPostOrderIterator

template <typename T>
inline RenderDescendantPostOrderIterator<T>::RenderDescendantPostOrderIterator(const RenderElement& root)
    : RenderPostOrderIterator<T>(&root)
{
}

template <typename T>
inline RenderDescendantPostOrderIterator<T>::RenderDescendantPostOrderIterator(const RenderElement& root, T* current)
    : RenderPostOrderIterator<T>(&root, current)
{
}

template <typename T>
inline RenderDescendantPostOrderIterator<T>& RenderDescendantPostOrderIterator<T>::operator++()
{
    return static_cast<RenderDescendantPostOrderIterator<T>&>(RenderPostOrderIterator<T>::traverseNext());
}

// RenderDescendantPostOrderConstIterator

template <typename T>
inline RenderDescendantPostOrderConstIterator<T>::RenderDescendantPostOrderConstIterator(const RenderElement& root)
    : RenderPostOrderConstIterator<T>(&root)
{
}

template <typename T>
inline RenderDescendantPostOrderConstIterator<T>::RenderDescendantPostOrderConstIterator(const RenderElement& root, const T* current)
    : RenderPostOrderConstIterator<T>(&root, current)
{
}

template <typename T>
inline RenderDescendantPostOrderConstIterator<T>& RenderDescendantPostOrderConstIterator<T>::operator++()
{
    return static_cast<RenderDescendantPostOrderConstIterator<T>&>(RenderPostOrderConstIterator<T>::traverseNext());
}

// RenderDescendantPostOrderIteratorAdapter

template <typename T>
inline RenderDescendantPostOrderIteratorAdapter<T>::RenderDescendantPostOrderIteratorAdapter(RenderElement& root)
    : m_root(root)
{
}

template <typename T>
inline RenderDescendantPostOrderIterator<T> RenderDescendantPostOrderIteratorAdapter<T>::begin()
{
    return RenderDescendantPostOrderIterator<T>(m_root, RenderPostOrderTraversal::firstWithin<T>(m_root));
}

template <typename T>
inline RenderDescendantPostOrderIterator<T> RenderDescendantPostOrderIteratorAdapter<T>::end()
{
    return RenderDescendantPostOrderIterator<T>(m_root);
}

template <typename T>
inline RenderDescendantPostOrderIterator<T> RenderDescendantPostOrderIteratorAdapter<T>::at(T& current)
{
    return RenderDescendantPostOrderIterator<T>(m_root, &current);
}

// RenderDescendantPostOrderConstIteratorAdapter

template <typename T>
inline RenderDescendantPostOrderConstIteratorAdapter<T>::RenderDescendantPostOrderConstIteratorAdapter(const RenderElement& root)
    : m_root(root)
{
}

template <typename T>
inline RenderDescendantPostOrderConstIterator<T> RenderDescendantPostOrderConstIteratorAdapter<T>::begin() const
{
    return RenderDescendantPostOrderConstIterator<T>(m_root, RenderPostOrderTraversal::firstWithin<T>(m_root));
}

template <typename T>
inline RenderDescendantPostOrderConstIterator<T> RenderDescendantPostOrderConstIteratorAdapter<T>::end() const
{
    return RenderDescendantPostOrderConstIterator<T>(m_root);
}

template <typename T>
inline RenderDescendantPostOrderConstIterator<T> RenderDescendantPostOrderConstIteratorAdapter<T>::at(const T& current) const
{
    return RenderDescendantPostOrderConstIterator<T>(m_root, &current);
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

template <typename T>
inline RenderDescendantPostOrderIteratorAdapter<T> descendantsOfTypePostOrder(RenderElement& root)
{
    return RenderDescendantPostOrderIteratorAdapter<T>(root);
}

template <typename T>
inline RenderDescendantPostOrderConstIteratorAdapter<T> descendantsOfTypePostOrder(const RenderElement& root)
{
    return RenderDescendantPostOrderConstIteratorAdapter<T>(root);
}

} // namespace WebCore
