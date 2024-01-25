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
enum class CascadeLevel : uint8_t;

struct MatchedRule {
    const RuleData* ruleData { nullptr };
    unsigned specificity { 0 };
    unsigned scopingRootDistance { 0 };
    ScopeOrdinal styleScopeOrdinal;
    CascadeLayerPriority cascadeLayerPriority;
};

class ElementRuleCollector {
public:
    ElementRuleCollector(const Element&, const ScopeRuleSets&, SelectorMatchingState*);
    ElementRuleCollector(const Element&, const RuleSet& authorStyle, SelectorMatchingState*);

    void setIncludeEmptyRules(bool value) { m_shouldIncludeEmptyRules = value; }

    void matchAllRules(bool matchAuthorAndUserStyles, bool includeSMILProperties);
    void matchUARules();
    void matchAuthorRules();
    void matchUserRules();

    bool matchesAnyAuthorRules();

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoElementRequest(const PseudoElementRequest& request) { m_pseudoElementRequest = request; }
    void setMedium(const MQ::MediaQueryEvaluator& medium) { m_isPrintStyle = medium.isPrintMedia(); }

    bool hasAnyMatchingRules(const RuleSet&);

    const MatchResult& matchResult() const;
    std::unique_ptr<MatchResult> releaseMatchResult();

    const Vector<RefPtr<const StyleRule>>& matchedRuleList() const;

    void clearMatchedRules();

    const PseudoIdSet& matchedPseudoElementIds() const { return m_matchedPseudoElementIds; }
    const Relations& styleRelations() const { return m_styleRelations; }
    bool didMatchUncommonAttributeSelector() const { return m_didMatchUncommonAttributeSelector; }

    void addAuthorKeyframeRules(const StyleRuleKeyframe&);

private:
    void addElementStyleProperties(const StyleProperties*, CascadeLayerPriority, bool isCacheable = true, FromStyleAttribute = FromStyleAttribute::No);

    void matchUARules(const RuleSet&);

    void addElementInlineStyleProperties(bool includeSMILProperties);

    void matchUserAgentPartRules(CascadeLevel);
    void matchHostPseudoClassRules(CascadeLevel);
    void matchSlottedPseudoElementRules(CascadeLevel);
    void matchPartPseudoElementRules(CascadeLevel);
    void matchPartPseudoElementRulesForScope(const Element& partMatchingElement, CascadeLevel);

    void collectMatchingUserAgentPartRules(const MatchRequest&);

    void collectMatchingRules(CascadeLevel);
    void collectMatchingRules(const MatchRequest&);
    void collectMatchingRulesForList(const RuleSet::RuleDataVector*, const MatchRequest&);
    bool ruleMatches(const RuleData&, unsigned& specificity, ScopeOrdinal, const ContainerNode* scopingRoot = nullptr);
    bool containerQueriesMatch(const RuleData&, const MatchRequest&);
    struct ScopingRootWithDistance {
        const ContainerNode* scopingRoot { nullptr };
        unsigned distance { std::numeric_limits<unsigned>::max() };
    };
    std::pair<bool, std::optional<Vector<ScopingRootWithDistance>>> scopeRulesMatch(const RuleData&, const MatchRequest&);

    void sortMatchedRules();

    enum class DeclarationOrigin { UserAgent, User, Author };
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
    SelectorMatchingState* m_selectorMatchingState;

    bool m_shouldIncludeEmptyRules { false };
    bool m_isPrintStyle { false };
    PseudoElementRequest m_pseudoElementRequest { PseudoId::None };
    SelectorChecker::Mode m_mode { SelectorChecker::Mode::ResolvingStyle };

    Vector<MatchedRule, 64> m_matchedRules;
    size_t m_matchedRuleTransferIndex { 0 };

    // Output.
    Vector<RefPtr<const StyleRule>> m_matchedRuleList;
    bool m_didMatchUncommonAttributeSelector { false };
    std::unique_ptr<MatchResult> m_result;
    Relations m_styleRelations;
    PseudoIdSet m_matchedPseudoElementIds;
};

}
