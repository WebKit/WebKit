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

#include "CSSSelectorList.h"
#include "CommonAtomStrings.h"
#include <wtf/Forward.h>
#include <wtf/GenericHashKey.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Markable.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class StyleRule;
class StyleRuleScope;

namespace Style {

class RuleData;

// MatchElement characterizes which elements a change in an element matched by a simple selector (as a part of a complex selector) may affect.
// Style::Invalidator uses these classifications to traverse a minimal number of elements after a DOM mutation.
// In the examples below the '.changed' simple selector will be classified with the given enum value.
// For :has() features, the matchElement field describes the position of :has() in the overall selector,
// while hasRelation describes the changed element's relationship to the :has() scope element.
struct MatchElement {
    enum class Relation : uint8_t {
        Subject, // .changed
        Parent, // .changed > .subject
        Ancestor, // .changed .subject
        DirectSibling, // .changed + .subject
        IndirectSibling, // .changed ~ .subject
        AnySibling, // :nth-last-child(even of .changed)
        ParentSibling, // .changed ~ .a > .subject
        AncestorSibling, // .changed ~ .a .subject
        ParentAnySibling, // :nth-last-child(even of .changed) > .subject
        AncestorAnySibling, // :nth-last-child(even of .changed) .subject
        Host, // :host(.changed) .subject
        HostChild // ::slotted(.changed)
    };

    // Relationship of the :has() argument subject element to the :has() scope element.
    enum class HasRelation : uint8_t {
        Child, // :has(> .changed)
        Descendant, // :has(.changed)
        DirectSibling, // :has(+ .changed)
        IndirectSibling, // :has(~ .changed)
        SiblingChild, // :has(~ .a > .changed)
        SiblingDescendant, // :has(~ .a .changed)
        ScopeBreaking // :has(:is(.changed .a))
    };

    Relation relation;
    Markable<HasRelation> hasRelation;

    bool operator==(const MatchElement&) const = default;
    unsigned hash() const { return pairIntHash(static_cast<unsigned>(relation), hasRelation ? static_cast<unsigned>(*hasRelation) : 255u); }
};
constexpr unsigned matchRelationCount = static_cast<unsigned>(MatchElement::Relation::HostChild) + 1;

enum class IsNegation : bool { No, Yes };

// For MSVC.
#pragma pack(push, 4)
struct RuleAndSelector {
    RuleAndSelector(const RuleData&);

    RefPtr<const StyleRule> styleRule;
    uint16_t selectorIndex; // Keep in sync with RuleData's selectorIndex size.
    uint16_t selectorListIndex; // Keep in sync with RuleData's selectorListIndex size.

    const CSSSelector& NODELETE selector() const;
};

struct RuleFeature : public RuleAndSelector {
    RuleFeature(const RuleData&, MatchElement, IsNegation);

    MatchElement matchElement;
    IsNegation isNegation; // Whether the selector is in a (non-paired) :not() context.
};
static_assert(sizeof(RuleFeature) <= 16, "RuleFeature is a frequently allocated object. Keep it small.");

struct RuleFeatureWithInvalidationSelector : public RuleFeature {
    RuleFeatureWithInvalidationSelector(const RuleData&, MatchElement, IsNegation, CSSSelectorList&& invalidationSelector, CSSSelectorList&& scopeSelector = { });

    CSSSelectorList invalidationSelector;
    CSSSelectorList scopeSelector;
};
#pragma pack(pop)

using PseudoClassInvalidationKey = std::tuple<unsigned, uint8_t, AtomString>;

using RuleFeatureVector = Vector<RuleFeature>;

struct SelectorDeduplicationKey {
    SelectorDeduplicationKey(const CSSSelector&);

    const CSSSelector* selector;
    unsigned cachedHash;

    unsigned hash() const { return cachedHash; }
    bool operator==(const SelectorDeduplicationKey&) const;
};

struct RuleFeatureSet {
    void add(const RuleFeatureSet&);
    void clear();
    void shrinkToFit();

    struct CollectionContext {
        HashSet<GenericHashKey<SelectorDeduplicationKey>> selectorDeduplicationSet;
    };
    void collectFeatures(CollectionContext&, const RuleData&, const Vector<Ref<const StyleRuleScope>>& scopeRules = { });
    void registerSubstitutionAttribute(const AtomString&);

    bool usesRelation(MatchElement::Relation relation) const { return usedRelations[std::to_underlying(relation)]; }
    void setUsesRelation(MatchElement::Relation relation) { usedRelations[std::to_underlying(relation)] = true; }

    HashSet<AtomString> idsInRules;
    HashSet<AtomString> idsMatchingAncestorsInRules;
    HashSet<AtomString> attributeLowercaseLocalNamesInRules;
    HashSet<AtomString> attributeLocalNamesInRules;
    HashSet<AtomString> substitutionAttributeNamesInRules;

    HashMap<AtomString, std::unique_ptr<RuleFeatureVector>> idRules;
    HashMap<AtomString, std::unique_ptr<RuleFeatureVector>> classRules;
    HashMap<AtomString, std::unique_ptr<Vector<RuleFeatureWithInvalidationSelector>>> attributeRules;
    HashMap<PseudoClassInvalidationKey, std::unique_ptr<RuleFeatureVector>> pseudoClassRules;
    HashMap<PseudoClassInvalidationKey, std::unique_ptr<Vector<RuleFeatureWithInvalidationSelector>>> hasPseudoClassRules;
    Vector<RuleAndSelector> scopeBreakingHasPseudoClassRules;

    HashSet<AtomString> classesAffectingHost;
    HashSet<AtomString> attributesAffectingHost;
    HashSet<CSSSelector::PseudoClass, IntHash<CSSSelector::PseudoClass>, WTF::StrongEnumHashTraits<CSSSelector::PseudoClass>> pseudoClassesAffectingHost;
    HashSet<CSSSelector::PseudoClass, IntHash<CSSSelector::PseudoClass>, WTF::StrongEnumHashTraits<CSSSelector::PseudoClass>> pseudoClasses;

    std::array<bool, matchRelationCount> usedRelations { };

    bool usesFirstLineRules { false };
    bool usesFirstLetterRules { false };
    bool hasStartingStyleRules { false };
    bool usesHasPseudoClass { false };

private:
    struct SelectorFeatures {
        using InvalidationFeature = std::tuple<const CSSSelector*, MatchElement, IsNegation>;
        using HasInvalidationFeature = std::tuple<const CSSSelector*, MatchElement, IsNegation, const CSSSelector*>;

        Vector<InvalidationFeature> ids;
        Vector<InvalidationFeature> classes;
        Vector<InvalidationFeature> attributes;
        Vector<InvalidationFeature> pseudoClasses;
        Vector<HasInvalidationFeature> hasPseudoClasses;
        bool containsScopeBreakingHasPseudoClass { false };
    };
    struct RecursiveCollectionContext;
    void collectFeaturesFromSelector(SelectorFeatures&, const CSSSelector&, MatchElement = { MatchElement::Relation::Subject, { } });
    void recursivelyCollectFeaturesFromSelector(SelectorFeatures&, const CSSSelector&, const RecursiveCollectionContext&);
    void NODELETE collectPseudoElementFeatures(const RuleData&);
};

MatchElement::HasRelation computeHasArgumentRelation(const CSSSelector&);

enum class InvalidationKeyType : uint8_t { Universal = 1, Class, Id, Attribute, Tag };
PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClass, InvalidationKeyType, const AtomString& = starAtom());

bool NODELETE unlikelyToHaveSelectorForAttribute(const AtomString&);

inline bool isUniversalInvalidation(const PseudoClassInvalidationKey& key)
{
    return static_cast<InvalidationKeyType>(std::get<1>(key)) == InvalidationKeyType::Universal;
}

} // namespace Style
} // namespace WebCore
