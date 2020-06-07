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
#include "HTMLNames.h"
#include "MediaQueryEvaluator.h"
#include "SecurityOrigin.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"

#if ENABLE(VIDEO)
#include "TextTrackCue.h"
#endif

namespace WebCore {
namespace Style {

using namespace HTMLNames;

struct SameSizeAsRuleData {
    void* a;
    unsigned b;
    unsigned c;
    unsigned d[4];
};

COMPILE_ASSERT(sizeof(RuleData) == sizeof(SameSizeAsRuleData), RuleData_should_stay_small);

static inline MatchBasedOnRuleHash computeMatchBasedOnRuleHash(const CSSSelector& selector)
{
    if (selector.tagHistory())
        return MatchBasedOnRuleHash::None;

    if (selector.match() == CSSSelector::Tag) {
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
    if (selector.match() == CSSSelector::Id)
        return MatchBasedOnRuleHash::ClassA;
    if (selector.match() == CSSSelector::Class)
        return MatchBasedOnRuleHash::ClassB;
    return MatchBasedOnRuleHash::None;
}

static bool selectorCanMatchPseudoElement(const CSSSelector& rootSelector)
{
    const CSSSelector* selector = &rootSelector;
    do {
        if (selector->matchesPseudoElement())
            return true;

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (selectorCanMatchPseudoElement(*subSelector))
                    return true;
            }
        }

        selector = selector->tagHistory();
    } while (selector);
    return false;
}

static inline bool isCommonAttributeSelectorAttribute(const QualifiedName& attribute)
{
    // These are explicitly tested for equality in canShareStyleWithElement.
    return attribute == typeAttr || attribute == readonlyAttr;
}

static bool computeContainsUncommonAttributeSelector(const CSSSelector& rootSelector, bool matchesRightmostElement = true)
{
    const CSSSelector* selector = &rootSelector;
    do {
        if (selector->isAttributeSelector()) {
            // FIXME: considering non-rightmost simple selectors is necessary because of the style sharing of cousins.
            // It is a primitive solution which disable a lot of style sharing on pages that rely on attributes for styling.
            // We should investigate better ways of doing this.
            if (!isCommonAttributeSelectorAttribute(selector->attribute()) || !matchesRightmostElement)
                return true;
        }

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (computeContainsUncommonAttributeSelector(*subSelector, matchesRightmostElement))
                    return true;
            }
        }

        if (selector->relation() != CSSSelector::Subselector)
            matchesRightmostElement = false;

        selector = selector->tagHistory();
    } while (selector);
    return false;
}

static inline PropertyWhitelistType determinePropertyWhitelistType(const CSSSelector* selector)
{
    for (const CSSSelector* component = selector; component; component = component->tagHistory()) {
#if ENABLE(VIDEO)
        if (component->match() == CSSSelector::PseudoElement && (component->pseudoElementType() == CSSSelector::PseudoElementCue || component->value() == TextTrackCue::cueShadowPseudoId()))
            return PropertyWhitelistCue;
#endif
        if (component->match() == CSSSelector::PseudoElement && component->pseudoElementType() == CSSSelector::PseudoElementMarker)
            return PropertyWhitelistMarker;

        if (const auto* selectorList = selector->selectorList()) {
            for (const auto* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                auto whitelistType = determinePropertyWhitelistType(subSelector);
                if (whitelistType != PropertyWhitelistNone)
                    return whitelistType;
            }
        }
    }
    return PropertyWhitelistNone;
}

RuleData::RuleData(const StyleRule& styleRule, unsigned selectorIndex, unsigned selectorListIndex, unsigned position)
    : m_styleRule(&styleRule)
    , m_selectorIndex(selectorIndex)
    , m_selectorListIndex(selectorListIndex)
    , m_position(position)
    , m_matchBasedOnRuleHash(static_cast<unsigned>(computeMatchBasedOnRuleHash(*selector())))
    , m_canMatchPseudoElement(selectorCanMatchPseudoElement(*selector()))
    , m_containsUncommonAttributeSelector(computeContainsUncommonAttributeSelector(*selector()))
    , m_linkMatchType(SelectorChecker::determineLinkMatchType(selector()))
    , m_propertyWhitelistType(determinePropertyWhitelistType(selector()))
    , m_isEnabled(true)
    , m_descendantSelectorIdentifierHashes(SelectorFilter::collectHashes(*selector()))
{
    ASSERT(m_position == position);
    ASSERT(m_selectorIndex == selectorIndex);
}

} // namespace Style
} // namespace WebCore
