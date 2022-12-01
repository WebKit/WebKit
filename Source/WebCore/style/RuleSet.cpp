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
#include "RuleSet.h"

#include "CSSFontSelector.h"
#include "CSSKeyframesRule.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CommonAtomStrings.h"
#include "HTMLNames.h"
#include "MediaQueryEvaluator.h"
#include "RuleSetBuilder.h"
#include "SVGElement.h"
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
    bool hasOnlyOneCompound = true;
    bool hasHostInLastCompound = false;
    for (auto* selector = &startSelector; selector; selector = selector->tagHistory()) {
        if (selector->match() == CSSSelector::PseudoClass && selector->pseudoClassType() == CSSSelector::PseudoClassHost)
            hasHostInLastCompound = true;
        if (selector->tagHistory() && selector->relation() != CSSSelector::Subselector) {
            hasOnlyOneCompound = false;
            hasHostInLastCompound = false;
        }
    }
    return !hasOnlyOneCompound && hasHostInLastCompound;
}

static bool shouldHaveBucketForAttributeName(const CSSSelector& attributeSelector)
{
    // Don't make buckets for lazy attributes since we don't want to synchronize.
    if (attributeSelector.attributeCanonicalLocalName() == HTMLNames::styleAttr->localName())
        return false;
    if (SVGElement::animatableAttributeForName(attributeSelector.attribute().localName()) != nullQName())
        return false;
    return true;
}

void RuleSet::addRule(const StyleRule& rule, unsigned selectorIndex, unsigned selectorListIndex)
{
    RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleCount);
    addRule(WTFMove(ruleData), 0, 0);
}

void RuleSet::addRule(RuleData&& ruleData, CascadeLayerIdentifier cascadeLayerIdentifier, ContainerQueryIdentifier containerQueryIdentifier)
{
    ASSERT(ruleData.position() == m_ruleCount);

    ++m_ruleCount;

    if (cascadeLayerIdentifier) {
        auto oldSize = m_cascadeLayerIdentifierForRulePosition.size();
        m_cascadeLayerIdentifierForRulePosition.grow(m_ruleCount);
        std::fill(m_cascadeLayerIdentifierForRulePosition.begin() + oldSize, m_cascadeLayerIdentifierForRulePosition.end(), 0);
        m_cascadeLayerIdentifierForRulePosition.last() = cascadeLayerIdentifier;
    }

    if (containerQueryIdentifier) {
        auto oldSize = m_containerQueryIdentifierForRulePosition.size();
        m_containerQueryIdentifierForRulePosition.grow(m_ruleCount);
        std::fill(m_containerQueryIdentifierForRulePosition.begin() + oldSize, m_containerQueryIdentifierForRulePosition.end(), 0);
        m_containerQueryIdentifierForRulePosition.last() = containerQueryIdentifier;
    }

    m_features.collectFeatures(ruleData);

    unsigned classBucketSize = 0;
    const CSSSelector* idSelector = nullptr;
    const CSSSelector* tagSelector = nullptr;
    const CSSSelector* classSelector = nullptr;
    const CSSSelector* attributeSelector = nullptr;
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
        case CSSSelector::Exact:
        case CSSSelector::Set:
        case CSSSelector::List:
        case CSSSelector::Hyphen:
        case CSSSelector::Contain:
        case CSSSelector::Begin:
        case CSSSelector::End:
            if (shouldHaveBucketForAttributeName(*selector))
                attributeSelector = selector;
            break;
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
            case CSSSelector::PseudoClassFocus:
            case CSSSelector::PseudoClassFocusVisible:
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
        case CSSSelector::PagePseudoClass:
            break;
        }
        if (selector->relation() != CSSSelector::Subselector)
            break;
        selector = selector->tagHistory();
    } while (selector);

    if (!m_hasHostPseudoClassRulesMatchingInShadowTree)
        m_hasHostPseudoClassRulesMatchingInShadowTree = isHostSelectorMatchingInShadowTree(*ruleData.selector());

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

    if (attributeSelector) {
        addToRuleSet(attributeSelector->attribute().localName(), m_attributeLocalNameRules, ruleData);
        addToRuleSet(attributeSelector->attributeCanonicalLocalName(), m_attributeCanonicalLocalNameRules, ruleData);
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
    traverseMap(m_attributeLocalNameRules);
    traverseMap(m_attributeCanonicalLocalNameRules);
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

std::optional<DynamicMediaQueryEvaluationChanges> RuleSet::evaluateDynamicMediaQueryRules(const MQ::MediaQueryEvaluator& evaluator)
{
    auto collectedChanges = evaluateDynamicMediaQueryRules(evaluator, 0);

    if (collectedChanges.requiredFullReset)
        return { { DynamicMediaQueryEvaluationChanges::Type::ResetStyle } };

    if (collectedChanges.changedQueryIndexes.isEmpty())
        return { };

    auto& ruleSet = m_mediaQueryInvalidationRuleSetCache.ensure(collectedChanges.changedQueryIndexes, [&] {
        auto ruleSet = RuleSet::create();
        RuleSetBuilder builder(ruleSet, MQ::MediaQueryEvaluator { screenAtom(), MQ::EvaluationResult::True });
        for (auto* rules : collectedChanges.affectedRules) {
            for (auto& rule : *rules)
                builder.addStyleRule(rule);
        }
        return ruleSet;
    }).iterator->value;

    return { { DynamicMediaQueryEvaluationChanges::Type::InvalidateStyle, { ruleSet.copyRef() } } };
}

RuleSet::CollectedMediaQueryChanges RuleSet::evaluateDynamicMediaQueryRules(const MQ::MediaQueryEvaluator& evaluator, size_t startIndex)
{
    CollectedMediaQueryChanges collectedChanges;

    HashMap<size_t, bool, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>> affectedRulePositionsAndResults;

    for (size_t i = startIndex; i < m_dynamicMediaQueryRules.size(); ++i) {
        auto& dynamicRules = m_dynamicMediaQueryRules[i];
        bool result = true;
        for (auto& queryList : dynamicRules.mediaQueries) {
            if (!evaluator.evaluate(queryList)) {
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
            collectedChanges.affectedRules.append(&dynamicRules.affectedRules);
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

void RuleSet::shrinkToFit()
{
    shrinkMapVectorsToFit(m_idRules);
    shrinkMapVectorsToFit(m_classRules);
    shrinkMapVectorsToFit(m_attributeLocalNameRules);
    shrinkMapVectorsToFit(m_attributeCanonicalLocalNameRules);
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

    for (auto& rule : m_dynamicMediaQueryRules)
        rule.shrinkToFit();
    m_dynamicMediaQueryRules.shrinkToFit();

    m_cascadeLayers.shrinkToFit();
    m_cascadeLayerIdentifierForRulePosition.shrinkToFit();
    m_containerQueries.shrinkToFit();
    m_containerQueryIdentifierForRulePosition.shrinkToFit();
    m_resolverMutatingRulesInLayers.shrinkToFit();
}

} // namespace Style
} // namespace WebCore
