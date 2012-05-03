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
#include "LinkHash.h"
#include "RenderStyleConstants.h"
#include "SpaceSplitString.h"
#include "StyledElement.h"
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

    enum SelectorMatch { SelectorMatches, SelectorFailsLocally, SelectorFailsAllSiblings, SelectorFailsCompletely };
    enum VisitedMatchType { VisitedMatchDisabled, VisitedMatchEnabled };
    enum Mode { ResolvingStyle = 0, CollectingRules, QueryingRules };

    struct SelectorCheckingContext {
        // Initial selector constructor
        SelectorCheckingContext(CSSSelector* selector, Element* element, VisitedMatchType visitedMatchType)
            : selector(selector)
            , element(element)
            , scope(0)
            , visitedMatchType(visitedMatchType)
            , elementStyle(0)
            , elementParentStyle(0)
            , isSubSelector(false)
        { }

        CSSSelector* selector;
        Element* element;
        const ContainerNode* scope;
        VisitedMatchType visitedMatchType;
        RenderStyle* elementStyle;
        RenderStyle* elementParentStyle;
        bool isSubSelector;
    };

    bool checkSelector(CSSSelector*, Element*, bool isFastCheckableSelector = false) const;
    SelectorMatch checkSelector(const SelectorCheckingContext&, PseudoId&) const;
    static bool isFastCheckableSelector(const CSSSelector*);
    bool fastCheckSelector(const CSSSelector*, const Element*) const;

    template <unsigned maximumIdentifierCount>
    inline bool fastRejectSelector(const unsigned* identifierHashes) const;
    static void collectIdentifierHashes(const CSSSelector*, unsigned* identifierHashes, unsigned maximumIdentifierCount);

    void setupParentStack(Element* parent);
    void pushParent(Element* parent);
    void popParent() { popParentStackFrame(); }
    bool parentStackIsEmpty() const { return m_parentStack.isEmpty(); }
    bool parentStackIsConsistent(const ContainerNode* parentNode) const { return !m_parentStack.isEmpty() && m_parentStack.last().element == parentNode; }

    EInsideLink determineLinkState(Element*) const;
    void allVisitedStateChanged();
    void visitedStateChanged(LinkHash visitedHash);

    Document* document() const { return m_document; }
    bool strictParsing() const { return m_strictParsing; }

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode = mode; }

    PseudoId pseudoStyle() const { return m_pseudoStyle; }
    void setPseudoStyle(PseudoId pseudoId) { m_pseudoStyle = pseudoId; }

    bool hasUnknownPseudoElements() const { return m_hasUnknownPseudoElements; }
    void clearHasUnknownPseudoElements() { m_hasUnknownPseudoElements = false; }

    static bool tagMatches(const Element*, const CSSSelector*);
    static bool attributeNameMatches(const Attribute*, const QualifiedName&);
    static bool isCommonPseudoClassSelector(const CSSSelector*);
    bool matchesFocusPseudoClass(const Element*) const;
    static bool fastCheckRightmostAttributeSelector(const Element*, const CSSSelector*);
    static bool checkExactAttribute(const Element*, const QualifiedName& selectorAttributeName, const AtomicStringImpl* value);

    enum LinkMatchMask { MatchLink = 1, MatchVisited = 2, MatchAll = MatchLink | MatchVisited };
    static unsigned determineLinkMatchType(const CSSSelector*);

    // Find the ids or classes selectors are scoped to. The selectors only apply to elements in subtrees where the root element matches the scope.
    static bool determineSelectorScopes(const CSSSelectorList&, HashSet<AtomicStringImpl*>& idScopes, HashSet<AtomicStringImpl*>& classScopes);
    static bool elementMatchesSelectorScopes(const StyledElement*, const HashSet<AtomicStringImpl*>& idScopes, const HashSet<AtomicStringImpl*>& classScopes);

private:
    bool checkOneSelector(const SelectorCheckingContext&, PseudoId&) const;
    bool checkScrollbarPseudoClass(CSSSelector*, PseudoId& dynamicPseudo) const;
    static bool isFrameFocused(const Element*);

    bool fastCheckRightmostSelector(const CSSSelector*, const Element*, VisitedMatchType) const;
    bool commonPseudoClassSelectorMatches(const Element*, const CSSSelector*, VisitedMatchType) const;

    EInsideLink determineLinkStateSlowCase(Element*) const;

    void pushParentStackFrame(Element* parent);
    void popParentStackFrame();

    Document* m_document;
    bool m_strictParsing;
    bool m_documentIsHTML;
    Mode m_mode;
    PseudoId m_pseudoStyle;
    mutable bool m_hasUnknownPseudoElements;
    mutable HashSet<LinkHash, LinkHashHash> m_linksCheckedForVisitedState;

    struct ParentStackFrame {
        ParentStackFrame() : element(0) { }
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
        Attribute* attribute = element->attributeItem(i);
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

inline bool SelectorChecker::elementMatchesSelectorScopes(const StyledElement* element, const HashSet<AtomicStringImpl*>& idScopes, const HashSet<AtomicStringImpl*>& classScopes)
{
    if (!idScopes.isEmpty() && element->hasID() && idScopes.contains(element->idForStyleResolution().impl()))
        return true;
    if (classScopes.isEmpty() || !element->hasClass())
        return false;
    const SpaceSplitString& classNames = element->classNames();
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (classScopes.contains(classNames[i].impl()))
            return true;
    }
    return false;
}

}

#endif
