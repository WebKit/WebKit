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

CSSParserSelector* CSSParserSelector::parsePagePseudoSelector(const AtomicString& pseudoTypeString)
{
    CSSSelector::PagePseudoClassType pseudoType;
    if (equalLettersIgnoringASCIICase(pseudoTypeString, "first"))
        pseudoType = CSSSelector::PagePseudoClassFirst;
    else if (equalLettersIgnoringASCIICase(pseudoTypeString, "left"))
        pseudoType = CSSSelector::PagePseudoClassLeft;
    else if (equalLettersIgnoringASCIICase(pseudoTypeString, "right"))
        pseudoType = CSSSelector::PagePseudoClassRight;
    else
        return nullptr;
    
    auto selector = std::make_unique<CSSParserSelector>();
    selector->m_selector->setMatch(CSSSelector::PagePseudoClass);
    selector->m_selector->setPagePseudoType(pseudoType);
    return selector.release();
}

CSSParserSelector* CSSParserSelector::parsePseudoElementSelectorFromStringView(StringView& pseudoTypeString)
{
    AtomicString name = pseudoTypeString.toAtomicString();
    
    CSSSelector::PseudoElementType pseudoType = CSSSelector::parsePseudoElementType(name);
    if (pseudoType == CSSSelector::PseudoElementUnknown) {
        // FIXME-NEWPARSER: We can't add "slotted" to the map without breaking the old
        // parser, so this hack ensures the new parser still recognizes it. When the new
        // parser turns on, we can add "slotted" to the map and remove this code.
        if (pseudoTypeString.startsWithIgnoringASCIICase("slotted"))
            pseudoType = CSSSelector::PseudoElementSlotted;
        else
            return nullptr;
    }

    auto selector = std::make_unique<CSSParserSelector>();
    selector->m_selector->setMatch(CSSSelector::PseudoElement);
    selector->m_selector->setPseudoElementType(pseudoType);
    if (pseudoType == CSSSelector::PseudoElementWebKitCustomLegacyPrefixed) {
        ASSERT_WITH_MESSAGE(name == "-webkit-input-placeholder", "-webkit-input-placeholder is the only LegacyPrefix pseudo type.");
        if (name == "-webkit-input-placeholder")
            name = AtomicString("placeholder", AtomicString::ConstructFromLiteral);
    }
    selector->m_selector->setValue(name);
    return selector.release();
}

CSSParserSelector* CSSParserSelector::parsePseudoClassSelectorFromStringView(StringView& pseudoTypeString)
{
    PseudoClassOrCompatibilityPseudoElement pseudoType = parsePseudoClassAndCompatibilityElementString(pseudoTypeString);
    if (pseudoType.pseudoClass != CSSSelector::PseudoClassUnknown) {
        auto selector = std::make_unique<CSSParserSelector>();
        selector->m_selector->setMatch(CSSSelector::PseudoClass);
        selector->m_selector->setPseudoClassType(pseudoType.pseudoClass);
        return selector.release();
    }
    if (pseudoType.compatibilityPseudoElement != CSSSelector::PseudoElementUnknown) {
        auto selector = std::make_unique<CSSParserSelector>();
        selector->m_selector->setMatch(CSSSelector::PseudoElement);
        selector->m_selector->setPseudoElementType(pseudoType.compatibilityPseudoElement);
        AtomicString name = pseudoTypeString.toAtomicString();
        selector->m_selector->setValue(name);
        return selector.release();
    }
    return nullptr;
}

CSSParserSelector::CSSParserSelector()
    : m_selector(std::make_unique<CSSSelector>())
{
}

CSSParserSelector::CSSParserSelector(const QualifiedName& tagQName)
    : m_selector(std::make_unique<CSSSelector>(tagQName))
{
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
    m_selector->setSelectorList(std::make_unique<CSSSelectorList>(WTFMove(selectorVector)));
}

void CSSParserSelector::setLangArgumentList(std::unique_ptr<Vector<AtomicString>> argumentList)
{
    ASSERT_WITH_MESSAGE(!argumentList->isEmpty(), "No CSS Selector takes an empty argument list.");
    m_selector->setLangArgumentList(WTFMove(argumentList));
}

void CSSParserSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    m_selector->setSelectorList(WTFMove(selectorList));
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

void CSSParserSelector::insertTagHistory(CSSSelector::RelationType before, std::unique_ptr<CSSParserSelector> selector, CSSSelector::RelationType after)
{
    if (m_tagHistory)
        selector->setTagHistory(WTFMove(m_tagHistory));
    setRelation(before);
    selector->setRelation(after);
    m_tagHistory = WTFMove(selector);
}

void CSSParserSelector::appendTagHistory(CSSSelector::RelationType relation, std::unique_ptr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();

    end->setRelation(relation);
    end->setTagHistory(WTFMove(selector));
}

void CSSParserSelector::appendTagHistory(CSSParserSelectorCombinator relation, std::unique_ptr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();

    CSSSelector::RelationType selectorRelation;
    switch (relation) {
    case CSSParserSelectorCombinator::Child:
        selectorRelation = CSSSelector::Child;
        break;
    case CSSParserSelectorCombinator::DescendantSpace:
        selectorRelation = CSSSelector::DescendantSpace;
        break;
    case CSSParserSelectorCombinator::DirectAdjacent:
        selectorRelation = CSSSelector::DirectAdjacent;
        break;
    case CSSParserSelectorCombinator::IndirectAdjacent:
        selectorRelation = CSSSelector::IndirectAdjacent;
        break;
    }
    end->setRelation(selectorRelation);
    end->setTagHistory(WTFMove(selector));
}

void CSSParserSelector::prependTagSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
{
    auto second = std::make_unique<CSSParserSelector>();
    second->m_selector = WTFMove(m_selector);
    second->m_tagHistory = WTFMove(m_tagHistory);
    m_tagHistory = WTFMove(second);

    m_selector = std::make_unique<CSSSelector>(tagQName, tagIsForNamespaceRule);
    m_selector->setRelation(CSSSelector::Subselector);
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::releaseTagHistory()
{
    setRelation(CSSSelector::Subselector);
    return WTFMove(m_tagHistory);
}

// FIXME-NEWPARSER: Add support for :host-context
bool CSSParserSelector::isHostPseudoSelector() const
{
    return match() == CSSSelector::PseudoClass && pseudoClassType() == CSSSelector::PseudoClassHost;
}

}

