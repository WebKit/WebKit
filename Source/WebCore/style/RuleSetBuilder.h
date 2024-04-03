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

#include "MediaQuery.h"
#include "RuleSet.h"

namespace WebCore {
namespace Style {

class RuleSetBuilder {
public:
    enum class ShrinkToFit : bool { Enable, Disable };
    enum class ShouldResolveNesting : bool { No, Yes };
    RuleSetBuilder(RuleSet&, const MQ::MediaQueryEvaluator&, Resolver* = nullptr, ShrinkToFit = ShrinkToFit::Enable, ShouldResolveNesting = ShouldResolveNesting::No);
    ~RuleSetBuilder();

    void addRulesFromSheet(const StyleSheetContents&, const MQ::MediaQueryList& sheetQuery = { });
    void addStyleRule(const StyleRule&);

private:
    RuleSetBuilder(const MQ::MediaQueryEvaluator&);

    void addStyleRule(StyleRuleWithNesting&);
    void addRulesFromSheetContents(const StyleSheetContents&);
    void addChildRules(const Vector<Ref<StyleRuleBase>>&);
    void addChildRule(Ref<StyleRuleBase>);
    void disallowDynamicMediaQueryEvaluationIfNeeded();
    void addStyleRuleWithSelectorList(const CSSSelectorList&, const StyleRule&);

    void registerLayers(const Vector<CascadeLayerName>&);
    void pushCascadeLayer(const CascadeLayerName&);
    void popCascadeLayer(const CascadeLayerName&);
    void updateCascadeLayerPriorities();

    void addMutatingRulesToResolver();
    void updateDynamicMediaQueries();
    void resolveSelectorListWithNesting(StyleRuleWithNesting&);

    struct MediaQueryCollector {
        ~MediaQueryCollector();

        const MQ::MediaQueryEvaluator& evaluator;
        bool collectDynamic { false };

        struct DynamicContext {
            const MQ::MediaQueryList& queries;
            Vector<size_t> affectedRulePositions { };
            HashSet<Ref<const StyleRule>> affectedRules { };
        };
        Vector<DynamicContext> dynamicContextStack { };

        Vector<RuleSet::DynamicMediaQueryRules> dynamicMediaQueryRules { };
        OptionSet<MQ::MediaQueryDynamicDependency> allDynamicDependencies { };

        bool pushAndEvaluate(const MQ::MediaQueryList&);
        void pop(const MQ::MediaQueryList&);
        void addRuleIfNeeded(const RuleData&);
    };

    RefPtr<RuleSet> m_ruleSet;
    MediaQueryCollector m_mediaQueryCollector;
    Resolver* m_resolver { nullptr };
    const ShrinkToFit m_shrinkToFit { ShrinkToFit::Enable };

    CascadeLayerName m_resolvedCascadeLayerName;
    HashMap<CascadeLayerName, RuleSet::CascadeLayerIdentifier> m_cascadeLayerIdentifierMap;
    RuleSet::CascadeLayerIdentifier m_currentCascadeLayerIdentifier { 0 };
    Vector<const CSSSelectorList*> m_selectorListStack;
    const ShouldResolveNesting m_shouldResolveNesting { ShouldResolveNesting::No };

    RuleSet::ContainerQueryIdentifier m_currentContainerQueryIdentifier { 0 };
    RuleSet::ScopeRuleIdentifier m_currentScopeIdentifier { 0 };

    IsStartingStyle m_isStartingStyle { IsStartingStyle::No };

    Vector<RuleSet::ResolverMutatingRule> m_collectedResolverMutatingRules;
    bool requiresStaticMediaQueryEvaluation { false };
};

}
}
