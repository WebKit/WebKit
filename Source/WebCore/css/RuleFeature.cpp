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

void RuleFeatureSet::recursivelyCollectFeaturesFromSelector(SelectorFeatures& selectorFeatures, const CSSSelector& firstSelector, bool matchesAncestor)
{
    const CSSSelector* selector = &firstSelector;
    do {
        if (selector->match() == CSSSelector::Id) {
            idsInRules.add(selector->value());
            if (matchesAncestor)
                idsMatchingAncestorsInRules.add(selector->value());
        } else if (selector->match() == CSSSelector::Class) {
            classesInRules.add(selector->value());
            if (matchesAncestor)
                selectorFeatures.classesMatchingAncestors.append(selector->value());
        } else if (selector->isAttributeSelector()) {
            auto& canonicalLocalName = selector->attributeCanonicalLocalName();
            auto& localName = selector->attribute().localName();
            attributeCanonicalLocalNamesInRules.add(canonicalLocalName);
            attributeLocalNamesInRules.add(localName);
            if (matchesAncestor)
                selectorFeatures.attributeSelectorsMatchingAncestors.append(selector);
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
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (!selectorFeatures.hasSiblingSelector && selector->isSiblingSelector())
                    selectorFeatures.hasSiblingSelector = true;
                recursivelyCollectFeaturesFromSelector(selectorFeatures, *subSelector, matchesAncestor);
            }
        }
        if (selector->hasDescendantOrChildRelation())
            matchesAncestor = true;

        selector = selector->tagHistory();
    } while (selector);
}

static RuleFeatureSet::AttributeRules::SelectorKey makeAttributeSelectorKey(const CSSSelector& selector)
{
    bool caseInsensitive = selector.attributeValueMatchingIsCaseInsensitive();
    unsigned matchAndCase = static_cast<unsigned>(selector.match()) << 1 | (caseInsensitive ? 1 : 0);
    return std::make_pair(selector.attributeCanonicalLocalName().impl(), std::make_pair(selector.value().impl(), matchAndCase));
}

void RuleFeatureSet::collectFeatures(const RuleData& ruleData)
{
    SelectorFeatures selectorFeatures;
    recursivelyCollectFeaturesFromSelector(selectorFeatures, *ruleData.selector());
    if (selectorFeatures.hasSiblingSelector)
        siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex()));
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex()));
    for (auto& className : selectorFeatures.classesMatchingAncestors) {
        auto addResult = ancestorClassRules.ensure(className, [] {
            return std::make_unique<Vector<RuleFeature>>();
        });
        addResult.iterator->value->append(RuleFeature(ruleData.rule(), ruleData.selectorIndex()));
    }
    for (auto* selector : selectorFeatures.attributeSelectorsMatchingAncestors) {
        // Hashing by attributeCanonicalLocalName makes this HTML specific.
        auto addResult = ancestorAttributeRulesForHTML.ensure(selector->attributeCanonicalLocalName(), [] {
            return std::make_unique<AttributeRules>();
        });
        auto& rules = *addResult.iterator->value;
        rules.features.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex()));
        // Deduplicate selectors.
        rules.selectors.add(makeAttributeSelectorKey(*selector), selector);
    }
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    idsInRules.add(other.idsInRules.begin(), other.idsInRules.end());
    idsMatchingAncestorsInRules.add(other.idsMatchingAncestorsInRules.begin(), other.idsMatchingAncestorsInRules.end());
    classesInRules.add(other.classesInRules.begin(), other.classesInRules.end());
    attributeCanonicalLocalNamesInRules.add(other.attributeCanonicalLocalNamesInRules.begin(), other.attributeCanonicalLocalNamesInRules.end());
    attributeLocalNamesInRules.add(other.attributeLocalNamesInRules.begin(), other.attributeLocalNamesInRules.end());
    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);
    for (auto& keyValuePair : other.ancestorClassRules) {
        auto addResult = ancestorClassRules.ensure(keyValuePair.key, [] {
            return std::make_unique<Vector<RuleFeature>>();
        });
        addResult.iterator->value->appendVector(*keyValuePair.value);
    }
    for (auto& keyValuePair : other.ancestorAttributeRulesForHTML) {
        auto addResult = ancestorAttributeRulesForHTML.ensure(keyValuePair.key, [] {
            return std::make_unique<AttributeRules>();
        });
        auto& rules = *addResult.iterator->value;
        rules.features.appendVector(keyValuePair.value->features);
        for (auto& selectorPair : keyValuePair.value->selectors)
            rules.selectors.add(selectorPair.key, selectorPair.value);
    }
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesFirstLetterRules = usesFirstLetterRules || other.usesFirstLetterRules;
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    idsMatchingAncestorsInRules.clear();
    classesInRules.clear();
    attributeCanonicalLocalNamesInRules.clear();
    attributeLocalNamesInRules.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
    ancestorClassRules.clear();
    ancestorAttributeRulesForHTML.clear();
    usesFirstLineRules = false;
    usesFirstLetterRules = false;
}

void RuleFeatureSet::shrinkToFit()
{
    siblingRules.shrinkToFit();
    uncommonAttributeRules.shrinkToFit();
    for (auto& rules : ancestorClassRules.values())
        rules->shrinkToFit();
    for (auto& rules : ancestorAttributeRulesForHTML.values())
        rules->features.shrinkToFit();
}

} // namespace WebCore
