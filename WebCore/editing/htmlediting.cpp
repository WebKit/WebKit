/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "Document.h"
#include "EditingText.h"
#include "HTMLElement.h"
#include "Text.h"
#include "VisiblePosition.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "RenderObject.h"
#include "RegularExpression.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Atomic means that the node has no children, or has children which are ignored for the
// purposes of editing.
bool isAtomicNode(const Node *node)
{
    return node && (!node->hasChildNodes() || editingIgnoresContent(node));
}

bool editingIgnoresContent(const Node *node)
{
    if (!node || !node->isHTMLElement())
        return false;
    
    if (node->renderer()) {
        if (node->renderer()->isWidget() || node->renderer()->isImage())
            return true;
    } else {
        // widgets
        if (static_cast<const HTMLElement *>(node)->isGenericFormElement())
            return true;
        if (node->hasTagName(appletTag))
            return true;
        if (node->hasTagName(embedTag))
            return true;
        if (node->hasTagName(iframeTag))
            return true;

        // images
        if (node->hasTagName(imgTag))
            return true;
    }
    
    return false;
}

bool canHaveChildrenForEditing(const Node* node)
{
    return !node->hasTagName(hrTag) &&
           !node->hasTagName(brTag) &&
           !node->hasTagName(imgTag) &&
           !node->hasTagName(buttonTag) &&
           !node->isTextNode();
}

// antidote for maxDeepOffset()
Position rangeCompliantEquivalent(const Position& pos)
{
    if (pos.isNull())
        return Position();

    Node *node = pos.node();
    
    if (pos.offset() <= 0) {
        // FIXME: createMarkup has a problem with BR 0 as the starting position
        // so workaround until I can come back and fix createMarkup.  The problem
        // is that createMarkup fails to include the initial BR in the markup.
        if (node->parentNode() && node->hasTagName(brTag))
            return positionBeforeNode(node);
        return Position(node, 0);
    }
    
    if (node->offsetInCharacters())
        return Position(node, min(node->maxOffset(), pos.offset()));
    
    int maxCompliantOffset = node->childNodeCount();
    if (pos.offset() > maxCompliantOffset) {
        if (node->parentNode())
            return positionAfterNode(node);
        
        // there is no other option at this point than to
        // use the highest allowed position in the node
        return Position(node, maxCompliantOffset);
    } 
    
    // "select" nodes, e.g., are ignored by editing but can have children.
    // For us, a range inside of that node is tough to deal with, so use
    // a more generic position.
    if ((pos.offset() < maxCompliantOffset) && editingIgnoresContent(node)) {
        // ... but we should not have generated any such positions
        ASSERT_NOT_REACHED();
        return node->parentNode() ? positionBeforeNode(node) : Position(node, 0);
    }
    
    return Position(pos);
}

Position rangeCompliantEquivalent(const VisiblePosition& vpos)
{
    return rangeCompliantEquivalent(vpos.deepEquivalent());
}

// This method is used to create positions in the DOM. It returns the maximum valid offset
// in a node.  It returns 1 for some elements even though they do not have children, which
// creates technically invalid DOM Positions.  Be sure to call rangeCompliantEquivalent
// on a Position before using it to create a DOM Range, or an exception will be thrown.
int maxDeepOffset(const Node *node)
{
    if (node->offsetInCharacters())
        return node->maxOffset();
        
    if (node->hasChildNodes())
        return node->childNodeCount();
    
    // NOTE: This should preempt the childNodeCount for, e.g., select nodes
    if (node->hasTagName(brTag) || editingIgnoresContent(node))
        return 1;

    return 0;
}

void rebalanceWhitespaceInTextNode(Node *node, unsigned int start, unsigned int length)
{
    static RegularExpression nonRegularWhitespace("[\xa0\n]");
    static DeprecatedString twoSpaces("  ");
    static DeprecatedString nbsp("\xa0");
    static DeprecatedString space(" ");
     
    ASSERT(node->isTextNode());
    Text *textNode = static_cast<Text *>(node);
    String text = textNode->data();
    ASSERT(length <= text.length() && start + length <= text.length());
    
    DeprecatedString substring = text.substring(start, length).deprecatedString();

    substring.replace(nonRegularWhitespace, space);
    
    // The sequence should alternate between spaces and nbsps, always ending in a regular space.
    // Note: This pattern doesn't mimic TextEdit editing behavior on other clients that don't
    // support our -webkit-nbsp-mode: space, but it comes close.
    static DeprecatedString pattern("\xa0 ");
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
    
    ExceptionCode ec = 0;
    textNode->deleteData(start, length, ec);
    ASSERT(!ec);
    textNode->insertData(start, String(substring), ec);
    ASSERT(!ec);
}

bool isTableStructureNode(const Node *node)
{
    RenderObject *r = node->renderer();
    return (r && (r->isTableCell() || r->isTableRow() || r->isTableSection() || r->isTableCol()));
}

const String& nonBreakingSpaceString()
{
    static String nonBreakingSpaceString = DeprecatedString(QChar(NON_BREAKING_SPACE));
    return nonBreakingSpaceString;
}

// FIXME: Why use this instead of maxDeepOffset?
static int maxRangeOffset(Node *n)
{
    if (n->offsetInCharacters())
        return n->maxOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

// FIXME: need to dump this
bool isSpecialElement(const Node *n)
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

    if (renderer->style()->position() != StaticPosition)
        return true;
        
    return false;
}

static Node* firstInSpecialElement(const Position& pos)
{
    Node* rootEditableElement = pos.node()->rootEditableElement();
    for (Node* n = pos.node(); n && n->rootEditableElement() == rootEditableElement; n = n->parentNode())
        if (isSpecialElement(n)) {
            VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
            VisiblePosition firstInElement = VisiblePosition(n, 0, DOWNSTREAM);
            if (isTableElement(n) && vPos == firstInElement.next())
                return n;
            if (vPos == firstInElement)
                return n;
        }
    return 0;
}

static Node* lastInSpecialElement(const Position& pos)
{
    Node* rootEditableElement = pos.node()->rootEditableElement();
    for (Node* n = pos.node(); n && n->rootEditableElement() == rootEditableElement; n = n->parentNode())
        if (isSpecialElement(n)) {
            VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
            VisiblePosition lastInElement = VisiblePosition(n, n->childNodeCount(), DOWNSTREAM);
            if (isTableElement(n) && vPos == lastInElement.previous())
                return n;
            if (vPos == lastInElement)
                return n;
        }
    return 0;
}

bool isFirstVisiblePositionInSpecialElement(const Position& pos)
{
    return firstInSpecialElement(pos);
}

Position positionBeforeContainingSpecialElement(const Position& pos, Node** containingSpecialElement)
{
    Node* n = firstInSpecialElement(pos);
    ASSERT(n);
    if (!n)
        return pos;
    Position result = positionBeforeNode(n);
    if (result.isNull() || result.node()->rootEditableElement() != pos.node()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

bool isLastVisiblePositionInSpecialElement(const Position& pos)
{
    return lastInSpecialElement(pos);
}

Position positionAfterContainingSpecialElement(const Position& pos, Node **containingSpecialElement)
{
    Node* n = lastInSpecialElement(pos);
    ASSERT(n);
    if (!n)
        return pos;
    Position result = positionAfterNode(n);
    if (result.isNull() || result.node()->rootEditableElement() != pos.node()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

Position positionOutsideContainingSpecialElement(const Position &pos, Node **containingSpecialElement)
{
    if (isFirstVisiblePositionInSpecialElement(pos))
        return positionBeforeContainingSpecialElement(pos, containingSpecialElement);
    if (isLastVisiblePositionInSpecialElement(pos))
        return positionAfterContainingSpecialElement(pos, containingSpecialElement);
    return pos;
}

Position positionBeforeNode(const Node *node)
{
    return Position(node->parentNode(), node->nodeIndex());
}

Position positionAfterNode(const Node *node)
{
    return Position(node->parentNode(), node->nodeIndex() + 1);
}

bool isListElement(Node *n)
{
    return (n && (n->hasTagName(ulTag) || n->hasTagName(olTag) || n->hasTagName(dlTag)));
}

Node* enclosingTableCell(Node* node)
{
    if (!node)
        return 0;
        
    for (Node* n = node->parentNode(); n; n = n->parentNode())
        if (n->renderer() && n->renderer()->isTableCell())
            return n;
            
    return 0;
}

Node* enclosingList(Node* node)
{
    if (!node)
        return 0;
        
    for (Node* n = node->parentNode(); n; n = n->parentNode())
        if (n->hasTagName(ulTag) || n->hasTagName(olTag))
            return n;
            
    return 0;
}

Node *enclosingListChild (Node *node)
{
    // check not just for li elements per se, but also
    // for any node whose parent is a list element
    for (Node *n = node; n && n->parentNode(); n = n->parentNode()) {
        if (isListElement(n->parentNode()))
            return n;
    }
    
    return 0;
}

// FIXME: do not require renderer, so that this can be used within fragments, or rename to isRenderedTable()
bool isTableElement(Node *n)
{
    RenderObject *renderer = n ? n->renderer() : 0;
    return (renderer && (renderer->style()->display() == TABLE || renderer->style()->display() == INLINE_TABLE));
}

bool isFirstVisiblePositionAfterTableElement(const Position& pos)
{
    return isTableElement(pos.upstream().node());
}

Position positionBeforePrecedingTableElement(const Position& pos)
{
    ASSERT(isFirstVisiblePositionAfterTableElement(pos));
    Position result = positionBeforeNode(pos.upstream().node());
    if (result.isNull() || !result.node()->rootEditableElement())
        return pos;
    return result;
}

bool isLastVisiblePositionBeforeTableElement(const Position &pos)
{
    return isTableElement(pos.downstream().node());
}

Position positionAfterFollowingTableElement(const Position &pos)
{
    ASSERT(isLastVisiblePositionBeforeTableElement(pos));
    Position result = positionAfterNode(pos.downstream().node());
    if (result.isNull() || !result.node()->rootEditableElement())
        return pos;
    return result;
}

// This function is necessary because a VisiblePosition is allowed
// to be at the start or end of elements where we do not want to
// add content directly.  For example, clicking at the end of a hyperlink,
// then typing, needs to add the text after the link.  Also, table
// offset 0 and table offset childNodeCount are valid VisiblePostions,
// but we can not add more content right there... it needs to go before
// or after the table.
// FIXME: Consider editable/non-editable boundaries?
Position positionAvoidingSpecialElementBoundary(const Position &pos)
{
    Node *compNode = pos.node();
    if (!compNode)
        return pos;
    
    if (compNode->parentNode() && compNode->parentNode()->isLink())
        compNode = compNode->parentNode();
    else if (!isTableElement(compNode))
        return pos;
    
    // FIXME: rangePos isn't being used to create DOM Ranges, so why does it need to be range compliant?
    Position rangePos = rangeCompliantEquivalent(VisiblePosition(pos, DOWNSTREAM));
    VisiblePosition vPos = VisiblePosition(rangePos, DOWNSTREAM);

    Position result;
    if (VisiblePosition(compNode, maxRangeOffset(compNode), DOWNSTREAM) == vPos)
        result = positionAfterNode(compNode);
    else if (VisiblePosition(compNode, 0, DOWNSTREAM) == vPos)
        result = positionBeforeNode(compNode);
    else
        return pos;
        
    if (result.isNull() || !result.node()->rootEditableElement())
        result = pos;
    
    return result;
}

PassRefPtr<Element> createDefaultParagraphElement(Document *document)
{
    ExceptionCode ec = 0;
    RefPtr<Element> element = document->createElementNS(xhtmlNamespaceURI, "div", ec);
    ASSERT(ec == 0);
    return element.release();
}

PassRefPtr<Element> createBreakElement(Document *document)
{
    ExceptionCode ec = 0;
    RefPtr<Element> breakNode = document->createElementNS(xhtmlNamespaceURI, "br", ec);
    ASSERT(ec == 0);
    return breakNode.release();
}

PassRefPtr<Element> createOrderedListElement(Document *document)
{
    ExceptionCode ec = 0;
    RefPtr<Element> element = document->createElementNS(xhtmlNamespaceURI, "ol", ec);
    ASSERT(ec == 0);
    return element.release();
}

PassRefPtr<Element> createUnorderedListElement(Document *document)
{
    ExceptionCode ec = 0;
    RefPtr<Element> element = document->createElementNS(xhtmlNamespaceURI, "ul", ec);
    ASSERT(ec == 0);
    return element.release();
}

bool isTabSpanNode(const Node *node)
{
    return (node && node->isElementNode() && static_cast<const Element *>(node)->getAttribute("class") == AppleTabSpanClass);
}

bool isTabSpanTextNode(const Node *node)
{
    return (node && node->parentNode() && isTabSpanNode(node->parentNode()));
}

Node *tabSpanNode(const Node *node)
{
    return isTabSpanTextNode(node) ? node->parentNode() : 0;
}

Position positionBeforeTabSpan(const Position& pos)
{
    Node *node = pos.node();
    if (isTabSpanTextNode(node))
        node = tabSpanNode(node);
    else if (!isTabSpanNode(node))
        return pos;
    
    return positionBeforeNode(node);
}

PassRefPtr<Element> createTabSpanElement(Document* document, PassRefPtr<Node> tabTextNode)
{
    // make the span to hold the tab
    ExceptionCode ec = 0;
    RefPtr<Element> spanElement = document->createElementNS(xhtmlNamespaceURI, "span", ec);
    assert(ec == 0);
    spanElement->setAttribute(classAttr, AppleTabSpanClass);
    spanElement->setAttribute(styleAttr, "white-space:pre");

    // add tab text to that span
    if (!tabTextNode)
        tabTextNode = document->createEditingTextNode("\t");
    spanElement->appendChild(tabTextNode, ec);
    assert(ec == 0);

    return spanElement.release();
}

PassRefPtr<Element> createTabSpanElement(Document* document, const String& tabText)
{
    return createTabSpanElement(document, document->createTextNode(tabText));
}

PassRefPtr<Element> createTabSpanElement(Document* document)
{
    return createTabSpanElement(document, PassRefPtr<Node>());
}

bool isNodeRendered(const Node *node)
{
    if (!node)
        return false;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return false;

    return renderer->style()->visibility() == VISIBLE;
}

Node *nearestMailBlockquote(const Node *node)
{
    for (Node *n = const_cast<Node *>(node); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            return n;
    }
    return 0;
}

bool isMailBlockquote(const Node *node)
{
    if (!node || !node->isElementNode() && !node->hasTagName(blockquoteTag))
        return false;
        
    return static_cast<const Element *>(node)->getAttribute("type") == "cite";
}

} // namespace WebCore
