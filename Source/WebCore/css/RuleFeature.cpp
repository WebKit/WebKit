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

namespace WebCore {

static void recursivelyCollectFeaturesFromSelector(RuleFeatureSet& features, const CSSSelector& firstSelector, bool& hasSiblingSelector)
{
    const CSSSelector* selector = &firstSelector;
    do {
        if (selector->match() == CSSSelector::Id)
            features.idsInRules.add(selector->value().impl());
        else if (selector->match() == CSSSelector::Class)
            features.classesInRules.add(selector->value().impl());
        else if (selector->isAttributeSelector()) {
            features.attributeCanonicalLocalNamesInRules.add(selector->attributeCanonicalLocalName().impl());
            features.attributeLocalNamesInRules.add(selector->attribute().localName().impl());
        } else if (selector->match() == CSSSelector::PseudoElement) {
            switch (selector->pseudoElementType()) {
            case CSSSelector::PseudoElementFirstLine:
                features.usesFirstLineRules = true;
                break;
            case CSSSelector::PseudoElementFirstLetter:
                features.usesFirstLetterRules = true;
                break;
            default:
                break;
            }
        }

        if (!hasSiblingSelector && selector->isSiblingSelector())
            hasSiblingSelector = true;

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (!hasSiblingSelector && selector->isSiblingSelector())
                    hasSiblingSelector = true;
                recursivelyCollectFeaturesFromSelector(features, *subSelector, hasSiblingSelector);
            }
        }

        selector = selector->tagHistory();
    } while (selector);
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& firstSelector, bool& hasSiblingSelector)
{
    hasSiblingSelector = false;
    recursivelyCollectFeaturesFromSelector(*this, firstSelector, hasSiblingSelector);
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    idsInRules.add(other.idsInRules.begin(), other.idsInRules.end());
    classesInRules.add(other.classesInRules.begin(), other.classesInRules.end());
    attributeCanonicalLocalNamesInRules.add(other.attributeCanonicalLocalNamesInRules.begin(), other.attributeCanonicalLocalNamesInRules.end());
    attributeLocalNamesInRules.add(other.attributeLocalNamesInRules.begin(), other.attributeLocalNamesInRules.end());
    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesFirstLetterRules = usesFirstLetterRules || other.usesFirstLetterRules;
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    classesInRules.clear();
    attributeCanonicalLocalNamesInRules.clear();
    attributeLocalNamesInRules.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
    usesFirstLineRules = false;
    usesFirstLetterRules = false;
}

void RuleFeatureSet::shrinkToFit()
{
    siblingRules.shrinkToFit();
    uncommonAttributeRules.shrinkToFit();
}

} // namespace WebCore
