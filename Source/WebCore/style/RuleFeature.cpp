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
    case MatchElement::Host:
        return true;
    case MatchElement::Parent:
    case MatchElement::Ancestor:
    case MatchElement::ParentSibling:
    case MatchElement::AncestorSibling:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

RuleFeature::RuleFeature(const RuleData& ruleData, Optional<MatchElement> matchElement)
    : styleRule(&ruleData.styleRule())
    , selectorIndex(ruleData.selectorIndex())
    , selectorListIndex(ruleData.selectorListIndex())
    , matchElement(matchElement)
{
    ASSERT(selectorIndex == ruleData.selectorIndex());
    ASSERT(selectorListIndex == ruleData.selectorListIndex());
}

MatchElement RuleFeatureSet::computeNextMatchElement(MatchElement matchElement, CSSSelector::RelationType relation)
{
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
            return MatchElement::Host;
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
        return MatchElement::Host;
    };
    ASSERT_NOT_REACHED();
    return matchElement;
};

MatchElement RuleFeatureSet::computeSubSelectorMatchElement(MatchElement matchElement, const CSSSelector& selector)
{
    ASSERT(selector.selectorList());

    if (selector.match() == CSSSelector::PseudoClass) {
        auto type = selector.pseudoClassType();
        // For :nth-child(n of .some-subselector) where an element change may affect other elements similar to sibling combinators.
        if (type == CSSSelector::PseudoClassNthChild || type == CSSSelector::PseudoClassNthLastChild)
            return MatchElement::AnySibling;

        // Similarly for :host().
        if (type == CSSSelector::PseudoClassHost)
            return MatchElement::Host;
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
        } else if (selector->match() == CSSSelector::Class)
            selectorFeatures.classes.append(std::make_pair(selector->value(), matchElement));
        else if (selector->isAttributeSelector()) {
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
        }

        if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
            selectorFeatures.hasSiblingSelector = true;

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            auto subSelectorMatchElement = computeSubSelectorMatchElement(matchElement, *selector);

            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
                    selectorFeatures.hasSiblingSelector = true;
                recursivelyCollectFeaturesFromSelector(selectorFeatures, *subSelector, subSelectorMatchElement);
            }
        }

        matchElement = computeNextMatchElement(matchElement, selector->relation());

        selector = selector->tagHistory();
    } while (selector);
}

void RuleFeatureSet::collectFeatures(const RuleData& ruleData)
{
    SelectorFeatures selectorFeatures;
    recursivelyCollectFeaturesFromSelector(selectorFeatures, *ruleData.selector());
    if (selectorFeatures.hasSiblingSelector)
        siblingRules.append({ ruleData });
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append({ ruleData });

    for (auto& nameAndMatch : selectorFeatures.classes) {
        classRules.ensure(nameAndMatch.first, [] {
            return makeUnique<Vector<RuleFeature>>();
        }).iterator->value->append({ ruleData, nameAndMatch.second });
        if (nameAndMatch.second == MatchElement::Host)
            classesAffectingHost.add(nameAndMatch.first);
    }
    for (auto& selectorAndMatch : selectorFeatures.attributes) {
        auto* selector = selectorAndMatch.first;
        auto matchElement = selectorAndMatch.second;
        attributeRules.ensure(selector->attribute().localName().convertToASCIILowercase(), [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value->append({ ruleData, matchElement, selector });
        if (matchElement == MatchElement::Host)
            attributesAffectingHost.add(selector->attribute().localName().convertToASCIILowercase());
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
    for (auto& keyValuePair : other.classRules) {
        classRules.ensure(keyValuePair.key, [] {
            return makeUnique<Vector<RuleFeature>>();
        }).iterator->value->appendVector(*keyValuePair.value);
    }
    classesAffectingHost.add(other.classesAffectingHost.begin(), other.classesAffectingHost.end());

    for (auto& keyValuePair : other.attributeRules) {
        attributeRules.ensure(keyValuePair.key, [] {
            return makeUnique<Vector<RuleFeatureWithInvalidationSelector>>();
        }).iterator->value->appendVector(*keyValuePair.value);
    }
    attributesAffectingHost.add(other.attributesAffectingHost.begin(), other.attributesAffectingHost.end());

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
    classRules.clear();
    classesAffectingHost.clear();
    attributeRules.clear();
    attributesAffectingHost.clear();
    usesFirstLineRules = false;
    usesFirstLetterRules = false;
}

void RuleFeatureSet::shrinkToFit()
{
    siblingRules.shrinkToFit();
    uncommonAttributeRules.shrinkToFit();
    for (auto& rules : classRules.values())
        rules->shrinkToFit();
    for (auto& rules : attributeRules.values())
        rules->shrinkToFit();
}

} // namespace Style
} // namespace WebCore
