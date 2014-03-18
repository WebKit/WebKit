/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef ElementDescendantIterator_h
#define ElementDescendantIterator_h

#include "Element.h"
#include "ElementIteratorAssertions.h"
#include "ElementTraversal.h"
#include <wtf/Vector.h>

namespace WebCore {

class ElementDescendantIterator {
public:
    ElementDescendantIterator();
    explicit ElementDescendantIterator(Element* current);

    ElementDescendantIterator& operator++();

    Element& operator*();
    Element* operator->();

    bool operator==(const ElementDescendantIterator& other) const;
    bool operator!=(const ElementDescendantIterator& other) const;

private:
    Element* m_current;
    Vector<Element*, 16, UnsafeVectorOverflow> m_ancestorSiblingStack;

#if !ASSERT_DISABLED
    ElementIteratorAssertions m_assertions;
#endif
};

class ElementDescendantConstIterator {
public:
    ElementDescendantConstIterator();
    explicit ElementDescendantConstIterator(const Element*);

    ElementDescendantConstIterator& operator++();

    const Element& operator*() const;
    const Element* operator->() const;

    bool operator==(const ElementDescendantConstIterator& other) const;
    bool operator!=(const ElementDescendantConstIterator& other) const;

private:
    const Element* m_current;
    Vector<Element*, 16, UnsafeVectorOverflow> m_ancestorSiblingStack;

#if !ASSERT_DISABLED
    ElementIteratorAssertions m_assertions;
#endif
};

class ElementDescendantIteratorAdapter {
public:
    ElementDescendantIteratorAdapter(ContainerNode& root);
    ElementDescendantIterator begin();
    ElementDescendantIterator end();

private:
    ContainerNode& m_root;
};

class ElementDescendantConstIteratorAdapter {
public:
    ElementDescendantConstIteratorAdapter(const ContainerNode& root);
    ElementDescendantConstIterator begin() const;
    ElementDescendantConstIterator end() const;

private:
    const ContainerNode& m_root;
};

ElementDescendantIteratorAdapter elementDescendants(ContainerNode&);
ElementDescendantConstIteratorAdapter elementDescendants(const ContainerNode&);

// ElementDescendantIterator

inline ElementDescendantIterator::ElementDescendantIterator()
    : m_current(nullptr)
{
}

inline ElementDescendantIterator::ElementDescendantIterator(Element* current)
    : m_current(current)
{
    m_ancestorSiblingStack.uncheckedAppend(nullptr);
}

ALWAYS_INLINE ElementDescendantIterator& ElementDescendantIterator::operator++()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());

    Element* firstChild = ElementTraversal::firstChild(m_current);
    Element* nextSibling = ElementTraversal::nextSibling(m_current);

    if (firstChild) {
        if (nextSibling)
            m_ancestorSiblingStack.append(nextSibling);
        m_current = firstChild;
        return *this;
    }

    if (nextSibling) {
        m_current = nextSibling;
        return *this;
    }

    m_current = m_ancestorSiblingStack.takeLast();

#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif

    return *this;
}

inline Element& ElementDescendantIterator::operator*()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

inline Element* ElementDescendantIterator::operator->()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

inline bool ElementDescendantIterator::operator==(const ElementDescendantIterator& other) const
{
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current == other.m_current;
}

inline bool ElementDescendantIterator::operator!=(const ElementDescendantIterator& other) const
{
    return !(*this == other);
}

// ElementDescendantConstIterator

inline ElementDescendantConstIterator::ElementDescendantConstIterator()
    : m_current(nullptr)
{
}

inline ElementDescendantConstIterator::ElementDescendantConstIterator(const Element* current)
    : m_current(current)
{
    m_ancestorSiblingStack.uncheckedAppend(nullptr);
}

ALWAYS_INLINE ElementDescendantConstIterator& ElementDescendantConstIterator::operator++()
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());

    Element* firstChild = ElementTraversal::firstChild(m_current);
    Element* nextSibling = ElementTraversal::nextSibling(m_current);

    if (firstChild) {
        if (nextSibling)
            m_ancestorSiblingStack.append(nextSibling);
        m_current = firstChild;
        return *this;
    }

    if (nextSibling) {
        m_current = nextSibling;
        return *this;
    }

    m_current = m_ancestorSiblingStack.takeLast();

#if !ASSERT_DISABLED
    // Drop the assertion when the iterator reaches the end.
    if (!m_current)
        m_assertions.dropEventDispatchAssertion();
#endif

    return *this;
}

inline const Element& ElementDescendantConstIterator::operator*() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return *m_current;
}

inline const Element* ElementDescendantConstIterator::operator->() const
{
    ASSERT(m_current);
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current;
}

inline bool ElementDescendantConstIterator::operator==(const ElementDescendantConstIterator& other) const
{
    ASSERT(!m_assertions.domTreeHasMutated());
    return m_current == other.m_current;
}

inline bool ElementDescendantConstIterator::operator!=(const ElementDescendantConstIterator& other) const
{
    return !(*this == other);
}

// ElementDescendantIteratorAdapter

inline ElementDescendantIteratorAdapter::ElementDescendantIteratorAdapter(ContainerNode& root)
    : m_root(root)
{
}

inline ElementDescendantIterator ElementDescendantIteratorAdapter::begin()
{
    return ElementDescendantIterator(ElementTraversal::firstChild(&m_root));
}

inline ElementDescendantIterator ElementDescendantIteratorAdapter::end()
{
    return ElementDescendantIterator();
}

// ElementDescendantConstIteratorAdapter

inline ElementDescendantConstIteratorAdapter::ElementDescendantConstIteratorAdapter(const ContainerNode& root)
    : m_root(root)
{
}

inline ElementDescendantConstIterator ElementDescendantConstIteratorAdapter::begin() const
{
    return ElementDescendantConstIterator(ElementTraversal::firstChild(&m_root));
}

inline ElementDescendantConstIterator ElementDescendantConstIteratorAdapter::end() const
{
    return ElementDescendantConstIterator();
}

// Standalone functions

inline ElementDescendantIteratorAdapter elementDescendants(ContainerNode& root)
{
    return ElementDescendantIteratorAdapter(root);
}

inline ElementDescendantConstIteratorAdapter elementDescendants(const ContainerNode& root)
{
    return ElementDescendantConstIteratorAdapter(root);
}

}

#endif
