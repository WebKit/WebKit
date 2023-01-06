/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2014 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelector.h"
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

enum class CSSParserSelectorCombinator {
    Child,
    DescendantSpace,
    DirectAdjacent,
    IndirectAdjacent
};

class CSSParserSelector {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<CSSParserSelector> parsePseudoClassSelector(StringView);
    static std::unique_ptr<CSSParserSelector> parsePseudoElementSelector(StringView);
    static std::unique_ptr<CSSParserSelector> parsePagePseudoSelector(StringView);

    CSSParserSelector();

    // Recursively copy the selector chain.
    CSSParserSelector(const CSSSelector&);

    explicit CSSParserSelector(const QualifiedName&);

    ~CSSParserSelector();

    std::unique_ptr<CSSSelector> releaseSelector() { return WTFMove(m_selector); }
    CSSSelector* selector() { return m_selector.get(); }

    void setValue(const AtomString& value, bool matchLowerCase = false) { m_selector->setValue(value, matchLowerCase); }

    void setAttribute(const QualifiedName& value, bool convertToLowercase, CSSSelector::AttributeMatchType type) { m_selector->setAttribute(value, convertToLowercase, type); }
    
    void setArgument(const AtomString& value) { m_selector->setArgument(value); }
    void setNth(int a, int b) { m_selector->setNth(a, b); }
    void setMatch(CSSSelector::Match value) { m_selector->setMatch(value); }
    void setRelation(CSSSelector::RelationType value) { m_selector->setRelation(value); }
    void setForPage() { m_selector->setForPage(); }

    CSSSelector::Match match() const { return m_selector->match(); }
    CSSSelector::PseudoElementType pseudoElementType() const { return m_selector->pseudoElementType(); }
    const CSSSelectorList* selectorList() const { return m_selector->selectorList(); }
    
    void setPseudoElementType(CSSSelector::PseudoElementType type) { m_selector->setPseudoElementType(type); }
    void setPseudoClassType(CSSSelector::PseudoClassType type) { m_selector->setPseudoClassType(type); }

    void adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>&&);
    void setArgumentList(FixedVector<PossiblyQuotedIdentifier>);
    void setSelectorList(std::unique_ptr<CSSSelectorList>);

    CSSSelector::PseudoClassType pseudoClassType() const { return m_selector->pseudoClassType(); }
    bool isCustomPseudoElement() const { return m_selector->isCustomPseudoElement(); }

    bool isPseudoElementCueFunction() const;

    bool matchesPseudoElement() const;

    bool isHostPseudoSelector() const;

    // FIXME-NEWPARSER: "slotted" was removed here for now, since it leads to a combinator
    // connection of ShadowDescendant, and the current shadow DOM code doesn't expect this. When
    // we do fix this issue, make sure to patch the namespace prependTag code to remove the slotted
    // special case, since it will be covered by this function once again.
    bool needsImplicitShadowCombinatorForMatching() const;

    CSSParserSelector* tagHistory() const { return m_tagHistory.get(); }
    CSSParserSelector* leftmostSimpleSelector();
    void setTagHistory(std::unique_ptr<CSSParserSelector> selector) { m_tagHistory = WTFMove(selector); }
    void clearTagHistory() { m_tagHistory.reset(); }
    void insertTagHistory(CSSSelector::RelationType before, std::unique_ptr<CSSParserSelector>, CSSSelector::RelationType after);
    void appendTagHistory(CSSSelector::RelationType, std::unique_ptr<CSSParserSelector>);
    void appendTagHistory(CSSParserSelectorCombinator, std::unique_ptr<CSSParserSelector>);
    void prependTagSelector(const QualifiedName&, bool tagIsForNamespaceRule = false);
    std::unique_ptr<CSSParserSelector> releaseTagHistory();

private:
    std::unique_ptr<CSSSelector> m_selector;
    std::unique_ptr<CSSParserSelector> m_tagHistory;
};

inline bool CSSParserSelector::needsImplicitShadowCombinatorForMatching() const
{
    return match() == CSSSelector::PseudoElement
        && (pseudoElementType() == CSSSelector::PseudoElementWebKitCustom
#if ENABLE(VIDEO)
            || pseudoElementType() == CSSSelector::PseudoElementCue
#endif
            || pseudoElementType() == CSSSelector::PseudoElementPart
            || pseudoElementType() == CSSSelector::PseudoElementSlotted
            || pseudoElementType() == CSSSelector::PseudoElementWebKitCustomLegacyPrefixed);
}

inline bool CSSParserSelector::isPseudoElementCueFunction() const
{
#if ENABLE(VIDEO)
    return m_selector->match() == CSSSelector::PseudoElement && m_selector->pseudoElementType() == CSSSelector::PseudoElementCue;
#else
    return false;
#endif
}

}
