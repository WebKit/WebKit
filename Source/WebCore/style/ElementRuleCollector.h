/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2013, 2014 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "MediaQueryEvaluator.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "StyleScope.h"
#include <memory>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class SelectorFilter;

namespace Style {

class MatchRequest;
class ScopeRuleSets;

class PseudoElementRequest {
public:
    PseudoElementRequest(PseudoId pseudoId, Optional<StyleScrollbarState> scrollbarState = WTF::nullopt)
        : pseudoId(pseudoId)
        , scrollbarState(scrollbarState)
    {
    }

    PseudoElementRequest(PseudoId pseudoId, const AtomString& highlightName)
        : pseudoId(pseudoId)
        , highlightName(highlightName)
    {
        ASSERT(pseudoId == PseudoId::Highlight);
    }

    PseudoId pseudoId;
    Optional<StyleScrollbarState> scrollbarState;
    AtomString highlightName;
};

struct MatchedRule {
    const RuleData* ruleData;
    unsigned specificity;
    ScopeOrdinal styleScopeOrdinal;
};

struct MatchedProperties {
    RefPtr<const StyleProperties> properties;
    uint16_t linkMatchType { SelectorChecker::MatchAll };
    uint16_t allowlistType { PropertyAllowlistNone };
    ScopeOrdinal styleScopeOrdinal { ScopeOrdinal::Element };
};

struct MatchResult {
    bool isCacheable { true };
    Vector<MatchedProperties> userAgentDeclarations;
    Vector<MatchedProperties> userDeclarations;
    Vector<MatchedProperties> authorDeclarations;

    bool operator==(const MatchResult& other) const
    {
        return isCacheable == other.isCacheable
            && userAgentDeclarations == other.userAgentDeclarations
            && userDeclarations == other.userDeclarations
            && authorDeclarations == other.authorDeclarations;
    }
    bool operator!=(const MatchResult& other) const { return !(*this == other); }

    bool isEmpty() const { return userAgentDeclarations.isEmpty() && userDeclarations.isEmpty() && authorDeclarations.isEmpty(); }
};

class ElementRuleCollector {
public:
    ElementRuleCollector(const Element&, const ScopeRuleSets&, const SelectorFilter*);
    ElementRuleCollector(const Element&, const RuleSet& authorStyle, const SelectorFilter*);

    void setIncludeEmptyRules(bool value) { m_shouldIncludeEmptyRules = value; }

    void matchAllRules(bool matchAuthorAndUserStyles, bool includeSMILProperties);
    void matchUARules();
    void matchAuthorRules();
    void matchUserRules();

    bool matchesAnyAuthorRules();

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoElementRequest(const PseudoElementRequest& request) { m_pseudoElementRequest = request; }
    void setMedium(const MediaQueryEvaluator* medium) { m_isPrintStyle = medium->mediaTypeMatchSpecific("print"); }

    bool hasAnyMatchingRules(const RuleSet*);

    const MatchResult& matchResult() const;
    const Vector<RefPtr<const StyleRule>>& matchedRuleList() const;

    void clearMatchedRules();

    const PseudoIdSet& matchedPseudoElementIds() const { return m_matchedPseudoElementIds; }
    const Relations& styleRelations() const { return m_styleRelations; }
    bool didMatchUncommonAttributeSelector() const { return m_didMatchUncommonAttributeSelector; }

private:
    void addElementStyleProperties(const StyleProperties*, bool isCacheable = true);

    void matchUARules(const RuleSet&);

    void collectMatchingAuthorRules();
    void addElementInlineStyleProperties(bool includeSMILProperties);

    void matchAuthorShadowPseudoElementRules();
    void matchHostPseudoClassRules();
    void matchSlottedPseudoElementRules();
    void matchPartPseudoElementRules();
    void matchPartPseudoElementRulesForScope(const ShadowRoot& scopeShadowRoot);

    void collectMatchingShadowPseudoElementRules(const MatchRequest&);
    std::unique_ptr<RuleSet::RuleDataVector> collectSlottedPseudoElementRulesForSlot();

    void collectMatchingRules(const MatchRequest&);
    void collectMatchingRulesForList(const RuleSet::RuleDataVector*, const MatchRequest&);
    bool ruleMatches(const RuleData&, unsigned& specificity);

    void sortMatchedRules();

    enum class DeclarationOrigin { UserAgent, User, Author };
    static Vector<MatchedProperties>& declarationsForOrigin(MatchResult&, DeclarationOrigin);
    void sortAndTransferMatchedRules(DeclarationOrigin);
    void transferMatchedRules(DeclarationOrigin, Optional<ScopeOrdinal> forScope = { });

    void addMatchedRule(const RuleData&, unsigned specificity, ScopeOrdinal);
    void addMatchedProperties(MatchedProperties&&, DeclarationOrigin);

    const Element& element() const { return m_element.get(); }

    const Ref<const Element> m_element;
    Ref<const RuleSet> m_authorStyle;
    RefPtr<const RuleSet> m_userStyle;
    RefPtr<const RuleSet> m_userAgentMediaQueryStyle;
    const SelectorFilter* m_selectorFilter;

    bool m_shouldIncludeEmptyRules { false };
    bool m_isPrintStyle { false };
    PseudoElementRequest m_pseudoElementRequest { PseudoId::None };
    SelectorChecker::Mode m_mode { SelectorChecker::Mode::ResolvingStyle };
    bool m_isMatchingSlottedPseudoElements { false };
    bool m_isMatchingHostPseudoClass { false };
    RefPtr<const Element> m_shadowHostInPartRuleScope;
    Vector<std::unique_ptr<RuleSet::RuleDataVector>> m_keepAliveSlottedPseudoElementRules;

    Vector<MatchedRule, 64> m_matchedRules;
    size_t m_matchedRuleTransferIndex { 0 };

    // Output.
    Vector<RefPtr<const StyleRule>> m_matchedRuleList;
    bool m_didMatchUncommonAttributeSelector { false };
    MatchResult m_result;
    Relations m_styleRelations;
    PseudoIdSet m_matchedPseudoElementIds;
};

inline bool operator==(const MatchedProperties& a, const MatchedProperties& b)
{
    return a.properties == b.properties && a.linkMatchType == b.linkMatchType;
}

inline bool operator!=(const MatchedProperties& a, const MatchedProperties& b)
{
    return !(a == b);
}

} // namespace Style
} // namespace WebCore
