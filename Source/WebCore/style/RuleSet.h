/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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
class MediaQueryEvaluator;
class StyleSheetContents;

namespace Style {

class Resolver;
class RuleSet;

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

    struct DynamicMediaQueryRules {
        Vector<Ref<const MediaQuerySet>> mediaQuerySets;
        Vector<size_t> affectedRulePositions;
        RuleFeatureVector ruleFeatures;
        bool requiresFullReset { false };
        bool result { true };

        void shrinkToFit()
        {
            mediaQuerySets.shrinkToFit();
            affectedRulePositions.shrinkToFit();
            ruleFeatures.shrinkToFit();
        }
    };

    struct MediaQueryCollector {
        ~MediaQueryCollector();

        const MediaQueryEvaluator& evaluator;
        const bool collectDynamic { false };

        struct DynamicContext {
            Ref<const MediaQuerySet> set;
            Vector<size_t> affectedRulePositions { };
            RuleFeatureVector ruleFeatures { };
        };
        Vector<DynamicContext> dynamicContextStack { };

        Vector<DynamicMediaQueryRules> dynamicMediaQueryRules { };
        bool didMutateResolverWithinDynamicMediaQuery { false };
        bool hasViewportDependentMediaQueries { false };

        bool pushAndEvaluate(const MediaQuerySet*);
        void pop(const MediaQuerySet*);
        void didMutateResolver();
        void addRuleIfNeeded(const RuleData&);
    };

    void addRulesFromSheet(const StyleSheetContents&, const MediaQueryEvaluator&);
    void addRulesFromSheet(const StyleSheetContents&, const MediaQuerySet* sheetQuery, const MediaQueryEvaluator&, Style::Resolver&);

    void addRule(const StyleRule&, unsigned selectorIndex, unsigned selectorListIndex, unsigned cascadeLayerOrder = 0, MediaQueryCollector* = nullptr);
    void addPageRule(StyleRulePage&);

    void addToRuleSet(const AtomString& key, AtomRuleMap&, const RuleData&);
    void shrinkToFit();
    void disableAutoShrinkToFit() { m_autoShrinkToFitEnabled = false; }

    bool hasViewportDependentMediaQueries() const { return m_hasViewportDependentMediaQueries; }

    std::optional<DynamicMediaQueryEvaluationChanges> evaluateDynamicMediaQueryRules(const MediaQueryEvaluator&);

    const RuleFeatureSet& features() const { return m_features; }

    const RuleDataVector* idRules(const AtomString& key) const { return m_idRules.get(key); }
    const RuleDataVector* classRules(const AtomString& key) const { return m_classRules.get(key); }
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

    bool hasShadowPseudoElementRules() const { return !m_shadowPseudoElementRules.isEmpty(); }
    bool hasHostPseudoClassRulesMatchingInShadowTree() const { return m_hasHostPseudoClassRulesMatchingInShadowTree; }

    unsigned cascadeLayerOrderFor(const RuleData&) const;

private:
    RuleSet();

    using CascadeLayerIdentifier = unsigned;

    struct Builder {
        enum class Mode { Normal, ResolverMutationScan };

        Ref<RuleSet> ruleSet;
        MediaQueryCollector mediaQueryCollector;
        Style::Resolver* resolver { nullptr };
        Mode mode { Mode::Normal };
        CascadeLayerName resolvedCascadeLayerName { };
        HashMap<CascadeLayerName, CascadeLayerIdentifier> cascadeLayerIdentifierMap { };
        CascadeLayerIdentifier currentCascadeLayerIdentifier { 0 };

        void addRulesFromSheet(const StyleSheetContents&);

        ~Builder();
        
    private:
        void addChildRules(const Vector<RefPtr<StyleRuleBase>>&);
        void addStyleRule(const StyleRule&);

        void pushCascadeLayer(const CascadeLayerName&);
        void popCascadeLayer(const CascadeLayerName&);
        void updateCascadeLayerOrder();
    };

    struct CollectedMediaQueryChanges {
        bool requiredFullReset { false };
        Vector<size_t> changedQueryIndexes { };
        Vector<RuleFeatureVector*> ruleFeatures { };
    };
    CollectedMediaQueryChanges evaluateDynamicMediaQueryRules(const MediaQueryEvaluator&, size_t startIndex);

    template<typename Function> void traverseRuleDatas(Function&&);

    struct CascadeLayer {
        CascadeLayerName resolvedName;
        CascadeLayerIdentifier parentIdentifier;
        unsigned order { 0 };
    };
    CascadeLayer& cascadeLayerForIdentifier(CascadeLayerIdentifier identifier) { return m_cascadeLayers[identifier - 1]; }
    const CascadeLayer& cascadeLayerForIdentifier(CascadeLayerIdentifier identifier) const { return m_cascadeLayers[identifier - 1]; }

    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
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

    bool m_hasHostPseudoClassRulesMatchingInShadowTree { false };
    bool m_autoShrinkToFitEnabled { true };
    bool m_hasViewportDependentMediaQueries { false };
};

inline const RuleSet::RuleDataVector* RuleSet::tagRules(const AtomString& key, bool isHTMLName) const
{
    const AtomRuleMap* tagRules;
    if (isHTMLName)
        tagRules = &m_tagLowercaseLocalNameRules;
    else
        tagRules = &m_tagLocalNameRules;
    return tagRules->get(key);
}

inline unsigned RuleSet::cascadeLayerOrderFor(const RuleData& ruleData) const
{
    if (m_cascadeLayerIdentifierForRulePosition.size() <= ruleData.position())
        return 0;
    auto identifier = m_cascadeLayerIdentifierForRulePosition[ruleData.position()];
    if (!identifier)
        return 0;
    return cascadeLayerForIdentifier(identifier).order;
}

} // namespace Style
} // namespace WebCore
