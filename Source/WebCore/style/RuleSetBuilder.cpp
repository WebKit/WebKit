/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "RuleSetBuilder.h"

namespace WebCore {
namespace Style {

RuleSetBuilder::RuleSetBuilder(RuleSet& ruleSet, const MediaQueryEvaluator& evaluator, Resolver* resolver)
    : m_ruleSet(&ruleSet)
    , m_mediaQueryCollector({ evaluator })
    , m_resolver(resolver)
{
}

RuleSetBuilder::RuleSetBuilder(const MediaQueryEvaluator& evaluator)
    : m_mediaQueryCollector({ evaluator, true })
{
}

RuleSetBuilder::~RuleSetBuilder()
{
    if (!m_ruleSet)
        return;

    updateCascadeLayerOrder();
    updateDynamicMediaQueries();
    addMutatingRulesToResolver();

    if (m_ruleSet->m_autoShrinkToFitEnabled)
        m_ruleSet->shrinkToFit();
}

void RuleSetBuilder::addRulesFromSheet(const StyleSheetContents& sheet, const MediaQuerySet* sheetQuery)
{
    auto canUseDynamicMediaQueryResolution = [&] {
        if (!m_resolver)
            return false;

        RuleSetBuilder mutationScanner(m_mediaQueryCollector.evaluator);
        if (mutationScanner.m_mediaQueryCollector.pushAndEvaluate(sheetQuery))
            mutationScanner.addRulesFromSheetContents(sheet);
        mutationScanner.m_mediaQueryCollector.pop(sheetQuery);

        return !mutationScanner.didSeeResolverMutationWithinDynamicMediaQuery;
    };

    m_mediaQueryCollector.collectDynamic = canUseDynamicMediaQueryResolution();

    if (m_mediaQueryCollector.pushAndEvaluate(sheetQuery))
        addRulesFromSheetContents(sheet);
    m_mediaQueryCollector.pop(sheetQuery);
}

void RuleSetBuilder::addChildRules(const Vector<RefPtr<StyleRuleBase>>& rules)
{
    for (auto& rule : rules) {
        if (didSeeResolverMutationWithinDynamicMediaQuery)
            return;

        if (is<StyleRule>(*rule)) {
            if (m_ruleSet)
                addStyleRule(downcast<StyleRule>(*rule));
            continue;
        }
        if (is<StyleRulePage>(*rule)) {
            if (m_ruleSet)
                m_ruleSet->addPageRule(downcast<StyleRulePage>(*rule));
            continue;
        }
        if (is<StyleRuleMedia>(*rule)) {
            auto& mediaRule = downcast<StyleRuleMedia>(*rule);
            if (m_mediaQueryCollector.pushAndEvaluate(&mediaRule.mediaQueries()))
                addChildRules(mediaRule.childRules());
            m_mediaQueryCollector.pop(&mediaRule.mediaQueries());
            continue;
        }
        if (is<StyleRuleLayer>(*rule)) {
            auto& layerRule = downcast<StyleRuleLayer>(*rule);
            if (layerRule.isStatement()) {
                // Statement syntax just registers the layers.
                registerLayers(layerRule.nameList());
                continue;
            }
            // Block syntax.
            pushCascadeLayer(layerRule.name());
            addChildRules(layerRule.childRules());
            popCascadeLayer(layerRule.name());
            continue;
        }
        if (is<StyleRuleFontFace>(*rule) || is<StyleRuleFontPaletteValues>(*rule) || is<StyleRuleKeyframes>(*rule)) {
            if (!m_ruleSet && !m_mediaQueryCollector.dynamicContextStack.isEmpty())
                didSeeResolverMutationWithinDynamicMediaQuery = true;

            if (m_resolver)
                m_collectedResolverMutatingRules.append({ *rule, m_currentCascadeLayerIdentifier });
            continue;
        }
        if (is<StyleRuleSupports>(*rule) && downcast<StyleRuleSupports>(*rule).conditionIsSupported()) {
            addChildRules(downcast<StyleRuleSupports>(*rule).childRules());
            continue;
        }
    }
}

void RuleSetBuilder::addRulesFromSheetContents(const StyleSheetContents& sheet)
{
    for (auto& rule : sheet.layerRulesBeforeImportRules())
        registerLayers(rule->nameList());

    for (auto& rule : sheet.importRules()) {
        if (!rule->styleSheet())
            continue;

        if (m_mediaQueryCollector.pushAndEvaluate(rule->mediaQueries())) {
            auto& cascadeLayerName = rule->cascadeLayerName();
            if (cascadeLayerName)
                pushCascadeLayer(*cascadeLayerName);

            addRulesFromSheetContents(*rule->styleSheet());

            if (cascadeLayerName)
                popCascadeLayer(*cascadeLayerName);
        }
        m_mediaQueryCollector.pop(rule->mediaQueries());
    }

    addChildRules(sheet.childRules());
}

void RuleSetBuilder::addStyleRule(const StyleRule& rule)
{
    auto& selectorList = rule.selectorList();
    if (selectorList.isEmpty())
        return;
    unsigned selectorListIndex = 0;
    for (size_t selectorIndex = 0; selectorIndex != notFound; selectorIndex = selectorList.indexOfNextSelectorAfter(selectorIndex)) {
        RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleSet->ruleCount());
        m_mediaQueryCollector.addRuleIfNeeded(ruleData);

        m_ruleSet->addRule(WTFMove(ruleData), m_currentCascadeLayerIdentifier);

        ++selectorListIndex;
    }
}

void RuleSetBuilder::registerLayers(const Vector<CascadeLayerName>& names)
{
    for (auto& name : names) {
        pushCascadeLayer(name);
        popCascadeLayer(name);
    }
}

void RuleSetBuilder::pushCascadeLayer(const CascadeLayerName& name)
{
    if (!m_ruleSet)
        return;

    if (m_cascadeLayerIdentifierMap.isEmpty() && !m_ruleSet->m_cascadeLayers.isEmpty()) {
        // For incremental build, reconstruct the name->identifier map.
        RuleSet::CascadeLayerIdentifier identifier = 0;
        for (auto& layer : m_ruleSet->m_cascadeLayers)
            m_cascadeLayerIdentifierMap.add(layer.resolvedName, ++identifier);
    }

    auto nameResolvingAnonymous = [&] {
        if (name.isEmpty()) {
            // Make unique name for an anonymous layer.
            unsigned long long random = randomNumber() * std::numeric_limits<unsigned long long>::max();
            return CascadeLayerName { "anon_"_s + String::number(random) };
        }
        return name;
    };

    // For hierarchical names we register the containing layers individually first.
    for (auto& nameSegment : nameResolvingAnonymous()) {
        m_resolvedCascadeLayerName.append(nameSegment);
        m_currentCascadeLayerIdentifier = m_cascadeLayerIdentifierMap.ensure(m_resolvedCascadeLayerName, [&] {
            // Previously unseen layer.
            m_ruleSet->m_cascadeLayers.append({ m_resolvedCascadeLayerName, m_currentCascadeLayerIdentifier });
            return m_ruleSet->m_cascadeLayers.size();
        }).iterator->value;
    }
}

void RuleSetBuilder::popCascadeLayer(const CascadeLayerName& name)
{
    if (!m_ruleSet)
        return;

    for (auto size = name.isEmpty() ? 1 : name.size(); size--;) {
        m_resolvedCascadeLayerName.removeLast();
        m_currentCascadeLayerIdentifier = m_ruleSet->cascadeLayerForIdentifier(m_currentCascadeLayerIdentifier).parentIdentifier;
    }
}

void RuleSetBuilder::updateCascadeLayerOrder()
{
    if (m_cascadeLayerIdentifierMap.isEmpty())
        return;

    auto compare = [&](auto a, auto b) {
        while (a && b) {
            // Identifiers are in parse order which almost corresponds to the layer priority order.
            // The only exception is when a sublayer gets added to a layer after adding other non-sublayers.
            // To resolve this we need look for a shared ancestor layer.
            auto aParent = m_ruleSet->cascadeLayerForIdentifier(a).parentIdentifier;
            auto bParent = m_ruleSet->cascadeLayerForIdentifier(b).parentIdentifier;
            if (aParent == bParent || aParent == b || bParent == a)
                break;
            if (aParent > bParent)
                a = aParent;
            else
                b = bParent;
        }
        return a < b;
    };

    Vector<RuleSet::CascadeLayerIdentifier> orderVector;
    auto layerCount = m_ruleSet->m_cascadeLayers.size();
    orderVector.reserveInitialCapacity(layerCount);
    for (RuleSet::CascadeLayerIdentifier identifier = 1; identifier <= layerCount; ++identifier)
        orderVector.uncheckedAppend(identifier);

    std::sort(orderVector.begin(), orderVector.end(), compare);

    for (unsigned i = 0; i < orderVector.size(); ++i)
        m_ruleSet->cascadeLayerForIdentifier(orderVector[i]).order = i + 1;
}

void RuleSetBuilder::addMutatingRulesToResolver()
{
    if (!m_resolver)
        return;

    auto compareLayers = [&](const auto& a, const auto& b) {
        auto aOrder = m_ruleSet->cascadeLayerOrderForIdentifier(a.layerIdentifier);
        auto bOrder = m_ruleSet->cascadeLayerOrderForIdentifier(b.layerIdentifier);
        return aOrder < bOrder;
    };

    // The order may change so we need to reprocess resolver mutating rules from earlier stylesheets.
    auto rulesToAdd = std::exchange(m_ruleSet->m_resolverMutatingRulesInLayers, { });
    rulesToAdd.appendVector(WTFMove(m_collectedResolverMutatingRules));

    if (!m_cascadeLayerIdentifierMap.isEmpty())
        std::stable_sort(rulesToAdd.begin(), rulesToAdd.end(), compareLayers);

    for (auto& collectedRule : rulesToAdd) {
        if (collectedRule.layerIdentifier)
            m_ruleSet->m_resolverMutatingRulesInLayers.append(collectedRule);

        auto& rule = collectedRule.rule;
        if (is<StyleRuleFontFace>(rule)) {
            m_resolver->document().fontSelector().addFontFaceRule(downcast<StyleRuleFontFace>(rule.get()), false);
            m_resolver->invalidateMatchedDeclarationsCache();
            continue;
        }
        if (is<StyleRuleFontPaletteValues>(rule)) {
            m_resolver->document().fontSelector().addFontPaletteValuesRule(downcast<StyleRuleFontPaletteValues>(rule.get()));
            m_resolver->invalidateMatchedDeclarationsCache();
            continue;
        }
        if (is<StyleRuleKeyframes>(rule)) {
            m_resolver->addKeyframeStyle(downcast<StyleRuleKeyframes>(rule.get()));
            continue;
        }
    }
}

void RuleSetBuilder::updateDynamicMediaQueries()
{
    if (m_mediaQueryCollector.hasViewportDependentMediaQueries)
        m_ruleSet->m_hasViewportDependentMediaQueries = true;

    if (!m_mediaQueryCollector.dynamicMediaQueryRules.isEmpty()) {
        auto firstNewIndex = m_ruleSet->m_dynamicMediaQueryRules.size();
        m_ruleSet->m_dynamicMediaQueryRules.appendVector(WTFMove(m_mediaQueryCollector.dynamicMediaQueryRules));

        // Set the initial values.
        m_ruleSet->evaluateDynamicMediaQueryRules(m_mediaQueryCollector.evaluator, firstNewIndex);
    }
}

RuleSetBuilder::MediaQueryCollector::~MediaQueryCollector() = default;

bool RuleSetBuilder::MediaQueryCollector::pushAndEvaluate(const MediaQuerySet* set)
{
    if (!set)
        return true;

    // Only evaluate static expressions that require style rebuild.
    MediaQueryDynamicResults dynamicResults;
    auto mode = collectDynamic ? MediaQueryEvaluator::Mode::AlwaysMatchDynamic : MediaQueryEvaluator::Mode::Normal;

    bool result = evaluator.evaluate(*set, &dynamicResults, mode);

    if (!dynamicResults.viewport.isEmpty())
        hasViewportDependentMediaQueries = true;

    if (!dynamicResults.isEmpty())
        dynamicContextStack.append({ *set });

    return result;
}

void RuleSetBuilder::MediaQueryCollector::pop(const MediaQuerySet* set)
{
    if (!set || dynamicContextStack.isEmpty() || set != &dynamicContextStack.last().set.get())
        return;

    if (!dynamicContextStack.last().affectedRulePositions.isEmpty() || !collectDynamic) {
        RuleSet::DynamicMediaQueryRules rules;
        for (auto& context : dynamicContextStack)
            rules.mediaQuerySets.append(context.set.get());

        if (collectDynamic) {
            rules.affectedRulePositions.appendVector(dynamicContextStack.last().affectedRulePositions);
            rules.ruleFeatures = WTFMove(dynamicContextStack.last().ruleFeatures);
            rules.ruleFeatures.shrinkToFit();
        } else
            rules.requiresFullReset = true;

        dynamicMediaQueryRules.append(WTFMove(rules));
    }

    dynamicContextStack.removeLast();
}

void RuleSetBuilder::MediaQueryCollector::addRuleIfNeeded(const RuleData& ruleData)
{
    if (dynamicContextStack.isEmpty())
        return;

    auto& context = dynamicContextStack.last();
    context.affectedRulePositions.append(ruleData.position());
    context.ruleFeatures.append({ ruleData });
}

}
}
