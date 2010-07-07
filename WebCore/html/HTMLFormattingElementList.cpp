/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLFormattingElementList.h"

#include "Element.h"
#include "NotImplemented.h"

namespace WebCore {

HTMLFormattingElementList::HTMLFormattingElementList()
{
}

HTMLFormattingElementList::~HTMLFormattingElementList()
{
}

Element* HTMLFormattingElementList::closestElementInScopeWithName(const AtomicString& targetName)
{
    for (unsigned i = 1; i <= m_entries.size(); ++i) {
        const Entry& entry = m_entries[m_entries.size() - i];
        if (entry.isMarker())
            return 0;
        if (entry.element()->hasLocalName(targetName))
            return entry.element();
    }
    return 0;
}

bool HTMLFormattingElementList::contains(Element* element)
{
    return !!find(element);
}

HTMLFormattingElementList::Entry* HTMLFormattingElementList::find(Element* element)
{
    size_t index = findIndex(element);
    if (index != notFound) {
        // This is somewhat of a hack, and is why this method can't be const.
        return &m_entries[index];
    }
    return 0;
}

HTMLFormattingElementList::Bookmark HTMLFormattingElementList::bookmarkFor(Element* element)
{
    size_t index = findIndex(element);
    ASSERT(index != notFound);
    Element* elementBefore = (index > 1) ? m_entries[index - 1].element() : 0;
    Element* elementAfter = (index < m_entries.size() - 1) ? m_entries[index + 1].element() : 0;
    return Bookmark(elementBefore, elementAfter);
}

void HTMLFormattingElementList::insertAt(Element* element, const Bookmark& bookmark)
{
    size_t beforeIndex = notFound;
    if (bookmark.elementBefore()) {
        beforeIndex = findIndex(bookmark.elementBefore());
        ASSERT(beforeIndex != notFound);
    }
    size_t afterIndex = notFound;
    if (bookmark.elementAfter()) {
        afterIndex = findIndex(bookmark.elementAfter());
        ASSERT(afterIndex != notFound);
    }

    if (!bookmark.elementBefore()) {
        if (bookmark.elementAfter())
            ASSERT(!afterIndex);
        m_entries.prepend(element);
    } else {
        if (bookmark.elementAfter()) {
            // Bookmarks are not general purpose.  They're only for the Adoption
            // Agency. Assume the bookmarked element was already removed.
            ASSERT(beforeIndex + 1 == afterIndex);
        }
        m_entries.insert(beforeIndex + 1, element);
    }
}

void HTMLFormattingElementList::append(Element* element)
{
    m_entries.append(element);
}

void HTMLFormattingElementList::remove(Element* element)
{
    size_t index = findIndex(element);
    if (index != notFound)
        m_entries.remove(index);
}

void HTMLFormattingElementList::appendMarker()
{
    m_entries.append(Entry::MarkerEntry);
}

void HTMLFormattingElementList::clearToLastMarker()
{
    while (m_entries.size() && !m_entries.last().isMarker())
        m_entries.removeLast();
}

#ifndef NDEBUG

void HTMLFormattingElementList::show()
{
    for (unsigned i = 1; i <= m_entries.size(); ++i) {
        const Entry& entry = m_entries[m_entries.size() - i];
        if (entry.isMarker())
            fprintf(stderr, "marker\n");
        else
            entry.element()->showNode();
    }
}

#endif

}
