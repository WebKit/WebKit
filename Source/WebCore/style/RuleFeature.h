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
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
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
// FIXME: Has* values should be separated so we could better describe both the :has() argument and its position in the selector.
enum class MatchElement : uint8_t {
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
    HasChild, // :has(> .changed)
    HasDescendant, // :has(.changed)
    HasSibling, // :has(~ .changed)
    HasSiblingDescendant, // :has(~ .a .changed)
    HasAnySibling, // :has(~ :is(.changed ~ .x))
    HasChildParent, // :has(> .changed) > .subject
    HasChildAncestor, // :has(> .changed) .subject
    HasDescendantParent, // :has(.changed) > .subject
    // FIXME: This is a catch-all for the rest of cases where :has() is in a non-subject position.
    HasNonSubject, // :has(.changed) .subject
    // FIXME: This is a catch-all for cases where :has() contains a scope breaking sub-selector.
    HasScopeBreaking, // :has(:is(.changed .a))
    Host, // :host(.changed) .subject
    HostChild // ::slotted(.changed)
};
constexpr unsigned matchElementCount = static_cast<unsigned>(MatchElement::HostChild) + 1;

enum class IsNegation : bool { No, Yes };
enum class CanBreakScope : bool { No, Yes }; // Are we inside a logical combination pseudo-class like :is() or :not(), which if we were inside a :has(), could break out of its scope?
enum class DoesBreakScope : bool { No, Yes }; // Did we find a logical combination pseudo-class like :is() or :not() with selector combinators that do break out of a :has() scope?

// For MSVC.
#pragma pack(push, 4)
struct RuleAndSelector {
    RuleAndSelector(const RuleData&);

    RefPtr<const StyleRule> styleRule;
    uint16_t selectorIndex; // Keep in sync with RuleData's selectorIndex size.
    uint16_t selectorListIndex; // Keep in sync with RuleData's selectorListIndex size.
};

struct RuleFeature : public RuleAndSelector {
    RuleFeature(const RuleData&, MatchElement, IsNegation);

    MatchElement matchElement;
    IsNegation isNegation; // Whether the selector is in a (non-paired) :not() context.
};
static_assert(sizeof(RuleFeature) <= 16, "RuleFeature is a frequently allocated object. Keep it small.");

struct RuleFeatureWithInvalidationSelector : public RuleFeature {
    RuleFeatureWithInvalidationSelector(const RuleData&, MatchElement, IsNegation, CSSSelectorList&& invalidationSelector);

    CSSSelectorList invalidationSelector;
};
#pragma pack(pop)

using PseudoClassInvalidationKey = std::tuple<unsigned, uint8_t, AtomString>;

using RuleFeatureVector = Vector<RuleFeature>;

struct RuleFeatureSet {
    void add(const RuleFeatureSet&);
    void clear();
    void shrinkToFit();
    void collectFeatures(const RuleData&, const Vector<Ref<const StyleRuleScope>>& scopeRules = { });
    void registerContentAttribute(const AtomString&);

    bool usesHasPseudoClass() const;
    bool usesMatchElement(MatchElement matchElement) const { return usedMatchElements[enumToUnderlyingType(matchElement)]; }
    void setUsesMatchElement(MatchElement matchElement) { usedMatchElements[enumToUnderlyingType(matchElement)] = true; }

    HashSet<AtomString> idsInRules;
    HashSet<AtomString> idsMatchingAncestorsInRules;
    HashSet<AtomString> attributeLowercaseLocalNamesInRules;
    HashSet<AtomString> attributeLocalNamesInRules;
    HashSet<AtomString> contentAttributeNamesInRules;

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

    std::array<bool, matchElementCount> usedMatchElements { };

    bool usesFirstLineRules { false };
    bool usesFirstLetterRules { false };
    bool hasStartingStyleRules { false };

private:
    struct SelectorFeatures {
        using InvalidationFeature = std::tuple<const CSSSelector*, MatchElement, IsNegation>;
        using HasInvalidationFeature = std::tuple<const CSSSelector*, MatchElement, IsNegation, DoesBreakScope>;

        Vector<InvalidationFeature> ids;
        Vector<InvalidationFeature> classes;
        Vector<InvalidationFeature> attributes;
        Vector<InvalidationFeature> pseudoClasses;
        Vector<HasInvalidationFeature> hasPseudoClasses;
    };
    DoesBreakScope recursivelyCollectFeaturesFromSelector(SelectorFeatures&, const CSSSelector&, MatchElement = MatchElement::Subject, IsNegation = IsNegation::No, CanBreakScope = CanBreakScope::No);
};

bool isHasPseudoClassMatchElement(MatchElement);
MatchElement computeHasPseudoClassMatchElement(const CSSSelector&);

enum class InvalidationKeyType : uint8_t { Universal = 1, Class, Id, Attribute, Tag };
PseudoClassInvalidationKey makePseudoClassInvalidationKey(CSSSelector::PseudoClass, InvalidationKeyType, const AtomString& = starAtom());

bool unlikelyToHaveSelectorForAttribute(const AtomString&);

inline bool isUniversalInvalidation(const PseudoClassInvalidationKey& key)
{
    return static_cast<InvalidationKeyType>(std::get<1>(key)) == InvalidationKeyType::Universal;
}

inline bool RuleFeatureSet::usesHasPseudoClass() const
{
    return usesMatchElement(MatchElement::HasChild)
        || usesMatchElement(MatchElement::HasDescendant)
        || usesMatchElement(MatchElement::HasSibling)
        || usesMatchElement(MatchElement::HasSiblingDescendant)
        || usesMatchElement(MatchElement::HasAnySibling)
        || usesMatchElement(MatchElement::HasNonSubject)
        || usesMatchElement(MatchElement::HasScopeBreaking);
}

} // namespace Style
} // namespace WebCore
