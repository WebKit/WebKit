/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2011, 2014 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "CSSSelector.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class RuleData;
class StyleRule;

enum class MatchElement { Subject, Parent, Ancestor, DirectSibling, IndirectSibling, AnySibling, ParentSibling, AncestorSibling, Host };
constexpr unsigned matchElementCount = static_cast<unsigned>(MatchElement::Host) + 1;

struct RuleFeature {
    RuleFeature(StyleRule* rule, unsigned selectorIndex, unsigned selectorListIndex, Optional<MatchElement> matchElement = WTF::nullopt, const CSSSelector* invalidationSelector = nullptr)
        : rule(rule)
        , selectorIndex(selectorIndex)
        , selectorListIndex(selectorListIndex)
        , matchElement(matchElement)
        , invalidationSelector(invalidationSelector)
    {
    }
    StyleRule* rule;
    unsigned selectorIndex;
    unsigned selectorListIndex;
    Optional<MatchElement> matchElement;
    const CSSSelector* invalidationSelector;
};

struct RuleFeatureSet {
    void add(const RuleFeatureSet&);
    void clear();
    void shrinkToFit();
    void collectFeatures(const RuleData&);
    void registerContentAttribute(const AtomicString&);

    HashSet<AtomicString> idsInRules;
    HashSet<AtomicString> idsMatchingAncestorsInRules;
    HashSet<AtomicString> attributeCanonicalLocalNamesInRules;
    HashSet<AtomicString> attributeLocalNamesInRules;
    HashSet<AtomicString> contentAttributeNamesInRules;
    Vector<RuleFeature> siblingRules;
    Vector<RuleFeature> uncommonAttributeRules;
    
    HashMap<AtomicString, std::unique_ptr<Vector<RuleFeature>>> classRules;
    HashMap<AtomicString, std::unique_ptr<Vector<RuleFeature>>> attributeRules;
    HashSet<AtomicString> classesAffectingHost;
    HashSet<AtomicString> attributesAffectingHost;

    bool usesFirstLineRules { false };
    bool usesFirstLetterRules { false };

private:
    static MatchElement computeNextMatchElement(MatchElement, CSSSelector::RelationType);
    static MatchElement computeSubSelectorMatchElement(MatchElement, const CSSSelector&);

    struct SelectorFeatures {
        bool hasSiblingSelector { false };

        Vector<std::pair<AtomicString, MatchElement>, 32> classes;
        Vector<std::pair<const CSSSelector*, MatchElement>, 32> attributes;
    };
    void recursivelyCollectFeaturesFromSelector(SelectorFeatures&, const CSSSelector&, MatchElement = MatchElement::Subject);
};

} // namespace WebCore
