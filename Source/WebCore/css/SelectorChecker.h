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

#include "CSSSelector.h"
#include "Element.h"
#include "InspectorInstrumentation.h"
#include "LinkHash.h"
#include "RenderStyleConstants.h"
#include <wtf/BloomFilter.h>
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
    bool fastCheckSelector(const CSSSelector*, const Element*) const;

    template <unsigned maximumIdentifierCount>
    inline bool fastRejectSelector(const unsigned* identifierHashes) const;
    static void collectIdentifierHashes(const CSSSelector*, unsigned* identifierHashes, unsigned maximumIdentifierCount);

    void pushParent(Element* parent);
    void popParent(Element* parent);
    bool parentStackIsConsistent(ContainerNode* parentNode) const { return !m_parentStack.isEmpty() && m_parentStack.last().element == parentNode; }

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
    
    static bool tagMatches(const Element*, const CSSSelector*);
    static bool isCommonPseudoClassSelector(const CSSSelector*);
    bool commonPseudoClassSelectorMatches(const Element*, const CSSSelector*) const;
    bool linkMatchesVisitedPseudoClass(const Element*) const;
    bool matchesFocusPseudoClass(const Element*) const;

private:
    bool checkOneSelector(CSSSelector*, Element*, PseudoId& dynamicPseudo, bool isSubSelector, bool encounteredLink, RenderStyle*, RenderStyle* elementParentStyle) const;
    bool checkScrollbarPseudoClass(CSSSelector*, PseudoId& dynamicPseudo) const;
    static bool isFrameFocused(const Element*);
    
    bool fastCheckRightmostSelector(const CSSSelector*, const Element*) const;

    EInsideLink determineLinkStateSlowCase(Element*) const;

    void pushParentStackFrame(Element* parent);
    void popParentStackFrame();

    Document* m_document;
    bool m_strictParsing;
    bool m_documentIsHTML;
    bool m_isCollectingRulesOnly;
    PseudoId m_pseudoStyle;
    mutable bool m_hasUnknownPseudoElements;
    mutable bool m_isMatchingVisitedPseudoClass;
    mutable HashSet<LinkHash, LinkHashHash> m_linksCheckedForVisitedState;

    struct ParentStackFrame {
        ParentStackFrame() { }
        ParentStackFrame(Element* element) : element(element) { }
        Element* element;
        Vector<unsigned, 4> identifierHashes;
    };
    Vector<ParentStackFrame> m_parentStack;

    // With 100 unique strings in the filter, 2^12 slot table has false positive rate of ~0.2%.
    static const unsigned bloomFilterKeyBits = 12;
    OwnPtr<BloomFilter<bloomFilterKeyBits> > m_ancestorIdentifierFilter;
};

inline EInsideLink SelectorChecker::determineLinkState(Element* element) const
{
    if (!element || !element->isLink())
        return NotInsideLink;
    return determineLinkStateSlowCase(element);
}
    
template <unsigned maximumIdentifierCount>
inline bool SelectorChecker::fastRejectSelector(const unsigned* identifierHashes) const
{
    ASSERT(m_ancestorIdentifierFilter);
    for (unsigned n = 0; n < maximumIdentifierCount && identifierHashes[n]; ++n) {
        if (!m_ancestorIdentifierFilter->mayContain(identifierHashes[n]))
            return true;
    }
    return false;
}
    
inline bool SelectorChecker::isCommonPseudoClassSelector(const CSSSelector* selector)
{
    if (selector->m_match != CSSSelector::PseudoClass)
        return false;
    CSSSelector::PseudoType pseudoType = selector->pseudoType();
    return pseudoType == CSSSelector::PseudoLink
        || pseudoType == CSSSelector::PseudoAnyLink
        || pseudoType == CSSSelector::PseudoVisited
        || pseudoType == CSSSelector::PseudoFocus;
}

inline bool SelectorChecker::linkMatchesVisitedPseudoClass(const Element* element) const
{
    ASSERT(element->isLink());
    return m_isMatchingVisitedPseudoClass || InspectorInstrumentation::forcePseudoState(const_cast<Element*>(element), CSSSelector::PseudoVisited);
}

inline bool SelectorChecker::matchesFocusPseudoClass(const Element* element) const
{
    if (InspectorInstrumentation::forcePseudoState(const_cast<Element*>(element), CSSSelector::PseudoFocus))
        return true;
    return element->focused() && isFrameFocused(element);
}
    
inline bool SelectorChecker::tagMatches(const Element* element, const CSSSelector* selector)
{
    if (!selector->hasTag())
        return true;
    const AtomicString& localName = selector->tag().localName();
    if (localName != starAtom && localName != element->localName())
        return false;
    const AtomicString& namespaceURI = selector->tag().namespaceURI();
    return namespaceURI == starAtom || namespaceURI == element->namespaceURI();
}

}

#endif
