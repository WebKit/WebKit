/*
 * Copyright (C) 2008, 2012, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "CSSSelectorList.h"

#include "CSSParserSelector.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSSelectorList::CSSSelectorList(const CSSSelectorList& other)
{
    unsigned otherComponentCount = other.componentCount();
    ASSERT_WITH_SECURITY_IMPLICATION(otherComponentCount);

    m_selectorArray = makeUniqueArray<CSSSelector>(otherComponentCount);
    for (unsigned i = 0; i < otherComponentCount; ++i)
        new (NotNull, &m_selectorArray[i]) CSSSelector(other.m_selectorArray[i]);
}

CSSSelectorList::CSSSelectorList(Vector<std::unique_ptr<CSSParserSelector>>&& selectorVector)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!selectorVector.isEmpty());

    size_t flattenedSize = 0;
    for (size_t i = 0; i < selectorVector.size(); ++i) {
        for (CSSParserSelector* selector = selectorVector[i].get(); selector; selector = selector->tagHistory())
            ++flattenedSize;
    }
    ASSERT(flattenedSize);
    m_selectorArray = makeUniqueArray<CSSSelector>(flattenedSize);
    size_t arrayIndex = 0;
    for (size_t i = 0; i < selectorVector.size(); ++i) {
        CSSParserSelector* current = selectorVector[i].get();
        while (current) {
            {
                // Move item from the parser selector vector into m_selectorArray without invoking destructor (Ugh.)
                CSSSelector* currentSelector = current->releaseSelector().release();
                memcpy(static_cast<void*>(&m_selectorArray[arrayIndex]), static_cast<void*>(currentSelector), sizeof(CSSSelector));

                // Free the underlying memory without invoking the destructor.
                operator delete (currentSelector);
            }
            current = current->tagHistory();
            ASSERT(!m_selectorArray[arrayIndex].isLastInSelectorList());
            if (current)
                m_selectorArray[arrayIndex].setNotLastInTagHistory();
            ++arrayIndex;
        }
        ASSERT(m_selectorArray[arrayIndex - 1].isLastInTagHistory());
    }
    ASSERT(flattenedSize == arrayIndex);
    m_selectorArray[arrayIndex - 1].setLastInSelectorList();
}

unsigned CSSSelectorList::componentCount() const
{
    if (!m_selectorArray)
        return 0;
    CSSSelector* current = m_selectorArray.get();
    while (!current->isLastInSelectorList())
        ++current;
    return (current - m_selectorArray.get()) + 1;
}

unsigned CSSSelectorList::listSize() const
{
    if (!m_selectorArray)
        return 0;
    unsigned size = 1;
    CSSSelector* current = m_selectorArray.get();
    while (!current->isLastInSelectorList()) {
        if (current->isLastInTagHistory())
            ++size;
        ++current;
    }
    return size;
}

String CSSSelectorList::selectorsText() const
{
    StringBuilder result;
    buildSelectorsText(result);
    return result.toString();
}

void CSSSelectorList::buildSelectorsText(StringBuilder& stringBuilder) const
{
    const CSSSelector* firstSubselector = first();
    for (const CSSSelector* subSelector = firstSubselector; subSelector; subSelector = CSSSelectorList::next(subSelector)) {
        if (subSelector != firstSubselector)
            stringBuilder.appendLiteral(", ");
        stringBuilder.append(subSelector->selectorText());
    }
}

template <typename Functor>
static bool forEachTagSelector(Functor& functor, const CSSSelector* selector)
{
    ASSERT(selector);

    do {
        if (functor(selector))
            return true;
        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (forEachTagSelector(functor, subSelector))
                    return true;
            }
        }
    } while ((selector = selector->tagHistory()));

    return false;
}

template <typename Functor>
static bool forEachSelector(Functor& functor, const CSSSelectorList* selectorList)
{
    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(selector)) {
        if (forEachTagSelector(functor, selector))
            return true;
    }

    return false;
}

class SelectorNeedsNamespaceResolutionFunctor {
public:
    bool operator()(const CSSSelector* selector)
    {
        if (selector->match() == CSSSelector::Tag && !selector->tagQName().prefix().isEmpty() && selector->tagQName().prefix() != starAtom())
            return true;
        if (selector->isAttributeSelector() && !selector->attribute().prefix().isEmpty() && selector->attribute().prefix() != starAtom())
            return true;
        return false;
    }
};

bool CSSSelectorList::selectorsNeedNamespaceResolution()
{
    SelectorNeedsNamespaceResolutionFunctor functor;
    return forEachSelector(functor, this);
}

class SelectorHasInvalidSelectorFunctor {
public:
    bool operator()(const CSSSelector* selector)
    {
        return selector->isUnknownPseudoElement() || selector->isCustomPseudoElement();
    }
};

bool CSSSelectorList::hasInvalidSelector() const
{
    SelectorHasInvalidSelectorFunctor functor;
    return forEachSelector(functor, this);
}

} // namespace WebCore
