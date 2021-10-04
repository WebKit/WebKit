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

#include "RuleSet.h"

namespace WebCore {
namespace Style {

struct RuleSetMediaQueryCollector {
    ~RuleSetMediaQueryCollector();

    const MediaQueryEvaluator& evaluator;
    const bool collectDynamic { false };

    struct DynamicContext {
        Ref<const MediaQuerySet> set;
        Vector<size_t> affectedRulePositions { };
        RuleFeatureVector ruleFeatures { };
    };
    Vector<DynamicContext> dynamicContextStack { };

    Vector<RuleSet::DynamicMediaQueryRules> dynamicMediaQueryRules { };
    bool didMutateResolverWithinDynamicMediaQuery { false };
    bool hasViewportDependentMediaQueries { false };

    bool pushAndEvaluate(const MediaQuerySet*);
    void pop(const MediaQuerySet*);
    void didMutateResolver();
    void addRuleIfNeeded(const RuleData&);
};

struct RuleSetBuilder {
    enum class Mode { Normal, ResolverMutationScan };

    Ref<RuleSet> ruleSet;
    RuleSetMediaQueryCollector mediaQueryCollector;
    Resolver* resolver { nullptr };
    Mode mode { Mode::Normal };
    CascadeLayerName resolvedCascadeLayerName { };
    HashMap<CascadeLayerName, RuleSet::CascadeLayerIdentifier> cascadeLayerIdentifierMap { };
    RuleSet::CascadeLayerIdentifier currentCascadeLayerIdentifier { 0 };
    Vector<RuleSet::ResolverMutatingRule> collectedResolverMutatingRules { };

    void addRulesFromSheet(const StyleSheetContents&);

    ~RuleSetBuilder();

private:
    void addChildRules(const Vector<RefPtr<StyleRuleBase>>&);
    void addStyleRule(const StyleRule&);

    void registerLayers(const Vector<CascadeLayerName>&);
    void pushCascadeLayer(const CascadeLayerName&);
    void popCascadeLayer(const CascadeLayerName&);
    void updateCascadeLayerOrder();
    void addMutatingRulesToResolver();
};

}
}
