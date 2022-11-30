/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "RuleFeature.h"
#include "RuleSet.h"
#include "UserAgentStyle.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSStyleRule;
class CSSStyleSheet;
class ExtensionStyleSheets;

namespace MQ {
class MediaQueryEvaluator;
};

namespace Style {

enum class CascadeLevel : uint8_t;
class InspectorCSSOMWrappers;
class Resolver;

struct InvalidationRuleSet {
    RefPtr<RuleSet> ruleSet;
    Vector<const CSSSelector*> invalidationSelectors;
    MatchElement matchElement;
    IsNegation isNegation;
};

class ScopeRuleSets {
public:
    ScopeRuleSets(Resolver&);
    ~ScopeRuleSets();

    bool isAuthorStyleDefined() const { return m_isAuthorStyleDefined; }
    RuleSet* userAgentMediaQueryStyle() const;
    RuleSet& authorStyle() const { return *m_authorStyle; }
    RuleSet* userStyle() const;
    RuleSet* styleForCascadeLevel(CascadeLevel);

    const RuleFeatureSet& features() const;
    RuleSet* sibling() const { return m_siblingRuleSet.get(); }
    RuleSet* uncommonAttribute() const { return m_uncommonAttributeRuleSet.get(); }

    const Vector<InvalidationRuleSet>* idInvalidationRuleSets(const AtomString&) const;
    const Vector<InvalidationRuleSet>* classInvalidationRuleSets(const AtomString&) const;
    const Vector<InvalidationRuleSet>* attributeInvalidationRuleSets(const AtomString&) const;
    const Vector<InvalidationRuleSet>* pseudoClassInvalidationRuleSets(const PseudoClassInvalidationKey&) const;
    const Vector<InvalidationRuleSet>* hasPseudoClassInvalidationRuleSets(const PseudoClassInvalidationKey&) const;

    bool hasComplexSelectorsForStyleAttribute() const;

    void setUsesSharedUserStyle(bool b) { m_usesSharedUserStyle = b; }
    void initializeUserStyle();

    void resetAuthorStyle();
    void appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&, MQ::MediaQueryEvaluator*, Style::InspectorCSSOMWrappers&);

    void resetUserAgentMediaQueryStyle();

    bool hasViewportDependentMediaQueries() const;
    bool hasContainerQueries() const;

    std::optional<DynamicMediaQueryEvaluationChanges> evaluateDynamicMediaQueryRules(const MQ::MediaQueryEvaluator&);

    RuleFeatureSet& mutableFeatures();

    bool& isInvalidatingStyleWithRuleSets() { return m_isInvalidatingStyleWithRuleSets; }

    bool hasMatchingUserOrAuthorStyle(const Function<bool(RuleSet&)>&);

private:
    void collectFeatures() const;
    void collectRulesFromUserStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&, RuleSet& userStyle, const MQ::MediaQueryEvaluator&);
    void updateUserAgentMediaQueryStyleIfNeeded() const;

    RefPtr<RuleSet> m_authorStyle;
    mutable RefPtr<RuleSet> m_userAgentMediaQueryStyle;
    RefPtr<RuleSet> m_userStyle;

    Resolver& m_styleResolver;
    mutable RuleFeatureSet m_features;
    mutable RefPtr<RuleSet> m_siblingRuleSet;
    mutable RefPtr<RuleSet> m_uncommonAttributeRuleSet;
    mutable HashMap<AtomString, std::unique_ptr<Vector<InvalidationRuleSet>>> m_idInvalidationRuleSets;
    mutable HashMap<AtomString, std::unique_ptr<Vector<InvalidationRuleSet>>> m_classInvalidationRuleSets;
    mutable HashMap<AtomString, std::unique_ptr<Vector<InvalidationRuleSet>>> m_attributeInvalidationRuleSets;
    mutable HashMap<PseudoClassInvalidationKey, std::unique_ptr<Vector<InvalidationRuleSet>>> m_pseudoClassInvalidationRuleSets;
    mutable HashMap<PseudoClassInvalidationKey, std::unique_ptr<Vector<InvalidationRuleSet>>> m_hasPseudoClassInvalidationRuleSets;

    mutable std::optional<bool> m_cachedHasComplexSelectorsForStyleAttribute;

    mutable unsigned m_defaultStyleVersionOnFeatureCollection { 0 };
    mutable unsigned m_userAgentMediaQueryRuleCountOnUpdate { 0 };

    bool m_usesSharedUserStyle { false };
    bool m_isAuthorStyleDefined { false };

    // For catching <rdar://problem/53413013>
    bool m_isInvalidatingStyleWithRuleSets { false };
};

inline const RuleFeatureSet& ScopeRuleSets::features() const
{
    if (m_defaultStyleVersionOnFeatureCollection < UserAgentStyle::defaultStyleVersion)
        collectFeatures();
    return m_features;
}

// FIXME: There should be just the const version.
inline RuleFeatureSet& ScopeRuleSets::mutableFeatures()
{
    if (m_defaultStyleVersionOnFeatureCollection < UserAgentStyle::defaultStyleVersion)
        collectFeatures();
    return m_features;
}

} // namespace Style
} // namespace WebCore
