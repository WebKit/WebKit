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

#include "LegacyMediaQueryEvaluator.h"
#include "MatchResult.h"
#include "PropertyAllowlist.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "StyleScopeOrdinal.h"
#include <memory>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore::Style {

class MatchRequest;
class ScopeRuleSets;
struct SelectorMatchingState;
enum class CascadeLevel : uint8_t;

class PseudoElementRequest {
public:
    PseudoElementRequest(PseudoId pseudoId, std::optional<StyleScrollbarState> scrollbarState = std::nullopt)
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
    std::optional<StyleScrollbarState> scrollbarState;
    AtomString highlightName;
};

struct MatchedRule {
    const RuleData* ruleData;
    unsigned specificity;
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
    void setMedium(const LegacyMediaQueryEvaluator* medium) { m_isPrintStyle = medium->mediaTypeMatchSpecific("print"_s); }

    bool hasAnyMatchingRules(const RuleSet&);

    const MatchResult& matchResult() const;
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

    void matchShadowPseudoElementRules(CascadeLevel);
    void matchHostPseudoClassRules(CascadeLevel);
    void matchSlottedPseudoElementRules(CascadeLevel);
    void matchPartPseudoElementRules(CascadeLevel);
    void matchPartPseudoElementRulesForScope(const Element& partMatchingElement, CascadeLevel);

    void collectMatchingShadowPseudoElementRules(const MatchRequest&);

    void collectMatchingRules(CascadeLevel);
    void collectMatchingRules(const MatchRequest&);
    void collectMatchingRulesForList(const RuleSet::RuleDataVector*, const MatchRequest&);
    bool ruleMatches(const RuleData&, unsigned& specificity, ScopeOrdinal);
    bool containerQueriesMatch(const RuleData&, const MatchRequest&);

    void sortMatchedRules();

    enum class DeclarationOrigin { UserAgent, User, Author };
    static Vector<MatchedProperties>& declarationsForOrigin(MatchResult&, DeclarationOrigin);
    void sortAndTransferMatchedRules(DeclarationOrigin);
    void transferMatchedRules(DeclarationOrigin, std::optional<ScopeOrdinal> forScope = { });

    void addMatchedRule(const RuleData&, unsigned specificity, const MatchRequest&);
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
    MatchResult m_result;
    Relations m_styleRelations;
    PseudoIdSet m_matchedPseudoElementIds;
};

}
