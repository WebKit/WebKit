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

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

// Biblically, Noah's Ark only had room for two of each animal, but in the
// Book of Hixie (aka http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#list-of-active-formatting-elements),
// Noah's Ark of Formatting Elements can fit three of each element.
static const size_t kNoahsArkCapacity = 3;

static inline size_t attributeCount(Element* element)
{
    return element->attributeMap() ? element->attributeMap()->length() : 0;
}

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
    size_t index = m_entries.reverseFind(element);
    if (index != notFound) {
        // This is somewhat of a hack, and is why this method can't be const.
        return &m_entries[index];
    }
    return 0;
}

HTMLFormattingElementList::Bookmark HTMLFormattingElementList::bookmarkFor(Element* element)
{
    size_t index = m_entries.reverseFind(element);
    ASSERT(index != notFound);
    return Bookmark(&at(index));
}

void HTMLFormattingElementList::swapTo(Element* oldElement, Element* newElement, const Bookmark& bookmark)
{
    ASSERT(contains(oldElement));
    ASSERT(!contains(newElement));
    if (!bookmark.hasBeenMoved()) {
        ASSERT(bookmark.mark()->element() == oldElement);
        bookmark.mark()->replaceElement(newElement);
        return;
    }
    size_t index = bookmark.mark() - first();
    ASSERT(index < size());
    m_entries.insert(index + 1, newElement);
    remove(oldElement);
}

void HTMLFormattingElementList::append(Element* element)
{
    ensureNoahsArkCondition(element);
    m_entries.append(element);
}

void HTMLFormattingElementList::remove(Element* element)
{
    size_t index = m_entries.reverseFind(element);
    if (index != notFound)
        m_entries.remove(index);
}

void HTMLFormattingElementList::appendMarker()
{
    m_entries.append(Entry::MarkerEntry);
}

void HTMLFormattingElementList::clearToLastMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#clear-the-list-of-active-formatting-elements-up-to-the-last-marker
    while (m_entries.size()) {
        bool shouldStop = m_entries.last().isMarker();
        m_entries.removeLast();
        if (shouldStop)
            break;
    }
}

void HTMLFormattingElementList::tryToEnsureNoahsArkConditionQuickly(Element* newElement, Vector<Element*>& remainingCandidates)
{
    ASSERT(remainingCandidates.isEmpty());

    if (!m_entries.size())
        return;

    // Use a vector with inline capacity to avoid a malloc in the common case
    // of a quickly ensuring the condition.
    Vector<Element*, 10> candidates;

    size_t newElementAttributeCount = attributeCount(newElement);

    for (size_t i = m_entries.size(); i; ) {
        --i;
        Entry& entry = m_entries[i];
        if (entry.isMarker())
            break;

        // Quickly reject obviously non-matching candidates.
        Element* candidate = entry.element();
        if (newElement->tagQName() != candidate->tagQName())
            continue;
        if (attributeCount(candidate) != newElementAttributeCount)
            continue;

        candidates.append(candidate);
    }

    if (candidates.size() < kNoahsArkCapacity)
        return; // There's room for the new element in the ark. There's no need to copy out the remainingCandidates.

    remainingCandidates.append(candidates);
}

void HTMLFormattingElementList::ensureNoahsArkCondition(Element* newElement)
{
    Vector<Element*> candidates;
    tryToEnsureNoahsArkConditionQuickly(newElement, candidates);
    if (candidates.isEmpty())
        return;

    // We pre-allocate and re-use this second vector to save one malloc per
    // attribute that we verify.
    Vector<Element*> remainingCandidates;
    remainingCandidates.reserveInitialCapacity(candidates.size());

    NamedNodeMap* attributeMap = newElement->attributeMap();
    size_t newElementAttributeCount = attributeCount(newElement);

    for (size_t i = 0; i < newElementAttributeCount; ++i) {
        QualifiedName attributeName = attributeMap->attributeItem(i)->name();
        AtomicString attributeValue = newElement->fastGetAttribute(attributeName);

        for (size_t j = 0; j < candidates.size(); ++j) {
            Element* candidate = candidates[j];

            // These properties should already have been checked by tryToEnsureNoahsArkConditionQuickly.
            ASSERT(newElement->attributeMap()->length() == candidate->attributeMap()->length());
            ASSERT(newElement->tagQName() == candidate->tagQName());

            // FIXME: Technically we shouldn't read this information back from
            // the DOM. Instead, the parser should keep a copy of the information.
            // This isn't really much of a problem for our implementation because
            // we run the parser on the main thread, but the spec is written so
            // that implementations can run off the main thread. If JavaScript
            // changes the attributes values, we could get a slightly wrong
            // output here.
            if (candidate->fastGetAttribute(attributeName) == attributeValue)
                remainingCandidates.append(candidate);
        }

        if (remainingCandidates.size() < kNoahsArkCapacity)
            return;

        candidates.swap(remainingCandidates);
        remainingCandidates.shrink(0);
    }

    // Inductively, we shouldn't spin this loop very many times. It's possible,
    // however, that we wil spin the loop more than once because of how the
    // formatting element list gets permuted.
    for (size_t i = kNoahsArkCapacity - 1; i < candidates.size(); ++i)
        remove(candidates[i]);
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
