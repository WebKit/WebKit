/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#include "AccessibilityObject.h"
#include "ComposedTreeIterator.h"
#include <iterator>

namespace WebCore {

// When using the previousSibling and nextSibling methods, we can alternate between walking the DOM and the
// the render tree. There are complications with this, especially introduced by display:contents, which
// removes the renderer for the given object, and moves its render tree children up one level higher than they
// otherwise would've been on. This iterator abstracts over this complexity, ensuring each object is actually a
// sibling of the last, by walking the composed tree for the children of a display:contents element.
class AccessibilityObject::iterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using element_type = AccessibilityObject;
    using pointer = AccessibilityObject*;
    using reference = AccessibilityObject&;

    iterator() = default;
    iterator(const AccessibilityObject*, const AccessibilityObject* displayContentsParent);
    iterator(const AccessibilityObject* object)
        : iterator(object, object ? protect(object->displayContentsParent()) : nullptr)
    { }
    iterator(const AccessibilityObject* object, const AccessibilityObject& parent)
        : iterator(object, parent.hasDisplayContents() ? &parent : nullptr)
    { }

    explicit operator bool() const { return m_current; }
    AccessibilityObject& operator*() const { return const_cast<AccessibilityObject&>(*m_current); }
    AccessibilityObject* operator->() const { return const_cast<AccessibilityObject*>(m_current.get()); }
    AccessibilityObject* ptr() const { return const_cast<AccessibilityObject*>(m_current.get()); }
    bool operator==(const iterator& other) const { return other.m_current == m_current; }

    // Prefix increment operator (++iterator).
    iterator& operator++()
    {
        m_current = m_displayContentsParent ? nextSiblingInComposedTree() : RefPtr { protect(m_current)->nextSibling() };
        ensureContentsParentValidity();
        return *this;
    }

    // Postfix increment operator (iterator++).
    iterator operator++(int)
    {
        auto copy = *this;
        ++(*this);
        return copy;
    }

    // --iterator
    iterator& operator--()
    {
        m_current = m_displayContentsParent ? previousSiblingInComposedTree() : RefPtr { protect(m_current)->previousSibling() };
        ensureContentsParentValidity();
        return *this;
    }

    // iterator--
    iterator operator--(int)
    {
        auto copy = *this;
        --(*this);
        return copy;
    }
private:
    RefPtr<AccessibilityObject> nextSiblingInComposedTree();
    RefPtr<AccessibilityObject> previousSiblingInComposedTree();

    void ensureContentsParentValidity()
    {
        if (!m_current || !m_displayContentsParent)
            return;
        // A child that another element owns has that element as its parent. Stop rather than walking into it.
        if (protect(m_current)->parentObject() != m_displayContentsParent.get())
            m_current = nullptr;
    }

    using ComposedTreeChildIterator = ComposedTreeChildAdapter</* InlineContextCapacity */ 0>::Iterator;
    struct ComposedTreeChildren {
        ComposedTreeChildIterator current;
        ComposedTreeChildIterator end;
    };

    RefPtr<const AccessibilityObject> m_current;
    // If the original object had a display:contents parent, it is stored here. This is nullptr otherwise.
    const RefPtr<const AccessibilityObject> m_displayContentsParent { nullptr };
    // Set when m_current is a composed tree child of m_displayContentsParent.
    std::optional<ComposedTreeChildren> m_composedTreeChildren;
}; // class AccessibilityObject::iterator

class AXChildIterator {
public:
    AXChildIterator(const AccessibilityObject& parent)
        : m_parent(parent)
    { }

    AccessibilityObject::iterator begin()
    {
        return AccessibilityObject::iterator { protect(m_parent->firstChild()), m_parent.get() };
    }
    AccessibilityObject::iterator end()
    {
        return AccessibilityObject::iterator { };
    }
private:
    const Ref<const AccessibilityObject> m_parent;
}; // class AXChildIterator

} // namespace WebCore
