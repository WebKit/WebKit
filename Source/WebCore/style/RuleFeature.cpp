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
#include "RuleSet.h"

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
    case MatchElement::Host:
        return true;
    case MatchElement::Parent:
    case MatchElement::Ancestor:
    case MatchElement::ParentSibling:
    case MatchElement::AncestorSibling:
    case MatchElement::HasChild:
    case MatchElement::HasDescendant:
    case MatchElement::HasSiblingDescendant:
    case MatchElement::HasNonSubjectOrScopeBreaking:
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
    case MatchElement::HasNonSubjectOrScopeBreaking:
        return true;
    default:
        return false;
    }
}

RuleAndSelector::RuleAndSelector(const RuleData& ruleData)
    : styleRule(&ruleData.styleRule())
    , selectorIndex(ruleData.selectorIndex())
    , selectorListIndex(ruleData.selectorListIndex())
{
    ASSERT(selectorIndex == ruleData.selectorIndex());
    ASSERT(selectorListIndex == ruleData.selectorListIndex());
}


RuleFeature::RuleFeature(const RuleData& ruleData, MatchElement matchElement, IsNegation isNegation)
    : RuleAndSelector(ruleData)
    , matchElement(matchElement)
    , isNegation(isNegation)
{
}

RuleFeatureWithInvalidationSelector::RuleFeatureWithInvalidationSelector(const RuleData& data, MatchElement matchElement, IsNegation isNegation, const CSSSelector* invalidationSelector)
    : RuleFeature(data, matchElement, isNegation)
    , invalidationSelector(invalidationSelector)
{
}

static MatchElement computeNextMatchElement(MatchElement matchElement, CSSSelector::RelationType relation)
{
    ASSERT(!isHasPseudoClassMatchElement(matchElement));

    if (isSiblingOrSubject(matchElement)) {
        switch (relation) {
        case CSSSelector::Subselector:
            return matchElement;
        case CSSSelector::DescendantSpace:
            return MatchElement::Ancestor;
        case CSSSelector::Child:
            return MatchElement::Parent;
        case CSSSelector::IndirectAdjacent:
            if (matchElement == MatchElement::AnySibling)
                return MatchElement::AnySibling;
            return MatchElement::IndirectSibling;
        case CSSSelector::DirectAdjacent:
            if (matchElement == MatchElement::AnySibling)
                return MatchElement::AnySibling;
            return matchElement == MatchElement::Subject ? MatchElement::DirectSibling : MatchElement::IndirectSibling;
        case CSSSelector::ShadowDescendant:
        case CSSSelector::ShadowPartDescendant:
            return MatchElement::Host;
        case CSSSelector::ShadowSlotted:
            // FIXME: Implement accurate invalidation.
            return matchElement;
        };
    }
    switch (relation) {
    case CSSSelector::Subselector:
        return matchElement;
    case CSSSelector::DescendantSpace:
    case CSSSelector::Child:
        return MatchElement::Ancestor;
    case CSSSelector::IndirectAdjacent:
    case CSSSelector::DirectAdjacent:
        return matchElement == MatchElement::Parent ? MatchElement::ParentSibling : MatchElement::AncestorSibling;
    case CSSSelector::ShadowDescendant:
    case CSSSelector::ShadowPartDescendant:
        return MatchElement::Host;
    case CSSSelector::ShadowSlotted:
        // FIXME: Implement accurate invalidation.
        return matchElement;
    };
    ASSERT_NOT_REACHED();
    return matchElement;
};

static MatchElement computeNextHasPseudoClassMatchElement(MatchElement matchElement, CSSSelector::RelationType relation, CanBreakScope canBreakScope)
{
    ASSERT(isHasPseudoClassMatchElement(matchElement));

    // :has(:is(foo bar)) can be affected by changes outside the :has scope.
    if (canBreakScope == CanBreakScope::Yes) {
        if (relation == CSSSelector::DescendantSpace || relation == CSSSelector::Child)
            return MatchElement::HasNonSubjectOrScopeBreaking;
    }
    return matchElement;
}

MatchElement computeHasPseudoClassMatchElement(const CSSSelector& hasSelector)
{
    auto hasMatchElement = MatchElement::Subject;
    for (auto* simpleSelector = &hasSelector; simpleSelector->tagHistory(); simpleSelector = simpleSelector->tagHistory())
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
        return MatchElement::HasSiblingDescendant;
    case MatchElement::HasChild:
    case MatchElement::HasDescendant:
    case MatchElement::HasSibling:
    case MatchElement::HasSiblingDescendant:
    case MatchElement::HasNonSubjectOrScopeBreaking:
    case MatchElement::Host:
        ASSERT_NOT_REACHED();
        break;
    }
    return MatchElement::HasChild;
}

static MatchElement computeSubSelectorMatchElement(MatchElement matchElement, const CSSSelector& selector, const CSSSelector& childSelector)
{
    if (selector.match() == CSSSelector::PseudoClass) {
        auto type = selector.pseudoClassType();
        // For :nth-child(n of .some-subselector) where an element change may affect other elements similar to sibling combinators.
        if (type == CSSSelector::PseudoClassNthChild || type == CSSSelector::PseudoClassNthLastChild)
            return MatchElement::AnySibling;

        // Similarly for :host().
        if (type == CSSSelector::PseudoClassHost)
            return MatchElement::Host;

        if (type == CSSSelector::PseudoClassHas) {
            if (matchElement != MatchElement::Subject)
                return MatchElement::HasNonSubjectOrScopeBreaking;
            return computeHasPseudoClassMatchElement(childSelector);
        }

    }
    if (selector.match() == CSSSelector::PseudoElement) {
        // Similarly for ::slotted().
        if (selector.pseudoElementType() == CSSSelector::PseudoElementSlotted)
            return MatchElement::Host;
    }

    return matchElement;
};

void RuleFeatureSet::recursivelyCollectFeaturesFromSelector(SelectorFeatures& selectorFeatures, const CSSSelector& firstSelector, MatchElement matchElement, IsNegation isNegation, CanBreakScope canBreakScope)
{
    const CSSSelector* selector = &firstSelector;
    do {
        if (selector->match() == CSSSelector::Id) {
            idsInRules.add(selector->value());
            if (matchElement == MatchElement::Parent || matchElement == MatchElement::Ancestor)
                idsMatchingAncestorsInRules.add(selector->value());
            else if (isHasPseudoClassMatchElement(matchElement))
                selectorFeatures.ids.append({ selector, matchElement, isNegation });
        } else if (selector->match() == CSSSelector::Class)
            selectorFeatures.classes.append({ selector, matchElement, isNegation });
        else if (selector->isAttributeSelector()) {
            auto& canonicalLocalName = selector->attributeCanonicalLocalName();
            auto& localName = selector->attribute().localName();
            attributeCanonicalLocalNamesInRules.add(canonicalLocalName);
            attributeLocalNamesInRules.add(localName);
            selectorFeatures.attributes.append({ selector, matchElement, isNegation });
        } else if (selector->match() == CSSSelector::PseudoElement) {
            switch (selector->pseudoElementType()) {
            case CSSSelector::PseudoElementFirstLine:
                usesFirstLineRules = true;
                break;
            case CSSSelector::PseudoElementFirstLetter:
                usesFirstLetterRules = true;
                break;
            default:
                break;
            }
        } else if (selector->match() == CSSSelector::PseudoClass) {
            bool isLogicalCombination = isLogicalCombinationPseudoClass(selector->pseudoClassType());
            if (!isLogicalCombination)
                selectorFeatures.pseudoClasses.append({ selector, matchElement, isNegation });
            canBreakScope = isLogicalCombination && selector->pseudoClassType() != CSSSelector::PseudoClassHas ? CanBreakScope::Yes : CanBreakScope::No;
        }

        if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
            selectorFeatures.hasSiblingSelector = true;

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            auto subSelectorIsNegation = isNegation;
            if (selector->match() == CSSSelector::PseudoClass && selector->pseudoClassType() == CSSSelector::PseudoClassNot)
                subSelectorIsNegation = isNegation == IsNegation::No ? IsNegation::Yes : IsNegation::No;

            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                auto subSelectorMatchElement = computeSubSelectorMatchElement(matchElement, *selector, *subSelector);
                if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
                    selectorFeatures.hasSiblingSelector = true;
                recursivelyCollectFeaturesFromSelector(selectorFeatures, *subSelector, subSelectorMatchElement, subSelectorIsNegation, canBreakScope);

                if (selector->match() == CSSSelector::PseudoClass && selector->pseudoClassType() == CSSSelector::PseudoClassHas)
                    selectorFeatures.hasPseudoClasses.append({ subSelector, subSelectorMatchElement, isNegation });
            }
        }

        matchElement = [&] {
            if (isHasPseudoClassMatchElement(matchElement))
                return computeNextHasPseudoClassMatchElement(matchElement, selector->relation(), canBreakScope);
            return computeNextMatchElement(matchElement, selector->relation());
        }();

        selector = selector->tagHistory();
    } while (selector);
}

PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClassType pseudoClass, InvalidationKeyType keyType, const AtomString& keyString)
{
    ASSERT(keyType != InvalidationKeyType::Universal || keyString == starAtom());
    return {
        pseudoClass,
        static_cast<uint8_t>(keyType),
        keyString
    };
};

static PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClassType pseudoClassType, const CSSSelector& selector)
{
    AtomString className;
    AtomString tagName;
    for (auto* simpleSelector = selector.firstInCompound(); simpleSelector; simpleSelector = simpleSelector->tagHistory()) {
        if (simpleSelector->match() == CSSSelector::Id)
            return makePseudoClassInvalidationKey(pseudoClassType, InvalidationKeyType::Id, simpleSelector->value());

        if (simpleSelector->match() == CSSSelector::Class && className.isNull())
            className = simpleSelector->value();

        if (simpleSelector->match() == CSSSelector::Tag)
            tagName = simpleSelector->tagLowercaseLocalName();

        if (simpleSelector->relation() != CSSSelector::Subselector)
            break;
    }
    if (!className.isEmpty())
        return makePseudoClassInvalidationKey(pseudoClassType, InvalidationKeyType::Class, className);

    if (!tagName.isEmpty() && tagName != starAtom())
        return makePseudoClassInvalidationKey(pseudoClassType, InvalidationKeyType::Tag, tagName);

    return makePseudoClassInvalidationKey(pseudoClassType, InvalidationKeyType::Universal);
};

void RuleFeatureSet::collectFeatures(const RuleData& ruleData)
{
    SelectorFeatures selectorFeatures;
    recursivelyCollectFeaturesFromSelector(selectorFeatures, *ruleData.selector());
    if (selectorFeatures.hasSiblingSelector)
        siblingRules.append({ ruleData });
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append({ ruleData });

    auto addToMap = [&](auto& map, auto& entries, auto hostAffectingNames) {
        for (auto& entry : entries) {
            auto& [selector, matchElement, isNegation] = entry;
            auto& name = selector->value();

            map.ensure(name, [] {
                return makeUnique<RuleFeatureVector>();
            }).iterator->value->append({ ruleData, matchElement, isNegation });

            setUsesMatchElement(matchElement);

            if constexpr (!std::is_same_v<std::nullptr_t, decltype(hostAffectingNames)>) {
                if (matchElement == MatchElement::Host)
                    hostAffectingNames->add(name);
            }
        }
    };

    addToMap(idRules, selectorFeatures.ids, nullptr);
    addToMap(classRules, selectorFeatures.classes, &classesAffectingHost);

    for (auto& entry : selectorFeatures.attributes) {
        auto& [selector, matchElement, isNegation] = entry;
        attributeRules.ensure(selector->attribute().localName().convertToASCIILowercase(), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value->append({ ruleData, matchElement, isNegation, selector });
        if (matchElement == MatchElement::Host)
            attributesAffectingHost.add(selector->attribute().localName().convertToASCIILowercase());
        setUsesMatchElement(matchElement);
    }

    for (auto& entry : selectorFeatures.pseudoClasses) {
        auto& [selector, matchElement, isNegation] = entry;
        pseudoClassRules.ensure(makePseudoClassInvalidationKey(selector->pseudoClassType(), *selector), [] {
            return makeUnique<Vector<RuleFeature>>();
        }).iterator->value->append({ ruleData, matchElement, isNegation });

        if (matchElement == MatchElement::Host)
            pseudoClassesAffectingHost.add(selector->pseudoClassType());
        pseudoClassTypes.add(selector->pseudoClassType());

        setUsesMatchElement(matchElement);
    }

    for (auto& entry : selectorFeatures.hasPseudoClasses) {
        auto& [selector, matchElement, isNegation] = entry;
        // The selector argument points to a selector inside :has() selector list instead of :has() itself.
        hasPseudoClassRules.ensure(makePseudoClassInvalidationKey(CSSSelector::PseudoClassHas, *selector), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value->append({ ruleData, matchElement, isNegation, selector });

        setUsesMatchElement(matchElement);
    }
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    idsInRules.add(other.idsInRules.begin(), other.idsInRules.end());
    idsMatchingAncestorsInRules.add(other.idsMatchingAncestorsInRules.begin(), other.idsMatchingAncestorsInRules.end());
    attributeCanonicalLocalNamesInRules.add(other.attributeCanonicalLocalNamesInRules.begin(), other.attributeCanonicalLocalNamesInRules.end());
    attributeLocalNamesInRules.add(other.attributeLocalNamesInRules.begin(), other.attributeLocalNamesInRules.end());
    contentAttributeNamesInRules.add(other.contentAttributeNamesInRules.begin(), other.contentAttributeNamesInRules.end());
    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);

    auto addMap = [&](auto& map, auto& otherMap) {
        for (auto& keyValuePair : otherMap) {
            map.ensure(keyValuePair.key, [] {
                return makeUnique<std::decay_t<decltype(*keyValuePair.value)>>();
            }).iterator->value->appendVector(*keyValuePair.value);
        }
    };

    addMap(idRules, other.idRules);

    addMap(classRules, other.classRules);
    classesAffectingHost.add(other.classesAffectingHost.begin(), other.classesAffectingHost.end());

    addMap(attributeRules, other.attributeRules);
    attributesAffectingHost.add(other.attributesAffectingHost.begin(), other.attributesAffectingHost.end());

    addMap(pseudoClassRules, other.pseudoClassRules);
    pseudoClassesAffectingHost.add(other.pseudoClassesAffectingHost.begin(), other.pseudoClassesAffectingHost.end());
    pseudoClassTypes.add(other.pseudoClassTypes.begin(), other.pseudoClassTypes.end());

    addMap(hasPseudoClassRules, other.hasPseudoClassRules);

    for (size_t i = 0; i < usedMatchElements.size(); ++i)
        usedMatchElements[i] = usedMatchElements[i] || other.usedMatchElements[i];

    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesFirstLetterRules = usesFirstLetterRules || other.usesFirstLetterRules;
}

void RuleFeatureSet::registerContentAttribute(const AtomString& attributeName)
{
    contentAttributeNamesInRules.add(attributeName.convertToASCIILowercase());
    attributeCanonicalLocalNamesInRules.add(attributeName);
    attributeLocalNamesInRules.add(attributeName);
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    idsMatchingAncestorsInRules.clear();
    attributeCanonicalLocalNamesInRules.clear();
    attributeLocalNamesInRules.clear();
    contentAttributeNamesInRules.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
    idRules.clear();
    classRules.clear();
    hasPseudoClassRules.clear();
    classesAffectingHost.clear();
    attributeRules.clear();
    attributesAffectingHost.clear();
    pseudoClassRules.clear();
    pseudoClassesAffectingHost.clear();
    pseudoClassTypes.clear();
    usesFirstLineRules = false;
    usesFirstLetterRules = false;
}

void RuleFeatureSet::shrinkToFit()
{
    siblingRules.shrinkToFit();
    uncommonAttributeRules.shrinkToFit();
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
