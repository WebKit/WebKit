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
#include "ViewportStyleResolver.h"

#if ENABLE(VIDEO_TRACK)
#include "TextTrackCue.h"
#endif

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

void RuleSet::addRule(const StyleRule& rule, unsigned selectorIndex, unsigned selectorListIndex, MediaQueryCollector* mediaQueryCollector)
{
    RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleCount++);

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
#if ENABLE(VIDEO_TRACK)
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
#if ENABLE(VIDEO_TRACK)
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

#if ENABLE(VIDEO_TRACK)
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

void RuleSet::addChildRules(const Vector<RefPtr<StyleRuleBase>>& rules, MediaQueryCollector& mediaQueryCollector, Resolver* resolver, AddRulesMode mode)
{
    for (auto& rule : rules) {
        if (mode == AddRulesMode::ResolverMutationScan && mediaQueryCollector.didMutateResolverWithinDynamicMediaQuery)
            break;

        if (is<StyleRule>(*rule)) {
            if (mode == AddRulesMode::Normal)
                addStyleRule(downcast<StyleRule>(*rule), mediaQueryCollector);
            continue;
        }
        if (is<StyleRulePage>(*rule)) {
            if (mode == AddRulesMode::Normal)
                addPageRule(downcast<StyleRulePage>(*rule));
            continue;
        }
        if (is<StyleRuleMedia>(*rule)) {
            auto& mediaRule = downcast<StyleRuleMedia>(*rule);
            if (mediaQueryCollector.pushAndEvaluate(mediaRule.mediaQueries())) {
                addChildRules(mediaRule.childRules(), mediaQueryCollector, resolver, mode);
                mediaQueryCollector.pop(mediaRule.mediaQueries());
            }
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
            addChildRules(downcast<StyleRuleSupports>(*rule).childRules(), mediaQueryCollector, resolver, mode);
            continue;
        }
#if ENABLE(CSS_DEVICE_ADAPTATION)
        if (is<StyleRuleViewport>(*rule)) {
            if (resolver)
                resolver->viewportStyleResolver()->addViewportRule(downcast<StyleRuleViewport>(rule.get()));
            mediaQueryCollector.didMutateResolver();
            continue;
        }
#endif
    }
}

void RuleSet::addRulesFromSheet(StyleSheetContents& sheet, const MediaQueryEvaluator& evaluator)
{
    auto mediaQueryCollector = MediaQueryCollector { evaluator };
    addRulesFromSheet(sheet, mediaQueryCollector, nullptr, AddRulesMode::Normal);
}

void RuleSet::addRulesFromSheet(StyleSheetContents& sheet, MediaQuerySet* sheetQuery, const MediaQueryEvaluator& evaluator, Style::Resolver& resolver)
{
    auto canUseDynamicMediaQueryResolution = [&] {
        auto mediaQueryCollector = MediaQueryCollector { evaluator, true };
        if (mediaQueryCollector.pushAndEvaluate(sheetQuery))
            addRulesFromSheet(sheet, mediaQueryCollector, nullptr, AddRulesMode::ResolverMutationScan);
        return !mediaQueryCollector.didMutateResolverWithinDynamicMediaQuery;
    }();

    auto mediaQueryCollector = MediaQueryCollector { evaluator, canUseDynamicMediaQueryResolution };

    if (mediaQueryCollector.pushAndEvaluate(sheetQuery)) {
        addRulesFromSheet(sheet, mediaQueryCollector, &resolver, AddRulesMode::Normal);
        mediaQueryCollector.pop(sheetQuery);
    }

    m_hasViewportDependentMediaQueries = mediaQueryCollector.hasViewportDependentMediaQueries;

    if (mediaQueryCollector.dynamicMediaQueryRules.isEmpty())
        return;

    auto firstNewIndex = m_dynamicMediaQueryRules.size();
    m_dynamicMediaQueryRules.appendVector(WTFMove(mediaQueryCollector.dynamicMediaQueryRules));

    // Set the initial values.
    evaluteDynamicMediaQueryRules(evaluator, firstNewIndex);
}

void RuleSet::addRulesFromSheet(StyleSheetContents& sheet, MediaQueryCollector& mediaQueryCollector, Resolver* resolver, AddRulesMode mode)
{
    for (auto& rule : sheet.importRules()) {
        if (!rule->styleSheet())
            continue;

        if (mediaQueryCollector.pushAndEvaluate(rule->mediaQueries())) {
            addRulesFromSheet(*rule->styleSheet(), mediaQueryCollector, resolver, mode);
            mediaQueryCollector.pop(rule->mediaQueries());
        }
    }

    addChildRules(sheet.childRules(), mediaQueryCollector, resolver, mode);

    if (m_autoShrinkToFitEnabled && mode == AddRulesMode::Normal)
        shrinkToFit();
}

void RuleSet::addStyleRule(const StyleRule& rule, MediaQueryCollector& mediaQueryCollector)
{
    unsigned selectorListIndex = 0;
    for (size_t selectorIndex = 0; selectorIndex != notFound; selectorIndex = rule.selectorList().indexOfNextSelectorAfter(selectorIndex))
        addRule(rule, selectorIndex, selectorListIndex++, &mediaQueryCollector);
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
#if ENABLE(VIDEO_TRACK)
    traverseVector(m_cuePseudoRules);
#endif
    traverseVector(m_hostPseudoClassRules);
    traverseVector(m_slottedPseudoElementRules);
    traverseVector(m_partPseudoElementRules);
    traverseVector(m_focusPseudoClassRules);
    traverseVector(m_universalRules);
}

Optional<DynamicMediaQueryEvaluationChanges> RuleSet::evaluteDynamicMediaQueryRules(const MediaQueryEvaluator& evaluator)
{
    auto collectedChanges = evaluteDynamicMediaQueryRules(evaluator, 0);

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
        return ruleSet;
    }).iterator->value;

    return { { DynamicMediaQueryEvaluationChanges::Type::InvalidateStyle, { ruleSet.copyRef() } } };
}

RuleSet::CollectedMediaQueryChanges RuleSet::evaluteDynamicMediaQueryRules(const MediaQueryEvaluator& evaluator, size_t startIndex)
{
    CollectedMediaQueryChanges collectedChanges;

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
                return collectedChanges;
            }

            traverseRuleDatas([&](RuleData& ruleData) {
                if (!dynamicRules.affectedRulePositions.contains(ruleData.position()))
                    return;
                ruleData.setEnabled(result);
            });

            collectedChanges.changedQueryIndexes.append(i);
            collectedChanges.ruleFeatures.append(&dynamicRules.ruleFeatures);
        }
    }

    return collectedChanges;
}

bool RuleSet::hasShadowPseudoElementRules() const
{
    if (!m_shadowPseudoElementRules.isEmpty())
        return true;
#if ENABLE(VIDEO_TRACK)
    if (!m_cuePseudoRules.isEmpty())
        return true;
#endif
    return false;
}

static inline void shrinkMapVectorsToFit(RuleSet::AtomRuleMap& map)
{
    for (auto& vector : map.values())
        vector->shrinkToFit();
}

void RuleSet::shrinkToFit()
{
    shrinkMapVectorsToFit(m_idRules);
    shrinkMapVectorsToFit(m_classRules);
    shrinkMapVectorsToFit(m_tagLocalNameRules);
    shrinkMapVectorsToFit(m_tagLowercaseLocalNameRules);
    shrinkMapVectorsToFit(m_shadowPseudoElementRules);
    m_linkPseudoClassRules.shrinkToFit();
#if ENABLE(VIDEO_TRACK)
    m_cuePseudoRules.shrinkToFit();
#endif
    m_hostPseudoClassRules.shrinkToFit();
    m_slottedPseudoElementRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();
    m_pageRules.shrinkToFit();
    m_features.shrinkToFit();
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

    if (!result)
        return false;

    if (!dynamicResults.isEmpty())
        dynamicContextStack.append({ *set });

    return true;
}

void RuleSet::MediaQueryCollector::pop(const MediaQuerySet* set)
{
    if (!set || dynamicContextStack.isEmpty() || set != &dynamicContextStack.last().set.get())
        return;

    if (!dynamicContextStack.last().affectedRulePositions.isEmpty()) {
        DynamicMediaQueryRules rules;
        for (auto& context : dynamicContextStack)
            rules.mediaQuerySets.append(context.set.get());

        if (collectDynamic) {
            auto& toAdd = dynamicContextStack.last().affectedRulePositions;
            rules.affectedRulePositions.add(toAdd.begin(), toAdd.end());

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
