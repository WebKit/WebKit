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
#include "CSSSelectorParser.h"
#include "HTMLNames.h"
#include "RuleSet.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"

namespace WebCore {
namespace Style {

static bool NODELETE isSiblingOrSubject(MatchElement::Relation relation)
{
    switch (relation) {
    case MatchElement::Relation::Subject:
    case MatchElement::Relation::IndirectSibling:
    case MatchElement::Relation::DirectSibling:
    case MatchElement::Relation::AnySibling:
    case MatchElement::Relation::Host:
    case MatchElement::Relation::HostChild:
        return true;
    case MatchElement::Relation::Parent:
    case MatchElement::Relation::Ancestor:
    case MatchElement::Relation::ParentSibling:
    case MatchElement::Relation::AncestorSibling:
    case MatchElement::Relation::ParentAnySibling:
    case MatchElement::Relation::AncestorAnySibling:
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

RuleFeatureWithInvalidationSelector::RuleFeatureWithInvalidationSelector(const RuleData& data, MatchElement matchElement, IsNegation isNegation, CSSSelectorList&& invalidationSelector, CSSSelectorList&& scopeSelector)
    : RuleFeature(data, matchElement, isNegation)
    , invalidationSelector(WTF::move(invalidationSelector))
    , scopeSelector(WTF::move(scopeSelector))
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

static MatchElement::Relation computeNextRelation(MatchElement::Relation relation, CSSSelector::Relation selectorRelation)
{
    if (isSiblingOrSubject(relation)) {
        switch (selectorRelation) {
        case CSSSelector::Relation::Subselector:
            return relation;
        case CSSSelector::Relation::DescendantSpace:
            return MatchElement::Relation::Ancestor;
        case CSSSelector::Relation::Child:
            return MatchElement::Relation::Parent;
        case CSSSelector::Relation::IndirectAdjacent:
            if (relation == MatchElement::Relation::AnySibling)
                return MatchElement::Relation::AnySibling;
            return MatchElement::Relation::IndirectSibling;
        case CSSSelector::Relation::DirectAdjacent:
            if (relation == MatchElement::Relation::AnySibling)
                return MatchElement::Relation::AnySibling;
            return relation == MatchElement::Relation::Subject ? MatchElement::Relation::DirectSibling : MatchElement::Relation::IndirectSibling;
        case CSSSelector::Relation::ShadowDescendant:
        case CSSSelector::Relation::ShadowPartDescendant:
            return MatchElement::Relation::Host;
        case CSSSelector::Relation::ShadowSlotted:
            return MatchElement::Relation::HostChild;
        };
    }
    switch (selectorRelation) {
    case CSSSelector::Relation::Subselector:
        return relation;
    case CSSSelector::Relation::DescendantSpace:
    case CSSSelector::Relation::Child:
        return MatchElement::Relation::Ancestor;
    case CSSSelector::Relation::IndirectAdjacent:
    case CSSSelector::Relation::DirectAdjacent:
        return relation == MatchElement::Relation::Parent ? MatchElement::Relation::ParentSibling : MatchElement::Relation::AncestorSibling;
    case CSSSelector::Relation::ShadowDescendant:
    case CSSSelector::Relation::ShadowPartDescendant:
        return MatchElement::Relation::Host;
    case CSSSelector::Relation::ShadowSlotted:
        return MatchElement::Relation::HostChild;
    };
    ASSERT_NOT_REACHED();
    return relation;
};

static MatchElement::HasRelation toHasRelation(MatchElement::Relation relation)
{
    switch (relation) {
    case MatchElement::Relation::Parent:
        return MatchElement::HasRelation::Child;
    case MatchElement::Relation::Ancestor:
        return MatchElement::HasRelation::Descendant;
    case MatchElement::Relation::DirectSibling:
        return MatchElement::HasRelation::DirectSibling;
    case MatchElement::Relation::IndirectSibling:
    case MatchElement::Relation::AnySibling:
        return MatchElement::HasRelation::IndirectSibling;
    case MatchElement::Relation::ParentSibling:
        return MatchElement::HasRelation::SiblingChild;
    case MatchElement::Relation::AncestorSibling:
    case MatchElement::Relation::ParentAnySibling:
    case MatchElement::Relation::AncestorAnySibling:
        return MatchElement::HasRelation::SiblingDescendant;
    case MatchElement::Relation::Subject:
    case MatchElement::Relation::Host:
    case MatchElement::Relation::HostChild:
        ASSERT_NOT_REACHED();
        return MatchElement::HasRelation::Child;
    }
    ASSERT_NOT_REACHED();
    return MatchElement::HasRelation::Child;
}

MatchElement::HasRelation computeHasArgumentRelation(const CSSSelector& hasSelector)
{
    auto relation = MatchElement::Relation::Subject;
    for (auto* simpleSelector = &hasSelector; simpleSelector->precedingInComplexSelector(); simpleSelector = simpleSelector->precedingInComplexSelector())
        relation = computeNextRelation(relation, simpleSelector->relation());
    return toHasRelation(relation);
}

static bool isSiblingCombinator(CSSSelector::Relation relation)
{
    return relation == CSSSelector::Relation::DirectAdjacent || relation == CSSSelector::Relation::IndirectAdjacent;
}

static MatchElement computeSubSelectorMatchElement(MatchElement matchElement, const CSSSelector& selector, const CSSSelector& childSelector)
{
    if (selector.match() == CSSSelector::Match::PseudoClass) {
        auto type = selector.pseudoClass();
        // For :nth-child(n of .some-subselector) where an element change may affect other elements similar to sibling combinators.
        if (type == CSSSelector::PseudoClass::NthChild || type == CSSSelector::PseudoClass::NthLastChild) {
            if (matchElement.relation == MatchElement::Relation::Parent)
                return { MatchElement::Relation::ParentAnySibling, matchElement.hasRelation };
            if (matchElement.relation == MatchElement::Relation::Ancestor)
                return { MatchElement::Relation::AncestorAnySibling, matchElement.hasRelation };
            return { MatchElement::Relation::AnySibling, matchElement.hasRelation };
        }

        // Similarly for :host().
        if (type == CSSSelector::PseudoClass::Host)
            return { MatchElement::Relation::Host, matchElement.hasRelation };

        if (type == CSSSelector::PseudoClass::Has) {
            auto hasArgumentRelation = computeHasArgumentRelation(childSelector);
            return { matchElement.relation, hasArgumentRelation };
        }
    }
    if (selector.match() == CSSSelector::Match::PseudoElement) {
        // Similarly for ::slotted().
        if (selector.pseudoElement() == CSSSelector::PseudoElement::Slotted)
            return { MatchElement::Relation::Host, matchElement.hasRelation };
    }

    return matchElement;
}

// Returns true if a combinator inside :is()/:not() within :has() can match elements outside the
// :has() scope. We use this to decide if a nested entry can be associated with a scope selector
// to bound invalidation traversal.
static bool isHasScopeBreakingCombinator(CSSSelector::Relation relation, MatchElement::HasRelation hasRelation)
{
    if (relation == CSSSelector::Relation::DescendantSpace)
        return true;
    if (relation == CSSSelector::Relation::Child)
        return hasRelation != MatchElement::HasRelation::Child;
    if (isSiblingCombinator(relation)) {
        switch (hasRelation) {
        case MatchElement::HasRelation::DirectSibling:
        case MatchElement::HasRelation::IndirectSibling:
            return true;
        case MatchElement::HasRelation::Child:
        case MatchElement::HasRelation::Descendant:
        case MatchElement::HasRelation::SiblingChild:
        case MatchElement::HasRelation::SiblingDescendant:
            return false;
        }
    }
    return false;
}

struct RuleFeatureSet::RecursiveCollectionContext {
    MatchElement matchElement { MatchElement::Relation::Subject, { } };
    IsNegation isNegation { IsNegation::No };
    const CSSSelector* outerCompoundSelector { nullptr };
    const CSSSelector* hasPseudoClass { nullptr };
    bool isNestedInLogicalCombination { false };
    bool crossedScopeBreakingCombinator { false };
};

void RuleFeatureSet::collectFeaturesFromSelector(SelectorFeatures& selectorFeatures, const CSSSelector& selector, MatchElement matchElement)
{
    recursivelyCollectFeaturesFromSelector(selectorFeatures, selector, { matchElement });
}

void RuleFeatureSet::recursivelyCollectFeaturesFromSelector(SelectorFeatures& selectorFeatures, const CSSSelector& firstSelector, const RecursiveCollectionContext& context)
{
    auto matchElement = context.matchElement;
    const CSSSelector* selector = &firstSelector;
    bool isRightmostCompound = true;
    bool crossedScopeBreakingCombinator = context.crossedScopeBreakingCombinator;

    // When walking a :has() argument chain, emit hasPseudoClasses entries for compounds
    // at sibling combinator boundaries or containing positional pseudo-classes.
    // Child mutations can break sibling adjacency and change positional matching,
    // so ChildChangeInvalidation needs entries keyed on these compounds.
    // At the direct :has() argument level, also emit for the rightmost compound.
    // Inside nested :is()/:not(), only emit at sibling/positional boundaries to avoid excessive entries.
    auto collectHasPseudoClassFeatureIfNeeded = [&] {
        if (!context.hasPseudoClass || selector->match() == CSSSelector::Match::HasScope)
            return;
        if (!isRightmostCompound || context.isNestedInLogicalCombination) {
            auto compoundIsAffectedByChildMutation = [&] {
                if (isSiblingCombinator(selector->relation()))
                    return true;
                for (auto* simple = selector; simple; simple = simple->precedingInCompound()) {
                    if (simple->match() == CSSSelector::Match::PseudoClass && pseudoClassIsRelativeToSiblings(simple->pseudoClass()))
                        return true;
                }
                return false;
            };
            if (!compoundIsAffectedByChildMutation())
                return;
        }
        // Entries inside nested :is()/:not() can only match elements outside the :has() scope if
        // we crossed a combinator that reaches outside (e.g. descendant inside :is, or sibling
        // inside :is when :has() itself is in sibling/subject position). Otherwise the matched
        // element is always within the scope subtree and the scope selector can bound traversal.
        auto scopeSource = [&] -> const CSSSelector* {
            if (context.isNestedInLogicalCombination && crossedScopeBreakingCombinator)
                return nullptr;
            return context.outerCompoundSelector ? context.outerCompoundSelector : context.hasPseudoClass;
        }();
        selectorFeatures.hasPseudoClasses.append({ selector, matchElement, context.isNegation, scopeSource });
    };

    while (true) {
        if (selector->match() == CSSSelector::Match::Id) {
            idsInRules.add(selector->value());
            if (matchElement.relation == MatchElement::Relation::Parent || matchElement.relation == MatchElement::Relation::Ancestor)
                idsMatchingAncestorsInRules.add(selector->value());
            else if (matchElement.hasRelation || matchElement.relation == MatchElement::Relation::AnySibling || matchElement.relation == MatchElement::Relation::Host || matchElement.relation == MatchElement::Relation::HostChild)
                selectorFeatures.ids.append({ selector, matchElement, context.isNegation });
        } else if (selector->match() == CSSSelector::Match::Class)
            selectorFeatures.classes.append({ selector, matchElement, context.isNegation });
        else if (selector->isAttributeSelector()) {
            attributeLowercaseLocalNamesInRules.add(selector->attribute().localNameLowercase());
            attributeLocalNamesInRules.add(selector->attribute().localName());
            selectorFeatures.attributes.append({ selector, matchElement, context.isNegation });
        } else if (selector->match() == CSSSelector::Match::PseudoElement) {
            // Don't put anything here as selectors that differ by pseudo-element only are collected only once.
            // Pseudo-elements are handled in collectPseudoElementFeatures.
        } else if (selector->match() == CSSSelector::Match::PseudoClass) {
            bool isLogicalCombination = isLogicalCombinationPseudoClass(selector->pseudoClass());
            if (!isLogicalCombination)
                selectorFeatures.pseudoClasses.append({ selector, matchElement, context.isNegation });
        }

        collectHasPseudoClassFeatureIfNeeded();

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            auto subSelectorIsNegation = context.isNegation;
            if (selector->match() == CSSSelector::Match::PseudoClass && selector->pseudoClass() == CSSSelector::PseudoClass::Not)
                subSelectorIsNegation = context.isNegation == IsNegation::No ? IsNegation::Yes : IsNegation::No;

            for (auto& subSelector : *selectorList) {
                auto subResult = computeSubSelectorMatchElement(matchElement, *selector, subSelector);

                RecursiveCollectionContext subContext { subResult, subSelectorIsNegation, context.outerCompoundSelector, context.hasPseudoClass, context.isNestedInLogicalCombination, crossedScopeBreakingCombinator };

                // When entering a logical combination (not :has() itself), record the outer compound
                // so nested :has() can use it for scope selector extraction.
                // Mark as nested so only sibling boundary entries are emitted, not rightmost.
                if (selector->match() == CSSSelector::Match::PseudoClass && isLogicalCombinationPseudoClass(selector->pseudoClass()) && selector->pseudoClass() != CSSSelector::PseudoClass::Has) {
                    if (!subContext.outerCompoundSelector)
                        subContext.outerCompoundSelector = selector;
                    if (subContext.hasPseudoClass)
                        subContext.isNestedInLogicalCombination = true;
                }

                if (selector->match() == CSSSelector::Match::PseudoClass && selector->pseudoClass() == CSSSelector::PseudoClass::Has)
                    subContext.hasPseudoClass = selector;

                recursivelyCollectFeaturesFromSelector(selectorFeatures, subSelector, subContext);
            }
        }

        if (!selector->precedingInComplexSelector())
            break;

        auto relation = selector->relation();
        isRightmostCompound = false;

        if (context.isNestedInLogicalCombination && matchElement.hasRelation && isHasScopeBreakingCombinator(relation, *matchElement.hasRelation))
            crossedScopeBreakingCombinator = true;

        matchElement.relation = computeNextRelation(matchElement.relation, relation);

        selector = selector->precedingInComplexSelector();
    };
}

PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClass pseudoClass, InvalidationKeyType keyType, const AtomString& keyString)
{
    ASSERT(keyType != InvalidationKeyType::Universal || keyString == starAtom());
    return {
        std::to_underlying(pseudoClass),
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
    // Empty rules don't affect style so we never need to invalidate for them.
    if (ruleData.styleRule().properties().isEmpty())
        return;

    SelectorFeatures selectorFeatures;

    auto& selector = ruleData.selector();
    bool firstSeen = collectionContext.selectorDeduplicationSet.add({ selector }).isNewEntry;
    if (firstSeen)
        collectFeaturesFromSelector(selectorFeatures, selector);

    if (ruleData.canMatchPseudoElement())
        collectPseudoElementFeatures(ruleData);

    for (auto& scopeRule : scopeRules) {
        auto collectSelectorList = [&] (const auto& selectorList) {
            if (!selectorList.isEmpty()) {
                for (auto& subSelector : selectorList) {
                    collectFeaturesFromSelector(selectorFeatures, subSelector, { MatchElement::Relation::Ancestor, { } });
                    collectFeaturesFromSelector(selectorFeatures, subSelector, { MatchElement::Relation::Subject, { } });
                }
            }
        };
        collectSelectorList(scopeRule->scopeStart());
        collectSelectorList(scopeRule->scopeEnd());
    }

    if (ruleData.isStartingStyle() == IsStartingStyle::Yes)
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

            setUsesRelation(matchElement.relation);

            if constexpr (!std::is_same_v<std::nullptr_t, HostAffectingNames>) {
                if (matchElement.relation == MatchElement::Relation::Host)
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

        if (matchElement.relation == MatchElement::Relation::Host)
            attributesAffectingHost.add(selector->attribute().localNameLowercase());
        setUsesRelation(matchElement.relation);
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

        if (matchElement.relation == MatchElement::Relation::Host)
            pseudoClassesAffectingHost.add(selector->pseudoClass());
        pseudoClasses.add(selector->pseudoClass());

        setUsesRelation(matchElement.relation);
    }

    for (auto& entry : selectorFeatures.hasPseudoClasses) {
        auto& [selector, matchElement, isNegation, hasPseudoClass] = entry;
        // The selector argument points to a selector inside :has() selector list instead of :has() itself.
        auto& featureVector = *hasPseudoClassRules.ensure(makePseudoClassInvalidationKey(CSSSelector::PseudoClass::Has, *selector), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value;

        addToVector(featureVector, RuleFeatureWithInvalidationSelector {
            ruleData,
            matchElement,
            isNegation,
            CSSSelectorList::makeCopyingComplexSelector(*selector),
            hasPseudoClass ? CSSSelectorParser::makeHasScopeSelector(*hasPseudoClass) : CSSSelectorList { }
        });

        setUsesRelation(matchElement.relation);
        usesHasPseudoClass = true;
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
    substitutionAttributeNamesInRules.addAll(other.substitutionAttributeNamesInRules);

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

    for (size_t i = 0; i < usedRelations.size(); ++i)
        usedRelations[i] = usedRelations[i] || other.usedRelations[i];

    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesFirstLetterRules = usesFirstLetterRules || other.usesFirstLetterRules;
    hasStartingStyleRules = hasStartingStyleRules || other.hasStartingStyleRules;
    usesHasPseudoClass = usesHasPseudoClass || other.usesHasPseudoClass;
}

void RuleFeatureSet::registerSubstitutionAttribute(const AtomString& attributeName)
{
    substitutionAttributeNamesInRules.add(attributeName.convertToASCIILowercase());
    attributeLowercaseLocalNamesInRules.add(attributeName);
    attributeLocalNamesInRules.add(attributeName);
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    idsMatchingAncestorsInRules.clear();
    attributeLowercaseLocalNamesInRules.clear();
    attributeLocalNamesInRules.clear();
    substitutionAttributeNamesInRules.clear();
    idRules.clear();
    classRules.clear();
    hasPseudoClassRules.clear();
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
