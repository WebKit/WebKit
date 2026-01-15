/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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
#include "RuleData.h"

#include "CSSFontSelector.h"
#include "CSSKeyframesRule.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CommonAtomStrings.h"
#include "HTMLNames.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "UserAgentParts.h"

namespace WebCore {
namespace Style {

using namespace HTMLNames;

struct SameSizeAsRuleData {
    uint64_t a;
    unsigned b;
    unsigned c;
    unsigned d[4];
};

static_assert(sizeof(RuleData) == sizeof(SameSizeAsRuleData), "RuleData should stay small");

static inline MatchBasedOnRuleHash computeMatchBasedOnRuleHash(const CSSSelector& selector)
{
    if (selector.precedingInComplexSelector())
        return MatchBasedOnRuleHash::None;

    if (selector.match() == CSSSelector::Match::Tag) {
        const QualifiedName& tagQualifiedName = selector.tagQName();
        const AtomString& selectorNamespace = tagQualifiedName.namespaceURI();
        if (selectorNamespace == starAtom() || selectorNamespace == xhtmlNamespaceURI) {
            if (tagQualifiedName == anyQName())
                return MatchBasedOnRuleHash::Universal;
            return MatchBasedOnRuleHash::ClassC;
        }
        return MatchBasedOnRuleHash::None;
    }
    if (SelectorChecker::isCommonPseudoClassSelector(&selector))
        return MatchBasedOnRuleHash::ClassB;
    if (selector.match() == CSSSelector::Match::Id)
        return MatchBasedOnRuleHash::ClassA;
    if (selector.match() == CSSSelector::Match::Class)
        return MatchBasedOnRuleHash::ClassB;
    // FIXME: Valueless [attribute] case can be handled here too.

    return MatchBasedOnRuleHash::None;
}

static inline PropertyAllowlist determinePropertyAllowlist(const CSSSelector& selector)
{
    for (const CSSSelector* component = &selector; component; component = component->precedingInComplexSelector()) {
#if ENABLE(VIDEO)
        // Property allow-list for `::cue`:
        if (component->match() == CSSSelector::Match::PseudoElement && component->pseudoElement() == CSSSelector::PseudoElement::UserAgentPart && component->value() == UserAgentParts::cue())
            return PropertyAllowlist::Cue;
        // Property allow-list for `::cue(selector)`:
        if (component->match() == CSSSelector::Match::PseudoElement && component->pseudoElement() == CSSSelector::PseudoElement::Cue)
            return PropertyAllowlist::CueSelector;
        // Property allow-list for '::-internal-cue-background':
        if (component->match() == CSSSelector::Match::PseudoElement && component->pseudoElement() == CSSSelector::PseudoElement::UserAgentPart && component->value() == UserAgentParts::internalCueBackground())
            return PropertyAllowlist::CueBackground;
#endif
        if (component->match() == CSSSelector::Match::PseudoElement && component->pseudoElement() == CSSSelector::PseudoElement::Marker)
            return propertyAllowlistForPseudoElement(PseudoElementType::Marker);

        if (const auto* selectorList = selector.selectorList()) {
            for (auto& subSelector : *selectorList) {
                auto allowlistType = determinePropertyAllowlist(subSelector);
                if (allowlistType != PropertyAllowlist::None)
                    return allowlistType;
            }
        }
    }
    return PropertyAllowlist::None;
}

// Returns true if the subject compound consists only of pseudo-elements
// with no element-matching constraints (tag, class, id, attribute).
// Example: "::marker" returns true, "div::before" returns false.
static inline bool computeSubjectIsPurePseudoElement(const CSSSelector& selector)
{
    // Must start with a pseudo-element.
    if (selector.match() != CSSSelector::Match::PseudoElement)
        return false;

    // UserAgentPart pseudo-elements (like ::-webkit-slider-thumb), ::part(), ::slotted(),
    // and ::cue() match actual elements, not virtual pseudo-elements. They need to go through
    // normal matching.
    // ::before and ::after also need normal matching because their existence tracking via
    // hasPseudoStyle() is critical for correct dynamic style updates (e.g., when stylesheets
    // are enabled/disabled). The skip path can cause timing issues where pseudo-element types
    // are added after setHasPseudoStyles() has already been called.
    auto pseudoElement = selector.pseudoElement();
    if (pseudoElement == CSSSelector::PseudoElement::UserAgentPart
        || pseudoElement == CSSSelector::PseudoElement::UserAgentPartLegacyAlias
        || pseudoElement == CSSSelector::PseudoElement::WebKitUnknown
        || pseudoElement == CSSSelector::PseudoElement::Part
        || pseudoElement == CSSSelector::PseudoElement::Slotted
        || pseudoElement == CSSSelector::PseudoElement::Before
        || pseudoElement == CSSSelector::PseudoElement::After
#if ENABLE(VIDEO)
        || pseudoElement == CSSSelector::PseudoElement::Cue
#endif
        ) {
        return false;
    }

    // Walk the subject compound checking for element-matching selectors.
    for (auto* currentSelector = &selector; currentSelector; currentSelector = currentSelector->precedingInComplexSelector()) {
        switch (currentSelector->match()) {
        case CSSSelector::Match::Tag:
            // Universal (*) is OK, specific tag is not.
            if (currentSelector->tagQName().localName() != starAtom())
                return false;
            break;
        case CSSSelector::Match::Id:
        case CSSSelector::Match::Class:
            return false;
        default:
            break;
        }

        // Continue past pseudo-elements.
        if (currentSelector->match() == CSSSelector::Match::PseudoElement)
            continue;

        // If there's a combinator, we've exited the subject compound and there are more
        // selectors that need to be matched (ancestors, siblings, etc.). We can't skip matching.
        if (currentSelector->relation() != CSSSelector::Relation::Subselector)
            return false;
    }

    return true;
}

RuleData::RuleData(const StyleRule& styleRule, unsigned selectorIndex, unsigned selectorListIndex, unsigned position, IsStartingStyle isStartingStyle)
    : m_styleRuleWithSelectorIndex(&styleRule, static_cast<uint16_t>(selectorIndex))
    , m_selectorListIndex(selectorListIndex)
    , m_matchBasedOnRuleHash(enumToUnderlyingType(computeMatchBasedOnRuleHash(selector())))
    , m_canMatchPseudoElement(complexSelectorCanMatchPseudoElement(selector()))
    , m_subjectIsPurePseudoElement(computeSubjectIsPurePseudoElement(selector()))
    , m_propertyAllowlist(enumToUnderlyingType(determinePropertyAllowlist(selector())))
    , m_isStartingStyle(enumToUnderlyingType(isStartingStyle))
    , m_isEnabled(true)
    , m_position(position)
    , m_descendantSelectorIdentifierHashes(SelectorFilter::collectHashes(selector()))
{
    ASSERT(m_position == position);
    ASSERT(this->selectorIndex() == selectorIndex);
}

} // namespace Style
} // namespace WebCore
