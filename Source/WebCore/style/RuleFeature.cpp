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
    case MatchElement::HasNonSubject:
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
    case MatchElement::HasNonSubject:
        return true;
    default:
        return false;
    }
}

RuleFeature::RuleFeature(const RuleData& ruleData, std::optional<MatchElement> matchElement)
    : styleRule(&ruleData.styleRule())
    , selectorIndex(ruleData.selectorIndex())
    , selectorListIndex(ruleData.selectorListIndex())
    , matchElement(matchElement)
{
    ASSERT(selectorIndex == ruleData.selectorIndex());
    ASSERT(selectorListIndex == ruleData.selectorListIndex());
}

static MatchElement computeNextMatchElement(MatchElement matchElement, CSSSelector::RelationType relation)
{
    if (isHasPseudoClassMatchElement(matchElement))
        return matchElement;

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

MatchElement computeHasPseudoClassMatchElement(const CSSSelector& hasSelector)
{
    auto hasMatchElement = MatchElement::Subject;
    for (auto* simpleSelector = &hasSelector; simpleSelector->tagHistory(); simpleSelector = simpleSelector->tagHistory())
        hasMatchElement = computeNextMatchElement(hasMatchElement, simpleSelector->relation());

    if (hasMatchElement == MatchElement::Parent)
        return MatchElement::HasChild;

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
    case MatchElement::HasNonSubject:
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
                return MatchElement::HasNonSubject;
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

void RuleFeatureSet::recursivelyCollectFeaturesFromSelector(SelectorFeatures& selectorFeatures, const CSSSelector& firstSelector, MatchElement matchElement)
{
    const CSSSelector* selector = &firstSelector;
    do {
        if (selector->match() == CSSSelector::Id) {
            idsInRules.add(selector->value());
            if (matchElement == MatchElement::Parent || matchElement == MatchElement::Ancestor)
                idsMatchingAncestorsInRules.add(selector->value());
            else if (isHasPseudoClassMatchElement(matchElement))
                selectorFeatures.ids.append(std::make_pair(selector->value(), matchElement));
        } else if (selector->match() == CSSSelector::Class)
            selectorFeatures.classes.append(std::make_pair(selector->value(), matchElement));
        else if (selector->match() == CSSSelector::Tag) {
            if (isHasPseudoClassMatchElement(matchElement))
                selectorFeatures.tags.append(std::make_pair(selector->tagLowercaseLocalName(), matchElement));
        } else if (selector->isAttributeSelector()) {
            auto& canonicalLocalName = selector->attributeCanonicalLocalName();
            auto& localName = selector->attribute().localName();
            attributeCanonicalLocalNamesInRules.add(canonicalLocalName);
            attributeLocalNamesInRules.add(localName);
            selectorFeatures.attributes.append(std::make_pair(selector, matchElement));
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
            if (!isLogicalCombinationPseudoClass(selector->pseudoClassType()))
                selectorFeatures.pseudoClasses.append(std::make_pair(selector, matchElement));
        }

        if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
            selectorFeatures.hasSiblingSelector = true;

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                auto subSelectorMatchElement = computeSubSelectorMatchElement(matchElement, *selector, *subSelector);
                if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
                    selectorFeatures.hasSiblingSelector = true;
                recursivelyCollectFeaturesFromSelector(selectorFeatures, *subSelector, subSelectorMatchElement);
            }
        }

        matchElement = computeNextMatchElement(matchElement, selector->relation());

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

static PseudoClassInvalidationKey makePseudoClassInvalidationKey(const CSSSelector& selector)
{
    ASSERT(selector.match() == CSSSelector::PseudoClass);

    auto pseudoClassType = selector.pseudoClassType();

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

    return makePseudoClassInvalidationKey(selector.pseudoClassType(), InvalidationKeyType::Universal);
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
            auto& [name, matchElement] = entry;
            map.ensure(name, [] {
                return makeUnique<RuleFeatureVector>();
            }).iterator->value->append({ ruleData, matchElement });

            setUsesMatchElement(matchElement);

            if constexpr (!std::is_same_v<std::nullptr_t, decltype(hostAffectingNames)>) {
                if (matchElement == MatchElement::Host)
                    hostAffectingNames->add(name);
            }
        }
    };

    addToMap(tagRules, selectorFeatures.tags, nullptr);
    addToMap(idRules, selectorFeatures.ids, nullptr);
    addToMap(classRules, selectorFeatures.classes, &classesAffectingHost);

    for (auto& selectorAndMatch : selectorFeatures.attributes) {
        auto* selector = selectorAndMatch.first;
        auto matchElement = selectorAndMatch.second;
        attributeRules.ensure(selector->attribute().localName().convertToASCIILowercase(), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value->append({ ruleData, matchElement, selector });
        if (matchElement == MatchElement::Host)
            attributesAffectingHost.add(selector->attribute().localName().convertToASCIILowercase());
        setUsesMatchElement(matchElement);
    }

    for (auto& selectorAndMatch : selectorFeatures.pseudoClasses) {
        auto* selector = selectorAndMatch.first;
        auto matchElement = selectorAndMatch.second;
        pseudoClassRules.ensure(makePseudoClassInvalidationKey(*selector), [] {
            return makeUnique<Vector<RuleFeature>>();
        }).iterator->value->append({ ruleData, matchElement });

        if (matchElement == MatchElement::Host)
            pseudoClassesAffectingHost.add(selector->pseudoClassType());
        pseudoClassTypes.add(selector->pseudoClassType());

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

    addMap(tagRules, other.tagRules);
    addMap(idRules, other.idRules);

    addMap(classRules, other.classRules);
    classesAffectingHost.add(other.classesAffectingHost.begin(), other.classesAffectingHost.end());

    addMap(attributeRules, other.attributeRules);
    attributesAffectingHost.add(other.attributesAffectingHost.begin(), other.attributesAffectingHost.end());

    addMap(pseudoClassRules, other.pseudoClassRules);
    pseudoClassesAffectingHost.add(other.pseudoClassesAffectingHost.begin(), other.pseudoClassesAffectingHost.end());
    pseudoClassTypes.add(other.pseudoClassTypes.begin(), other.pseudoClassTypes.end());

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
    tagRules.clear();
    idRules.clear();
    classRules.clear();
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
    for (auto& rules : tagRules.values())
        rules->shrinkToFit();
    for (auto& rules : idRules.values())
        rules->shrinkToFit();
    for (auto& rules : classRules.values())
        rules->shrinkToFit();
    for (auto& rules : attributeRules.values())
        rules->shrinkToFit();
    for (auto& rules : pseudoClassRules.values())
        rules->shrinkToFit();
}

} // namespace Style
} // namespace WebCore
