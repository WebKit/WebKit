/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#pragma once

#include "CSSSelector.h"
#include "Element.h"
#include "StyleRelations.h"

namespace WebCore {

class CSSSelector;
class Element;
class RenderScrollbar;
class RenderStyle;

class SelectorChecker {
    WTF_MAKE_NONCOPYABLE(SelectorChecker);
    enum class Match { SelectorMatches, SelectorFailsLocally, SelectorFailsAllSiblings, SelectorFailsCompletely };

    enum class MatchType { VirtualPseudoElementOnly, Element };

    struct MatchResult {
        Match match;
        MatchType matchType;

        static MatchResult matches(MatchType matchType)
        {
            return { Match::SelectorMatches, matchType };
        }

        static MatchResult updateWithMatchType(MatchResult result, MatchType matchType)
        {
            if (matchType == MatchType::VirtualPseudoElementOnly)
                result.matchType = MatchType::VirtualPseudoElementOnly;
            return result;
        }

        static MatchResult fails(Match match)
        {
            return { match, MatchType::Element };
        }
    };

public:
    enum class Mode : unsigned char {
        ResolvingStyle = 0, CollectingRules, CollectingRulesIgnoringVirtualPseudoElements, QueryingRules
    };

    SelectorChecker(Document&);

    struct CheckingContext {
        CheckingContext(SelectorChecker::Mode resolvingMode)
            : resolvingMode(resolvingMode)
        { }

        const SelectorChecker::Mode resolvingMode;
        PseudoId pseudoId { PseudoId::None };
        RenderScrollbar* scrollbar { nullptr };
        ScrollbarPart scrollbarPart { NoPart };
        const ContainerNode* scope { nullptr };
        bool isMatchingHostPseudoClass { false };

        // FIXME: It would be nicer to have a separate object for return values. This requires some more work in the selector compiler.
        Style::Relations styleRelations;
        PseudoIdSet pseudoIDSet;
    };

    bool match(const CSSSelector&, const Element&, CheckingContext&, unsigned& specificity) const;

    bool matchHostPseudoClass(const CSSSelector&, const Element&, CheckingContext&, unsigned& specificity) const;

    static bool isCommonPseudoClassSelector(const CSSSelector*);
    static bool matchesFocusPseudoClass(const Element&);
    static bool attributeSelectorMatches(const Element&, const QualifiedName&, const AtomString& attributeValue, const CSSSelector&);

    enum LinkMatchMask { MatchDefault = 0, MatchLink = 1, MatchVisited = 2, MatchAll = MatchLink | MatchVisited };
    static unsigned determineLinkMatchType(const CSSSelector*);

    struct LocalContext;
    
private:
    MatchResult matchRecursively(CheckingContext&, const LocalContext&, PseudoIdSet&, unsigned& specificity) const;
    bool checkOne(CheckingContext&, const LocalContext&, PseudoIdSet&, MatchType&, unsigned& specificity) const;
    bool matchSelectorList(CheckingContext&, const LocalContext&, const Element&, const CSSSelectorList&, unsigned& specificity) const;

    bool checkScrollbarPseudoClass(const CheckingContext&, const Element&, const CSSSelector&) const;

    bool m_strictParsing;
    bool m_documentIsHTML;
};

inline bool SelectorChecker::isCommonPseudoClassSelector(const CSSSelector* selector)
{
    if (selector->match() != CSSSelector::PseudoClass)
        return false;
    CSSSelector::PseudoClassType pseudoType = selector->pseudoClassType();
    return pseudoType == CSSSelector::PseudoClassLink
        || pseudoType == CSSSelector::PseudoClassAnyLink
        || pseudoType == CSSSelector::PseudoClassAnyLinkDeprecated
        || pseudoType == CSSSelector::PseudoClassVisited
        || pseudoType == CSSSelector::PseudoClassFocus;
}

} // namespace WebCore
