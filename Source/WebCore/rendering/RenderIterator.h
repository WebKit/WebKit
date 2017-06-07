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

#include "RenderElement.h"

namespace WebCore {

class RenderText;

template <typename T>
class RenderIterator {
public:
    RenderIterator(const RenderElement* root);
    RenderIterator(const RenderElement* root, T* current);

    T& operator*();
    T* operator->();

    bool operator==(const RenderIterator& other) const;
    bool operator!=(const RenderIterator& other) const;

    RenderIterator& traverseNext();
    RenderIterator& traverseNextSibling();
    RenderIterator& traversePreviousSibling();
    RenderIterator& traverseAncestor();

private:
    const RenderElement* m_root;
    T* m_current;
};

template <typename T>
class RenderConstIterator {
public:
    RenderConstIterator(const RenderElement* root);
    RenderConstIterator(const RenderElement* root, const T* current);

    const T& operator*() const;
    const T* operator->() const;

    bool operator==(const RenderConstIterator& other) const;
    bool operator!=(const RenderConstIterator& other) const;

    RenderConstIterator& traverseNext();
    RenderConstIterator& traverseNextSibling();
    RenderConstIterator& traversePreviousSibling();
    RenderConstIterator& traverseAncestor();

private:
    const RenderElement* m_root;
    const T* m_current;
};

// Similar to WTF::is<>() but without the static_assert() making sure the check is necessary.
template <typename T, typename U>
inline bool isRendererOfType(const U& renderer) { return TypeCastTraits<const T, const U>::isOfType(renderer); }

// Traversal helpers

namespace RenderObjectTraversal {

template <typename U>
inline RenderObject* firstChild(U& object)
{
    return object.firstChild();
}

inline RenderObject* firstChild(RenderObject& object)
{
    return object.firstChildSlow();
}

inline RenderObject* firstChild(RenderText&)
{
    return nullptr;
}

inline RenderObject* nextAncestorSibling(RenderObject& current, const RenderObject* stayWithin)
{
    for (auto* ancestor = current.parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == stayWithin)
            return nullptr;
        if (auto* sibling = ancestor->nextSibling())
            return sibling;
    }
    return nullptr;
}

template <typename U>
inline RenderObject* next(U& current, const RenderObject* stayWithin)
{
    if (auto* child = firstChild(current))
        return child;

    if (&current == stayWithin)
        return nullptr;

    if (auto* sibling = current.nextSibling())
        return sibling;

    return nextAncestorSibling(current, stayWithin);
}

inline RenderObject* nextSkippingChildren(RenderObject& current, const RenderObject* stayWithin)
{
    if (&current == stayWithin)
        return nullptr;

    if (auto* sibling = current.nextSibling())
        return sibling;

    return nextAncestorSibling(current, stayWithin);
}

}

namespace RenderTraversal {

template <typename T, typename U>
inline T* firstChild(U& current)
{
    RenderObject* object = RenderObjectTraversal::firstChild(current);
    while (object && !isRendererOfType<T>(*object))
        object = object->nextSibling();
    return static_cast<T*>(object);
}

template <typename T, typename U>
inline T* lastChild(U& current)
{
    RenderObject* object = current.lastChild();
    while (object && !isRendererOfType<T>(*object))
        object = object->previousSibling();
    return static_cast<T*>(object);
}

template <typename T, typename U>
inline T* nextSibling(U& current)
{
    RenderObject* object = current.nextSibling();
    while (object && !isRendererOfType<T>(*object))
        object = object->nextSibling();
    return static_cast<T*>(object);
}

template <typename T, typename U>
inline T* previousSibling(U& current)
{
    RenderObject* object = current.previousSibling();
    while (object && !isRendererOfType<T>(*object))
        object = object->previousSibling();
    return static_cast<T*>(object);
}

template <typename T>
inline T* findAncestorOfType(const RenderObject& current)
{
    for (auto* ancestor = current.parent(); ancestor; ancestor = ancestor->parent()) {
        if (isRendererOfType<T>(*ancestor))
            return static_cast<T*>(ancestor);
    }
    return nullptr;
}

template <typename T, typename U>
inline T* firstWithin(U& current)
{
    auto* descendant = RenderObjectTraversal::firstChild(current);
    while (descendant && !isRendererOfType<T>(*descendant))
        descendant = RenderObjectTraversal::next(*descendant, &current);
    return static_cast<T*>(descendant);
}

template <typename T, typename U>
inline T* next(U& current, const RenderObject* stayWithin)
{
    auto* descendant = RenderObjectTraversal::next(current, stayWithin);
    while (descendant && !isRendererOfType<T>(*descendant))
        descendant = RenderObjectTraversal::next(*descendant, stayWithin);
    return static_cast<T*>(descendant);
}

} // namespace WebCore::RenderTraversal

// RenderIterator

template <typename T>
inline RenderIterator<T>::RenderIterator(const RenderElement* root)
    : m_root(root)
    , m_current(nullptr)
{
}

template <typename T>
inline RenderIterator<T>::RenderIterator(const RenderElement* root, T* current)
    : m_root(root)
    , m_current(current)
{
}

template <typename T>
inline RenderIterator<T>& RenderIterator<T>::traverseNextSibling()
{
    ASSERT(m_current);
    m_current = RenderTraversal::nextSibling<T>(*m_current);
    return *this;
}

template <typename T>
inline RenderIterator<T>& RenderIterator<T>::traverseNext()
{
    ASSERT(m_current);
    m_current = RenderTraversal::next<T>(*m_current, m_root);
    return *this;
}

template <typename T>
inline RenderIterator<T>& RenderIterator<T>::traversePreviousSibling()
{
    ASSERT(m_current);
    m_current = RenderTraversal::previousSibling<T>(*m_current);
    return *this;
}

template <typename T>
inline RenderIterator<T>& RenderIterator<T>::traverseAncestor()
{
    ASSERT(m_current);
    ASSERT(m_current != m_root);
    m_current = RenderTraversal::findAncestorOfType<T>(*m_current);
    return *this;
}

template <typename T>
inline T& RenderIterator<T>::operator*()
{
    ASSERT(m_current);
    return *m_current;
}

template <typename T>
inline T* RenderIterator<T>::operator->()
{
    ASSERT(m_current);
    return m_current;
}

template <typename T>
inline bool RenderIterator<T>::operator==(const RenderIterator& other) const
{
    ASSERT(m_root == other.m_root);
    return m_current == other.m_current;
}

template <typename T>
inline bool RenderIterator<T>::operator!=(const RenderIterator& other) const
{
    return !(*this == other);
}

// RenderConstIterator

template <typename T>
inline RenderConstIterator<T>::RenderConstIterator(const RenderElement* root)
    : m_root(root)
    , m_current(nullptr)
{
}

template <typename T>
inline RenderConstIterator<T>::RenderConstIterator(const RenderElement* root, const T* current)
    : m_root(root)
    , m_current(current)
{
}

template <typename T>
inline RenderConstIterator<T>& RenderConstIterator<T>::traverseNextSibling()
{
    ASSERT(m_current);
    m_current = RenderTraversal::nextSibling<T>(*m_current);
    return *this;
}

template <typename T>
inline RenderConstIterator<T>& RenderConstIterator<T>::traverseNext()
{
    ASSERT(m_current);
    m_current = RenderTraversal::next<T>(*m_current, m_root);
    return *this;
}

template <typename T>
inline RenderConstIterator<T>& RenderConstIterator<T>::traversePreviousSibling()
{
    ASSERT(m_current);
    m_current = RenderTraversal::previousSibling<T>(m_current);
    return *this;
}


template <typename T>
inline RenderConstIterator<T>& RenderConstIterator<T>::traverseAncestor()
{
    ASSERT(m_current);
    ASSERT(m_current != m_root);
    m_current = RenderTraversal::findAncestorOfType<const T>(*m_current);
    return *this;
}

template <typename T>
inline const T& RenderConstIterator<T>::operator*() const
{
    ASSERT(m_current);
    return *m_current;
}

template <typename T>
inline const T* RenderConstIterator<T>::operator->() const
{
    ASSERT(m_current);
    return m_current;
}

template <typename T>
inline bool RenderConstIterator<T>::operator==(const RenderConstIterator& other) const
{
    ASSERT(m_root == other.m_root);
    return m_current == other.m_current;
}

template <typename T>
inline bool RenderConstIterator<T>::operator!=(const RenderConstIterator& other) const
{
    return !(*this == other);
}

} // namespace WebCore

#include "RenderAncestorIterator.h"
#include "RenderChildIterator.h"
