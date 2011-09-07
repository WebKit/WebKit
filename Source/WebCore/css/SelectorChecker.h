/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef SelectorChecker_h
#define SelectorChecker_h

#include "Element.h"
#include "LinkHash.h"
#include "RenderStyleConstants.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSSelector;
class Document;
class RenderStyle;

class SelectorChecker {
    WTF_MAKE_NONCOPYABLE(SelectorChecker);
public:
    SelectorChecker(Document*, bool strictParsing);

    enum SelectorMatch { SelectorMatches, SelectorFailsLocally, SelectorFailsCompletely };

    bool checkSelector(CSSSelector*, Element*, bool isFastCheckableSelector = false) const;
    SelectorMatch checkSelector(CSSSelector*, Element*, PseudoId& dynamicPseudo, bool isSubSelector, bool encounteredLink, RenderStyle* = 0, RenderStyle* elementParentStyle = 0) const;
    static bool isFastCheckableSelector(const CSSSelector*);
    static bool fastCheckSelector(const CSSSelector*, const Element*);

    EInsideLink determineLinkState(Element*) const;
    void allVisitedStateChanged();
    void visitedStateChanged(LinkHash visitedHash);
    
    Document* document() const { return m_document; }
    bool strictParsing() const { return m_strictParsing; }
    
    bool isCollectingRulesOnly() const { return m_isCollectingRulesOnly; }
    void setCollectingRulesOnly(bool b) { m_isCollectingRulesOnly = b; }
    
    bool isMatchingVisitedPseudoClass() const { return m_isMatchingVisitedPseudoClass; }
    void setMatchingVisitedPseudoClass(bool b) { m_isMatchingVisitedPseudoClass = b; }
    
    PseudoId pseudoStyle() const { return m_pseudoStyle; }
    void setPseudoStyle(PseudoId pseudoId) { m_pseudoStyle = pseudoId; }

    bool hasUnknownPseudoElements() const { return m_hasUnknownPseudoElements; }
    void clearHasUnknownPseudoElements() { m_hasUnknownPseudoElements = false; }

private:
    bool checkOneSelector(CSSSelector*, Element*, PseudoId& dynamicPseudo, bool isSubSelector, bool encounteredLink, RenderStyle*, RenderStyle* elementParentStyle) const;
    bool checkScrollbarPseudoClass(CSSSelector*, PseudoId& dynamicPseudo) const;

    EInsideLink determineLinkStateSlowCase(Element*) const;

    Document* m_document;
    bool m_strictParsing;
    bool m_documentIsHTML;
    bool m_isCollectingRulesOnly;
    PseudoId m_pseudoStyle;
    mutable bool m_hasUnknownPseudoElements;
    mutable bool m_isMatchingVisitedPseudoClass;
    mutable HashSet<LinkHash, LinkHashHash> m_linksCheckedForVisitedState;
};

inline EInsideLink SelectorChecker::determineLinkState(Element* element) const
{
    if (!element || !element->isLink())
        return NotInsideLink;
    return determineLinkStateSlowCase(element);
}

}

#endif
