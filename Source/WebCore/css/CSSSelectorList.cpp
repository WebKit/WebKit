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

#include "CommonAtomStrings.h"
#include "MutableCSSSelector.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSSelectorList);

CSSSelectorList::CSSSelectorList(const CSSSelectorList& other)
{
    unsigned otherComponentCount = other.componentCount();
    if (!otherComponentCount)
        return;

    m_selectorArray = makeUniqueArray<CSSSelector>(otherComponentCount);
    for (unsigned i = 0; i < otherComponentCount; ++i)
        new (NotNull, &m_selectorArray[i]) CSSSelector(other.m_selectorArray[i]);
}

CSSSelectorList::CSSSelectorList(MutableCSSSelectorList&& selectorVector)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!selectorVector.isEmpty());

    size_t flattenedSize = 0;
    for (size_t i = 0; i < selectorVector.size(); ++i) {
        for (auto* selector = selectorVector[i].get(); selector; selector = selector->precedingInComplexSelector())
            ++flattenedSize;
    }
    ASSERT(flattenedSize);
    m_selectorArray = makeUniqueArray<CSSSelector>(flattenedSize);
    size_t arrayIndex = 0;
    for (size_t i = 0; i < selectorVector.size(); ++i) {
        auto* last = selectorVector[i].get();
        auto* current = last;
        while (current) {
            {
                // Move item from the parser selector vector into m_selectorArray without invoking destructor (Ugh.)
                auto* currentSelector = current->releaseSelector().release();
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
                memcpy(static_cast<void*>(&m_selectorArray[arrayIndex]), static_cast<void*>(currentSelector), sizeof(CSSSelector));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

                // Free the underlying memory without invoking the destructor.
                operator delete (currentSelector);
            }
            if (current != last)
                m_selectorArray[arrayIndex].m_isLastInComplexSelector = false;
            current = current->precedingInComplexSelector();
            ASSERT(!m_selectorArray[arrayIndex].isLastInSelectorList() || (flattenedSize == arrayIndex + 1));
            if (current)
                m_selectorArray[arrayIndex].m_isFirstInComplexSelector = false;
            ++arrayIndex;
        }
        ASSERT(m_selectorArray[arrayIndex - 1].isFirstInComplexSelector());
    }
    ASSERT(flattenedSize == arrayIndex);
    m_selectorArray[arrayIndex - 1].m_isLastInSelectorList = true;
}

CSSSelectorList CSSSelectorList::makeCopyingSimpleSelector(const CSSSelector& simpleSelector)
{
    auto selectorArray = makeUniqueArray<CSSSelector>(1);

    new (NotNull, &selectorArray[0]) CSSSelector(simpleSelector);
    selectorArray[0].m_isFirstInComplexSelector = true;
    selectorArray[0].m_isLastInComplexSelector = true;
    selectorArray[0].m_isLastInSelectorList = true;

    return CSSSelectorList { WTFMove(selectorArray) };
}

CSSSelectorList CSSSelectorList::makeCopyingComplexSelector(const CSSSelector& complexSelector)
{
    size_t length = 0;
    for (auto* selector = &complexSelector; selector; selector = selector->precedingInComplexSelector())
        ++length;

    auto selectorArray = makeUniqueArray<CSSSelector>(length);

    size_t i = 0;
    for (auto* selector = &complexSelector; selector; selector = selector->precedingInComplexSelector(), ++i)
        new (NotNull, &selectorArray[i]) CSSSelector(*selector);
    selectorArray[length - 1].m_isLastInSelectorList = true;

    return CSSSelectorList { WTFMove(selectorArray) };
}

CSSSelectorList CSSSelectorList::makeJoining(const CSSSelectorList& a, const CSSSelectorList& b)
{
    if (a.isEmpty())
        return b;
    if (b.isEmpty())
        return a;

    auto aComponentCount = a.componentCount();
    auto bComponentCount = b.componentCount();

    auto selectorArray = makeUniqueArray<CSSSelector>(aComponentCount + bComponentCount);

    for (size_t i = 0; i < aComponentCount; ++i)
        new (NotNull, &selectorArray[i]) CSSSelector(a.m_selectorArray[i]);
    for (size_t i = 0; i < bComponentCount; ++i)
        new (NotNull, &selectorArray[aComponentCount + i]) CSSSelector(b.m_selectorArray[i]);

    selectorArray[aComponentCount - 1].m_isLastInSelectorList = false;
    selectorArray[aComponentCount + bComponentCount - 1].m_isLastInSelectorList = true;

    return CSSSelectorList { WTFMove(selectorArray) };
}

CSSSelectorList CSSSelectorList::makeJoining(const Vector<const CSSSelectorList*>& lists)
{
    size_t totalComponentCount = 0;
    for (auto list : lists)
        totalComponentCount += list->componentCount();

    if (!totalComponentCount)
        return { };

    auto selectorArray = makeUniqueArray<CSSSelector>(totalComponentCount);

    size_t componentIndex = 0;
    for (auto list : lists) {
        auto count = list->componentCount();
        for (size_t i = 0; i < count; ++i)
            new (NotNull, &selectorArray[componentIndex++]) CSSSelector(list->m_selectorArray[i]);
        selectorArray[componentIndex - 1].m_isLastInSelectorList = false;
    }

    ASSERT(componentIndex == totalComponentCount);
    selectorArray[componentIndex - 1].m_isLastInSelectorList = true;

    return CSSSelectorList { WTFMove(selectorArray) };
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
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
        if (current->isFirstInComplexSelector())
            ++size;
        ++current;
    }
    return size;
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

String CSSSelectorList::selectorsText() const
{
    StringBuilder result;
    buildSelectorsText(result);
    return result.toString();
}

void CSSSelectorList::buildSelectorsText(StringBuilder& stringBuilder) const
{
    stringBuilder.append(interleave(*this, [](auto& subSelector) { return subSelector.selectorText(); }, ", "_s));
}

template <typename Functor>
static bool forEachTagSelector(Functor& functor, const CSSSelector* selector)
{
    ASSERT(selector);

    do {
        if (functor(selector))
            return true;
        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const auto& subSelector : *selectorList) {
                if (forEachTagSelector(functor, &subSelector))
                    return true;
            }
        }
    } while ((selector = selector->precedingInComplexSelector()));

    return false;
}

template <typename Functor>
static bool forEachSelector(Functor& functor, const CSSSelectorList& selectorList)
{
    for (const auto& selector : selectorList) {
        if (forEachTagSelector(functor, &selector))
            return true;
    }

    return false;
}

bool CSSSelectorList::hasExplicitNestingParent() const
{
    auto functor = [](auto* selector) {
        return selector->hasExplicitNestingParent();
    };

    return forEachSelector(functor, *this);
}

bool CSSSelectorList::hasOnlyNestingSelector() const
{
    if (componentCount() != 1)
        return false;

    auto singleSelector = first();

    if (!singleSelector)
        return false;
    
    // Selector should be a single selector
    if (singleSelector->precedingInComplexSelector())
        return false;

    return singleSelector->match() == CSSSelector::Match::NestingParent;
}

} // namespace WebCore
