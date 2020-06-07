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
        HashSet<size_t, DefaultHash<size_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<size_t>> affectedRulePositions;
        Vector<RuleFeature> ruleFeatures;
        bool requiresFullReset { false };
        bool result { true };
    };

    struct MediaQueryCollector {
        ~MediaQueryCollector();

        const MediaQueryEvaluator& evaluator;
        const bool collectDynamic { false };

        struct DynamicContext {
            Ref<const MediaQuerySet> set;
            Vector<size_t> affectedRulePositions { };
            Vector<RuleFeature> ruleFeatures { };
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

    void addRulesFromSheet(StyleSheetContents&, const MediaQueryEvaluator&);
    void addRulesFromSheet(StyleSheetContents&, MediaQuerySet* sheetQuery, const MediaQueryEvaluator&, Style::Resolver&);

    void addStyleRule(const StyleRule&, MediaQueryCollector&);
    void addRule(const StyleRule&, unsigned selectorIndex, unsigned selectorListIndex, MediaQueryCollector* = nullptr);
    void addPageRule(StyleRulePage&);
    void addToRuleSet(const AtomString& key, AtomRuleMap&, const RuleData&);
    void shrinkToFit();
    void disableAutoShrinkToFit() { m_autoShrinkToFitEnabled = false; }

    bool hasViewportDependentMediaQueries() const { return m_hasViewportDependentMediaQueries; }

    Optional<DynamicMediaQueryEvaluationChanges> evaluteDynamicMediaQueryRules(const MediaQueryEvaluator&);

    const RuleFeatureSet& features() const { return m_features; }

    const RuleDataVector* idRules(const AtomString& key) const { return m_idRules.get(key); }
    const RuleDataVector* classRules(const AtomString& key) const { return m_classRules.get(key); }
    const RuleDataVector* tagRules(const AtomString& key, bool isHTMLName) const;
    const RuleDataVector* shadowPseudoElementRules(const AtomString& key) const { return m_shadowPseudoElementRules.get(key); }
    const RuleDataVector* linkPseudoClassRules() const { return &m_linkPseudoClassRules; }
#if ENABLE(VIDEO)
    const RuleDataVector* cuePseudoRules() const { return &m_cuePseudoRules; }
#endif
    const RuleDataVector& hostPseudoClassRules() const { return m_hostPseudoClassRules; }
    const RuleDataVector& slottedPseudoElementRules() const { return m_slottedPseudoElementRules; }
    const RuleDataVector& partPseudoElementRules() const { return m_partPseudoElementRules; }
    const RuleDataVector* focusPseudoClassRules() const { return &m_focusPseudoClassRules; }
    const RuleDataVector* universalRules() const { return &m_universalRules; }

    const Vector<StyleRulePage*>& pageRules() const { return m_pageRules; }

    unsigned ruleCount() const { return m_ruleCount; }

    bool hasShadowPseudoElementRules() const;
    bool hasHostPseudoClassRulesMatchingInShadowTree() const { return m_hasHostPseudoClassRulesMatchingInShadowTree; }

private:
    RuleSet();

    enum class AddRulesMode { Normal, ResolverMutationScan };
    void addRulesFromSheet(StyleSheetContents&, MediaQueryCollector&, Style::Resolver*, AddRulesMode);
    void addChildRules(const Vector<RefPtr<StyleRuleBase>>&, MediaQueryCollector&, Style::Resolver*, AddRulesMode);
    struct CollectedMediaQueryChanges {
        bool requiredFullReset { false };
        Vector<size_t> changedQueryIndexes { };
        Vector<const Vector<RuleFeature>*> ruleFeatures { };
    };
    CollectedMediaQueryChanges evaluteDynamicMediaQueryRules(const MediaQueryEvaluator&, size_t startIndex);

    template<typename Function> void traverseRuleDatas(Function&&);


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
    unsigned m_ruleCount { 0 };
    bool m_hasHostPseudoClassRulesMatchingInShadowTree { false };
    bool m_autoShrinkToFitEnabled { true };
    RuleFeatureSet m_features;
    bool m_hasViewportDependentMediaQueries { false };
    Vector<DynamicMediaQueryRules> m_dynamicMediaQueryRules;
    HashMap<Vector<size_t>, Ref<const RuleSet>> m_mediaQueryInvalidationRuleSetCache;
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

} // namespace Style
} // namespace WebCore
