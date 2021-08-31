/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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
#include "RuleSet.h"

#include "CSSFontSelector.h"
#include "CSSKeyframesRule.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "HTMLNames.h"
#include "MediaQueryEvaluator.h"
#include "SecurityOrigin.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"

namespace WebCore {
namespace Style {

using namespace HTMLNames;

RuleSet::RuleSet() = default;

RuleSet::~RuleSet() = default;

void RuleSet::addToRuleSet(const AtomString& key, AtomRuleMap& map, const RuleData& ruleData)
{
    if (key.isNull())
        return;
    auto& rules = map.add(key, nullptr).iterator->value;
    if (!rules)
        rules = makeUnique<RuleDataVector>();
    rules->append(ruleData);
}

static unsigned rulesCountForName(const RuleSet::AtomRuleMap& map, const AtomString& name)
{
    if (const auto* rules = map.get(name))
        return rules->size();
    return 0;
}

static bool isHostSelectorMatchingInShadowTree(const CSSSelector& startSelector)
{
    auto* leftmostSelector = &startSelector;
    bool hasDescendantOrChildRelation = false;
    while (auto* previous = leftmostSelector->tagHistory()) {
        hasDescendantOrChildRelation = leftmostSelector->hasDescendantOrChildRelation();
        leftmostSelector = previous;
    }
    if (!hasDescendantOrChildRelation)
        return false;

    return leftmostSelector->match() == CSSSelector::PseudoClass && leftmostSelector->pseudoClassType() == CSSSelector::PseudoClassHost;
}

void RuleSet::addRule(const StyleRule& rule, unsigned selectorIndex, unsigned selectorListIndex, unsigned cascadeLayerIdentifier, MediaQueryCollector* mediaQueryCollector)
{
    RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleCount++);

    if (cascadeLayerIdentifier) {
        auto oldSize = m_cascadeLayerIdentifierForRulePosition.size();
        m_cascadeLayerIdentifierForRulePosition.grow(m_ruleCount);
        std::fill(m_cascadeLayerIdentifierForRulePosition.begin() + oldSize, m_cascadeLayerIdentifierForRulePosition.end(), 0);
        m_cascadeLayerIdentifierForRulePosition.last() = cascadeLayerIdentifier;
    }

    m_features.collectFeatures(ruleData);

    if (mediaQueryCollector)
        mediaQueryCollector->addRuleIfNeeded(ruleData);

    unsigned classBucketSize = 0;
    const CSSSelector* idSelector = nullptr;
    const CSSSelector* tagSelector = nullptr;
    const CSSSelector* classSelector = nullptr;
    const CSSSelector* linkSelector = nullptr;
    const CSSSelector* focusSelector = nullptr;
    const CSSSelector* hostPseudoClassSelector = nullptr;
    const CSSSelector* customPseudoElementSelector = nullptr;
    const CSSSelector* slottedPseudoElementSelector = nullptr;
    const CSSSelector* partPseudoElementSelector = nullptr;
#if ENABLE(VIDEO)
    const CSSSelector* cuePseudoElementSelector = nullptr;
#endif
    const CSSSelector* selector = ruleData.selector();
    do {
        switch (selector->match()) {
        case CSSSelector::Id:
            idSelector = selector;
            break;
        case CSSSelector::Class: {
            auto& className = selector->value();
            if (!classSelector) {
                classSelector = selector;
                classBucketSize = rulesCountForName(m_classRules, className);
            } else if (classBucketSize) {
                unsigned newClassBucketSize = rulesCountForName(m_classRules, className);
                if (newClassBucketSize < classBucketSize) {
                    classSelector = selector;
                    classBucketSize = newClassBucketSize;
                }
            }
            break;
        }
        case CSSSelector::Tag:
            if (selector->tagQName().localName() != starAtom())
                tagSelector = selector;
            break;
        case CSSSelector::PseudoElement:
            switch (selector->pseudoElementType()) {
            case CSSSelector::PseudoElementWebKitCustom:
            case CSSSelector::PseudoElementWebKitCustomLegacyPrefixed:
                customPseudoElementSelector = selector;
                break;
            case CSSSelector::PseudoElementSlotted:
                slottedPseudoElementSelector = selector;
                break;
            case CSSSelector::PseudoElementPart:
                partPseudoElementSelector = selector;
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoElementCue:
                cuePseudoElementSelector = selector;
                break;
#endif
            default:
                break;
            }
            break;
        case CSSSelector::PseudoClass:
            switch (selector->pseudoClassType()) {
            case CSSSelector::PseudoClassLink:
            case CSSSelector::PseudoClassVisited:
            case CSSSelector::PseudoClassAnyLink:
            case CSSSelector::PseudoClassAnyLinkDeprecated:
                linkSelector = selector;
                break;
            case CSSSelector::PseudoClassDirectFocus:
            case CSSSelector::PseudoClassFocus:
                focusSelector = selector;
                break;
            case CSSSelector::PseudoClassHost:
                hostPseudoClassSelector = selector;
                break;
            default:
                break;
            }
            break;
        case CSSSelector::Unknown:
        case CSSSelector::Exact:
        case CSSSelector::Set:
        case CSSSelector::List:
        case CSSSelector::Hyphen:
        case CSSSelector::Contain:
        case CSSSelector::Begin:
        case CSSSelector::End:
        case CSSSelector::PagePseudoClass:
            break;
        }
        if (selector->relation() != CSSSelector::Subselector)
            break;
        selector = selector->tagHistory();
    } while (selector);

#if ENABLE(VIDEO)
    if (cuePseudoElementSelector) {
        m_cuePseudoRules.append(ruleData);
        return;
    }
#endif

    if (slottedPseudoElementSelector) {
        // ::slotted pseudo elements work accross shadow boundary making filtering difficult.
        ruleData.disableSelectorFiltering();
        m_slottedPseudoElementRules.append(ruleData);
        return;
    }

    if (partPseudoElementSelector) {
        // Filtering doesn't work accross shadow boundaries.
        ruleData.disableSelectorFiltering();
        m_partPseudoElementRules.append(ruleData);
        return;
    }

    if (customPseudoElementSelector) {
        // FIXME: Custom pseudo elements are handled by the shadow tree's selector filter. It doesn't know about the main DOM.
        ruleData.disableSelectorFiltering();

        auto* nextSelector = customPseudoElementSelector->tagHistory();
        if (nextSelector && nextSelector->match() == CSSSelector::PseudoElement && nextSelector->pseudoElementType() == CSSSelector::PseudoElementPart) {
            // Handle selectors like ::part(foo)::placeholder with the part codepath.
            m_partPseudoElementRules.append(ruleData);
            return;
        }

        addToRuleSet(customPseudoElementSelector->value(), m_shadowPseudoElementRules, ruleData);
        return;
    }

    if (!m_hasHostPseudoClassRulesMatchingInShadowTree)
        m_hasHostPseudoClassRulesMatchingInShadowTree = isHostSelectorMatchingInShadowTree(*ruleData.selector());

    if (hostPseudoClassSelector) {
        m_hostPseudoClassRules.append(ruleData);
        return;
    }

    if (idSelector) {
        addToRuleSet(idSelector->value(), m_idRules, ruleData);
        return;
    }

    if (classSelector) {
        addToRuleSet(classSelector->value(), m_classRules, ruleData);
        return;
    }

    if (linkSelector) {
        m_linkPseudoClassRules.append(ruleData);
        return;
    }

    if (focusSelector) {
        m_focusPseudoClassRules.append(ruleData);
        return;
    }

    if (tagSelector) {
        addToRuleSet(tagSelector->tagQName().localName(), m_tagLocalNameRules, ruleData);
        addToRuleSet(tagSelector->tagLowercaseLocalName(), m_tagLowercaseLocalNameRules, ruleData);
        return;
    }

    // If we didn't find a specialized map to stick it in, file under universal rules.
    m_universalRules.append(ruleData);
}

void RuleSet::addPageRule(StyleRulePage& rule)
{
    m_pageRules.append(&rule);
}

void RuleSet::addRulesFromSheet(const StyleSheetContents& sheet, const MediaQueryEvaluator& evaluator)
{
    Builder builder { *this, MediaQueryCollector { evaluator } };
    builder.addRulesFromSheet(sheet);

    if (m_autoShrinkToFitEnabled)
        shrinkToFit();
}

void RuleSet::addRulesFromSheet(const StyleSheetContents& sheet, const MediaQuerySet* sheetQuery, const MediaQueryEvaluator& evaluator, Style::Resolver& resolver)
{
    auto canUseDynamicMediaQueryResolution = [&] {
        Builder builder { *this, MediaQueryCollector { evaluator, true }, nullptr, Builder::Mode::ResolverMutationScan };
        if (builder.mediaQueryCollector.pushAndEvaluate(sheetQuery))
            builder.addRulesFromSheet(sheet);
        builder.mediaQueryCollector.pop(sheetQuery);
        return !builder.mediaQueryCollector.didMutateResolverWithinDynamicMediaQuery;
    }();

    Builder builder { *this, MediaQueryCollector { evaluator, canUseDynamicMediaQueryResolution }, &resolver };

    if (builder.mediaQueryCollector.pushAndEvaluate(sheetQuery))
        builder.addRulesFromSheet(sheet);
    builder.mediaQueryCollector.pop(sheetQuery);

    m_hasViewportDependentMediaQueries = builder.mediaQueryCollector.hasViewportDependentMediaQueries;

    if (!builder.mediaQueryCollector.dynamicMediaQueryRules.isEmpty()) {
        auto firstNewIndex = m_dynamicMediaQueryRules.size();
        m_dynamicMediaQueryRules.appendVector(WTFMove(builder.mediaQueryCollector.dynamicMediaQueryRules));

        // Set the initial values.
        evaluateDynamicMediaQueryRules(evaluator, firstNewIndex);
    }

    if (m_autoShrinkToFitEnabled)
        shrinkToFit();
}

void RuleSet::Builder::addChildRules(const Vector<RefPtr<StyleRuleBase>>& rules)
{
    for (auto& rule : rules) {
        if (mode == Mode::ResolverMutationScan && mediaQueryCollector.didMutateResolverWithinDynamicMediaQuery)
            break;

        if (is<StyleRule>(*rule)) {
            if (mode == Mode::Normal)
                addStyleRule(downcast<StyleRule>(*rule));
            continue;
        }
        if (is<StyleRulePage>(*rule)) {
            if (mode == Mode::Normal)
                ruleSet->addPageRule(downcast<StyleRulePage>(*rule));
            continue;
        }
        if (is<StyleRuleMedia>(*rule)) {
            auto& mediaRule = downcast<StyleRuleMedia>(*rule);
            if (mediaQueryCollector.pushAndEvaluate(&mediaRule.mediaQueries()))
                addChildRules(mediaRule.childRules());
            mediaQueryCollector.pop(&mediaRule.mediaQueries());
            continue;
        }
        if (is<StyleRuleLayer>(*rule)) {
            auto& layerRule = downcast<StyleRuleLayer>(*rule);
            if (layerRule.isList()) {
                // List syntax that just registers the layers.
                for (auto& name : layerRule.nameList()) {
                    pushCascadeLayer(name);
                    popCascadeLayer(name);
                }
                continue;
            }
            // Block syntax.
            pushCascadeLayer(layerRule.name());
            addChildRules(layerRule.childRules());
            popCascadeLayer(layerRule.name());
            continue;
        }
        if (is<StyleRuleFontFace>(*rule)) {
            // Add this font face to our set.
            if (resolver) {
                resolver->document().fontSelector().addFontFaceRule(downcast<StyleRuleFontFace>(*rule.get()), false);
                resolver->invalidateMatchedDeclarationsCache();
            }
            mediaQueryCollector.didMutateResolver();
            continue;
        }
        if (is<StyleRuleKeyframes>(*rule)) {
            if (resolver)
                resolver->addKeyframeStyle(downcast<StyleRuleKeyframes>(*rule));
            mediaQueryCollector.didMutateResolver();
            continue;
        }
        if (is<StyleRuleSupports>(*rule) && downcast<StyleRuleSupports>(*rule).conditionIsSupported()) {
            addChildRules(downcast<StyleRuleSupports>(*rule).childRules());
            continue;
        }
    }
}

void RuleSet::Builder::addRulesFromSheet(const StyleSheetContents& sheet)
{
    for (auto& rule : sheet.importRules()) {
        if (!rule->styleSheet())
            continue;

        if (mediaQueryCollector.pushAndEvaluate(rule->mediaQueries()))
            addRulesFromSheet(*rule->styleSheet());
        mediaQueryCollector.pop(rule->mediaQueries());
    }

    addChildRules(sheet.childRules());
}

RuleSet::Builder::~Builder()
{
    if (mode == Mode::Normal && !cascadeLayerIdentifierMap.isEmpty())
        updateCascadeLayerOrder();
}

void RuleSet::Builder::addStyleRule(const StyleRule& rule)
{
    auto& selectorList = rule.selectorList();
    if (selectorList.isEmpty())
        return;
    unsigned selectorListIndex = 0;
    for (size_t selectorIndex = 0; selectorIndex != notFound; selectorIndex = selectorList.indexOfNextSelectorAfter(selectorIndex))
        ruleSet->addRule(rule, selectorIndex, selectorListIndex++, currentCascadeLayerIdentifier, &mediaQueryCollector);
}

void RuleSet::Builder::pushCascadeLayer(const CascadeLayerName& name)
{
    if (mode != Mode::Normal)
        return;

    if (cascadeLayerIdentifierMap.isEmpty() && !ruleSet->m_cascadeLayers.isEmpty()) {
        // For incremental build, reconstruct the name->identifier map.
        CascadeLayerIdentifier identifier = 0;
        for (auto& layer : ruleSet->m_cascadeLayers)
            cascadeLayerIdentifierMap.add(layer.resolvedName, ++identifier);
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
        resolvedCascadeLayerName.append(nameSegment);
        currentCascadeLayerIdentifier = cascadeLayerIdentifierMap.ensure(resolvedCascadeLayerName, [&] {
            // Previously unseen layer.
            ruleSet->m_cascadeLayers.append({ resolvedCascadeLayerName, currentCascadeLayerIdentifier });
            return ruleSet->m_cascadeLayers.size();
        }).iterator->value;
    }
}

void RuleSet::Builder::popCascadeLayer(const CascadeLayerName& name)
{
    if (mode != Mode::Normal)
        return;

    for (auto size = name.isEmpty() ? 1 : name.size(); size--;) {
        resolvedCascadeLayerName.removeLast();
        currentCascadeLayerIdentifier = ruleSet->cascadeLayerForIdentifier(currentCascadeLayerIdentifier).parentIdentifier;
    }
}

void RuleSet::Builder::updateCascadeLayerOrder()
{
    auto compare = [&](CascadeLayerIdentifier a, CascadeLayerIdentifier b) {
        while (a && b) {
            // Identifiers are in parse order which almost corresponds to the layer priority order.
            // The only exception is when a sublayer gets added to a layer after adding other non-sublayers.
            // To resolve this we need look for a shared ancestor layer.
            auto aParent = ruleSet->cascadeLayerForIdentifier(a).parentIdentifier;
            auto bParent = ruleSet->cascadeLayerForIdentifier(b).parentIdentifier;
            if (aParent == bParent || aParent == b || bParent == a)
                break;
            if (aParent > bParent)
                a = aParent;
            else
                b = bParent;
        }
        return a < b;
    };

    Vector<CascadeLayerIdentifier> orderVector;
    auto layerCount = ruleSet->m_cascadeLayers.size();
    orderVector.reserveInitialCapacity(layerCount);
    for (CascadeLayerIdentifier identifier = 1; identifier <= layerCount; ++identifier)
        orderVector.uncheckedAppend(identifier);

    std::sort(orderVector.begin(), orderVector.end(), compare);

    for (unsigned i = 0; i < orderVector.size(); ++i)
        ruleSet->cascadeLayerForIdentifier(orderVector[i]).order = i + 1;
}

template<typename Function>
void RuleSet::traverseRuleDatas(Function&& function)
{
    auto traverseVector = [&](auto& vector) {
        for (auto& ruleData : vector)
            function(ruleData);
    };

    auto traverseMap = [&](auto& map) {
        for (auto& ruleDatas : map.values())
            traverseVector(*ruleDatas);
    };

    traverseMap(m_idRules);
    traverseMap(m_classRules);
    traverseMap(m_tagLocalNameRules);
    traverseMap(m_tagLowercaseLocalNameRules);
    traverseMap(m_shadowPseudoElementRules);
    traverseVector(m_linkPseudoClassRules);
#if ENABLE(VIDEO)
    traverseVector(m_cuePseudoRules);
#endif
    traverseVector(m_hostPseudoClassRules);
    traverseVector(m_slottedPseudoElementRules);
    traverseVector(m_partPseudoElementRules);
    traverseVector(m_focusPseudoClassRules);
    traverseVector(m_universalRules);
}

std::optional<DynamicMediaQueryEvaluationChanges> RuleSet::evaluateDynamicMediaQueryRules(const MediaQueryEvaluator& evaluator)
{
    auto collectedChanges = evaluateDynamicMediaQueryRules(evaluator, 0);

    if (collectedChanges.requiredFullReset)
        return { { DynamicMediaQueryEvaluationChanges::Type::ResetStyle } };

    if (collectedChanges.changedQueryIndexes.isEmpty())
        return { };

    auto& ruleSet = m_mediaQueryInvalidationRuleSetCache.ensure(collectedChanges.changedQueryIndexes, [&] {
        auto ruleSet = RuleSet::create();
        for (auto* featureVector : collectedChanges.ruleFeatures) {
            for (auto& feature : *featureVector)
                ruleSet->addRule(*feature.styleRule, feature.selectorIndex, feature.selectorListIndex);
        }
        ruleSet->shrinkToFit();
        return ruleSet;
    }).iterator->value;

    return { { DynamicMediaQueryEvaluationChanges::Type::InvalidateStyle, { ruleSet.copyRef() } } };
}

RuleSet::CollectedMediaQueryChanges RuleSet::evaluateDynamicMediaQueryRules(const MediaQueryEvaluator& evaluator, size_t startIndex)
{
    CollectedMediaQueryChanges collectedChanges;

    HashMap<size_t, bool, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>> affectedRulePositionsAndResults;

    for (size_t i = startIndex; i < m_dynamicMediaQueryRules.size(); ++i) {
        auto& dynamicRules = m_dynamicMediaQueryRules[i];
        bool result = true;
        for (auto& set : dynamicRules.mediaQuerySets) {
            if (!evaluator.evaluate(set.get())) {
                result = false;
                break;
            }
        }

        if (result != dynamicRules.result) {
            dynamicRules.result = result;

            if (dynamicRules.requiresFullReset) {
                collectedChanges.requiredFullReset = true;
                continue;
            }

            for (auto position : dynamicRules.affectedRulePositions)
                affectedRulePositionsAndResults.add(position, result);

            collectedChanges.changedQueryIndexes.append(i);
            collectedChanges.ruleFeatures.append(&dynamicRules.ruleFeatures);
        }
    }

    if (affectedRulePositionsAndResults.isEmpty())
        return collectedChanges;

    traverseRuleDatas([&](RuleData& ruleData) {
        auto it = affectedRulePositionsAndResults.find(ruleData.position());
        if (it == affectedRulePositionsAndResults.end())
            return;
        ruleData.setEnabled(it->value);
    });

    return collectedChanges;
}

static inline void shrinkMapVectorsToFit(RuleSet::AtomRuleMap& map)
{
    for (auto& vector : map.values())
        vector->shrinkToFit();
}

static inline void shrinkDynamicRules(Vector<RuleSet::DynamicMediaQueryRules>& dynamicRules)
{
    for (auto& rule : dynamicRules)
        rule.shrinkToFit();

    dynamicRules.shrinkToFit();
}

void RuleSet::shrinkToFit()
{
    shrinkMapVectorsToFit(m_idRules);
    shrinkMapVectorsToFit(m_classRules);
    shrinkMapVectorsToFit(m_tagLocalNameRules);
    shrinkMapVectorsToFit(m_tagLowercaseLocalNameRules);
    shrinkMapVectorsToFit(m_shadowPseudoElementRules);

    m_linkPseudoClassRules.shrinkToFit();
#if ENABLE(VIDEO)
    m_cuePseudoRules.shrinkToFit();
#endif
    m_hostPseudoClassRules.shrinkToFit();
    m_slottedPseudoElementRules.shrinkToFit();
    m_partPseudoElementRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();

    m_pageRules.shrinkToFit();
    m_features.shrinkToFit();

    shrinkDynamicRules(m_dynamicMediaQueryRules);

    m_cascadeLayers.shrinkToFit();
    m_cascadeLayerIdentifierForRulePosition.shrinkToFit();
}

RuleSet::MediaQueryCollector::~MediaQueryCollector() = default;

bool RuleSet::MediaQueryCollector::pushAndEvaluate(const MediaQuerySet* set)
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

void RuleSet::MediaQueryCollector::pop(const MediaQuerySet* set)
{
    if (!set || dynamicContextStack.isEmpty() || set != &dynamicContextStack.last().set.get())
        return;

    if (!dynamicContextStack.last().affectedRulePositions.isEmpty() || !collectDynamic) {
        DynamicMediaQueryRules rules;
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

void RuleSet::MediaQueryCollector::didMutateResolver()
{
    if (dynamicContextStack.isEmpty())
        return;
    didMutateResolverWithinDynamicMediaQuery = true;
}

void RuleSet::MediaQueryCollector::addRuleIfNeeded(const RuleData& ruleData)
{
    if (dynamicContextStack.isEmpty())
        return;

    auto& context = dynamicContextStack.last();
    context.affectedRulePositions.append(ruleData.position());
    context.ruleFeatures.append({ ruleData });
}

} // namespace Style
} // namespace WebCore
