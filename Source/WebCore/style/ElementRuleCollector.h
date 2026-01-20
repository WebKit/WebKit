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

#include "MatchResult.h"
#include "MediaQueryEvaluator.h"
#include "PropertyAllowlist.h"
#include "PseudoElementRequest.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "StyleScopeOrdinal.h"
#include <memory>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore::Style {

class ScopeRuleSets;
struct MatchRequest;
struct SelectorMatchingState;
enum class DeclarationOrigin : uint8_t;

struct MatchedRule {
    const RuleData* ruleData { nullptr };
    unsigned specificity { 0 };
    unsigned scopingRootDistance { 0 };
    ScopeOrdinal styleScopeOrdinal;
    CascadeLayerPriority cascadeLayerPriority;
};

class ElementRuleCollector {
public:
    ElementRuleCollector(const Element&, const ScopeRuleSets&, SelectorMatchingState*, SelectorChecker::Mode = SelectorChecker::Mode::ResolvingStyle);
    ElementRuleCollector(const Element&, const RuleSet& authorStyle, SelectorMatchingState*, SelectorChecker::Mode = SelectorChecker::Mode::ResolvingStyle);

    void setIncludeEmptyRules(bool value) { m_shouldIncludeEmptyRules = value; }

    void matchAllRules(bool matchAuthorAndUserStyles, bool includeSMILProperties);
    void matchUARules();
    void matchAuthorRules();
    void matchUserRules();

    bool matchesAnyAuthorRules();

    void setPseudoElementRequest(const std::optional<PseudoElementRequest>& request) { m_pseudoElementRequest = request; }
    void setMedium(const MQ::MediaQueryEvaluator& medium) { m_isPrintStyle = medium.isPrintMedia(); }


    const MatchResult& matchResult() const;
    Ref<MatchResult> releaseMatchResult();

    const Vector<Ref<const StyleRule>>& matchedRuleList() const;

    void clearMatchedRules();

    EnumSet<PseudoElementType> matchedPseudoElements() const { return m_matchedPseudoElements; }
    const Relations& styleRelations() const { return m_styleRelations; }

    void addAuthorKeyframeRules(const StyleRuleKeyframe&);

private:
    void addElementStyleProperties(const StyleProperties*, CascadeLayerPriority, IsCacheable = IsCacheable::Yes, FromStyleAttribute = FromStyleAttribute::No);

    void matchUARules(const RuleSet&);

    void addElementInlineStyleProperties(bool includeSMILProperties);

    void matchUserAgentPartRules(DeclarationOrigin);
    void matchHostPseudoClassRules(DeclarationOrigin);
    void matchSlottedPseudoElementRules(DeclarationOrigin);
    void matchPartPseudoElementRules(DeclarationOrigin);
    void matchPartPseudoElementRulesForScope(const Element& partMatchingElement, DeclarationOrigin);

    void collectMatchingUserAgentPartRules(const MatchRequest&);

    void collectMatchingRules(DeclarationOrigin);
    void collectMatchingRules(const MatchRequest&);
    void collectMatchingRulesForList(const RuleSet::RuleDataVector*, const MatchRequest&);
    void collectMatchingRulesForListSlow(const RuleSet::RuleDataVector&, const MatchRequest&);
    bool isFirstMatchModeAndHasMatchedAnyRules() const;
    struct ScopingRootWithDistance {
        RefPtr<const ContainerNode> scopingRoot;
        unsigned distance { std::numeric_limits<unsigned>::max() };
        bool matchesVisited { false };
    };
    bool ruleMatches(const RuleData&, unsigned& specificity, ScopeOrdinal, std::optional<ScopingRootWithDistance> scopingRoot = { });
    bool containerQueriesMatch(const RuleData&, const MatchRequest&);
    std::pair<bool, std::optional<Vector<ScopingRootWithDistance>>> scopeRulesMatch(const RuleData&, const MatchRequest&);

    void sortMatchedRules();

    Vector<MatchedProperties>& declarationsForOrigin(DeclarationOrigin);
    void sortAndTransferMatchedRules(DeclarationOrigin);
    void transferMatchedRules(DeclarationOrigin, std::optional<ScopeOrdinal> forScope = { });

    void addMatchedRule(const RuleData&, unsigned specificity, unsigned scopingRootDistance, const MatchRequest&);
    void addMatchedProperties(MatchedProperties&&, DeclarationOrigin);

    const Element& element() const { return m_element.get(); }

    const Ref<const Element> m_element;
    Ref<const RuleSet> m_authorStyle;
    RefPtr<const RuleSet> m_userStyle;
    RefPtr<const RuleSet> m_userAgentMediaQueryStyle;
    RefPtr<const RuleSet> m_dynamicViewTransitionsStyle;
    SelectorMatchingState* m_selectorMatchingState;

    bool m_shouldIncludeEmptyRules { false };
    bool m_isPrintStyle { false };
    std::optional<PseudoElementRequest> m_pseudoElementRequest { };
    const SelectorChecker::Mode m_mode { SelectorChecker::Mode::ResolvingStyle };

    Vector<MatchedRule, 64> m_matchedRules;
    size_t m_matchedRuleTransferIndex { 0 };

    // Output.
    Vector<Ref<const StyleRule>> m_matchedRuleList;
    Ref<MatchResult> m_result;
    Relations m_styleRelations;
    EnumSet<PseudoElementType> m_matchedPseudoElements;
};

ALWAYS_INLINE void ElementRuleCollector::collectMatchingRulesForList(const RuleSet::RuleDataVector* rules, const MatchRequest& matchRequest)
{
    if (!rules || rules->isEmpty())
        return;
    collectMatchingRulesForListSlow(*rules, matchRequest);
}

}
