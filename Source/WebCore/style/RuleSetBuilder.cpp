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

#include "CSSCounterStyleRegistry.h"
#include "CSSCounterStyleRule.h"
#include "CSSFontSelector.h"
#include "CSSKeyframesRule.h"
#include "CSSSelectorParser.h"
#include "CustomPropertyRegistry.h"
#include "Document.h"
#include "MediaQueryEvaluator.h"
#include "StyleResolver.h"
#include "StyleRuleImport.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {
namespace Style {

RuleSetBuilder::RuleSetBuilder(RuleSet& ruleSet, const MQ::MediaQueryEvaluator& evaluator, Resolver* resolver, ShrinkToFit shrinkToFit)
    : m_ruleSet(&ruleSet)
    , m_mediaQueryCollector({ evaluator })
    , m_resolver(resolver)
    , m_shrinkToFit(shrinkToFit)
{
}

RuleSetBuilder::RuleSetBuilder(const MQ::MediaQueryEvaluator& evaluator)
    : m_mediaQueryCollector({ evaluator, true })
{
}

RuleSetBuilder::~RuleSetBuilder()
{
    if (!m_ruleSet)
        return;

    updateCascadeLayerPriorities();
    updateDynamicMediaQueries();
    addMutatingRulesToResolver();

    if (m_shrinkToFit == ShrinkToFit::Enable)
        m_ruleSet->shrinkToFit();
}

void RuleSetBuilder::addRulesFromSheet(const StyleSheetContents& sheet, const MQ::MediaQueryList& sheetQuery)
{
    auto canUseDynamicMediaQueryEvaluation = [&] {
        if (!m_resolver)
            return false;

        RuleSetBuilder dynamicEvaluationScanner(m_mediaQueryCollector.evaluator);
        if (dynamicEvaluationScanner.m_mediaQueryCollector.pushAndEvaluate(sheetQuery))
            dynamicEvaluationScanner.addRulesFromSheetContents(sheet);
        dynamicEvaluationScanner.m_mediaQueryCollector.pop(sheetQuery);

        return !dynamicEvaluationScanner.requiresStaticMediaQueryEvaluation;
    };

    m_mediaQueryCollector.collectDynamic = canUseDynamicMediaQueryEvaluation();

    if (m_mediaQueryCollector.pushAndEvaluate(sheetQuery))
        addRulesFromSheetContents(sheet);
    m_mediaQueryCollector.pop(sheetQuery);
}

void RuleSetBuilder::addChildRules(const Vector<RefPtr<StyleRuleBase>>& rules)
{
    for (auto& rule : rules) {
        if (requiresStaticMediaQueryEvaluation)
            return;
        addChildRule(rule);
    }
}

void RuleSetBuilder::addChildRule(RefPtr<StyleRuleBase> rule)
{
    switch (rule->type()) {
    case StyleRuleType::StyleWithNesting:
        if (m_ruleSet)
            addStyleRule(downcast<StyleRuleWithNesting>(*rule));
        return;

    case StyleRuleType::Style:
        if (m_ruleSet)
            addStyleRule(downcast<StyleRule>(*rule));
        return;

    case StyleRuleType::Page:
        if (m_ruleSet)
            m_ruleSet->addPageRule(downcast<StyleRulePage>(*rule));
        return;

    case StyleRuleType::Media: {
        auto& mediaRule = downcast<StyleRuleMedia>(*rule);
        if (m_mediaQueryCollector.pushAndEvaluate(mediaRule.mediaQueries()))
            addChildRules(mediaRule.childRules());
        m_mediaQueryCollector.pop(mediaRule.mediaQueries());
        return;
    }

    case StyleRuleType::Container: {
        auto& containerRule = downcast<StyleRuleContainer>(*rule);
        auto previousContainerQueryIdentifier = m_currentContainerQueryIdentifier;
        if (m_ruleSet) {
            m_ruleSet->m_containerQueries.append({ containerRule, previousContainerQueryIdentifier });
            m_currentContainerQueryIdentifier = m_ruleSet->m_containerQueries.size();
        }
        addChildRules(containerRule.childRules());
        if (m_ruleSet)
            m_currentContainerQueryIdentifier = previousContainerQueryIdentifier;
        return;
    }

    case StyleRuleType::LayerBlock:
    case StyleRuleType::LayerStatement: {
        disallowDynamicMediaQueryEvaluationIfNeeded();

        auto& layerRule = downcast<StyleRuleLayer>(*rule);
        if (layerRule.isStatement()) {
            // Statement syntax just registers the layers.
            registerLayers(layerRule.nameList());
            return;
        }
        // Block syntax.
        pushCascadeLayer(layerRule.name());
        addChildRules(layerRule.childRules());
        popCascadeLayer(layerRule.name());
        return;
    }
    case StyleRuleType::CounterStyle:
    case StyleRuleType::FontFace:
    case StyleRuleType::FontPaletteValues:
    case StyleRuleType::FontFeatureValues:
    case StyleRuleType::Keyframes:
    case StyleRuleType::Property:
        disallowDynamicMediaQueryEvaluationIfNeeded();
        if (m_resolver)
            m_collectedResolverMutatingRules.append({ *rule, m_currentCascadeLayerIdentifier });
        return;

    case StyleRuleType::Supports:
        if (downcast<StyleRuleSupports>(*rule).conditionIsSupported())
            addChildRules(downcast<StyleRuleSupports>(*rule).childRules());
        return;

    case StyleRuleType::Import:
    case StyleRuleType::Margin:
    case StyleRuleType::Namespace:
    case StyleRuleType::FontFeatureValuesBlock:
        return;

    case StyleRuleType::Unknown:
    case StyleRuleType::Charset:
    case StyleRuleType::Keyframe:
        ASSERT_NOT_REACHED();
        return;
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
            if (cascadeLayerName) {
                disallowDynamicMediaQueryEvaluationIfNeeded();
                pushCascadeLayer(*cascadeLayerName);
            }

            addRulesFromSheetContents(*rule->styleSheet());

            if (cascadeLayerName)
                popCascadeLayer(*cascadeLayerName);
        }
        m_mediaQueryCollector.pop(rule->mediaQueries());
    }

    addChildRules(sheet.childRules());
}

void RuleSetBuilder::populateStyleRuleResolvedSelectorList(const StyleRuleWithNesting& rule)
{
    auto resolvedSelectorList = rule.selectorList();

    // FIXME: handle top-level nesting selector
    if (m_styleRuleStack.size()) 
        resolvedSelectorList = CSSSelectorParser::resolveNestingParent(rule.selectorList(), m_styleRuleStack.last());

    rule.setResolvedSelectorList(WTFMove(resolvedSelectorList));    
}

void RuleSetBuilder::addStyleRuleWithSelectorList(const CSSSelectorList& selectorList, const StyleRule& rule)
{
    if (!selectorList.isEmpty()) {
        unsigned selectorListIndex = 0;
        for (size_t selectorIndex = 0; selectorIndex != notFound; selectorIndex = selectorList.indexOfNextSelectorAfter(selectorIndex)) {
            RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleSet->ruleCount());
            m_mediaQueryCollector.addRuleIfNeeded(ruleData);
            m_ruleSet->addRule(WTFMove(ruleData), m_currentCascadeLayerIdentifier, m_currentContainerQueryIdentifier);
            ++selectorListIndex;
        }
    }
}

void RuleSetBuilder::addStyleRule(const StyleRuleWithNesting& rule)
{
    populateStyleRuleResolvedSelectorList(rule);

    auto& selectorList = rule.resolvedSelectorList();
    addStyleRuleWithSelectorList(selectorList, rule);
    
    // Process nested rules
    m_styleRuleStack.append(&selectorList);
    for (auto& nestedRule : rule.nestedRules())
        addChildRule(nestedRule.ptr());
    m_styleRuleStack.removeLast();
}

void RuleSetBuilder::addStyleRule(const StyleRule& rule)
{
    addStyleRuleWithSelectorList(rule.selectorList(), rule);
}

void RuleSetBuilder::disallowDynamicMediaQueryEvaluationIfNeeded()
{
    bool isScanningForDynamicEvaluation = !m_ruleSet;
    if (isScanningForDynamicEvaluation && !m_mediaQueryCollector.dynamicContextStack.isEmpty())
        requiresStaticMediaQueryEvaluation = true;
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
            return CascadeLayerName { makeAtomString("anon_"_s, cryptographicallyRandomNumber<uint64_t>()) };
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

void RuleSetBuilder::updateCascadeLayerPriorities()
{
    if (m_cascadeLayerIdentifierMap.isEmpty())
        return;

    auto compare = [&](auto a, auto b) {
        while (true) {
            // Identifiers are in parse order.
            auto aParent = m_ruleSet->cascadeLayerForIdentifier(a).parentIdentifier;
            auto bParent = m_ruleSet->cascadeLayerForIdentifier(b).parentIdentifier;

            // For sibling layers, the later layer in parse order has a higher priority.
            if (aParent == bParent)
                return a < b;

            // For nested layers, the parent layer has a higher priority.
            if (aParent == b)
                return true;
            if (a == bParent)
                return false;

            // Traverse to parent. Parent layer identifiers are always lower.
            if (aParent > bParent)
                a = aParent;
            else
                b = bParent;
        }
    };

    auto layerCount = m_ruleSet->m_cascadeLayers.size();

    Vector<RuleSet::CascadeLayerIdentifier> layersInPriorityOrder;
    layersInPriorityOrder.reserveInitialCapacity(layerCount);
    for (RuleSet::CascadeLayerIdentifier identifier = 1; identifier <= layerCount; ++identifier)
        layersInPriorityOrder.uncheckedAppend(identifier);

    std::sort(layersInPriorityOrder.begin(), layersInPriorityOrder.end(), compare);

    // Priorities matter only relative to each other, so assign them enforcing these constraints:
    // - Layers must get a priority greater than RuleSet::cascadeLayerPriorityForPresentationalHints.
    // - Layers must get a priority smaller than RuleSet::cascadeLayerPriorityForUnlayered.
    // - A layer must get at least the same priority as the previous one.
    // - A layer should get more priority than the previous one, but this may be impossible if there are too many layers.
    //   In that case, the last layers will get the maximum priority for layers, RuleSet::cascadeLayerPriorityForUnlayered - 1.
    for (unsigned i = 0; i < layerCount; ++i) {
        auto priority = std::min<unsigned>(i + RuleSet::cascadeLayerPriorityForPresentationalHints + 1, RuleSet::cascadeLayerPriorityForUnlayered - 1);
        m_ruleSet->cascadeLayerForIdentifier(layersInPriorityOrder[i]).priority = priority;
    }
}

void RuleSetBuilder::addMutatingRulesToResolver()
{
    if (!m_resolver)
        return;

    auto compareLayers = [&](const auto& a, const auto& b) {
        auto aPriority = m_ruleSet->cascadeLayerPriorityForIdentifier(a.layerIdentifier);
        auto bPriority = m_ruleSet->cascadeLayerPriorityForIdentifier(b.layerIdentifier);
        return aPriority < bPriority;
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
        if (is<StyleRuleFontFeatureValues>(rule)) {
            m_resolver->document().fontSelector().addFontFeatureValuesRule(downcast<StyleRuleFontFeatureValues>(rule.get()));
            m_resolver->invalidateMatchedDeclarationsCache();
            continue;
        }
        if (is<StyleRuleKeyframes>(rule)) {
            m_resolver->addKeyframeStyle(downcast<StyleRuleKeyframes>(rule.get()));
            continue;
        }
        if (is<StyleRuleCounterStyle>(rule)) {
            if (m_resolver->scopeType() == Resolver::ScopeType::ShadowTree)
                continue;
            auto& registry = m_resolver->document().styleScope().counterStyleRegistry();
            registry.addCounterStyle(downcast<StyleRuleCounterStyle>(rule.get()).descriptors());
            // FIXME: we probably need a cache solultion like fontSelector (invalidateMatchedDeclarations)
            // KeyFrame does it differently, search for keyframesRuleDidChange. (rdar://103018993)
            continue;
        }
        if (is<StyleRuleProperty>(rule)) {
            // "A @property is invalid if it occurs in a stylesheet inside of a shadow tree, and must be ignored."
            // https://drafts.css-houdini.org/css-properties-values-api/#at-property-rule
            if (m_resolver->scopeType() == Resolver::ScopeType::ShadowTree)
                continue;
            auto& registry = m_resolver->document().styleScope().customPropertyRegistry();
            registry.registerFromStylesheet(downcast<StyleRuleProperty>(rule.get()).descriptor());
            continue;
        }
    }
}

void RuleSetBuilder::updateDynamicMediaQueries()
{
    if (m_mediaQueryCollector.allDynamicDependencies.contains(MQ::MediaQueryDynamicDependency::Viewport))
        m_ruleSet->m_hasViewportDependentMediaQueries = true;

    if (!m_mediaQueryCollector.dynamicMediaQueryRules.isEmpty()) {
        auto firstNewIndex = m_ruleSet->m_dynamicMediaQueryRules.size();
        m_ruleSet->m_dynamicMediaQueryRules.appendVector(WTFMove(m_mediaQueryCollector.dynamicMediaQueryRules));

        // Set the initial values.
        m_ruleSet->evaluateDynamicMediaQueryRules(m_mediaQueryCollector.evaluator, firstNewIndex);
    }
}

RuleSetBuilder::MediaQueryCollector::~MediaQueryCollector() = default;

bool RuleSetBuilder::MediaQueryCollector::pushAndEvaluate(const MQ::MediaQueryList& mediaQueries)
{
    if (mediaQueries.isEmpty())
        return true;

    auto dynamicDependencies = evaluator.collectDynamicDependencies(mediaQueries);

    allDynamicDependencies.add(dynamicDependencies);

    if (!dynamicDependencies.isEmpty()) {
        dynamicContextStack.append({ mediaQueries });
        if (collectDynamic)
            return true;
    }

    return evaluator.evaluate(mediaQueries);
}

void RuleSetBuilder::MediaQueryCollector::pop(const MQ::MediaQueryList& mediaQueries)
{
    if (dynamicContextStack.isEmpty() || &dynamicContextStack.last().queries != &mediaQueries)
        return;

    if (!dynamicContextStack.last().affectedRulePositions.isEmpty() || !collectDynamic) {
        RuleSet::DynamicMediaQueryRules rules;
        rules.mediaQueries.reserveCapacity(rules.mediaQueries.size() + dynamicContextStack.size());
        for (auto& context : dynamicContextStack)
            rules.mediaQueries.uncheckedAppend(context.queries);

        if (collectDynamic) {
            rules.affectedRulePositions.appendVector(dynamicContextStack.last().affectedRulePositions);
            rules.affectedRules = copyToVector(dynamicContextStack.last().affectedRules);
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
    context.affectedRules.add(ruleData.styleRule());
}

}
}
