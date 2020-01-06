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
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class StyleRule;

namespace Style {

class RuleData;

enum class MatchElement : uint8_t { Subject, Parent, Ancestor, DirectSibling, IndirectSibling, AnySibling, ParentSibling, AncestorSibling, Host };
constexpr unsigned matchElementCount = static_cast<unsigned>(MatchElement::Host) + 1;

struct RuleFeature {
    RuleFeature(const RuleData&, Optional<MatchElement> = WTF::nullopt);

    RefPtr<const StyleRule> styleRule;
    uint16_t selectorIndex; // Keep in sync with RuleData's selectorIndex size.
    uint16_t selectorListIndex; // Keep in sync with RuleData's selectorListIndex size.
    Optional<MatchElement> matchElement { };
};
static_assert(sizeof(RuleFeature) <= 16, "RuleFeature is a frquently alocated object. Keep it small.");

struct RuleFeatureWithInvalidationSelector : public RuleFeature {
    RuleFeatureWithInvalidationSelector(const RuleData& data, Optional<MatchElement> matchElement = WTF::nullopt, const CSSSelector* invalidationSelector = nullptr)
        : RuleFeature(data, WTFMove(matchElement))
        , invalidationSelector(invalidationSelector)
    { }

    const CSSSelector* invalidationSelector { nullptr };
};

struct RuleFeatureSet {
    void add(const RuleFeatureSet&);
    void clear();
    void shrinkToFit();
    void collectFeatures(const RuleData&);
    void registerContentAttribute(const AtomString&);

    HashSet<AtomString> idsInRules;
    HashSet<AtomString> idsMatchingAncestorsInRules;
    HashSet<AtomString> attributeCanonicalLocalNamesInRules;
    HashSet<AtomString> attributeLocalNamesInRules;
    HashSet<AtomString> contentAttributeNamesInRules;
    Vector<RuleFeature> siblingRules;
    Vector<RuleFeature> uncommonAttributeRules;
    
    HashMap<AtomString, std::unique_ptr<Vector<RuleFeature>>> classRules;
    HashMap<AtomString, std::unique_ptr<Vector<RuleFeatureWithInvalidationSelector>>> attributeRules;
    HashSet<AtomString> classesAffectingHost;
    HashSet<AtomString> attributesAffectingHost;

    bool usesFirstLineRules { false };
    bool usesFirstLetterRules { false };

private:
    static MatchElement computeNextMatchElement(MatchElement, CSSSelector::RelationType);
    static MatchElement computeSubSelectorMatchElement(MatchElement, const CSSSelector&);

    struct SelectorFeatures {
        bool hasSiblingSelector { false };

        Vector<std::pair<AtomString, MatchElement>, 32> classes;
        Vector<std::pair<const CSSSelector*, MatchElement>, 32> attributes;
    };
    void recursivelyCollectFeaturesFromSelector(SelectorFeatures&, const CSSSelector&, MatchElement = MatchElement::Subject);
};

} // namespace Style
} // namespace WebCore
