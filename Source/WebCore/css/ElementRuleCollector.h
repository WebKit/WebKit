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

#ifndef ElementRuleCollector_h
#define ElementRuleCollector_h

#include "MediaQueryEvaluator.h"
#include "SelectorChecker.h"
#include "StyleResolver.h"
#include <memory>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class DocumentRuleSets;
class MatchRequest;
class RenderRegion;
class RuleData;
class RuleSet;
class SelectorFilter;

struct MatchedRule {
    const RuleData* ruleData;
    unsigned specificity;
};

class ElementRuleCollector {
public:
    ElementRuleCollector(const Element&, const DocumentRuleSets&, const SelectorFilter*);
    ElementRuleCollector(const Element&, const RuleSet& authorStyle, const SelectorFilter*);

    void matchAllRules(bool matchAuthorAndUserStyles, bool includeSMILProperties);
    void matchUARules();
    void matchAuthorRules(bool includeEmptyRules);
    void matchUserRules(bool includeEmptyRules);

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoStyleRequest(const PseudoStyleRequest& request) { m_pseudoStyleRequest = request; }
    void setSameOriginOnly(bool f) { m_sameOriginOnly = f; } 
    void setRegionForStyling(const RenderRegion* regionForStyling) { m_regionForStyling = regionForStyling; }
    void setMedium(const MediaQueryEvaluator* medium) { m_isPrintStyle = medium->mediaTypeMatchSpecific("print"); }

    bool hasAnyMatchingRules(const RuleSet*);

    StyleResolver::MatchResult& matchedResult();
    const Vector<RefPtr<StyleRule>>& matchedRuleList() const;

    bool hasMatchedRules() const { return !m_matchedRules.isEmpty(); }
    void clearMatchedRules();

    const PseudoIdSet& matchedPseudoElementIds() const { return m_matchedPseudoElementIds; }
    const Style::Relations& styleRelations() const { return m_styleRelations; }
    bool didMatchUncommonAttributeSelector() const { return m_didMatchUncommonAttributeSelector; }

private:
    void addElementStyleProperties(const StyleProperties*, bool isCacheable = true);

    void matchUARules(RuleSet*);
#if ENABLE(SHADOW_DOM)
    void matchHostPseudoClassRules(bool includeEmptyRules);
    void matchSlottedPseudoElementRules(bool includeEmptyRules);
    RuleSet::RuleDataVector collectSlottedPseudoElementRulesForSlot(bool includeEmptyRules);
#endif

    void collectMatchingRules(const MatchRequest&, StyleResolver::RuleRange&);
    void collectMatchingRulesForRegion(const MatchRequest&, StyleResolver::RuleRange&);
    void collectMatchingRulesForList(const RuleSet::RuleDataVector*, const MatchRequest&, StyleResolver::RuleRange&);
    bool ruleMatches(const RuleData&, unsigned &specificity);

    void sortMatchedRules();
    void sortAndTransferMatchedRules();

    void addMatchedRule(const RuleData&, unsigned specificity, StyleResolver::RuleRange&);

    const Element& m_element;
    const RuleSet& m_authorStyle;
    const RuleSet* m_userStyle { nullptr };
    const SelectorFilter* m_selectorFilter { nullptr };

    bool m_isPrintStyle { false };
    const RenderRegion* m_regionForStyling { nullptr };
    PseudoStyleRequest m_pseudoStyleRequest { NOPSEUDO };
    bool m_sameOriginOnly { false };
    SelectorChecker::Mode m_mode { SelectorChecker::Mode::ResolvingStyle };
#if ENABLE(SHADOW_DOM)
    bool m_isMatchingSlottedPseudoElements { false };
#endif

    Vector<MatchedRule, 64> m_matchedRules;

    // Output.
    Vector<RefPtr<StyleRule>> m_matchedRuleList;
    bool m_didMatchUncommonAttributeSelector { false };
    StyleResolver::MatchResult m_result;
    Style::Relations m_styleRelations;
    PseudoIdSet m_matchedPseudoElementIds;
};

} // namespace WebCore

#endif // ElementRuleCollector_h
