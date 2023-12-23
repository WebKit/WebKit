/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2014 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSParserSelector.h"

#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "SelectorPseudoTypeMap.h"

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

namespace WebCore {

std::unique_ptr<CSSParserSelector> CSSParserSelector::parsePagePseudoSelector(StringView pseudoTypeString)
{
    CSSSelector::PagePseudoClass pseudoType;
    if (equalLettersIgnoringASCIICase(pseudoTypeString, "first"_s))
        pseudoType = CSSSelector::PagePseudoClass::First;
    else if (equalLettersIgnoringASCIICase(pseudoTypeString, "left"_s))
        pseudoType = CSSSelector::PagePseudoClass::Left;
    else if (equalLettersIgnoringASCIICase(pseudoTypeString, "right"_s))
        pseudoType = CSSSelector::PagePseudoClass::Right;
    else
        return nullptr;

    auto selector = makeUnique<CSSParserSelector>();
    selector->m_selector->setMatch(CSSSelector::Match::PagePseudoClass);
    selector->m_selector->setPagePseudoClass(pseudoType);
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::parsePseudoElementSelector(StringView pseudoTypeString, const CSSSelectorParserContext& context)
{
    auto pseudoType = CSSSelector::parsePseudoElement(pseudoTypeString, context);
    if (!pseudoType)
        return nullptr;

    auto selector = makeUnique<CSSParserSelector>();
    selector->m_selector->setMatch(CSSSelector::Match::PseudoElement);
    selector->m_selector->setPseudoElement(*pseudoType);
    AtomString name;
    if (*pseudoType != CSSSelector::PseudoElement::WebKitCustomLegacyPrefixed)
        name = pseudoTypeString.convertToASCIILowercaseAtom();
    else {
        if (equalLettersIgnoringASCIICase(pseudoTypeString, "-webkit-input-placeholder"_s))
            name = "placeholder"_s;
        else if (equalLettersIgnoringASCIICase(pseudoTypeString, "-webkit-file-upload-button"_s))
            name = "file-selector-button"_s;
        else {
            ASSERT_NOT_REACHED();
            name = pseudoTypeString.convertToASCIILowercaseAtom();
        }
    }
    selector->m_selector->setValue(name);
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::parsePseudoClassSelector(StringView pseudoTypeString)
{
    auto pseudoType = parsePseudoClassAndCompatibilityElementString(pseudoTypeString);
    if (pseudoType.pseudoClass) {
        auto selector = makeUnique<CSSParserSelector>();
        selector->m_selector->setMatch(CSSSelector::Match::PseudoClass);
        selector->m_selector->setPseudoClass(*pseudoType.pseudoClass);
        return selector;
    }
    if (pseudoType.compatibilityPseudoElement) {
        auto selector = makeUnique<CSSParserSelector>();
        selector->m_selector->setMatch(CSSSelector::Match::PseudoElement);
        selector->m_selector->setPseudoElement(*pseudoType.compatibilityPseudoElement);
        selector->m_selector->setValue(pseudoTypeString.convertToASCIILowercaseAtom());
        return selector;
    }
    return nullptr;
}

CSSParserSelector::CSSParserSelector()
    : m_selector(makeUnique<CSSSelector>())
{
}

CSSParserSelector::CSSParserSelector(const QualifiedName& tagQName)
    : m_selector(makeUnique<CSSSelector>(tagQName))
{
}

CSSParserSelector::CSSParserSelector(const CSSSelector& selector)
    : m_selector(makeUnique<CSSSelector>(selector))
{
    if (auto next = selector.tagHistory())
        m_tagHistory = makeUnique<CSSParserSelector>(*next);
}


CSSParserSelector::~CSSParserSelector()
{
    if (!m_tagHistory)
        return;
    Vector<std::unique_ptr<CSSParserSelector>, 16> toDelete;
    std::unique_ptr<CSSParserSelector> selector = WTFMove(m_tagHistory);
    while (true) {
        std::unique_ptr<CSSParserSelector> next = WTFMove(selector->m_tagHistory);
        toDelete.append(WTFMove(selector));
        if (!next)
            break;
        selector = WTFMove(next);
    }
}

void CSSParserSelector::adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>&& selectorVector)
{
    m_selector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(selectorVector)));
}

void CSSParserSelector::setArgumentList(FixedVector<PossiblyQuotedIdentifier> list)
{
    ASSERT(!list.isEmpty());
    m_selector->setArgumentList(WTFMove(list));
}

void CSSParserSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    m_selector->setSelectorList(WTFMove(selectorList));
}

const CSSParserSelector* CSSParserSelector::leftmostSimpleSelector() const
{
    auto selector = this;
    while (auto next = selector->tagHistory())
        selector = next;
    return selector;
}

CSSParserSelector* CSSParserSelector::leftmostSimpleSelector()
{
    auto selector = this;
    while (auto next = selector->tagHistory())
        selector = next;
    return selector;
}

bool CSSParserSelector::hasExplicitNestingParent() const
{
    auto selector = this;
    while (selector) {
        if (selector->selector()->hasExplicitNestingParent())
            return true;

        selector = selector->tagHistory();
    }
    return false;
}

bool CSSParserSelector::hasExplicitPseudoClassScope() const
{
    auto selector = this;
    while (selector) {
        if (selector->selector()->hasExplicitPseudoClassScope())
            return true;

        selector = selector->tagHistory();
    }
    return false;
}

static bool selectorListMatchesPseudoElement(const CSSSelectorList* selectorList)
{
    if (!selectorList)
        return false;

    for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
        for (const CSSSelector* selector = subSelector; selector; selector = selector->tagHistory()) {
            if (selector->matchesPseudoElement())
                return true;
            if (const CSSSelectorList* subselectorList = selector->selectorList()) {
                if (selectorListMatchesPseudoElement(subselectorList))
                    return true;
            }
        }
    }
    return false;
}

bool CSSParserSelector::matchesPseudoElement() const
{
    return m_selector->matchesPseudoElement() || selectorListMatchesPseudoElement(m_selector->selectorList());
}

void CSSParserSelector::insertTagHistory(CSSSelector::Relation before, std::unique_ptr<CSSParserSelector> selector, CSSSelector::Relation after)
{
    if (m_tagHistory)
        selector->setTagHistory(WTFMove(m_tagHistory));
    setRelation(before);
    selector->setRelation(after);
    m_tagHistory = WTFMove(selector);
}

void CSSParserSelector::appendTagHistory(CSSSelector::Relation relation, std::unique_ptr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();

    end->setRelation(relation);
    end->setTagHistory(WTFMove(selector));
}

void CSSParserSelector::appendTagHistoryAsRelative(std::unique_ptr<CSSParserSelector> selector)
{
    auto lastSelector = leftmostSimpleSelector()->selector();
    ASSERT(lastSelector);

    // Relation is Descendant by default.
    auto relation = lastSelector->relation();
    if (relation == CSSSelector::Relation::Subselector)
        relation = CSSSelector::Relation::DescendantSpace;

    appendTagHistory(relation, WTFMove(selector));
}

void CSSParserSelector::appendTagHistory(CSSParserSelectorCombinator relation, std::unique_ptr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();

    CSSSelector::Relation selectorRelation;
    switch (relation) {
    case CSSParserSelectorCombinator::Child:
        selectorRelation = CSSSelector::Relation::Child;
        break;
    case CSSParserSelectorCombinator::DescendantSpace:
        selectorRelation = CSSSelector::Relation::DescendantSpace;
        break;
    case CSSParserSelectorCombinator::DirectAdjacent:
        selectorRelation = CSSSelector::Relation::DirectAdjacent;
        break;
    case CSSParserSelectorCombinator::IndirectAdjacent:
        selectorRelation = CSSSelector::Relation::IndirectAdjacent;
        break;
    }
    end->setRelation(selectorRelation);
    end->setTagHistory(WTFMove(selector));
}

void CSSParserSelector::prependTagSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
{
    auto second = makeUnique<CSSParserSelector>();
    second->m_selector = WTFMove(m_selector);
    second->m_tagHistory = WTFMove(m_tagHistory);
    m_tagHistory = WTFMove(second);

    m_selector = makeUnique<CSSSelector>(tagQName, tagIsForNamespaceRule);
    m_selector->setRelation(CSSSelector::Relation::Subselector);
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::releaseTagHistory()
{
    setRelation(CSSSelector::Relation::Subselector);
    return WTFMove(m_tagHistory);
}

// FIXME-NEWPARSER: Add support for :host-context
bool CSSParserSelector::isHostPseudoSelector() const
{
    return match() == CSSSelector::Match::PseudoClass && pseudoClass() == CSSSelector::PseudoClass::Host;
}

bool CSSParserSelector::startsWithExplicitCombinator() const
{
    auto relation = leftmostSimpleSelector()->selector()->relation();
    return relation != CSSSelector::Relation::Subselector && relation != CSSSelector::Relation::DescendantSpace;
}

}

