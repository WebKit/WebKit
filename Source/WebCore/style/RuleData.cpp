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

static inline PropertyAllowlist determinePropertyAllowlist(const CSSSelector* selector)
{
    for (const CSSSelector* component = selector; component; component = component->precedingInComplexSelector()) {
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
            return propertyAllowlistForPseudoId(PseudoId::Marker);

        if (const auto* selectorList = selector->selectorList()) {
            for (auto& subSelector : *selectorList) {
                auto allowlistType = determinePropertyAllowlist(&subSelector);
                if (allowlistType != PropertyAllowlist::None)
                    return allowlistType;
            }
        }
    }
    return PropertyAllowlist::None;
}

RuleData::RuleData(const StyleRule& styleRule, unsigned selectorIndex, unsigned selectorListIndex, unsigned position, OptionSet<UsedRuleType> usedRuleTypes)
    : m_styleRuleWithSelectorIndex(&styleRule, static_cast<uint16_t>(selectorIndex))
    , m_selectorListIndex(selectorListIndex)
    , m_matchBasedOnRuleHash(enumToUnderlyingType(computeMatchBasedOnRuleHash(*selector())))
    , m_canMatchPseudoElement(complexSelectorCanMatchPseudoElement(*selector()))
    , m_propertyAllowlist(enumToUnderlyingType(determinePropertyAllowlist(selector())))
    , m_usedRuleTypes(usedRuleTypes.toRaw())
    , m_isEnabled(true)
    , m_position(position)
    , m_descendantSelectorIdentifierHashes(SelectorFilter::collectHashes(*selector()))
{
    ASSERT(m_position == position);
    ASSERT(this->selectorIndex() == selectorIndex);
}

} // namespace Style
} // namespace WebCore
