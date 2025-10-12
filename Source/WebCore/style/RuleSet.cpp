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
#include "CSSPositionTryRule.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSViewTransitionRule.h"
#include "CommonAtomStrings.h"
#include "HTMLNames.h"
#include "MediaQueryEvaluator.h"
#include "MutableCSSSelector.h"
#include "RuleSetBuilder.h"
#include "SVGElement.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "UserAgentParts.h"
#include <ranges>

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

// FIXME: Maybe we can unify both following functions

static bool hasHostOrScopePseudoClassSubjectInSelectorList(const CSSSelectorList* selectorList)
{
    if (!selectorList)
        return false;

    for (auto& selector : *selectorList) {
        if (selector.isHostPseudoClass() || selector.isScopePseudoClass())
            return true;

        if (hasHostOrScopePseudoClassSubjectInSelectorList(selector.selectorList()))
            return true;
    }

    return false;
}

static bool isHostSelectorMatchingInShadowTree(const CSSSelector& startSelector)
{
    auto isHostSelectorMatchingInShadowTreeInSelectorList = [](const CSSSelectorList* selectorList) {
        if (!selectorList || selectorList->isEmpty())
            return false;
        for (auto& selector : *selectorList) {
            if (isHostSelectorMatchingInShadowTree(selector))
                return true;
        }
        return false;
    };

    bool hasOnlyOneCompound = true;
    bool hasHostInLastCompound = false;
    for (auto* selector = &startSelector; selector; selector = selector->precedingInComplexSelector()) {
        if (selector->match() == CSSSelector::Match::PseudoClass && selector->pseudoClass() == CSSSelector::PseudoClass::Host)
            hasHostInLastCompound = true;
        if (isHostSelectorMatchingInShadowTreeInSelectorList(selector->selectorList()))
            return true;
        if (selector->precedingInComplexSelector() && selector->relation() != CSSSelector::Relation::Subselector) {
            hasOnlyOneCompound = false;
            hasHostInLastCompound = false;
        }
    }
    return !hasOnlyOneCompound && hasHostInLastCompound;
}

static bool shouldHaveBucketForAttributeName(const CSSSelector& attributeSelector)
{
    // Don't make buckets for lazy attributes since we don't want to synchronize.
    if (attributeSelector.attribute().localNameLowercase() == HTMLNames::styleAttr->localName())
        return false;
    if (SVGElement::animatableAttributeForName(attributeSelector.attribute().localName()) != nullQName())
        return false;
    return true;
}

void RuleSet::addRule(const StyleRule& rule, unsigned selectorIndex, unsigned selectorListIndex)
{
    RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleCount, { });
    addRule(WTFMove(ruleData), 0, 0, 0);
}

void RuleSet::addRule(RuleData&& ruleData, CascadeLayerIdentifier cascadeLayerIdentifier, ContainerQueryIdentifier containerQueryIdentifier, ScopeRuleIdentifier scopeRuleIdentifier)
{
    ASSERT(ruleData.position() == m_ruleCount);

    ++m_ruleCount;

    auto storeIdentifier = [&](auto identifier, auto& container) {
        if (identifier) {
            auto oldSize = container.size();
            container.grow(m_ruleCount);
            auto newlyAllocated = container.mutableSpan().subspan(oldSize);
            std::ranges::fill(newlyAllocated, 0);
            container.last() = identifier;
        }
    };
    storeIdentifier(cascadeLayerIdentifier, m_cascadeLayerIdentifierForRulePosition);
    storeIdentifier(containerQueryIdentifier, m_containerQueryIdentifierForRulePosition);
    storeIdentifier(scopeRuleIdentifier, m_scopeRuleIdentifierForRulePosition);

    const auto& scopeRules = scopeRulesFor(ruleData);

    auto computeLinkMatchType = [&] {
        // General case: no @scope rule or current rule selector is not :scope.
        if (scopeRules.isEmpty() || !ruleData.selector()->hasScope())
            return SelectorChecker::determineLinkMatchType(ruleData.selector());
        // When current rule is :scope, we need to take into account the @scope selectors to determine the link match type.
        Ref scopeRule = scopeRules.last();
        return SelectorChecker::determineLinkMatchType(ruleData.selector(), scopeRule.ptr());
    };
    ruleData.setLinkMatchType(computeLinkMatchType());

    m_features.collectFeatures(ruleData, scopeRules);

    unsigned classBucketSize = 0;
    const CSSSelector* idSelector = nullptr;
    const CSSSelector* tagSelector = nullptr;
    const CSSSelector* classSelector = nullptr;
    const CSSSelector* attributeSelector = nullptr;
    const CSSSelector* linkSelector = nullptr;
    const CSSSelector* focusSelector = nullptr;
    const CSSSelector* rootElementSelector = nullptr;
    const CSSSelector* hostPseudoClassSelector = nullptr;
    const CSSSelector* customPseudoElementSelector = nullptr;
    const CSSSelector* slottedPseudoElementSelector = nullptr;
    const CSSSelector* partPseudoElementSelector = nullptr;
    const CSSSelector* namedPseudoElementSelector = nullptr;
#if ENABLE(VIDEO)
    const CSSSelector* cuePseudoElementSelector = nullptr;
#endif
    const CSSSelector* selector = ruleData.selector();
    do {
        switch (selector->match()) {
        case CSSSelector::Match::Id:
            idSelector = selector;
            break;
        case CSSSelector::Match::Class: {
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
        case CSSSelector::Match::Exact:
        case CSSSelector::Match::Set:
        case CSSSelector::Match::List:
        case CSSSelector::Match::Hyphen:
        case CSSSelector::Match::Contain:
        case CSSSelector::Match::Begin:
        case CSSSelector::Match::End:
            if (shouldHaveBucketForAttributeName(*selector))
                attributeSelector = selector;
            break;
        case CSSSelector::Match::Tag:
            if (selector->tagQName().localName() != starAtom())
                tagSelector = selector;
            break;
        case CSSSelector::Match::PseudoElement:
            switch (selector->pseudoElement()) {
            case CSSSelector::PseudoElement::UserAgentPart:
            case CSSSelector::PseudoElement::UserAgentPartLegacyAlias:
                customPseudoElementSelector = selector;
                break;
            case CSSSelector::PseudoElement::Slotted:
                slottedPseudoElementSelector = selector;
                break;
            case CSSSelector::PseudoElement::Part:
                partPseudoElementSelector = selector;
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoElement::Cue:
                cuePseudoElementSelector = selector;
                break;
#endif
            case CSSSelector::PseudoElement::ViewTransitionGroup:
            case CSSSelector::PseudoElement::ViewTransitionImagePair:
            case CSSSelector::PseudoElement::ViewTransitionOld:
            case CSSSelector::PseudoElement::ViewTransitionNew:
                if (selector->argumentList()->first() != starAtom())
                    namedPseudoElementSelector = selector;
                break;
            default:
                break;
            }
            break;
        case CSSSelector::Match::PseudoClass:
            switch (selector->pseudoClass()) {
            case CSSSelector::PseudoClass::Link:
            case CSSSelector::PseudoClass::Visited:
            case CSSSelector::PseudoClass::AnyLink:
                linkSelector = selector;
                break;
            case CSSSelector::PseudoClass::Focus:
            case CSSSelector::PseudoClass::FocusVisible:
                focusSelector = selector;
                break;
            case CSSSelector::PseudoClass::Host:
                hostPseudoClassSelector = selector;
                break;
            case CSSSelector::PseudoClass::Root:
                rootElementSelector = selector;
                break;
            case CSSSelector::PseudoClass::Scope:
                m_hasHostOrScopePseudoClassRulesInUniversalBucket = true;
                break;
            default:
                if (hasHostOrScopePseudoClassSubjectInSelectorList(selector->selectorList()))
                    m_hasHostOrScopePseudoClassRulesInUniversalBucket = true;
                break;
            }
            break;
        case CSSSelector::Match::Unknown:
        case CSSSelector::Match::ForgivingUnknown:
        case CSSSelector::Match::ForgivingUnknownNestContaining:
        case CSSSelector::Match::HasScope:
        case CSSSelector::Match::NestingParent:
        case CSSSelector::Match::PagePseudoClass:
            break;
        }
        // We only process the subject (rightmost compound selector).
        if (selector->relation() != CSSSelector::Relation::Subselector)
            break;
        selector = selector->precedingInComplexSelector();
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

        auto* nextSelector = customPseudoElementSelector->precedingInComplexSelector();
        if (nextSelector && nextSelector->match() == CSSSelector::Match::PseudoElement && nextSelector->pseudoElement() == CSSSelector::PseudoElement::Part) {
            // Handle selectors like ::part(foo)::placeholder with the part codepath.
            m_partPseudoElementRules.append(ruleData);
            return;
        }

        addToRuleSet(customPseudoElementSelector->value(), m_userAgentPartRules, ruleData);

#if ENABLE(VIDEO)
        // <https://w3c.github.io/webvtt/#the-cue-pseudo-element>
        // * 8.2.1. The ::cue pseudo-element:
        //     As a special exception, the properties corresponding to the background shorthand,
        //     when they would have been applied to the list of WebVTT Node Objects, must instead
        //     be applied to the WebVTT cue background box.
        // To implement this exception, clone rules whose selector matches the `::cue` (a.k.a,
        // `user-agent-part="cue"`), and replace the selector with one that matches the cue background
        // box (a.k.a. `user-agent-part="internal-cue-background"`).
        if (customPseudoElementSelector->value() == UserAgentParts::cue()
            && customPseudoElementSelector->argument() == nullAtom()) {
            std::unique_ptr cueBackgroundSelector = makeUnique<MutableCSSSelector>(*customPseudoElementSelector);
            cueBackgroundSelector->setMatch(CSSSelector::Match::PseudoElement);
            cueBackgroundSelector->setPseudoElement(CSSSelector::PseudoElement::UserAgentPart);
            cueBackgroundSelector->setValue(UserAgentParts::internalCueBackground());

            Ref cueBackgroundStyleRule = StyleRule::create(ruleData.styleRule().properties().immutableCopyIfNeeded(), ruleData.styleRule().hasDocumentSecurityOrigin(), CSSSelectorList { MutableCSSSelectorList::from(WTFMove(cueBackgroundSelector)) });

            // Warning: Recursion!
            addRule(WTFMove(cueBackgroundStyleRule), 0, 0);
        }
#endif
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
        addToRuleSet(attributeSelector->attribute().localNameLowercase(), m_attributeLowercaseLocalNameRules, ruleData);
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

    if (namedPseudoElementSelector) {
        addToRuleSet(namedPseudoElementSelector->argumentList()->first(), m_namedPseudoElementRules, ruleData);
        return;
    }

    if (rootElementSelector) {
        m_rootElementRules.append(ruleData);
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

void RuleSet::setViewTransitionRule(StyleRuleViewTransition& rule)
{
    m_viewTransitionRule = rule;
}

RefPtr<StyleRuleViewTransition> RuleSet::viewTransitionRule() const
{
    return m_viewTransitionRule;
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
    traverseMap(m_attributeLowercaseLocalNameRules);
    traverseMap(m_tagLocalNameRules);
    traverseMap(m_tagLowercaseLocalNameRules);
    traverseMap(m_userAgentPartRules);
    traverseMap(m_namedPseudoElementRules);
    traverseVector(m_linkPseudoClassRules);
#if ENABLE(VIDEO)
    traverseVector(m_cuePseudoRules);
#endif
    traverseVector(m_hostPseudoClassRules);
    traverseVector(m_slottedPseudoElementRules);
    traverseVector(m_partPseudoElementRules);
    traverseVector(m_focusPseudoClassRules);
    traverseVector(m_rootElementRules);
    traverseVector(m_universalRules);
}

template<typename Function> void RuleSet::traverseRuleDatas(Function&& function) const
{
    const_cast<RuleSet&>(*this).traverseRuleDatas([&](const RuleData& ruleData) {
        function(ruleData);
    });
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

    return { { DynamicMediaQueryEvaluationChanges::Type::InvalidateStyle, { { ruleSet.copyRef() } } } };
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
        if (auto result = affectedRulePositionsAndResults.getOptional(ruleData.position()))
            ruleData.setEnabled(*result);
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
    shrinkMapVectorsToFit(m_attributeLowercaseLocalNameRules);
    shrinkMapVectorsToFit(m_tagLocalNameRules);
    shrinkMapVectorsToFit(m_tagLowercaseLocalNameRules);
    shrinkMapVectorsToFit(m_userAgentPartRules);
    shrinkMapVectorsToFit(m_namedPseudoElementRules);

    m_linkPseudoClassRules.shrinkToFit();
#if ENABLE(VIDEO)
    m_cuePseudoRules.shrinkToFit();
#endif
    m_hostPseudoClassRules.shrinkToFit();
    m_slottedPseudoElementRules.shrinkToFit();
    m_partPseudoElementRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_rootElementRules.shrinkToFit();
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

Vector<Ref<const StyleRuleContainer>> RuleSet::containerQueryRules() const
{
    return m_containerQueries.map([](auto& entry) {
        return entry.containerRule;
    });
}

Vector<Ref<const StyleRuleScope>> RuleSet::scopeRulesFor(const RuleData& ruleData) const
{
    if (m_scopeRuleIdentifierForRulePosition.size() <= ruleData.position())
        return { };

    Vector<Ref<const StyleRuleScope>> queries;

    auto identifier = m_scopeRuleIdentifierForRulePosition[ruleData.position()];
    while (identifier) {
        auto& query = m_scopeRules[identifier - 1];
        queries.append(query.scopeRule);
        identifier = query.parent;
    };

    // Order scopes from outermost to innermost.
    queries.reverse();

    return queries;
}

const RefPtr<const StyleRulePositionTry> RuleSet::positionTryRuleForName(const AtomString& name) const
{
    return m_positionTryRules.get(name);
}

String RuleSet::selectorsForDebugging() const
{
    TextStream ts;
    ts << "RuleSet size " << ruleCount();
    ts.nextLine();
    traverseRuleDatas([&](auto& ruleData) {
        ts << ruleData.selector()->selectorText();
        ts.nextLine();
    });
    return ts.release();
}

} // namespace Style
} // namespace WebCore
