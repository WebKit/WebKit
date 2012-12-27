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

#include "Attribute.h"
#include "CSSSelector.h"
#include "InspectorInstrumentation.h"
#include "SpaceSplitString.h"
#include "StyledElement.h"
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

    enum SelectorMatch { SelectorMatches, SelectorFailsLocally, SelectorFailsAllSiblings, SelectorFailsCompletely };
    enum VisitedMatchType { VisitedMatchDisabled, VisitedMatchEnabled };
    enum Mode { ResolvingStyle = 0, CollectingRules, QueryingRules, SharingRules };

    struct SelectorCheckingContext {
        // Initial selector constructor
        SelectorCheckingContext(CSSSelector* selector, Element* element, VisitedMatchType visitedMatchType)
            : selector(selector)
            , element(element)
            , scope(0)
            , visitedMatchType(visitedMatchType)
            , pseudoStyle(NOPSEUDO)
            , elementStyle(0)
            , elementParentStyle(0)
            , isSubSelector(false)
            , hasScrollbarPseudo(false)
            , hasSelectionPseudo(false)
        { }

        CSSSelector* selector;
        Element* element;
        const ContainerNode* scope;
        VisitedMatchType visitedMatchType;
        PseudoId pseudoStyle;
        RenderStyle* elementStyle;
        RenderStyle* elementParentStyle;
        bool isSubSelector;
        bool hasScrollbarPseudo;
        bool hasSelectionPseudo;
    };

    bool checkSelector(CSSSelector*, Element*, bool isFastCheckableSelector = false) const;
    SelectorMatch checkSelector(const SelectorCheckingContext&, PseudoId&) const;
    template<typename SiblingTraversalStrategy>
    bool checkOneSelector(const SelectorCheckingContext&, const SiblingTraversalStrategy&) const;

    static bool isFastCheckableSelector(const CSSSelector*);
    bool fastCheckSelector(const CSSSelector*, const Element*) const;

    Document* document() const { return m_document; }
    bool strictParsing() const { return m_strictParsing; }

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode = mode; }

    static bool tagMatches(const Element*, const CSSSelector*);
    static bool attributeNameMatches(const Attribute*, const QualifiedName&);
    static bool isCommonPseudoClassSelector(const CSSSelector*);
    bool matchesFocusPseudoClass(const Element*) const;
    static bool fastCheckRightmostAttributeSelector(const Element*, const CSSSelector*);
    static bool checkExactAttribute(const Element*, const QualifiedName& selectorAttributeName, const AtomicStringImpl* value);

    enum LinkMatchMask { MatchLink = 1, MatchVisited = 2, MatchAll = MatchLink | MatchVisited };
    static unsigned determineLinkMatchType(const CSSSelector*);

private:
    bool checkScrollbarPseudoClass(CSSSelector*) const;
    static bool isFrameFocused(const Element*);

    bool fastCheckRightmostSelector(const CSSSelector*, const Element*, VisitedMatchType) const;
    bool commonPseudoClassSelectorMatches(const Element*, const CSSSelector*, VisitedMatchType) const;

    Document* m_document;
    bool m_strictParsing;
    bool m_documentIsHTML;
    Mode m_mode;
};

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

inline bool SelectorChecker::attributeNameMatches(const Attribute* attribute, const QualifiedName& selectorAttributeName)
{
    if (selectorAttributeName.localName() != attribute->localName())
        return false;
    return selectorAttributeName.prefix() == starAtom || selectorAttributeName.namespaceURI() == attribute->namespaceURI();
}

inline bool SelectorChecker::checkExactAttribute(const Element* element, const QualifiedName& selectorAttributeName, const AtomicStringImpl* value)
{
    if (!element->hasAttributesWithoutUpdate())
        return false;
    unsigned size = element->attributeCount();
    for (unsigned i = 0; i < size; ++i) {
        const Attribute* attribute = element->attributeItem(i);
        if (attributeNameMatches(attribute, selectorAttributeName) && (!value || attribute->value().impl() == value))
            return true;
    }
    return false;
}

inline bool SelectorChecker::fastCheckRightmostAttributeSelector(const Element* element, const CSSSelector* selector)
{
    if (selector->m_match == CSSSelector::Exact || selector->m_match == CSSSelector::Set)
        return checkExactAttribute(element, selector->attribute(), selector->value().impl());
    ASSERT(!selector->isAttributeSelector());
    return true;
}

}

#endif
