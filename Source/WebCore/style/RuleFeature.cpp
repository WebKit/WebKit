/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2012, 2014 Apple Inc. All rights reserved.
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
#include "RuleFeature.h"

#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "HTMLNames.h"
#include "RuleSet.h"
#include "StyleRule.h"

namespace WebCore {
namespace Style {

static bool isSiblingOrSubject(MatchElement matchElement)
{
    switch (matchElement) {
    case MatchElement::Subject:
    case MatchElement::IndirectSibling:
    case MatchElement::DirectSibling:
    case MatchElement::AnySibling:
    case MatchElement::HasSibling:
    case MatchElement::HasAnySibling:
    case MatchElement::Host:
    case MatchElement::HostChild:
        return true;
    case MatchElement::Parent:
    case MatchElement::Ancestor:
    case MatchElement::ParentSibling:
    case MatchElement::AncestorSibling:
    case MatchElement::ParentAnySibling:
    case MatchElement::AncestorAnySibling:
    case MatchElement::HasChild:
    case MatchElement::HasDescendant:
    case MatchElement::HasSiblingDescendant:
    case MatchElement::HasChildParent:
    case MatchElement::HasChildAncestor:
    case MatchElement::HasDescendantParent:
    case MatchElement::HasNonSubject:
    case MatchElement::HasScopeBreaking:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool isHasPseudoClassMatchElement(MatchElement matchElement)
{
    switch (matchElement) {
    case MatchElement::HasChild:
    case MatchElement::HasDescendant:
    case MatchElement::HasSibling:
    case MatchElement::HasSiblingDescendant:
    case MatchElement::HasAnySibling:
    case MatchElement::HasNonSubject:
    case MatchElement::HasScopeBreaking:
        return true;
    default:
        return false;
    }
}

static bool isScopeBreaking(MatchElement matchElement)
{
    switch (matchElement) {
    case MatchElement::HasAnySibling:
    case MatchElement::HasScopeBreaking:
        return true;
    case MatchElement::Subject:
    case MatchElement::IndirectSibling:
    case MatchElement::DirectSibling:
    case MatchElement::AnySibling:
    case MatchElement::HasSibling:
    case MatchElement::Host:
    case MatchElement::HostChild:
    case MatchElement::Parent:
    case MatchElement::Ancestor:
    case MatchElement::ParentSibling:
    case MatchElement::AncestorSibling:
    case MatchElement::ParentAnySibling:
    case MatchElement::AncestorAnySibling:
    case MatchElement::HasChild:
    case MatchElement::HasDescendant:
    case MatchElement::HasSiblingDescendant:
    case MatchElement::HasChildParent:
    case MatchElement::HasChildAncestor:
    case MatchElement::HasDescendantParent:
    case MatchElement::HasNonSubject:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

RuleAndSelector::RuleAndSelector(const RuleData& ruleData)
    : styleRule(&ruleData.styleRule())
    , selectorIndex(ruleData.selectorIndex())
    , selectorListIndex(ruleData.selectorListIndex())
{
    ASSERT(selectorIndex == ruleData.selectorIndex());
    ASSERT(selectorListIndex == ruleData.selectorListIndex());
}

const CSSSelector& RuleAndSelector::selector() const
{
    return styleRule->selectorList().selectorAt(selectorIndex);
}

RuleFeature::RuleFeature(const RuleData& ruleData, MatchElement matchElement, IsNegation isNegation)
    : RuleAndSelector(ruleData)
    , matchElement(matchElement)
    , isNegation(isNegation)
{
}

RuleFeatureWithInvalidationSelector::RuleFeatureWithInvalidationSelector(const RuleData& data, MatchElement matchElement, IsNegation isNegation, CSSSelectorList&& invalidationSelector)
    : RuleFeature(data, matchElement, isNegation)
    , invalidationSelector(WTF::move(invalidationSelector))
{
}

SelectorDeduplicationKey::SelectorDeduplicationKey(const CSSSelector& selector)
    : selector(&selector)
{
    Hasher hasher;
    addComplexSelector(hasher, selector, ComplexSelectorsEqualMode::IgnoreNonElementBackedPseudoElements);
    cachedHash = hasher.hash();
}

bool SelectorDeduplicationKey::operator==(const SelectorDeduplicationKey& other) const
{
    // Selectors like '.foo' and '.foo::before' are equal for invalidation as they both invalidate the generating element.
    return complexSelectorsEqual(*selector, *other.selector, ComplexSelectorsEqualMode::IgnoreNonElementBackedPseudoElements);
}

static MatchElement computeNextMatchElement(MatchElement matchElement, CSSSelector::Relation relation)
{
    ASSERT(!isHasPseudoClassMatchElement(matchElement));

    if (isSiblingOrSubject(matchElement)) {
        switch (relation) {
        case CSSSelector::Relation::Subselector:
            return matchElement;
        case CSSSelector::Relation::DescendantSpace:
            return MatchElement::Ancestor;
        case CSSSelector::Relation::Child:
            return MatchElement::Parent;
        case CSSSelector::Relation::IndirectAdjacent:
            if (matchElement == MatchElement::AnySibling)
                return MatchElement::AnySibling;
            return MatchElement::IndirectSibling;
        case CSSSelector::Relation::DirectAdjacent:
            if (matchElement == MatchElement::AnySibling)
                return MatchElement::AnySibling;
            return matchElement == MatchElement::Subject ? MatchElement::DirectSibling : MatchElement::IndirectSibling;
        case CSSSelector::Relation::ShadowDescendant:
        case CSSSelector::Relation::ShadowPartDescendant:
            return MatchElement::Host;
        case CSSSelector::Relation::ShadowSlotted:
            return MatchElement::HostChild;
        };
    }
    switch (relation) {
    case CSSSelector::Relation::Subselector:
        return matchElement;
    case CSSSelector::Relation::DescendantSpace:
    case CSSSelector::Relation::Child:
        return MatchElement::Ancestor;
    case CSSSelector::Relation::IndirectAdjacent:
    case CSSSelector::Relation::DirectAdjacent:
        return matchElement == MatchElement::Parent ? MatchElement::ParentSibling : MatchElement::AncestorSibling;
    case CSSSelector::Relation::ShadowDescendant:
    case CSSSelector::Relation::ShadowPartDescendant:
        return MatchElement::Host;
    case CSSSelector::Relation::ShadowSlotted:
        return MatchElement::HostChild;
    };
    ASSERT_NOT_REACHED();
    return matchElement;
};

static MatchElement computeNextHasPseudoClassMatchElement(MatchElement matchElement, CSSSelector::Relation relation, CanBreakScope canBreakScope)
{
    ASSERT(isHasPseudoClassMatchElement(matchElement));

    if (canBreakScope == CanBreakScope::No)
        return matchElement;

    // `:has(:is(foo bar))` can be affected by changes outside the :has scope.
    if (relation == CSSSelector::Relation::DescendantSpace || relation == CSSSelector::Relation::Child) {
        // However, for `:has(> :is(.x > .y))`, the child combinator (>) inside :is() is still scoped to the direct child's tree.
        // The parent in the relationship must be the direct child itself, which is within the :has(>) scope.
        // Only descendant combinators can reach outside this scope (to ancestors of the subject element).
        if (matchElement == MatchElement::HasChild && relation == CSSSelector::Relation::Child)
            return matchElement;

        return MatchElement::HasScopeBreaking;
    }

    if (relation == CSSSelector::Relation::IndirectAdjacent || relation == CSSSelector::Relation::DirectAdjacent) {
        // `:has(~ :is(.x ~ .y))` must look at previous siblings of the :scope scope too.
        if (matchElement == MatchElement::HasSibling)
            return MatchElement::HasAnySibling;

        // `:has(~ :is(.x ~ .y)) .z` must be treated as scope breaking, rather than HasAnySibling like the previous case.
        if (matchElement == MatchElement::HasNonSubject)
            return MatchElement::HasScopeBreaking;
    }

    return matchElement;
}

MatchElement computeHasPseudoClassMatchElement(const CSSSelector& hasSelector)
{
    auto hasMatchElement = MatchElement::Subject;
    for (auto* simpleSelector = &hasSelector; simpleSelector->precedingInComplexSelector(); simpleSelector = simpleSelector->precedingInComplexSelector())
        hasMatchElement = computeNextMatchElement(hasMatchElement, simpleSelector->relation());

    switch (hasMatchElement) {
    case MatchElement::Parent:
    case MatchElement::Subject:
        return MatchElement::HasChild;
    case MatchElement::Ancestor:
        return MatchElement::HasDescendant;
    case MatchElement::IndirectSibling:
    case MatchElement::DirectSibling:
    case MatchElement::AnySibling:
        return MatchElement::HasSibling;
    case MatchElement::ParentSibling:
    case MatchElement::AncestorSibling:
    case MatchElement::ParentAnySibling:
    case MatchElement::AncestorAnySibling:
        return MatchElement::HasSiblingDescendant;
    case MatchElement::HasChild:
    case MatchElement::HasDescendant:
    case MatchElement::HasSibling:
    case MatchElement::HasSiblingDescendant:
    case MatchElement::HasAnySibling:
    case MatchElement::HasChildParent:
    case MatchElement::HasChildAncestor:
    case MatchElement::HasDescendantParent:
    case MatchElement::HasNonSubject:
    case MatchElement::HasScopeBreaking:
    case MatchElement::Host:
    case MatchElement::HostChild:
        ASSERT_NOT_REACHED();
        break;
    }
    return MatchElement::HasChild;
}

static MatchElement computeSubSelectorMatchElement(MatchElement matchElement, const CSSSelector& selector, const CSSSelector& childSelector)
{
    if (selector.match() == CSSSelector::Match::PseudoClass) {
        auto type = selector.pseudoClass();
        // For :nth-child(n of .some-subselector) where an element change may affect other elements similar to sibling combinators.
        if (type == CSSSelector::PseudoClass::NthChild || type == CSSSelector::PseudoClass::NthLastChild) {
            if (matchElement == MatchElement::Parent)
                return MatchElement::ParentAnySibling;
            if (matchElement == MatchElement::Ancestor)
                return MatchElement::AncestorAnySibling;
            return MatchElement::AnySibling;
        }

        // Similarly for :host().
        if (type == CSSSelector::PseudoClass::Host)
            return MatchElement::Host;

        if (type == CSSSelector::PseudoClass::Has) {
            auto hasSelectorMatchElement = computeHasPseudoClassMatchElement(childSelector);

            if (hasSelectorMatchElement == MatchElement::HasChild) {
                // :has(> .changed) > .subject
                if (matchElement == MatchElement::Parent)
                    return MatchElement::HasChildParent;
                // :has(> .changed) .subject
                if (matchElement == MatchElement::Ancestor)
                    return MatchElement::HasChildAncestor;
            }

            if (hasSelectorMatchElement == MatchElement::HasDescendant) {
                if (matchElement == MatchElement::Parent)
                    return MatchElement::HasDescendantParent;
            }


            if (matchElement != MatchElement::Subject)
                return MatchElement::HasNonSubject;

            return hasSelectorMatchElement;
        }

    }
    if (selector.match() == CSSSelector::Match::PseudoElement) {
        // Similarly for ::slotted().
        if (selector.pseudoElement() == CSSSelector::PseudoElement::Slotted)
            return MatchElement::Host;
    }

    return matchElement;
}

DoesBreakScope RuleFeatureSet::recursivelyCollectFeaturesFromSelector(SelectorFeatures& selectorFeatures, const CSSSelector& firstSelector, MatchElement matchElement, IsNegation isNegation, CanBreakScope allComponentsCanBreakScope)
{
    auto doesBreakScope = DoesBreakScope::No;
    const CSSSelector* selector = &firstSelector;
    while (true) {
        auto canBreakScope = allComponentsCanBreakScope;
        if (selector->match() == CSSSelector::Match::Id) {
            idsInRules.add(selector->value());
            if (matchElement == MatchElement::Parent || matchElement == MatchElement::Ancestor)
                idsMatchingAncestorsInRules.add(selector->value());
            else if (isHasPseudoClassMatchElement(matchElement) || matchElement == MatchElement::AnySibling || matchElement == MatchElement::Host || matchElement == MatchElement::HostChild)
                selectorFeatures.ids.append({ selector, matchElement, isNegation });
        } else if (selector->match() == CSSSelector::Match::Class)
            selectorFeatures.classes.append({ selector, matchElement, isNegation });
        else if (selector->isAttributeSelector()) {
            attributeLowercaseLocalNamesInRules.add(selector->attribute().localNameLowercase());
            attributeLocalNamesInRules.add(selector->attribute().localName());
            selectorFeatures.attributes.append({ selector, matchElement, isNegation });
        } else if (selector->match() == CSSSelector::Match::PseudoElement) {
            // Don't put anything here as selectors that differ by pseudo-element only are collected only once.
            // Pseudo-elements are handled in collectPseudoElementFeatures.
        } else if (selector->match() == CSSSelector::Match::PseudoClass) {
            bool isLogicalCombination = isLogicalCombinationPseudoClass(selector->pseudoClass());
            if (!isLogicalCombination)
                selectorFeatures.pseudoClasses.append({ selector, matchElement, isNegation });

            // Check for the :has(:is(foo bar)) case. In this case `foo` can match elements outside the :has() scope.
            if (isLogicalCombination && isHasPseudoClassMatchElement(matchElement))
                canBreakScope = CanBreakScope::Yes;
        }

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            auto subSelectorIsNegation = isNegation;
            if (selector->match() == CSSSelector::Match::PseudoClass && selector->pseudoClass() == CSSSelector::PseudoClass::Not)
                subSelectorIsNegation = isNegation == IsNegation::No ? IsNegation::Yes : IsNegation::No;

            for (auto& subSelector : *selectorList) {
                auto subSelectorMatchElement = computeSubSelectorMatchElement(matchElement, *selector, subSelector);
                auto pseudoClassDoesBreakScope = recursivelyCollectFeaturesFromSelector(selectorFeatures, subSelector, subSelectorMatchElement, subSelectorIsNegation, canBreakScope);

                if (selector->match() == CSSSelector::Match::PseudoClass && selector->pseudoClass() == CSSSelector::PseudoClass::Has)
                    selectorFeatures.hasPseudoClasses.append({ &subSelector, subSelectorMatchElement, isNegation, pseudoClassDoesBreakScope });

                if (pseudoClassDoesBreakScope == DoesBreakScope::Yes)
                    doesBreakScope = DoesBreakScope::Yes;
            }
        }

        if (!selector->precedingInComplexSelector())
            break;

        matchElement = [&] {
            if (isHasPseudoClassMatchElement(matchElement))
                return computeNextHasPseudoClassMatchElement(matchElement, selector->relation(), allComponentsCanBreakScope);
            return computeNextMatchElement(matchElement, selector->relation());
        }();

        if (isScopeBreaking(matchElement))
            doesBreakScope = DoesBreakScope::Yes;

        selector = selector->precedingInComplexSelector();
    };

    return doesBreakScope;
}

PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClass pseudoClass, InvalidationKeyType keyType, const AtomString& keyString)
{
    ASSERT(keyType != InvalidationKeyType::Universal || keyString == starAtom());
    return {
        enumToUnderlyingType(pseudoClass),
        static_cast<uint8_t>(keyType),
        keyString
    };
};

bool unlikelyToHaveSelectorForAttribute(const AtomString& name)
{
    return name == HTMLNames::classAttr->localName() || name == HTMLNames::idAttr->localName() || name == HTMLNames::styleAttr->localName();
}

static PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClass pseudoClass, const CSSSelector& selector)
{
    AtomString attributeName;
    AtomString className;
    AtomString tagName;
    for (auto* simpleSelector = selector.lastInCompound(); simpleSelector; simpleSelector = simpleSelector->precedingInComplexSelector()) {
        if (simpleSelector->match() == CSSSelector::Match::Id)
            return makePseudoClassInvalidationKey(pseudoClass, InvalidationKeyType::Id, simpleSelector->value());

        if (simpleSelector->match() == CSSSelector::Match::Class && className.isNull())
            className = simpleSelector->value();

        if (simpleSelector->match() == CSSSelector::Match::Tag)
            tagName = simpleSelector->tagLowercaseLocalName();

        if (simpleSelector->isAttributeSelector() && !unlikelyToHaveSelectorForAttribute(simpleSelector->attribute().localNameLowercase()))
            attributeName = simpleSelector->attribute().localNameLowercase();

        if (simpleSelector->relation() != CSSSelector::Relation::Subselector)
            break;
    }
    if (!attributeName.isEmpty())
        return makePseudoClassInvalidationKey(pseudoClass, InvalidationKeyType::Attribute, attributeName);

    if (!className.isEmpty())
        return makePseudoClassInvalidationKey(pseudoClass, InvalidationKeyType::Class, className);

    if (!tagName.isEmpty() && tagName != starAtom())
        return makePseudoClassInvalidationKey(pseudoClass, InvalidationKeyType::Tag, tagName);

    return makePseudoClassInvalidationKey(pseudoClass, InvalidationKeyType::Universal);
};

void RuleFeatureSet::collectFeatures(CollectionContext& collectionContext, const RuleData& ruleData, const Vector<Ref<const StyleRuleScope>>& scopeRules)
{
    SelectorFeatures selectorFeatures;

    auto& selector = ruleData.selector();
    bool firstSeen = collectionContext.selectorDeduplicationSet.add({ selector }).isNewEntry;
    if (firstSeen)
        recursivelyCollectFeaturesFromSelector(selectorFeatures, selector);

    if (ruleData.canMatchPseudoElement())
        collectPseudoElementFeatures(ruleData);

    for (auto& scopeRule : scopeRules) {
        auto collectSelectorList = [&] (const auto& selectorList) {
            if (!selectorList.isEmpty()) {
                for (auto& subSelector : selectorList) {
                    recursivelyCollectFeaturesFromSelector(selectorFeatures, subSelector, MatchElement::Ancestor);
                    recursivelyCollectFeaturesFromSelector(selectorFeatures, subSelector, MatchElement::Subject);
                }
            }
        };
        collectSelectorList(scopeRule->scopeStart());
        collectSelectorList(scopeRule->scopeEnd());
    }

    if (ruleData.usedRuleTypes().contains(UsedRuleType::StartingStyle))
        hasStartingStyleRules = true;

    auto addToVector = [&](auto& featureVector, auto&& featureToAdd) {
        featureVector.append(WTF::move(featureToAdd));
    };

    auto addToMap = [&]<typename HostAffectingNames>(auto& map, auto& entries, HostAffectingNames hostAffectingNames) {
        for (auto& entry : entries) {
            auto& [selector, matchElement, isNegation] = entry;
            auto& name = selector->value();

            auto& featureVector = *map.ensure(name, [] {
                return makeUnique<RuleFeatureVector>();
            }).iterator->value;

            addToVector(featureVector, RuleFeature {
                ruleData,
                matchElement,
                isNegation
            });

            setUsesMatchElement(matchElement);

            if constexpr (!std::is_same_v<std::nullptr_t, HostAffectingNames>) {
                if (matchElement == MatchElement::Host)
                    hostAffectingNames->add(name);
            }
        }
    };

    addToMap(idRules, selectorFeatures.ids, nullptr);
    addToMap(classRules, selectorFeatures.classes, &classesAffectingHost);

    for (auto& entry : selectorFeatures.attributes) {
        auto& [selector, matchElement, isNegation] = entry;
        auto& featureVector = *attributeRules.ensure(selector->attribute().localNameLowercase(), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value;

        addToVector(featureVector, RuleFeatureWithInvalidationSelector {
            ruleData,
            matchElement,
            isNegation,
            CSSSelectorList::makeCopyingSimpleSelector(*selector)
        });

        if (matchElement == MatchElement::Host)
            attributesAffectingHost.add(selector->attribute().localNameLowercase());
        setUsesMatchElement(matchElement);
    }

    for (auto& entry : selectorFeatures.pseudoClasses) {
        auto& [selector, matchElement, isNegation] = entry;
        auto& featureVector = *pseudoClassRules.ensure(makePseudoClassInvalidationKey(selector->pseudoClass(), *selector), [] {
            return makeUnique<Vector<RuleFeature>>();
        }).iterator->value;

        addToVector(featureVector, RuleFeature {
            ruleData,
            matchElement,
            isNegation
        });

        if (matchElement == MatchElement::Host)
            pseudoClassesAffectingHost.add(selector->pseudoClass());
        pseudoClasses.add(selector->pseudoClass());

        setUsesMatchElement(matchElement);
    }

    for (auto& entry : selectorFeatures.hasPseudoClasses) {
        auto& [selector, matchElement, isNegation, doesBreakScope] = entry;
        // The selector argument points to a selector inside :has() selector list instead of :has() itself.
        auto& featureVector = *hasPseudoClassRules.ensure(makePseudoClassInvalidationKey(CSSSelector::PseudoClass::Has, *selector), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value;

        addToVector(featureVector, RuleFeatureWithInvalidationSelector {
            ruleData,
            matchElement,
            isNegation,
            CSSSelectorList::makeCopyingComplexSelector(*selector)
        });

        if (doesBreakScope == DoesBreakScope::Yes)
            scopeBreakingHasPseudoClassRules.append({ ruleData });

        setUsesMatchElement(matchElement);
    }
}

void RuleFeatureSet::collectPseudoElementFeatures(const RuleData& ruleData)
{
    ASSERT(ruleData.canMatchPseudoElement());

    auto& selector = ruleData.selector();
    for (auto* simpleSelector = &selector; simpleSelector; simpleSelector = simpleSelector->precedingInCompound()) {
        if (simpleSelector->match() != CSSSelector::Match::PseudoElement)
            continue;
        switch (simpleSelector->pseudoElement()) {
        case CSSSelector::PseudoElement::FirstLine:
            usesFirstLineRules = true;
            continue;
        case CSSSelector::PseudoElement::FirstLetter:
            usesFirstLetterRules = true;
            continue;
        default:
            continue;
        }
    }
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    idsInRules.addAll(other.idsInRules);
    idsMatchingAncestorsInRules.addAll(other.idsMatchingAncestorsInRules);
    attributeLowercaseLocalNamesInRules.addAll(other.attributeLowercaseLocalNamesInRules);
    attributeLocalNamesInRules.addAll(other.attributeLocalNamesInRules);
    contentAttributeNamesInRules.addAll(other.contentAttributeNamesInRules);

    auto addMap = [&](auto& map, auto& otherMap) {
        for (auto& keyValuePair : otherMap) {
            map.ensure(keyValuePair.key, [] {
                return makeUnique<std::decay_t<decltype(*keyValuePair.value)>>();
            }).iterator->value->appendVector(*keyValuePair.value);
        }
    };

    addMap(idRules, other.idRules);

    addMap(classRules, other.classRules);
    classesAffectingHost.addAll(other.classesAffectingHost);

    addMap(attributeRules, other.attributeRules);
    attributesAffectingHost.addAll(other.attributesAffectingHost);

    addMap(pseudoClassRules, other.pseudoClassRules);
    pseudoClassesAffectingHost.addAll(other.pseudoClassesAffectingHost);
    pseudoClasses.addAll(other.pseudoClasses);

    addMap(hasPseudoClassRules, other.hasPseudoClassRules);
    scopeBreakingHasPseudoClassRules.appendVector(other.scopeBreakingHasPseudoClassRules);

    for (size_t i = 0; i < usedMatchElements.size(); ++i)
        usedMatchElements[i] = usedMatchElements[i] || other.usedMatchElements[i];

    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesFirstLetterRules = usesFirstLetterRules || other.usesFirstLetterRules;
    hasStartingStyleRules = hasStartingStyleRules || other.hasStartingStyleRules;
}

void RuleFeatureSet::registerContentAttribute(const AtomString& attributeName)
{
    contentAttributeNamesInRules.add(attributeName.convertToASCIILowercase());
    attributeLowercaseLocalNamesInRules.add(attributeName);
    attributeLocalNamesInRules.add(attributeName);
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    idsMatchingAncestorsInRules.clear();
    attributeLowercaseLocalNamesInRules.clear();
    attributeLocalNamesInRules.clear();
    contentAttributeNamesInRules.clear();
    idRules.clear();
    classRules.clear();
    hasPseudoClassRules.clear();
    scopeBreakingHasPseudoClassRules.clear();
    classesAffectingHost.clear();
    attributeRules.clear();
    attributesAffectingHost.clear();
    pseudoClassRules.clear();
    pseudoClassesAffectingHost.clear();
    pseudoClasses.clear();
    usesFirstLineRules = false;
    usesFirstLetterRules = false;
    hasStartingStyleRules = false;
}

void RuleFeatureSet::shrinkToFit()
{
    scopeBreakingHasPseudoClassRules.shrinkToFit();
    for (auto& rules : idRules.values())
        rules->shrinkToFit();
    for (auto& rules : classRules.values())
        rules->shrinkToFit();
    for (auto& rules : attributeRules.values())
        rules->shrinkToFit();
    for (auto& rules : pseudoClassRules.values())
        rules->shrinkToFit();
    for (auto& rules : hasPseudoClassRules.values())
        rules->shrinkToFit();
}

} // namespace Style
} // namespace WebCore
