/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "htmlediting.h"

#include "css_computedstyle.h"
#include "css_value.h"
#include "css_valueimpl.h"
#include "cssparser.h"
#include "cssproperties.h"
#include "DocumentImpl.h"
#include "dom_elementimpl.h"
#include "dom_nodeimpl.h"
#include "dom_position.h"
#include "dom_stringimpl.h"
#include "dom_textimpl.h"
#include "dom2_range.h"
#include "dom2_rangeimpl.h"
#include "html_elementimpl.h"
#include "html_imageimpl.h"
#include "html_interchange.h"
#include "htmlnames.h"
#include "qcolor.h"
#include "qptrlist.h"
#include "render_object.h"
#include "render_style.h"
#include "RenderText.h"
#include "visible_position.h"
#include "visible_text.h"
#include "visible_units.h"

using namespace DOM::HTMLNames;

using DOM::AttrImpl;
using DOM::CSSComputedStyleDeclarationImpl;
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::CSSParser;
using DOM::CSSPrimitiveValue;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSProperty;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSValue;
using DOM::CSSValueImpl;
using DOM::DocumentFragmentImpl;
using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::DoNotUpdateLayout;
using DOM::EditingTextImpl;
using DOM::ElementImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLImageElementImpl;
using DOM::NamedAttrMapImpl;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::Position;
using DOM::RangeImpl;
using DOM::TextImpl;
using DOM::TreeWalkerImpl;

#include <kxmlcore/Assertions.h>
#include "KWQLogging.h"
#include "KWQRegExp.h"

namespace khtml {

// Atomic means that the node has no children, or has children which are ignored for the
// purposes of editing.
bool isAtomicNode(const NodeImpl *node)
{
    return node && (!node->hasChildNodes() || node->renderer() && node->renderer()->isWidget());
}

// This method is used to create positions in the DOM. It returns the maximum valid offset
// in a node.  It returns 1 for <br>s and replaced elements, which creates 
// technically invalid DOM Positions.  Be sure to call equivalentRangeCompliantPosition
// on a Position before using it to create a DOM Range, or an exception will be thrown.
int maxDeepOffset(const NodeImpl *node)
{
    if (node->isTextNode())
        return static_cast<const TextImpl*>(node)->length();
        
    if (node->hasChildNodes())
        return node->childNodeCount();
    
    RenderObject *renderer = node->renderer();
    if (node->hasTagName(brTag) || (renderer && renderer->isReplaced()))
        return 1;

    return 0;
}

void rebalanceWhitespaceInTextNode(NodeImpl *node, unsigned int start, unsigned int length)
{
    static QRegExp nonRegularWhitespace("[\xa0\n]");
    static QString twoSpaces("  ");
    static QString nbsp("\xa0");
    static QString space(" ");
     
    ASSERT(node->isTextNode());
    TextImpl *textNode = static_cast<TextImpl *>(node);
    DOMString text = textNode->data();
    ASSERT(length <= text.length() && start + length <= text.length());
    
    QString substring = text.substring(start, length).qstring();

    substring.replace(nonRegularWhitespace, space);
    
    // The sequence should alternate between spaces and nbsps, always ending in a regular space.
    // Note: This pattern doesn't mimic TextEdit editing behavior on other clients that don't
    // support our -khtml-nbsp-mode: space, but it comes close.
    static QString pattern("\xa0 ");
    int end = length - 1; 
    int i = substring.findRev(twoSpaces, end);
    while (i >= 0) {
        substring.replace(i , 2, pattern);
        i = substring.findRev(twoSpaces, i);
    }
    
    // Rendering will collapse any regular whitespace at the start or end of a line.  To prevent this, we use
    // a nbsp at the start and end of a text node.  This is sometimes unnecessary,  E.G. <a>link</a> text
    if (start == 0 && substring[0] == ' ')
        substring.replace(0, 1, nbsp);
    if (start + length == text.length() && substring[end] == ' ')
        substring.replace(end, 1, nbsp);
    
    text.remove(start, length);
    text.insert(DOMString(substring), start);
}

bool isTableStructureNode(const NodeImpl *node)
{
    RenderObject *r = node->renderer();
    return (r && (r->isTableCell() || r->isTableRow() || r->isTableSection() || r->isTableCol()));
}

DOMString &nonBreakingSpaceString()
{
    static DOMString nonBreakingSpaceString = QString(QChar(NON_BREAKING_SPACE));
    return nonBreakingSpaceString;
}

void derefNodesInList(QPtrList<NodeImpl> &list)
{
    for (QPtrListIterator<NodeImpl> it(list); it.current(); ++it)
        it.current()->deref();
}

static int maxRangeOffset(NodeImpl *n)
{
    if (DOM::offsetInCharacters(n->nodeType()))
        return n->maxOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

bool isSpecialElement(const NodeImpl *n)
{
    if (!n)
        return false;
        
    if (!n->isHTMLElement())
        return false;

    if (n->isLink())
        return true;

    if (n->hasTagName(ulTag) || n->hasTagName(olTag) || n->hasTagName(dlTag))
        return true;

    RenderObject *renderer = n->renderer();
    if (!renderer)
        return false;
        
    if (renderer->style()->display() == TABLE || renderer->style()->display() == INLINE_TABLE)
        return true;

    if (renderer->style()->isFloating())
        return true;

    if (renderer->style()->position() != STATIC)
        return true;
        
    return false;
}

bool isFirstVisiblePositionInSpecialElement(const Position& pos)
{
    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            return false;
        if (n->rootEditableElement() == NULL)
            return false;
        if (isSpecialElement(n))
            return true;
    }

    return false;
}

Position positionBeforeNode(const NodeImpl *node)
{
    return Position(node->parentNode(), node->nodeIndex());
}

Position positionBeforeContainingSpecialElement(const Position& pos)
{
    ASSERT(isFirstVisiblePositionInSpecialElement(pos));

    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
    
    NodeImpl *outermostSpecialElement = NULL;

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            break;
        if (n->rootEditableElement() == NULL)
            break;
        if (isSpecialElement(n))
            outermostSpecialElement = n;
    }
    
    ASSERT(outermostSpecialElement);

    Position result = positionBeforeNode(outermostSpecialElement);
    if (result.isNull() || !result.node()->rootEditableElement())
        return pos;
    
    return result;
}

bool isLastVisiblePositionInSpecialElement(const Position& pos)
{
    // make sure to get a range-compliant version of the position
    // FIXME: rangePos isn't being used to create DOM Ranges, so why does it need to be range compliant?
    Position rangePos = VisiblePosition(pos, DOWNSTREAM).deepEquivalent().equivalentRangeCompliantPosition();

    VisiblePosition vPos = VisiblePosition(rangePos, DOWNSTREAM);

    for (NodeImpl *n = rangePos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, maxRangeOffset(n), DOWNSTREAM) != vPos)
            return false;
        if (n->rootEditableElement() == NULL)
            return false;
        if (isSpecialElement(n))
            return true;
    }

    return false;
}

Position positionAfterNode(const NodeImpl *node)
{
    return Position(node->parentNode(), node->nodeIndex() + 1);
}

Position positionAfterContainingSpecialElement(const Position& pos)
{
    ASSERT(isLastVisiblePositionInSpecialElement(pos));

    // make sure to get a range-compliant version of the position
    // FIXME: rangePos isn't being used to create DOM Ranges, so why does it need to be range compliant?
    Position rangePos = VisiblePosition(pos, DOWNSTREAM).deepEquivalent().equivalentRangeCompliantPosition();

    VisiblePosition vPos = VisiblePosition(rangePos, DOWNSTREAM);

    NodeImpl *outermostSpecialElement = NULL;

    for (NodeImpl *n = rangePos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, maxRangeOffset(n), DOWNSTREAM) != vPos)
            break;
        if (n->rootEditableElement() == NULL)
            break;
        if (isSpecialElement(n))
            outermostSpecialElement = n;
    }
    
    ASSERT(outermostSpecialElement);

    Position result = positionAfterNode(outermostSpecialElement);
    if (result.isNull() || !result.node()->rootEditableElement())
        return pos;
    
    return result;
}

Position positionOutsideContainingSpecialElement(const Position &pos)
{
    if (isFirstVisiblePositionInSpecialElement(pos)) {
        return positionBeforeContainingSpecialElement(pos);
    } else if (isLastVisiblePositionInSpecialElement(pos)) {
        return positionAfterContainingSpecialElement(pos);
    }

    return pos;
}

ElementImpl *createDefaultParagraphElement(DocumentImpl *document)
{
    // We would need this margin-zeroing code back if we ever return to using <p> elements for default paragraphs.
    // static const DOMString defaultParagraphStyle("margin-top: 0; margin-bottom: 0");    
    int exceptionCode = 0;
    ElementImpl *element = document->createElementNS(xhtmlNamespaceURI, "div", exceptionCode);
    ASSERT(exceptionCode == 0);
    return element;
}

ElementImpl *createBreakElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *breakNode = document->createElementNS(xhtmlNamespaceURI, "br", exceptionCode);
    ASSERT(exceptionCode == 0);
    return breakNode;
}

bool isTabSpanNode(const NodeImpl *node)
{
    return (node && node->isElementNode() && static_cast<const ElementImpl *>(node)->getAttribute("class") == AppleTabSpanClass);
}

bool isTabSpanTextNode(const NodeImpl *node)
{
    return (node && node->parentNode() && isTabSpanNode(node->parentNode()));
}

NodeImpl *tabSpanNode(const NodeImpl *node)
{
    return isTabSpanTextNode(node) ? node->parentNode() : 0;
}

Position positionBeforeTabSpan(const Position& pos)
{
    NodeImpl *node = pos.node();
    if (isTabSpanTextNode(node))
        node = tabSpanNode(node);
    else if (!isTabSpanNode(node))
        return pos;
    
    return Position(node->parentNode(), node->nodeIndex());
}

ElementImpl *createTabSpanElement(DocumentImpl *document, NodeImpl *tabTextNode)
{
    // make the span to hold the tab
    int exceptionCode = 0;
    ElementImpl *spanElement = document->createElementNS(xhtmlNamespaceURI, "span", exceptionCode);
    assert(exceptionCode == 0);
    spanElement->setAttribute(classAttr, AppleTabSpanClass);
    spanElement->setAttribute(styleAttr, "white-space:pre");

    // add tab text to that span
    if (!tabTextNode)
        tabTextNode = document->createEditingTextNode("\t");
    spanElement->appendChild(tabTextNode, exceptionCode);
    assert(exceptionCode == 0);

    return spanElement;
}

ElementImpl *createTabSpanElement(DocumentImpl *document, QString *tabText)
{
    return createTabSpanElement(document, document->createTextNode(*tabText));
}

bool isNodeRendered(const NodeImpl *node)
{
    if (!node)
        return false;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return false;

    return renderer->style()->visibility() == VISIBLE;
}

NodeImpl *nearestMailBlockquote(const NodeImpl *node)
{
    for (NodeImpl *n = const_cast<NodeImpl *>(node); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            return n;
    }
    return 0;
}

bool isMailBlockquote(const NodeImpl *node)
{
    if (!node || !node->renderer() || !node->isElementNode() && !node->hasTagName(blockquoteTag))
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("type") == "cite";
}

} // namespace khtml
