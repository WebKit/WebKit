/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "MediaList.h"
#include "RuleData.h"
#include "RuleFeature.h"
#include "SelectorCompiler.h"
#include "StyleRule.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/VectorHash.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSSelector;
class LegacyMediaQueryEvaluator;
class StyleSheetContents;

namespace Style {

class Resolver;
class RuleSet;

using CascadeLayerPriority = uint16_t;

using InvalidationRuleSetVector = Vector<RefPtr<const RuleSet>, 1>;

struct DynamicMediaQueryEvaluationChanges {
    enum class Type { InvalidateStyle, ResetStyle };
    Type type;
    InvalidationRuleSetVector invalidationRuleSets { };

    void append(DynamicMediaQueryEvaluationChanges&& other)
    {
        type = std::max(type, other.type);
        if (type == Type::ResetStyle)
            invalidationRuleSets.clear();
        else
            invalidationRuleSets.appendVector(WTFMove(other.invalidationRuleSets));
    };
};

class RuleSet : public RefCounted<RuleSet> {
    WTF_MAKE_NONCOPYABLE(RuleSet);
public:
    static Ref<RuleSet> create() { return adoptRef(*new RuleSet); }

    ~RuleSet();

    typedef Vector<RuleData, 1> RuleDataVector;
    typedef HashMap<AtomString, std::unique_ptr<RuleDataVector>> AtomRuleMap;

    void addRule(const StyleRule&, unsigned selectorIndex, unsigned selectorListIndex);
    void addPageRule(StyleRulePage&);

    void addToRuleSet(const AtomString& key, AtomRuleMap&, const RuleData&);
    void shrinkToFit();

    bool hasViewportDependentMediaQueries() const { return m_hasViewportDependentMediaQueries; }

    std::optional<DynamicMediaQueryEvaluationChanges> evaluateDynamicMediaQueryRules(const LegacyMediaQueryEvaluator&);

    const RuleFeatureSet& features() const { return m_features; }

    const RuleDataVector* idRules(const AtomString& key) const { return m_idRules.get(key); }
    const RuleDataVector* classRules(const AtomString& key) const { return m_classRules.get(key); }
    const RuleDataVector* attributeRules(const AtomString& key, bool isHTMLName) const;
    const RuleDataVector* tagRules(const AtomString& key, bool isHTMLName) const;
    const RuleDataVector* shadowPseudoElementRules(const AtomString& key) const { return m_shadowPseudoElementRules.get(key); }
    const RuleDataVector* linkPseudoClassRules() const { return &m_linkPseudoClassRules; }
#if ENABLE(VIDEO)
    const RuleDataVector& cuePseudoRules() const { return m_cuePseudoRules; }
#endif
    const RuleDataVector& hostPseudoClassRules() const { return m_hostPseudoClassRules; }
    const RuleDataVector& slottedPseudoElementRules() const { return m_slottedPseudoElementRules; }
    const RuleDataVector& partPseudoElementRules() const { return m_partPseudoElementRules; }
    const RuleDataVector* focusPseudoClassRules() const { return &m_focusPseudoClassRules; }
    const RuleDataVector* universalRules() const { return &m_universalRules; }

    const Vector<StyleRulePage*>& pageRules() const { return m_pageRules; }

    unsigned ruleCount() const { return m_ruleCount; }

    bool hasAttributeRules() const { return !m_attributeLocalNameRules.isEmpty(); }
    bool hasShadowPseudoElementRules() const { return !m_shadowPseudoElementRules.isEmpty(); }
    bool hasHostPseudoClassRulesMatchingInShadowTree() const { return m_hasHostPseudoClassRulesMatchingInShadowTree; }

    static constexpr auto cascadeLayerPriorityForPresentationalHints = std::numeric_limits<CascadeLayerPriority>::min();
    static constexpr auto cascadeLayerPriorityForUnlayered = std::numeric_limits<CascadeLayerPriority>::max();

    CascadeLayerPriority cascadeLayerPriorityFor(const RuleData&) const;

    bool hasContainerQueries() const { return !m_containerQueries.isEmpty(); }
    Vector<const CQ::ContainerQuery*> containerQueriesFor(const RuleData&) const;

private:
    friend class RuleSetBuilder;

    RuleSet();

    using CascadeLayerIdentifier = unsigned;
    using ContainerQueryIdentifier = unsigned;

    void addRule(RuleData&&, CascadeLayerIdentifier, ContainerQueryIdentifier);

    struct ResolverMutatingRule {
        Ref<StyleRuleBase> rule;
        CascadeLayerIdentifier layerIdentifier;
    };

    struct CollectedMediaQueryChanges {
        bool requiredFullReset { false };
        Vector<size_t> changedQueryIndexes { };
        Vector<Vector<Ref<const StyleRule>>*> affectedRules { };
    };
    CollectedMediaQueryChanges evaluateDynamicMediaQueryRules(const LegacyMediaQueryEvaluator&, size_t startIndex);

    template<typename Function> void traverseRuleDatas(Function&&);

    struct CascadeLayer {
        CascadeLayerName resolvedName;
        CascadeLayerIdentifier parentIdentifier;
        CascadeLayerPriority priority { 0 };
    };
    CascadeLayer& cascadeLayerForIdentifier(CascadeLayerIdentifier identifier) { return m_cascadeLayers[identifier - 1]; }
    const CascadeLayer& cascadeLayerForIdentifier(CascadeLayerIdentifier identifier) const { return m_cascadeLayers[identifier - 1]; }
    CascadeLayerPriority cascadeLayerPriorityForIdentifier(CascadeLayerIdentifier) const;

    struct ContainerQueryAndParent {
        Ref<StyleRuleContainer> containerRule;
        ContainerQueryIdentifier parent;
    };

    struct DynamicMediaQueryRules {
        Vector<Ref<const MediaQuerySet>> mediaQuerySets;
        Vector<size_t> affectedRulePositions;
        Vector<Ref<const StyleRule>> affectedRules;
        bool requiresFullReset { false };
        bool result { true };

        void shrinkToFit()
        {
            mediaQuerySets.shrinkToFit();
            affectedRulePositions.shrinkToFit();
            affectedRules.shrinkToFit();
        }
    };

    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
    AtomRuleMap m_attributeLocalNameRules;
    AtomRuleMap m_attributeCanonicalLocalNameRules;
    AtomRuleMap m_tagLocalNameRules;
    AtomRuleMap m_tagLowercaseLocalNameRules;
    AtomRuleMap m_shadowPseudoElementRules;
    RuleDataVector m_linkPseudoClassRules;
#if ENABLE(VIDEO)
    RuleDataVector m_cuePseudoRules;
#endif
    RuleDataVector m_hostPseudoClassRules;
    RuleDataVector m_slottedPseudoElementRules;
    RuleDataVector m_partPseudoElementRules;
    RuleDataVector m_focusPseudoClassRules;
    RuleDataVector m_universalRules;
    Vector<StyleRulePage*> m_pageRules;
    RuleFeatureSet m_features;
    Vector<DynamicMediaQueryRules> m_dynamicMediaQueryRules;
    HashMap<Vector<size_t>, Ref<const RuleSet>> m_mediaQueryInvalidationRuleSetCache;
    unsigned m_ruleCount { 0 };

    Vector<CascadeLayer> m_cascadeLayers;
    // This is a side vector to hold layer identifiers without bloating RuleData.
    Vector<CascadeLayerIdentifier> m_cascadeLayerIdentifierForRulePosition;

    Vector<ResolverMutatingRule> m_resolverMutatingRulesInLayers;

    Vector<ContainerQueryAndParent> m_containerQueries;
    Vector<ContainerQueryIdentifier> m_containerQueryIdentifierForRulePosition;

    bool m_hasHostPseudoClassRulesMatchingInShadowTree { false };
    bool m_hasViewportDependentMediaQueries { false };
};

inline const RuleSet::RuleDataVector* RuleSet::attributeRules(const AtomString& key, bool isHTMLName) const
{
    auto& rules = isHTMLName ? m_attributeCanonicalLocalNameRules : m_attributeLocalNameRules;
    return rules.get(key);
}

inline const RuleSet::RuleDataVector* RuleSet::tagRules(const AtomString& key, bool isHTMLName) const
{
    auto& rules = isHTMLName ? m_tagLowercaseLocalNameRules : m_tagLocalNameRules;
    return rules.get(key);
}

inline CascadeLayerPriority RuleSet::cascadeLayerPriorityForIdentifier(CascadeLayerIdentifier identifier) const
{
    if (!identifier)
        return cascadeLayerPriorityForUnlayered;
    return cascadeLayerForIdentifier(identifier).priority;
}

inline CascadeLayerPriority RuleSet::cascadeLayerPriorityFor(const RuleData& ruleData) const
{
    if (m_cascadeLayerIdentifierForRulePosition.size() <= ruleData.position())
        return cascadeLayerPriorityForUnlayered;
    auto identifier = m_cascadeLayerIdentifierForRulePosition[ruleData.position()];
    return cascadeLayerPriorityForIdentifier(identifier);
}

inline Vector<const CQ::ContainerQuery*> RuleSet::containerQueriesFor(const RuleData& ruleData) const
{
    if (m_containerQueryIdentifierForRulePosition.size() <= ruleData.position())
        return { };

    Vector<const CQ::ContainerQuery*> queries;

    auto identifier = m_containerQueryIdentifierForRulePosition[ruleData.position()];
    while (identifier) {
        auto& query = m_containerQueries[identifier - 1];
        queries.append(&query.containerRule->containerQuery());
        identifier = query.parent;
    };

    return queries;
}

} // namespace Style
} // namespace WebCore
