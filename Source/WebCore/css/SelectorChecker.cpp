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

#include "config.h"
#include "SelectorChecker.h"

#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "Document.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLAnchorElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLProgressElement.h"
#include "InspectorInstrumentation.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderObject.h"
#include "RenderScrollbar.h"
#include "RenderStyle.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include "StyledElement.h"
#include "Text.h"

#if USE(PLATFORM_STRATEGIES)
#include "PlatformStrategies.h"
#include "VisitedLinkStrategy.h"
#endif

#if ENABLE(SVG)
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static bool htmlAttributeHasCaseInsensitiveValue(const QualifiedName& attr);

SelectorChecker::SelectorChecker(Document* document, bool strictParsing)
    : m_document(document)
    , m_strictParsing(strictParsing)
    , m_documentIsHTML(document->isHTMLDocument())
    , m_isCollectingRulesOnly(false)
    , m_pseudoStyle(NOPSEUDO)
    , m_hasUnknownPseudoElements(false)
{
}

static inline void collectElementIdentifierHashes(const Element* element, Vector<unsigned, 4>& identifierHashes)
{
    identifierHashes.append(element->localName().impl()->existingHash());
    if (element->hasID())
        identifierHashes.append(element->idForStyleResolution().impl()->existingHash());
    const StyledElement* styledElement = element->isStyledElement() ? static_cast<const StyledElement*>(element) : 0;
    if (styledElement && styledElement->hasClass()) {
        const SpaceSplitString& classNames = styledElement->classNames();
        size_t count = classNames.size();
        for (size_t i = 0; i < count; ++i)
            identifierHashes.append(classNames[i].impl()->existingHash());
    }
}

void SelectorChecker::pushParentStackFrame(Element* parent)
{
    ASSERT(m_ancestorIdentifierFilter);
    ASSERT(m_parentStack.isEmpty() || m_parentStack.last().element == parent->parentOrHostElement());
    ASSERT(!m_parentStack.isEmpty() || !parent->parentOrHostElement());
    m_parentStack.append(ParentStackFrame(parent));
    ParentStackFrame& parentFrame = m_parentStack.last();
    // Mix tags, class names and ids into some sort of weird bouillabaisse.
    // The filter is used for fast rejection of child and descendant selectors.
    collectElementIdentifierHashes(parent, parentFrame.identifierHashes);
    size_t count = parentFrame.identifierHashes.size();
    for (size_t i = 0; i < count; ++i)
        m_ancestorIdentifierFilter->add(parentFrame.identifierHashes[i]);
}

void SelectorChecker::popParentStackFrame()
{
    ASSERT(!m_parentStack.isEmpty());
    ASSERT(m_ancestorIdentifierFilter);
    const ParentStackFrame& parentFrame = m_parentStack.last();
    size_t count = parentFrame.identifierHashes.size();
    for (size_t i = 0; i < count; ++i)
        m_ancestorIdentifierFilter->remove(parentFrame.identifierHashes[i]);
    m_parentStack.removeLast();
    if (m_parentStack.isEmpty()) {
        ASSERT(m_ancestorIdentifierFilter->likelyEmpty());
        m_ancestorIdentifierFilter.clear();
    }
}

void SelectorChecker::pushParent(Element* parent)
{
    if (m_parentStack.isEmpty()) {
        ASSERT(!m_ancestorIdentifierFilter);
        m_ancestorIdentifierFilter = adoptPtr(new BloomFilter<bloomFilterKeyBits>);
        // If the element is not the root itself, build the stack starting from the root.
        if (parent->parentOrHostNode()) {
            Vector<Element*, 30> ancestors;
            for (Element* ancestor = parent; ancestor; ancestor = ancestor->parentOrHostElement())
                ancestors.append(ancestor);
            int count = ancestors.size();
            for (int n = count - 1; n >= 0; --n)
                pushParentStackFrame(ancestors[n]);
            return;
        }
    } else if (!parent->parentOrHostElement()) {
        // We are not always invoked consistently. For example, script execution can cause us to enter
        // style recalc in the middle of tree building. Reset the stack if we see a new root element.
        ASSERT(m_ancestorIdentifierFilter);
        m_ancestorIdentifierFilter->clear();
        m_parentStack.resize(0);
    } else {
        ASSERT(m_ancestorIdentifierFilter);
        // We may get invoked for some random elements in some wacky cases during style resolve.
        // Pause maintaining the stack in this case.
        if (m_parentStack.last().element != parent->parentOrHostElement())
            return;
    }
    pushParentStackFrame(parent);
}

void SelectorChecker::popParent(Element* parent)
{
    if (m_parentStack.isEmpty() || m_parentStack.last().element != parent)
        return;
    popParentStackFrame();
}

static inline void collectDescendantSelectorIdentifierHashes(const CSSSelector* selector, unsigned*& hash, const unsigned* end)
{
    if ((selector->m_match == CSSSelector::Id || selector->m_match == CSSSelector::Class) && !selector->value().isEmpty())
        (*hash++) = selector->value().impl()->existingHash();
    if (hash == end)
        return;
    const AtomicString& localName = selector->tag().localName();
    if (localName != starAtom)
        (*hash++) = localName.impl()->existingHash();
}

void SelectorChecker::collectIdentifierHashes(const CSSSelector* selector, unsigned* identifierHashes, unsigned maximumIdentifierCount)
{
    unsigned* hash = identifierHashes;
    unsigned* end = identifierHashes + maximumIdentifierCount;
    CSSSelector::Relation relation = selector->relation();

    // Skip the topmost selector. It is handled quickly by the rule hashes.
    bool skipOverSubselectors = true;
    for (selector = selector->tagHistory(); selector; selector = selector->tagHistory()) {
        // Only collect identifiers that match ancestors.
        switch (relation) {
        case CSSSelector::SubSelector:
            if (!skipOverSubselectors)
                collectDescendantSelectorIdentifierHashes(selector, hash, end);
            break;
        case CSSSelector::DirectAdjacent:
        case CSSSelector::IndirectAdjacent:
        case CSSSelector::ShadowDescendant:
            skipOverSubselectors = true;
            break;
        case CSSSelector::Descendant:
        case CSSSelector::Child:
            skipOverSubselectors = false;
            collectDescendantSelectorIdentifierHashes(selector, hash, end);
            break;
        }
        if (hash == end)
            return;
        relation = selector->relation();
    }
    *hash = 0;
}

static inline const AtomicString* linkAttribute(Node* node)
{
    if (!node->isLink())
        return 0;

    ASSERT(node->isElementNode());
    Element* element = static_cast<Element*>(node);
    if (element->isHTMLElement())
        return &element->fastGetAttribute(hrefAttr);

#if ENABLE(SVG)
    if (element->isSVGElement())
        return &element->getAttribute(XLinkNames::hrefAttr);
#endif

    return 0;
}

EInsideLink SelectorChecker::determineLinkStateSlowCase(Element* element) const
{
    ASSERT(element->isLink());

    const AtomicString* attribute = linkAttribute(element);
    if (!attribute || attribute->isNull())
        return NotInsideLink;

    // An empty href refers to the document itself which is always visited. It is useful to check this explicitly so
    // that visited links can be tested in platform independent manner, without explicit support in the test harness.
    if (attribute->isEmpty())
        return InsideVisitedLink;
    
    LinkHash hash;
    if (element->hasTagName(aTag)) 
        hash = static_cast<HTMLAnchorElement*>(element)->visitedLinkHash();
    else
        hash = visitedLinkHash(m_document->baseURL(), *attribute);

    if (!hash)
        return InsideUnvisitedLink;

    Frame* frame = m_document->frame();
    if (!frame)
        return InsideUnvisitedLink;

    Page* page = frame->page();
    if (!page)
        return InsideUnvisitedLink;

    m_linksCheckedForVisitedState.add(hash);

#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->visitedLinkStrategy()->isLinkVisited(page, hash, m_document->baseURL(), *attribute) ? InsideVisitedLink : InsideUnvisitedLink;
#else
    return page->group().isLinkVisited(hash) ? InsideVisitedLink : InsideUnvisitedLink;
#endif
}

bool SelectorChecker::checkSelector(CSSSelector* sel, Element* element, bool isFastCheckableSelector) const
{
    PseudoId dynamicPseudo = NOPSEUDO;
    if (isFastCheckableSelector && !element->isSVGElement()) {
        if (!fastCheckRightmostSelector(sel, element, VisitedMatchDisabled))
            return false;
        return fastCheckSelector(sel, element);
    }
    return checkSelector(sel, element, dynamicPseudo, false, VisitedMatchDisabled) == SelectorMatches;
}

namespace {

template <bool checkValue(const Element*, AtomicStringImpl*, const QualifiedName&), bool initAttributeName>
inline bool fastCheckSingleSelector(const CSSSelector*& selector, const Element*& element, const CSSSelector*& topChildOrSubselector, const Element*& topChildOrSubselectorMatchElement)
{
    AtomicStringImpl* value = selector->value().impl();
    const QualifiedName& attribute = initAttributeName ? selector->attribute() : anyQName();
    for (; element; element = element->parentElement()) {
        if (checkValue(element, value, attribute) && SelectorChecker::tagMatches(element, selector)) {
            if (selector->relation() == CSSSelector::Descendant)
                topChildOrSubselector = 0;
            else if (!topChildOrSubselector) {
                ASSERT(selector->relation() == CSSSelector::Child || selector->relation() == CSSSelector::SubSelector);
                topChildOrSubselector = selector;
                topChildOrSubselectorMatchElement = element;
            }
            if (selector->relation() != CSSSelector::SubSelector)
                element = element->parentElement();
            selector = selector->tagHistory();
            return true;
        }
        if (topChildOrSubselector) {
            // Child or subselector check failed.
            // If the match element is null, topChildOrSubselector was also the very topmost selector and had to match
            // the original element we were checking.
            if (!topChildOrSubselectorMatchElement)
                return false;
            // There may be other matches down the ancestor chain.
            // Rewind to the topmost child or subselector and the element it matched, continue checking ancestors.
            selector = topChildOrSubselector;
            element = topChildOrSubselectorMatchElement->parentElement();
            topChildOrSubselector = 0;
            return true;
        }
    }
    return false;
}

inline bool checkClassValue(const Element* element, AtomicStringImpl* value, const QualifiedName&)
{
    return element->hasClass() && static_cast<const StyledElement*>(element)->classNames().contains(value);
}

inline bool checkIDValue(const Element* element, AtomicStringImpl* value, const QualifiedName&)
{
    return element->hasID() && element->idForStyleResolution().impl() == value;
}

inline bool checkExactAttributeValue(const Element* element, AtomicStringImpl* value, const QualifiedName& attributeName)
{
    return SelectorChecker::checkExactAttribute(element, attributeName, value);
}

inline bool checkTagValue(const Element*, AtomicStringImpl*, const QualifiedName&)
{
    return true;
}

}

inline bool SelectorChecker::fastCheckRightmostSelector(const CSSSelector* selector, const Element* element, VisitedMatchType visitedMatchType) const
{
    ASSERT(isFastCheckableSelector(selector));

    if (!SelectorChecker::tagMatches(element, selector))
        return false;
    switch (selector->m_match) {
    case CSSSelector::None:
        return true;
    case CSSSelector::Class:
        return checkClassValue(element, selector->value().impl(), anyQName());
    case CSSSelector::Id:
        return checkIDValue(element, selector->value().impl(), anyQName());
    case CSSSelector::Exact:
    case CSSSelector::Set:
        return checkExactAttributeValue(element, selector->value().impl(), selector->attribute());
    case CSSSelector::PseudoClass:
        return commonPseudoClassSelectorMatches(element, selector, visitedMatchType);
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

bool SelectorChecker::fastCheckSelector(const CSSSelector* selector, const Element* element) const
{
    ASSERT(fastCheckRightmostSelector(selector, element, VisitedMatchEnabled));

    const CSSSelector* topChildOrSubselector = 0;
    const Element* topChildOrSubselectorMatchElement = 0;
    if (selector->relation() == CSSSelector::Child || selector->relation() == CSSSelector::SubSelector)
        topChildOrSubselector = selector;

    if (selector->relation() != CSSSelector::SubSelector)
        element = element->parentElement();

    selector = selector->tagHistory();

    // We know this compound selector has descendant, child and subselector combinators only and all components are simple.
    while (selector) {
        switch (selector->m_match) {
        case CSSSelector::Class:
            if (!fastCheckSingleSelector<checkClassValue, false>(selector, element, topChildOrSubselector, topChildOrSubselectorMatchElement))
                return false;
            break;
        case CSSSelector::Id:
            if (!fastCheckSingleSelector<checkIDValue, false>(selector, element, topChildOrSubselector, topChildOrSubselectorMatchElement))
                return false;
            break;
        case CSSSelector::None:
            if (!fastCheckSingleSelector<checkTagValue, false>(selector, element, topChildOrSubselector, topChildOrSubselectorMatchElement))
                return false;
            break;
        case CSSSelector::Set:
        case CSSSelector::Exact:
            if (!fastCheckSingleSelector<checkExactAttributeValue, true>(selector, element, topChildOrSubselector, topChildOrSubselectorMatchElement))
                return false;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return true;
}

static inline bool isFastCheckableRelation(CSSSelector::Relation relation)
{
    return relation == CSSSelector::Descendant || relation == CSSSelector::Child || relation == CSSSelector::SubSelector;
}

static inline bool isFastCheckableMatch(const CSSSelector* selector)
{
    if (selector->m_match == CSSSelector::Set)
        return true;
    if (selector->m_match == CSSSelector::Exact)
        return !htmlAttributeHasCaseInsensitiveValue(selector->attribute());
    return selector->m_match == CSSSelector::None || selector->m_match == CSSSelector::Id || selector->m_match == CSSSelector::Class;
}

static inline bool isFastCheckableRightmostSelector(const CSSSelector* selector)
{
    if (!isFastCheckableRelation(selector->relation()))
        return false;
    return isFastCheckableMatch(selector) || SelectorChecker::isCommonPseudoClassSelector(selector);
}

bool SelectorChecker::isFastCheckableSelector(const CSSSelector* selector)
{
    if (!isFastCheckableRightmostSelector(selector))
        return false;
    for (selector = selector->tagHistory(); selector; selector = selector->tagHistory()) {
        if (!isFastCheckableRelation(selector->relation()))
            return false;
        if (!isFastCheckableMatch(selector))
            return false;
    }
    return true;
}

// Recursive check of selectors and combinators
// It can return 3 different values:
// * SelectorMatches         - the selector matches the element e
// * SelectorFailsLocally    - the selector fails for the element e
// * SelectorFailsCompletely - the selector fails for e and any sibling or ancestor of e
SelectorChecker::SelectorMatch SelectorChecker::checkSelector(CSSSelector* sel, Element* e, PseudoId& dynamicPseudo, bool isSubSelector, VisitedMatchType visitedMatchType, RenderStyle* elementStyle, RenderStyle* elementParentStyle) const
{
#if ENABLE(SVG)
    // Spec: CSS2 selectors cannot be applied to the (conceptually) cloned DOM tree
    // because its contents are not part of the formal document structure.
    if (e->isSVGShadowRoot())
        return SelectorFailsCompletely;
#endif

    // first selector has to match
    if (!checkOneSelector(sel, e, dynamicPseudo, isSubSelector, visitedMatchType, elementStyle, elementParentStyle))
        return SelectorFailsLocally;

    // The rest of the selectors has to match
    CSSSelector::Relation relation = sel->relation();

    // Prepare next sel
    sel = sel->tagHistory();
    if (!sel)
        return SelectorMatches;

    if (relation != CSSSelector::SubSelector) {
        // Bail-out if this selector is irrelevant for the pseudoStyle
        if (m_pseudoStyle != NOPSEUDO && m_pseudoStyle != dynamicPseudo)
            return SelectorFailsCompletely;

        // Disable :visited matching when we see the first link or try to match anything else than an ancestors.
        if (!isSubSelector && (e->isLink() || (relation != CSSSelector::Descendant && relation != CSSSelector::Child)))
            visitedMatchType = VisitedMatchDisabled;
    }

    switch (relation) {
    case CSSSelector::Descendant:
        while (true) {
            ContainerNode* n = e->parentNode();
            if (!n || !n->isElementNode())
                return SelectorFailsCompletely;
            e = static_cast<Element*>(n);
            SelectorMatch match = checkSelector(sel, e, dynamicPseudo, false, visitedMatchType);
            if (match != SelectorFailsLocally)
                return match;
        }
        break;
    case CSSSelector::Child:
        {
            ContainerNode* n = e->parentNode();
            if (!n || !n->isElementNode())
                return SelectorFailsCompletely;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, dynamicPseudo, false, visitedMatchType);
        }
    case CSSSelector::DirectAdjacent:
        {
            if (!m_isCollectingRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                if (parentStyle)
                    parentStyle->setChildrenAffectedByDirectAdjacentRules();
            }
            Node* n = e->previousSibling();
            while (n && !n->isElementNode())
                n = n->previousSibling();
            if (!n)
                return SelectorFailsLocally;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, dynamicPseudo, false, visitedMatchType);
        }
    case CSSSelector::IndirectAdjacent:
        if (!m_isCollectingRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
            RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
            if (parentStyle)
                parentStyle->setChildrenAffectedByForwardPositionalRules();
        }
        while (true) {
            Node* n = e->previousSibling();
            while (n && !n->isElementNode())
                n = n->previousSibling();
            if (!n)
                return SelectorFailsLocally;
            e = static_cast<Element*>(n);
            SelectorMatch match = checkSelector(sel, e, dynamicPseudo, false, visitedMatchType);
            if (match != SelectorFailsLocally)
                return match;
        };
        break;
    case CSSSelector::SubSelector:
        // a selector is invalid if something follows a pseudo-element
        // We make an exception for scrollbar pseudo elements and allow a set of pseudo classes (but nothing else)
        // to follow the pseudo elements.
        if ((elementStyle || m_isCollectingRulesOnly) && dynamicPseudo != NOPSEUDO && dynamicPseudo != SELECTION
             && !((RenderScrollbar::scrollbarForStyleResolve() || dynamicPseudo == SCROLLBAR_CORNER || dynamicPseudo == RESIZER) && sel->m_match == CSSSelector::PseudoClass))
            return SelectorFailsCompletely;
        return checkSelector(sel, e, dynamicPseudo, true, visitedMatchType, elementStyle, elementParentStyle);
    case CSSSelector::ShadowDescendant:
        {
            Node* shadowHostNode = e->shadowAncestorNode();
            if (shadowHostNode == e || !shadowHostNode->isElementNode())
                return SelectorFailsCompletely;
            e = static_cast<Element*>(shadowHostNode);
            return checkSelector(sel, e, dynamicPseudo, false, visitedMatchType);
        }
    }

    return SelectorFailsCompletely;
}

static void addLocalNameToSet(HashSet<AtomicStringImpl*>* set, const QualifiedName& qName)
{
    set->add(qName.localName().impl());
}

static HashSet<AtomicStringImpl*>* createHtmlCaseInsensitiveAttributesSet()
{
    // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
    // Mozilla treats all other values as case-sensitive, thus so do we.
    HashSet<AtomicStringImpl*>* attrSet = new HashSet<AtomicStringImpl*>;

    addLocalNameToSet(attrSet, accept_charsetAttr);
    addLocalNameToSet(attrSet, acceptAttr);
    addLocalNameToSet(attrSet, alignAttr);
    addLocalNameToSet(attrSet, alinkAttr);
    addLocalNameToSet(attrSet, axisAttr);
    addLocalNameToSet(attrSet, bgcolorAttr);
    addLocalNameToSet(attrSet, charsetAttr);
    addLocalNameToSet(attrSet, checkedAttr);
    addLocalNameToSet(attrSet, clearAttr);
    addLocalNameToSet(attrSet, codetypeAttr);
    addLocalNameToSet(attrSet, colorAttr);
    addLocalNameToSet(attrSet, compactAttr);
    addLocalNameToSet(attrSet, declareAttr);
    addLocalNameToSet(attrSet, deferAttr);
    addLocalNameToSet(attrSet, dirAttr);
    addLocalNameToSet(attrSet, disabledAttr);
    addLocalNameToSet(attrSet, enctypeAttr);
    addLocalNameToSet(attrSet, faceAttr);
    addLocalNameToSet(attrSet, frameAttr);
    addLocalNameToSet(attrSet, hreflangAttr);
    addLocalNameToSet(attrSet, http_equivAttr);
    addLocalNameToSet(attrSet, langAttr);
    addLocalNameToSet(attrSet, languageAttr);
    addLocalNameToSet(attrSet, linkAttr);
    addLocalNameToSet(attrSet, mediaAttr);
    addLocalNameToSet(attrSet, methodAttr);
    addLocalNameToSet(attrSet, multipleAttr);
    addLocalNameToSet(attrSet, nohrefAttr);
    addLocalNameToSet(attrSet, noresizeAttr);
    addLocalNameToSet(attrSet, noshadeAttr);
    addLocalNameToSet(attrSet, nowrapAttr);
    addLocalNameToSet(attrSet, readonlyAttr);
    addLocalNameToSet(attrSet, relAttr);
    addLocalNameToSet(attrSet, revAttr);
    addLocalNameToSet(attrSet, rulesAttr);
    addLocalNameToSet(attrSet, scopeAttr);
    addLocalNameToSet(attrSet, scrollingAttr);
    addLocalNameToSet(attrSet, selectedAttr);
    addLocalNameToSet(attrSet, shapeAttr);
    addLocalNameToSet(attrSet, targetAttr);
    addLocalNameToSet(attrSet, textAttr);
    addLocalNameToSet(attrSet, typeAttr);
    addLocalNameToSet(attrSet, valignAttr);
    addLocalNameToSet(attrSet, valuetypeAttr);
    addLocalNameToSet(attrSet, vlinkAttr);

    return attrSet;
}

bool htmlAttributeHasCaseInsensitiveValue(const QualifiedName& attr)
{
    static HashSet<AtomicStringImpl*>* htmlCaseInsensitiveAttributesSet = createHtmlCaseInsensitiveAttributesSet();
    bool isPossibleHTMLAttr = !attr.hasPrefix() && (attr.namespaceURI() == nullAtom);
    return isPossibleHTMLAttr && htmlCaseInsensitiveAttributesSet->contains(attr.localName().impl());
}

static bool attributeValueMatches(Attribute* attributeItem, CSSSelector::Match match, const AtomicString& selectorValue, bool caseSensitive)
{
    const AtomicString& value = attributeItem->value();
    if (value.isNull())
        return false;

    switch (match) {
    case CSSSelector::Exact:
        if (caseSensitive ? selectorValue != value : !equalIgnoringCase(selectorValue, value))
            return false;
        break;
    case CSSSelector::List:
        {
            // Ignore empty selectors or selectors containing spaces
            if (selectorValue.contains(' ') || selectorValue.isEmpty())
                return false;

            unsigned startSearchAt = 0;
            while (true) {
                size_t foundPos = value.find(selectorValue, startSearchAt, caseSensitive);
                if (foundPos == notFound)
                    return false;
                if (!foundPos || value[foundPos - 1] == ' ') {
                    unsigned endStr = foundPos + selectorValue.length();
                    if (endStr == value.length() || value[endStr] == ' ')
                        break; // We found a match.
                }

                // No match. Keep looking.
                startSearchAt = foundPos + 1;
            }
            break;
        }
    case CSSSelector::Contain:
        if (!value.contains(selectorValue, caseSensitive) || selectorValue.isEmpty())
            return false;
        break;
    case CSSSelector::Begin:
        if (!value.startsWith(selectorValue, caseSensitive) || selectorValue.isEmpty())
            return false;
        break;
    case CSSSelector::End:
        if (!value.endsWith(selectorValue, caseSensitive) || selectorValue.isEmpty())
            return false;
        break;
    case CSSSelector::Hyphen:
        if (value.length() < selectorValue.length())
            return false;
        if (!value.startsWith(selectorValue, caseSensitive))
            return false;
        // It they start the same, check for exact match or following '-':
        if (value.length() != selectorValue.length() && value[selectorValue.length()] != '-')
            return false;
        break;
    case CSSSelector::PseudoClass:
    case CSSSelector::PseudoElement:
    default:
        break;
    }

    return true;
}

static bool anyAttributeMatches(NamedNodeMap* attributes, CSSSelector::Match match, const QualifiedName& selectorAttr, const AtomicString& selectorValue, bool caseSensitive)
{
    for (size_t i = 0; i < attributes->length(); ++i) {
        Attribute* attributeItem = attributes->attributeItem(i);

        if (!SelectorChecker::attributeNameMatches(attributeItem, selectorAttr))
            continue;

        if (attributeValueMatches(attributeItem, match, selectorValue, caseSensitive))
            return true;
    }

    return false;
}

bool SelectorChecker::checkOneSelector(CSSSelector* sel, Element* e, PseudoId& dynamicPseudo, bool isSubSelector, VisitedMatchType visitedMatchType, RenderStyle* elementStyle, RenderStyle* elementParentStyle) const
{
    ASSERT(e);
    if (!SelectorChecker::tagMatches(e, sel))
        return false;

    if (sel->m_match == CSSSelector::Class)
        return e->hasClass() && static_cast<StyledElement*>(e)->classNames().contains(sel->value());

    if (sel->m_match == CSSSelector::Id)
        return e->hasID() && e->idForStyleResolution() == sel->value();

    if (sel->isAttributeSelector()) {
        const QualifiedName& attr = sel->attribute();

        NamedNodeMap* attributes = e->attributes(true);
        if (!attributes)
            return false;

        bool caseSensitive = !m_documentIsHTML || !htmlAttributeHasCaseInsensitiveValue(attr);

        if (!anyAttributeMatches(attributes, static_cast<CSSSelector::Match>(sel->m_match), attr, sel->value(), caseSensitive))
            return false;
    }

    if (sel->m_match == CSSSelector::PseudoClass) {
        // Handle :not up front.
        if (sel->pseudoType() == CSSSelector::PseudoNot) {
            ASSERT(sel->selectorList());
            for (CSSSelector* subSel = sel->selectorList()->first(); subSel; subSel = subSel->tagHistory()) {
                // :not cannot nest. I don't really know why this is a
                // restriction in CSS3, but it is, so let's honor it.
                // the parser enforces that this never occurs
                ASSERT(subSel->pseudoType() != CSSSelector::PseudoNot);
                // We select between :visited and :link when applying. We don't know which one applied (or not) yet.
                if (subSel->pseudoType() == CSSSelector::PseudoVisited || (subSel->pseudoType() == CSSSelector::PseudoLink && visitedMatchType == VisitedMatchEnabled))
                    return true;
                if (!checkOneSelector(subSel, e, dynamicPseudo, true, visitedMatchType, elementStyle, elementParentStyle))
                    return true;
            }
        } else if (dynamicPseudo != NOPSEUDO && (RenderScrollbar::scrollbarForStyleResolve() || dynamicPseudo == SCROLLBAR_CORNER || dynamicPseudo == RESIZER)) {
            // CSS scrollbars match a specific subset of pseudo classes, and they have specialized rules for each
            // (since there are no elements involved).
            return checkScrollbarPseudoClass(sel, dynamicPseudo);
        } else if (dynamicPseudo == SELECTION) {
            if (sel->pseudoType() == CSSSelector::PseudoWindowInactive)
                return !m_document->page()->focusController()->isActive();
        }

        // Normal element pseudo class checking.
        switch (sel->pseudoType()) {
            // Pseudo classes:
        case CSSSelector::PseudoNot:
            break; // Already handled up above.
        case CSSSelector::PseudoEmpty:
            {
                bool result = true;
                for (Node* n = e->firstChild(); n; n = n->nextSibling()) {
                    if (n->isElementNode()) {
                        result = false;
                        break;
                    }
                    if (n->isTextNode()) {
                        Text* textNode = static_cast<Text*>(n);
                        if (!textNode->data().isEmpty()) {
                            result = false;
                            break;
                        }
                    }
                }
                if (!m_isCollectingRulesOnly) {
                    if (elementStyle)
                        elementStyle->setEmptyState(result);
                    else if (e->renderStyle() && (e->document()->usesSiblingRules() || e->renderStyle()->unique()))
                        e->renderStyle()->setEmptyState(result);
                }
                return result;
            }
        case CSSSelector::PseudoFirstChild:
            // first-child matches the first child that is an element
            if (e->parentNode() && e->parentNode()->isElementNode()) {
                bool result = false;
                Node* n = e->previousSibling();
                while (n && !n->isElementNode())
                    n = n->previousSibling();
                if (!n)
                    result = true;
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByFirstChildRules();
                    if (result && childStyle)
                        childStyle->setFirstChildState();
                }
                return result;
            }
            break;
        case CSSSelector::PseudoFirstOfType:
            // first-of-type matches the first element of its type
            if (e->parentNode() && e->parentNode()->isElementNode()) {
                bool result = false;
                const QualifiedName& type = e->tagQName();
                Node* n = e->previousSibling();
                while (n) {
                    if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                        break;
                    n = n->previousSibling();
                }
                if (!n)
                    result = true;
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByForwardPositionalRules();
                }
                return result;
            }
            break;
        case CSSSelector::PseudoLastChild:
            // last-child matches the last child that is an element
            if (Element* parentElement = e->parentElement()) {
                bool result = false;
                if (parentElement->isFinishedParsingChildren()) {
                    Node* n = e->nextSibling();
                    while (n && !n->isElementNode())
                        n = n->nextSibling();
                    if (!n)
                        result = true;
                }
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByLastChildRules();
                    if (result && childStyle)
                        childStyle->setLastChildState();
                }
                return result;
            }
            break;
        case CSSSelector::PseudoLastOfType:
            // last-of-type matches the last element of its type
            if (Element* parentElement = e->parentElement()) {
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByBackwardPositionalRules();
                }
                if (!parentElement->isFinishedParsingChildren())
                    return false;
                bool result = false;
                const QualifiedName& type = e->tagQName();
                Node* n = e->nextSibling();
                while (n) {
                    if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                        break;
                    n = n->nextSibling();
                }
                if (!n)
                    result = true;
                return result;
            }
            break;
        case CSSSelector::PseudoOnlyChild:
            if (Element* parentElement = e->parentElement()) {
                bool firstChild = false;
                bool lastChild = false;

                Node* n = e->previousSibling();
                while (n && !n->isElementNode())
                    n = n->previousSibling();
                if (!n)
                    firstChild = true;
                if (firstChild && parentElement->isFinishedParsingChildren()) {
                    n = e->nextSibling();
                    while (n && !n->isElementNode())
                        n = n->nextSibling();
                    if (!n)
                        lastChild = true;
                }
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle) {
                        parentStyle->setChildrenAffectedByFirstChildRules();
                        parentStyle->setChildrenAffectedByLastChildRules();
                    }
                    if (firstChild && childStyle)
                        childStyle->setFirstChildState();
                    if (lastChild && childStyle)
                        childStyle->setLastChildState();
                }
                return firstChild && lastChild;
            }
            break;
        case CSSSelector::PseudoOnlyOfType:
            // FIXME: This selector is very slow.
            if (Element* parentElement = e->parentElement()) {
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle) {
                        parentStyle->setChildrenAffectedByForwardPositionalRules();
                        parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                }
                if (!parentElement->isFinishedParsingChildren())
                    return false;
                bool firstChild = false;
                bool lastChild = false;
                const QualifiedName& type = e->tagQName();
                Node* n = e->previousSibling();
                while (n) {
                    if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                        break;
                    n = n->previousSibling();
                }
                if (!n)
                    firstChild = true;
                if (firstChild) {
                    n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->nextSibling();
                    }
                    if (!n)
                        lastChild = true;
                }
                return firstChild && lastChild;
            }
            break;
        case CSSSelector::PseudoNthChild:
            if (!sel->parseNth())
                break;
            if (Element* parentElement = e->parentElement()) {
                int count = 1;
                Node* n = e->previousSibling();
                while (n) {
                    if (n->isElementNode()) {
                        RenderStyle* s = n->renderStyle();
                        unsigned index = s ? s->childIndex() : 0;
                        if (index) {
                            count += index;
                            break;
                        }
                        count++;
                    }
                    n = n->previousSibling();
                }

                if (!m_isCollectingRulesOnly) {
                    RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (childStyle)
                        childStyle->setChildIndex(count);
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByForwardPositionalRules();
                }

                if (sel->matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoNthOfType:
            if (!sel->parseNth())
                break;
            if (Element* parentElement = e->parentElement()) {
                int count = 1;
                const QualifiedName& type = e->tagQName();
                Node* n = e->previousSibling();
                while (n) {
                    if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                        count++;
                    n = n->previousSibling();
                }

                if (!m_isCollectingRulesOnly) {
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByForwardPositionalRules();
                }

                if (sel->matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoNthLastChild:
            if (!sel->parseNth())
                break;
            if (Element* parentElement = e->parentElement()) {
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByBackwardPositionalRules();
                }
                if (!parentElement->isFinishedParsingChildren())
                    return false;
                int count = 1;
                Node* n = e->nextSibling();
                while (n) {
                    if (n->isElementNode())
                        count++;
                    n = n->nextSibling();
                }
                if (sel->matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoNthLastOfType:
            if (!sel->parseNth())
                break;
            if (Element* parentElement = e->parentElement()) {
                if (!m_isCollectingRulesOnly) {
                    RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                    if (parentStyle)
                        parentStyle->setChildrenAffectedByBackwardPositionalRules();
                }
                if (!parentElement->isFinishedParsingChildren())
                    return false;
                int count = 1;
                const QualifiedName& type = e->tagQName();
                Node* n = e->nextSibling();
                while (n) {
                    if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                        count++;
                    n = n->nextSibling();
                }
                if (sel->matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoTarget:
            if (e == e->document()->cssTarget())
                return true;
            break;
        case CSSSelector::PseudoAny:
            for (CSSSelector* selector = sel->selectorList()->first(); selector; selector = CSSSelectorList::next(selector)) {
                if (checkSelector(selector, e, dynamicPseudo, true, visitedMatchType, elementStyle, elementParentStyle) == SelectorMatches)
                    return true;
            }
            break;
        case CSSSelector::PseudoAutofill:
            if (!e || !e->isFormControlElement())
                break;
            if (HTMLInputElement* inputElement = e->toInputElement())
                return inputElement->isAutofilled();
            break;
        case CSSSelector::PseudoAnyLink:
        case CSSSelector::PseudoLink:
            // :visited and :link matches are separated later when applying the style. Here both classes match all links...
            return e->isLink();
        case CSSSelector::PseudoVisited:
            // ...except if :visited matching is disabled for ancestor/sibling matching.
            return e->isLink() && visitedMatchType == VisitedMatchEnabled;
        case CSSSelector::PseudoDrag:
            if (elementStyle)
                elementStyle->setAffectedByDragRules(true);
            else if (e->renderStyle())
                e->renderStyle()->setAffectedByDragRules(true);
            if (e->renderer() && e->renderer()->isDragging())
                return true;
            break;
        case CSSSelector::PseudoFocus:
            return matchesFocusPseudoClass(e);
        case CSSSelector::PseudoHover:
            // If we're in quirks mode, then hover should never match anchors with no
            // href and *:hover should not match anything. This is important for sites like wsj.com.
            if (m_strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                if (elementStyle)
                    elementStyle->setAffectedByHoverRules(true);
                else if (e->renderStyle())
                    e->renderStyle()->setAffectedByHoverRules(true);
                if (e->hovered() || InspectorInstrumentation::forcePseudoState(e, CSSSelector::PseudoHover))
                    return true;
            }
            break;
        case CSSSelector::PseudoActive:
            // If we're in quirks mode, then :active should never match anchors with no
            // href and *:active should not match anything.
            if (m_strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                if (elementStyle)
                    elementStyle->setAffectedByActiveRules(true);
                else if (e->renderStyle())
                    e->renderStyle()->setAffectedByActiveRules(true);
                if (e->active() || InspectorInstrumentation::forcePseudoState(e, CSSSelector::PseudoActive))
                    return true;
            }
            break;
        case CSSSelector::PseudoEnabled:
            if (e && e->isFormControlElement())
                return e->isEnabledFormControl();
            break;
        case CSSSelector::PseudoFullPageMedia:
            return e && e->document() && e->document()->isMediaDocument();
            break;
        case CSSSelector::PseudoDefault:
            return e && e->isDefaultButtonForForm();
        case CSSSelector::PseudoDisabled:
            if (e && e->isFormControlElement())
                return !e->isEnabledFormControl();
            break;
        case CSSSelector::PseudoReadOnly:
            if (!e || !e->isFormControlElement())
                return false;
            return e->isTextFormControl() && e->isReadOnlyFormControl();
        case CSSSelector::PseudoReadWrite:
            if (!e || !e->isFormControlElement())
                return false;
            return e->isTextFormControl() && !e->isReadOnlyFormControl();
        case CSSSelector::PseudoOptional:
            return e && e->isOptionalFormControl();
        case CSSSelector::PseudoRequired:
            return e && e->isRequiredFormControl();
        case CSSSelector::PseudoValid:
            if (!e)
                return false;
            e->document()->setContainsValidityStyleRules();
            return e->willValidate() && e->isValidFormControlElement();
        case CSSSelector::PseudoInvalid:
            if (!e)
                return false;
            e->document()->setContainsValidityStyleRules();
            return (e->willValidate() && !e->isValidFormControlElement()) || e->hasUnacceptableValue();
        case CSSSelector::PseudoChecked:
            {
                if (!e || !e->isFormControlElement())
                    break;
                // Even though WinIE allows checked and indeterminate to co-exist, the CSS selector spec says that
                // you can't be both checked and indeterminate. We will behave like WinIE behind the scenes and just
                // obey the CSS spec here in the test for matching the pseudo.
                HTMLInputElement* inputElement = e->toInputElement();
                if (inputElement && inputElement->shouldAppearChecked() && !inputElement->isIndeterminate())
                    return true;
                if (e->hasTagName(optionTag) && toHTMLOptionElement(e)->selected())
                    return true;
                break;
            }
        case CSSSelector::PseudoIndeterminate:
            {
                if (!e || !e->isFormControlElement())
                    break;
#if ENABLE(PROGRESS_TAG)
                if (e->hasTagName(progressTag)) {
                    HTMLProgressElement* progress = static_cast<HTMLProgressElement*>(e);
                    if (progress && !progress->isDeterminate())
                        return true;
                    break;
                }
#endif
                HTMLInputElement* inputElement = e->toInputElement();
                if (inputElement && inputElement->isIndeterminate())
                    return true;
                break;
            }
        case CSSSelector::PseudoRoot:
            if (e == e->document()->documentElement())
                return true;
            break;
        case CSSSelector::PseudoLang:
            {
                AtomicString value = e->computeInheritedLanguage();
                const AtomicString& argument = sel->argument();
                if (value.isEmpty() || !value.startsWith(argument, false))
                    break;
                if (value.length() != argument.length() && value[argument.length()] != '-')
                    break;
                return true;
            }
#if ENABLE(FULLSCREEN_API)
        case CSSSelector::PseudoFullScreen:
            // While a Document is in the fullscreen state, and the document's current fullscreen
            // element is an element in the document, the 'full-screen' pseudoclass applies to
            // that element. Also, an <iframe>, <object> or <embed> element whose child browsing
            // context's Document is in the fullscreen state has the 'full-screen' pseudoclass applied.
            if (e->isFrameElementBase() && static_cast<HTMLFrameElementBase*>(e)->containsFullScreenElement())
                return true;
            if (!e->document()->webkitIsFullScreen())
                return false;
            return e == e->document()->webkitCurrentFullScreenElement();
        case CSSSelector::PseudoAnimatingFullScreenTransition:
            if (e != e->document()->webkitCurrentFullScreenElement())
                return false;
            return e->document()->isAnimatingFullScreen();
        case CSSSelector::PseudoFullScreenAncestor:
            return e->containsFullScreenElement();
        case CSSSelector::PseudoFullScreenDocument:
            // While a Document is in the fullscreen state, the 'full-screen-document' pseudoclass applies
            // to all elements of that Document.
            if (!e->document()->webkitIsFullScreen())
                return false;
            return true;
#endif
        case CSSSelector::PseudoInRange:
            if (!e)
                return false;
            e->document()->setContainsValidityStyleRules();
            return e->isInRange();
        case CSSSelector::PseudoOutOfRange:
            if (!e)
                return false;
            e->document()->setContainsValidityStyleRules();
            return e->isOutOfRange();
        case CSSSelector::PseudoUnknown:
        case CSSSelector::PseudoNotParsed:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return false;
    }
    if (sel->m_match == CSSSelector::PseudoElement) {
        if (!elementStyle && !m_isCollectingRulesOnly)
            return false;

        if (sel->isUnknownPseudoElement()) {
            m_hasUnknownPseudoElements = true;
            return e->shadowPseudoId() == sel->value();
        }

        PseudoId pseudoId = CSSSelector::pseudoId(sel->pseudoType());
        if (pseudoId == FIRST_LETTER) {
            if (Document* document = e->document())
                document->setUsesFirstLetterRules(true);
        }
        if (pseudoId != NOPSEUDO)
            dynamicPseudo = pseudoId;
    }
    // ### add the rest of the checks...
    return true;
}

bool SelectorChecker::checkScrollbarPseudoClass(CSSSelector* sel, PseudoId&) const
{
    RenderScrollbar* scrollbar = RenderScrollbar::scrollbarForStyleResolve();
    ScrollbarPart part = RenderScrollbar::partForStyleResolve();

    // FIXME: This is a temporary hack for resizers and scrollbar corners. Eventually :window-inactive should become a real
    // pseudo class and just apply to everything.
    if (sel->pseudoType() == CSSSelector::PseudoWindowInactive)
        return !m_document->page()->focusController()->isActive();

    if (!scrollbar)
        return false;

    ASSERT(sel->m_match == CSSSelector::PseudoClass);
    switch (sel->pseudoType()) {
    case CSSSelector::PseudoEnabled:
        return scrollbar->enabled();
    case CSSSelector::PseudoDisabled:
        return !scrollbar->enabled();
    case CSSSelector::PseudoHover:
        {
            ScrollbarPart hoveredPart = scrollbar->hoveredPart();
            if (part == ScrollbarBGPart)
                return hoveredPart != NoPart;
            if (part == TrackBGPart)
                return hoveredPart == BackTrackPart || hoveredPart == ForwardTrackPart || hoveredPart == ThumbPart;
            return part == hoveredPart;
        }
    case CSSSelector::PseudoActive:
        {
            ScrollbarPart pressedPart = scrollbar->pressedPart();
            if (part == ScrollbarBGPart)
                return pressedPart != NoPart;
            if (part == TrackBGPart)
                return pressedPart == BackTrackPart || pressedPart == ForwardTrackPart || pressedPart == ThumbPart;
            return part == pressedPart;
        }
    case CSSSelector::PseudoHorizontal:
        return scrollbar->orientation() == HorizontalScrollbar;
    case CSSSelector::PseudoVertical:
        return scrollbar->orientation() == VerticalScrollbar;
    case CSSSelector::PseudoDecrement:
        return part == BackButtonStartPart || part == BackButtonEndPart || part == BackTrackPart;
    case CSSSelector::PseudoIncrement:
        return part == ForwardButtonStartPart || part == ForwardButtonEndPart || part == ForwardTrackPart;
    case CSSSelector::PseudoStart:
        return part == BackButtonStartPart || part == ForwardButtonStartPart || part == BackTrackPart;
    case CSSSelector::PseudoEnd:
        return part == BackButtonEndPart || part == ForwardButtonEndPart || part == ForwardTrackPart;
    case CSSSelector::PseudoDoubleButton:
        {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackButtonStartPart || part == ForwardButtonStartPart || part == BackTrackPart)
                return buttonsPlacement == ScrollbarButtonsDoubleStart || buttonsPlacement == ScrollbarButtonsDoubleBoth;
            if (part == BackButtonEndPart || part == ForwardButtonEndPart || part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsDoubleEnd || buttonsPlacement == ScrollbarButtonsDoubleBoth;
            return false;
        }
    case CSSSelector::PseudoSingleButton:
        {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackButtonStartPart || part == ForwardButtonEndPart || part == BackTrackPart || part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsSingle;
            return false;
        }
    case CSSSelector::PseudoNoButton:
        {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackTrackPart)
                return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleEnd;
            if (part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleStart;
            return false;
        }
    case CSSSelector::PseudoCornerPresent:
        return scrollbar->scrollableArea()->isScrollCornerVisible();
    default:
        return false;
    }
}

void SelectorChecker::allVisitedStateChanged()
{
    if (m_linksCheckedForVisitedState.isEmpty())
        return;
    for (Node* node = m_document; node; node = node->traverseNextNode()) {
        if (node->isLink())
            node->setNeedsStyleRecalc();
    }
}

void SelectorChecker::visitedStateChanged(LinkHash visitedHash)
{
    if (!m_linksCheckedForVisitedState.contains(visitedHash))
        return;
    for (Node* node = m_document; node; node = node->traverseNextNode()) {
        LinkHash hash = 0;
        if (node->hasTagName(aTag))
            hash = static_cast<HTMLAnchorElement*>(node)->visitedLinkHash();
        else if (const AtomicString* attr = linkAttribute(node))
            hash = visitedLinkHash(m_document->baseURL(), *attr);
        if (hash == visitedHash)
            node->setNeedsStyleRecalc();
    }
}

bool SelectorChecker::commonPseudoClassSelectorMatches(const Element* element, const CSSSelector* selector, VisitedMatchType visitedMatchType) const
{
    ASSERT(isCommonPseudoClassSelector(selector));
    switch (selector->pseudoType()) {
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoAnyLink:
        return element->isLink();
    case CSSSelector::PseudoVisited:
        return element->isLink() && visitedMatchType == VisitedMatchEnabled;
    case CSSSelector::PseudoFocus:
        return matchesFocusPseudoClass(element);
    default:
        ASSERT_NOT_REACHED();
    }
    return true;
}

unsigned SelectorChecker::determineLinkMatchType(const CSSSelector* selector)
{
    unsigned linkMatchType = MatchAll;

    // Statically determine if this selector will match a link in visited, unvisited or any state, or never.
    // :visited never matches other elements than the innermost link element.
    for (; selector; selector = selector->tagHistory()) {
        switch (selector->pseudoType()) {
        case CSSSelector::PseudoNot:
            // :not(:visited) is equivalent to :link. Parser enforces that :not can't nest.
            for (CSSSelector* subSelector = selector->selectorList()->first(); subSelector; subSelector = subSelector->tagHistory()) {
                CSSSelector::PseudoType subType = subSelector->pseudoType();
                if (subType == CSSSelector::PseudoVisited)
                    linkMatchType &= ~SelectorChecker::MatchVisited;
                else if (subType == CSSSelector::PseudoLink)
                    linkMatchType &= ~SelectorChecker::MatchLink;
            }
            break;
        case CSSSelector::PseudoLink:
            linkMatchType &= ~SelectorChecker::MatchVisited;
            break;
        case CSSSelector::PseudoVisited:
            linkMatchType &= ~SelectorChecker::MatchLink;
            break;
        default:
            // We don't support :link and :visited inside :-webkit-any.
            break;
        }
        CSSSelector::Relation relation = selector->relation();
        if (relation == CSSSelector::SubSelector)
            continue;
        if (relation != CSSSelector::Descendant && relation != CSSSelector::Child)
            return linkMatchType;
        if (linkMatchType != MatchAll)
            return linkMatchType;
    }
    return linkMatchType;
}

bool SelectorChecker::isFrameFocused(const Element* element)
{
    return element->document()->frame() && element->document()->frame()->selection()->isFocusedAndActive();
}

}
