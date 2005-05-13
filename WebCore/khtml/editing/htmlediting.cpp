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

#include "htmlediting.h"

#include "css_computedstyle.h"
#include "css_value.h"
#include "css_valueimpl.h"
#include "cssparser.h"
#include "cssproperties.h"
#include "dom_docimpl.h"
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
#include "htmlattrs.h"
#include "htmltags.h"
#include "khtml_part.h"
#include "khtml_part.h"
#include "khtmlview.h"
#include "qcolor.h"
#include "qptrlist.h"
#include "render_object.h"
#include "render_style.h"
#include "render_text.h"
#include "visible_position.h"
#include "visible_text.h"
#include "visible_units.h"

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
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;
using DOM::TreeWalkerImpl;

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#include "KWQKHTMLPart.h"
#else
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#define ASSERT(assertion) assert(assertion)
#endif

namespace khtml {

static inline bool isNBSP(const QChar &c)
{
    return c.unicode() == 0xa0;
}

static const int spacesPerTab = 4;

bool isTableStructureNode(const NodeImpl *node)
{
    RenderObject *r = node->renderer();
    return (r && (r->isTableCell() || r->isTableRow() || r->isTableSection() || r->isTableCol()));
}

static DOMString &nonBreakingSpaceString()
{
    static DOMString nonBreakingSpaceString = QString(QChar(0xa0));
    return nonBreakingSpaceString;
}

static DOMString &matchNearestBlockquoteColorString()
{
    static DOMString matchNearestBlockquoteColorString = "match";
    return matchNearestBlockquoteColorString;
}

static void derefNodesInList(QPtrList<NodeImpl> &list)
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

static bool isSpecialElement(NodeImpl *n)
{
    if (!n->isHTMLElement())
        return false;

    if (n->id() == ID_A && n->isLink())
        return true;

    if (n->id() == ID_UL || n->id() == ID_OL || n->id() == ID_DL)
        return true;

    RenderObject *renderer = n->renderer();

    if (renderer && (renderer->style()->display() == TABLE || renderer->style()->display() == INLINE_TABLE))
        return true;

    if (renderer && renderer->style()->isFloating())
        return true;

    if (renderer && renderer->style()->position() != STATIC)
        return true;

    return false;
}

// This version of the function is meant to be called on positions in a document fragment,
// so it does not check for a root editable element, it is assumed these nodes will be put
// somewhere editable in the future
bool isFirstVisiblePositionInSpecialElementInFragment(const Position& pos)
{
    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            return false;
        if (isSpecialElement(n))
            return true;
    }

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

static Position positionBeforeNode(NodeImpl *node)
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
    Position rangePos = VisiblePosition(pos, DOWNSTREAM).position();

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

static Position positionAfterNode(NodeImpl *node)
{
    return Position(node->parentNode(), node->nodeIndex() + 1);
}

Position positionAfterContainingSpecialElement(const Position& pos)
{
    ASSERT(isLastVisiblePositionInSpecialElement(pos));

    // make sure to get a range-compliant version of the position
    Position rangePos = VisiblePosition(pos, DOWNSTREAM).position();

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

//------------------------------------------------------------------------------------------
// InsertLineBreakCommand

InsertLineBreakCommand::InsertLineBreakCommand(DocumentImpl *document) 
    : CompositeEditCommand(document)
{
}

bool InsertLineBreakCommand::preservesTypingStyle() const
{
    return true;
}

void InsertLineBreakCommand::insertNodeAfterPosition(NodeImpl *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *after* the block.
    Position upstream(pos.upstream());
    NodeImpl *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(node, cb);
    else
        insertNodeAfter(node, pos.node());
}

void InsertLineBreakCommand::insertNodeBeforePosition(NodeImpl *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *before* the block.
    Position upstream(pos.upstream());
    NodeImpl *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(node, cb);
    else
        insertNodeBefore(node, pos.node());
}

void InsertLineBreakCommand::doApply()
{
    deleteSelection();
    Selection selection = endingSelection();

    ElementImpl *breakNode = createBreakElement(document());
    NodeImpl *nodeToInsert = breakNode;
    
    Position pos(selection.start().upstream());

    pos = positionOutsideContainingSpecialElement(pos);

    bool atStart = pos.offset() <= pos.node()->caretMinOffset();
    bool atEnd = pos.offset() >= pos.node()->caretMaxOffset();
    bool atEndOfBlock = isEndOfBlock(VisiblePosition(pos, selection.startAffinity()));
    
    if (atEndOfBlock) {
        LOG(Editing, "input newline case 1");
        // Check for a trailing BR. If there isn't one, we'll need to insert an "extra" one.
        // This makes the "real" BR we want to insert appear in the rendering without any 
        // significant side effects (and no real worries either since you can't arrow past 
        // this extra one.
        if (pos.node()->id() == ID_BR && pos.offset() == 0) {
            // Already placed in a trailing BR. Insert "real" BR before it and leave the selection alone.
            insertNodeBefore(nodeToInsert, pos.node());
        }
        else {
            NodeImpl *next = pos.node()->traverseNextNode();
            bool hasTrailingBR = next && next->id() == ID_BR && pos.node()->enclosingBlockFlowElement() == next->enclosingBlockFlowElement();
            insertNodeAfterPosition(nodeToInsert, pos);
            if (hasTrailingBR) {
                setEndingSelection(Selection(Position(next, 0), DOWNSTREAM));
            }
            else if (!document()->inStrictMode()) {
                // Insert an "extra" BR at the end of the block. 
                ElementImpl *extraBreakNode = createBreakElement(document());
                insertNodeAfter(extraBreakNode, nodeToInsert);
                setEndingSelection(Position(extraBreakNode, 0), DOWNSTREAM);
            }
        }
    }
    else if (atStart) {
        LOG(Editing, "input newline case 2");
        // Insert node before downstream position, and place caret there as well. 
        Position endingPosition = pos.downstream();
        insertNodeBeforePosition(nodeToInsert, endingPosition);
        setEndingSelection(endingPosition, DOWNSTREAM);
    }
    else if (atEnd) {
        LOG(Editing, "input newline case 3");
        // Insert BR after this node. Place caret in the position that is downstream
        // of the current position, reckoned before inserting the BR in between.
        Position endingPosition = pos.downstream();
        insertNodeAfterPosition(nodeToInsert, pos);
        setEndingSelection(endingPosition, DOWNSTREAM);
    }
    else {
        // Split a text node
        LOG(Editing, "input newline case 4");
        ASSERT(pos.node()->isTextNode());
        
        // Do the split
        int exceptionCode = 0;
        TextImpl *textNode = static_cast<TextImpl *>(pos.node());
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.start().offset(), exceptionCode));
        deleteTextFromNode(textNode, 0, pos.offset());
        insertNodeBefore(textBeforeNode, textNode);
        insertNodeBefore(nodeToInsert, textNode);
        Position endingPosition = Position(textNode, 0);
        
        // Handle whitespace that occurs after the split
        document()->updateLayout();
        if (!endingPosition.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            deleteInsignificantTextDownstream(endingPosition);
            insertTextIntoNode(textNode, 0, nonBreakingSpaceString());
        }
        
        setEndingSelection(endingPosition, DOWNSTREAM);
    }

    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    
    if (typingStyle && typingStyle->length() > 0) {
        Selection selectionBeforeStyle = endingSelection();

        DOM::RangeImpl *rangeAroundNode = document()->createRange();
        int exception;
        rangeAroundNode->selectNode(nodeToInsert, exception);

        // affinity is not really important since this is a temp selection
        // just for calling applyStyle
        setEndingSelection(Selection(rangeAroundNode, khtml::SEL_DEFAULT_AFFINITY, khtml::SEL_DEFAULT_AFFINITY));
        applyStyle(typingStyle);

        setEndingSelection(selectionBeforeStyle);
    }

    rebalanceWhitespace();
}

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorCommand

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(DocumentImpl *document) 
    : CompositeEditCommand(document), m_style(0)
{
}

InsertParagraphSeparatorCommand::~InsertParagraphSeparatorCommand() 
{
    derefNodesInList(clonedNodes);
    if (m_style)
        m_style->deref();
}

bool InsertParagraphSeparatorCommand::preservesTypingStyle() const
{
    return true;
}

ElementImpl *InsertParagraphSeparatorCommand::createParagraphElement()
{
    ElementImpl *element = createDefaultParagraphElement(document());
    element->ref();
    clonedNodes.append(element);
    return element;
}

void InsertParagraphSeparatorCommand::calculateStyleBeforeInsertion(const Position &pos)
{
    // It is only important to set a style to apply later if we're at the boundaries of
    // a paragraph. Otherwise, content that is moved as part of the work of the command
    // will lend their styles to the new paragraph without any extra work needed.
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    if (!isStartOfParagraph(visiblePos) && !isEndOfParagraph(visiblePos))
        return;
    
    if (m_style)
        m_style->deref();
    m_style = styleAtPosition(pos);
    m_style->ref();
}

void InsertParagraphSeparatorCommand::applyStyleAfterInsertion()
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!m_style)
        return;

    CSSComputedStyleDeclarationImpl endingStyle(endingSelection().start().node());
    endingStyle.diff(m_style);
    if (m_style->length() > 0) {
        applyStyle(m_style);
    }
}

void InsertParagraphSeparatorCommand::doApply()
{
    bool splitText = false;
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    Position pos = selection.start();
    EAffinity affinity = selection.startAffinity();
        
    // Delete the current selection.
    if (selection.isRange()) {
        calculateStyleBeforeInsertion(pos);
        deleteSelection(false, false);
        pos = endingSelection().start();
        affinity = endingSelection().startAffinity();
    }

    pos = positionOutsideContainingSpecialElement(pos);

    calculateStyleBeforeInsertion(pos);

    // Find the start block.
    NodeImpl *startNode = pos.node();
    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    if (!startBlock || !startBlock->parentNode())
        return;

    VisiblePosition visiblePos(pos, affinity);
    bool isFirstInBlock = isStartOfBlock(visiblePos);
    bool isLastInBlock = isEndOfBlock(visiblePos);
    bool startBlockIsRoot = startBlock == startBlock->rootEditableElement();

    // This is the block that is going to be inserted.
    NodeImpl *blockToInsert = startBlockIsRoot ? createParagraphElement() : startBlock->cloneNode(false);

    //---------------------------------------------------------------------
    // Handle empty block case.
    if (isFirstInBlock && isLastInBlock) {
        LOG(Editing, "insert paragraph separator: empty block case");
        if (startBlockIsRoot) {
            NodeImpl *extraBlock = createParagraphElement();
            appendNode(extraBlock, startBlock);
            appendBlockPlaceholder(extraBlock);
            appendNode(blockToInsert, startBlock);
        }
        else {
            insertNodeAfter(blockToInsert, startBlock);
        }
        appendBlockPlaceholder(blockToInsert);
        setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block. 
    if (isLastInBlock) {
        LOG(Editing, "insert paragraph separator: last in block case");
        if (startBlockIsRoot)
            appendNode(blockToInsert, startBlock);
        else
            insertNodeAfter(blockToInsert, startBlock);
        appendBlockPlaceholder(blockToInsert);
        setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block.
    // and similar case where upstream position is in another block.
    bool prevInDifferentBlock = !inSameBlock(visiblePos, visiblePos.previous());

    if (prevInDifferentBlock || isFirstInBlock) {
        LOG(Editing, "insert paragraph separator: first in block case");
        pos = pos.downstream();
        pos = positionOutsideContainingSpecialElement(pos);
        Position refPos;
        NodeImpl *refNode;
        if (isFirstInBlock && !startBlockIsRoot) {
            refNode = startBlock;
        } else if (pos.node() == startBlock && startBlockIsRoot) {
            ASSERT(startBlock->childNode(pos.offset())); // must be true or we'd be in the end of block case
            refNode = startBlock->childNode(pos.offset());
        } else {
            refNode = pos.node();
        }

        insertNodeBefore(blockToInsert, refNode);
        appendBlockPlaceholder(blockToInsert);
        setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        setEndingSelection(pos, DOWNSTREAM);
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    LOG(Editing, "insert paragraph separator: general case");

    // Check if pos.node() is a <br>. If it is, and the document is in quirks mode, 
    // then this <br> will collapse away when we add a block after it. Add an extra <br>.
    if (!document()->inStrictMode()) {
        Position upstreamPos = pos.upstream();
        if (upstreamPos.node()->id() == ID_BR)
            insertNodeAfter(createBreakElement(document()), upstreamPos.node());
    }
    
    // Move downstream. Typing style code will take care of carrying along the 
    // style of the upstream position.
    pos = pos.downstream();
    startNode = pos.node();

    // Build up list of ancestors in between the start node and the start block.
    if (startNode != startBlock) {
        for (NodeImpl *n = startNode->parentNode(); n && n != startBlock; n = n->parentNode())
            ancestors.prepend(n);
    }

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but pos.downstream() does not give it
    Position leadingWhitespace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY);
    if (leadingWhitespace.isNotNull()) {
        TextImpl *textNode = static_cast<TextImpl *>(leadingWhitespace.node());
        replaceTextInNode(textNode, leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    
    // Split at pos if in the middle of a text node.
    if (startNode->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(startNode);
        bool atEnd = (unsigned long)pos.offset() >= textNode->length();
        if (pos.offset() > 0 && !atEnd) {
            splitTextNode(textNode, pos.offset());
            pos = Position(startNode, 0);
            splitText = true;
        }
    }

    // Put the added block in the tree.
    if (startBlockIsRoot) {
        appendNode(blockToInsert, startBlock);
    } else {
        insertNodeAfter(blockToInsert, startBlock);
    }

    // Make clones of ancestors in between the start node and the start block.
    NodeImpl *parent = blockToInsert;
    for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
        NodeImpl *child = it.current()->cloneNode(false); // shallow clone
        child->ref();
        clonedNodes.append(child);
        appendNode(child, parent);
        parent = child;
    }

    // Insert a block placeholder if the next visible position is in a different paragraph,
    // because we know that there will be no content on the first line of the new block 
    // before the first block child. So, we need the placeholder to "hold the first line open".
    VisiblePosition next = visiblePos.next();
    if (!next.isNull() && !inSameBlock(visiblePos, next))
        appendBlockPlaceholder(blockToInsert);

    // Move the start node and the siblings of the start node.
    if (startNode != startBlock) {
        NodeImpl *n = startNode;
        if (pos.offset() >= startNode->caretMaxOffset()) {
            n = startNode->nextSibling();
        }
        while (n && n != blockToInsert) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent);
            n = next;
        }
    }            

    // Move everything after the start node.
    NodeImpl *leftParent = ancestors.last();
    while (leftParent && leftParent != startBlock) {
        parent = parent->parentNode();
        NodeImpl *n = leftParent->nextSibling();
        while (n && n != blockToInsert) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent);
            n = next;
        }
        leftParent = leftParent->parentNode();
    }

    // Handle whitespace that occurs after the split
    if (splitText) {
        document()->updateLayout();
        pos = Position(startNode, 0);
        if (!pos.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            ASSERT(startNode && startNode->isTextNode());
            deleteInsignificantTextDownstream(pos);
            insertTextIntoNode(static_cast<TextImpl *>(startNode), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
    rebalanceWhitespace();
    applyStyleAfterInsertion();
}

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorInQuotedContentCommand

InsertParagraphSeparatorInQuotedContentCommand::InsertParagraphSeparatorInQuotedContentCommand(DocumentImpl *document)
    : CompositeEditCommand(document), m_breakNode(0)
{
}

InsertParagraphSeparatorInQuotedContentCommand::~InsertParagraphSeparatorInQuotedContentCommand()
{
    derefNodesInList(clonedNodes);
    if (m_breakNode)
        m_breakNode->deref();
}

void InsertParagraphSeparatorInQuotedContentCommand::doApply()
{
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    // Delete the current selection.
    Position pos = selection.start();
    EAffinity affinity = selection.startAffinity();
    if (selection.isRange()) {
        deleteSelection(false, false);
        pos = endingSelection().start().upstream();
        affinity = endingSelection().startAffinity();
    }
    
    // Find the top-most blockquote from the start.
    NodeImpl *startNode = pos.node();
    NodeImpl *topBlockquote = 0;
    for (NodeImpl *n = startNode->parentNode(); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            topBlockquote = n;
    }
    if (!topBlockquote || !topBlockquote->parentNode())
        return;
    
    // Insert a break after the top blockquote.
    m_breakNode = createBreakElement(document());
    m_breakNode->ref();
    insertNodeAfter(m_breakNode, topBlockquote);
    
    if (!isLastVisiblePositionInNode(VisiblePosition(pos, affinity), topBlockquote)) {
        
        NodeImpl *newStartNode = 0;
        // Split at pos if in the middle of a text node.
        if (startNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(startNode);
            bool atEnd = (unsigned long)pos.offset() >= textNode->length();
            if (pos.offset() > 0 && !atEnd) {
                splitTextNode(textNode, pos.offset());
                pos = Position(startNode, 0);
            }
            else if (atEnd) {
                newStartNode = startNode->traverseNextNode();
                ASSERT(newStartNode);
            }
        }
        else if (pos.offset() > 0) {
            newStartNode = startNode->traverseNextNode();
            ASSERT(newStartNode);
        }
        
        // If a new start node was determined, find a new top block quote.
        if (newStartNode) {
            startNode = newStartNode;
            for (NodeImpl *n = startNode->parentNode(); n; n = n->parentNode()) {
                if (isMailBlockquote(n))
                    topBlockquote = n;
            }
            if (!topBlockquote || !topBlockquote->parentNode())
                return;
        }
        
        // Build up list of ancestors in between the start node and the top blockquote.
        if (startNode != topBlockquote) {
            for (NodeImpl *n = startNode->parentNode(); n && n != topBlockquote; n = n->parentNode())
                ancestors.prepend(n);
        }                    
        
        // Insert a clone of the top blockquote after the break.
        NodeImpl *clonedBlockquote = topBlockquote->cloneNode(false);
        clonedBlockquote->ref();
        clonedNodes.append(clonedBlockquote);
        insertNodeAfter(clonedBlockquote, m_breakNode);
        
        // Make clones of ancestors in between the start node and the top blockquote.
        NodeImpl *parent = clonedBlockquote;
        for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
            NodeImpl *child = it.current()->cloneNode(false); // shallow clone
            child->ref();
            clonedNodes.append(child);
            appendNode(child, parent);
            parent = child;
        }
        
        // Move the start node and the siblings of the start node.
        bool startIsBR = false;
        if (startNode != topBlockquote) {
            NodeImpl *n = startNode;
            startIsBR = n->id() == ID_BR;
            if (startIsBR)
                n = n->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
        }
        
        // Move everything after the start node.
        NodeImpl *leftParent = ancestors.last();
        
        // Insert an extra new line when the start is at the beginning of a line.
        if (!newStartNode && !startIsBR) {
            if (!leftParent)
                leftParent = topBlockquote;
            ElementImpl *b = createBreakElement(document());
            b->ref();
            clonedNodes.append(b);
            appendNode(b, leftParent);
        }        
        
        leftParent = ancestors.last();
        while (leftParent && leftParent != topBlockquote) {
            parent = parent->parentNode();
            NodeImpl *n = leftParent->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
            leftParent = leftParent->parentNode();
        }
        
        // Make sure the cloned block quote renders.
        addBlockPlaceholderIfNeeded(clonedBlockquote);
    }
    
    // Put the selection right before the break.
    setEndingSelection(Position(m_breakNode, 0), DOWNSTREAM);
    rebalanceWhitespace();
}

//------------------------------------------------------------------------------------------
// InsertTextCommand

InsertTextCommand::InsertTextCommand(DocumentImpl *document) 
    : CompositeEditCommand(document), m_charactersAdded(0)
{
}

void InsertTextCommand::doApply()
{
}

Position InsertTextCommand::prepareForTextInsertion(bool adjustDownstream)
{
    // Prepare for text input by looking at the current position.
    // It may be necessary to insert a text node to receive characters.
    Selection selection = endingSelection();
    ASSERT(selection.isCaret());
    
    Position pos = selection.start();
    if (adjustDownstream)
        pos = pos.downstream();
    else
        pos = pos.upstream();
    
    Selection typingStyleRange;

    pos = positionOutsideContainingSpecialElement(pos);

    if (!pos.node()->isTextNode()) {
        NodeImpl *textNode = document()->createEditingTextNode("");
        NodeImpl *nodeToInsert = textNode;

        // Now insert the node in the right place
        if (pos.node()->rootEditableElement() != NULL) {
            LOG(Editing, "prepareForTextInsertion case 1");
            insertNodeAt(nodeToInsert, pos.node(), pos.offset());
        }
        else if (pos.node()->caretMinOffset() == pos.offset()) {
            LOG(Editing, "prepareForTextInsertion case 2");
            insertNodeBefore(nodeToInsert, pos.node());
        }
        else if (pos.node()->caretMaxOffset() == pos.offset()) {
            LOG(Editing, "prepareForTextInsertion case 3");
            insertNodeAfter(nodeToInsert, pos.node());
        }
        else
            ASSERT_NOT_REACHED();
        
        pos = Position(textNode, 0);
    }

    return pos;
}

void InsertTextCommand::input(const DOMString &text, bool selectInsertedText)
{
    assert(text.find('\n') == -1);

    Selection selection = endingSelection();
    bool adjustDownstream = isStartOfLine(VisiblePosition(selection.start().downstream(), DOWNSTREAM));

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.isRange())
        deleteSelection();
    
    // Delete any insignificant text that could get in the way of whitespace turning
    // out correctly after the insertion.
    selection = endingSelection();
    deleteInsignificantTextDownstream(selection.end().trailingWhitespacePosition(selection.endAffinity()));
    
    // Make sure the document is set up to receive text
    Position startPosition = prepareForTextInsertion(adjustDownstream);
    
    Position endPosition;

    TextImpl *textNode = static_cast<TextImpl *>(startPosition.node());
    long offset = startPosition.offset();

    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    removeBlockPlaceholder(textNode->enclosingBlockFlowElement());
    
    // These are temporary implementations for inserting adjoining spaces
    // into a document. We are working on a CSS-related whitespace solution
    // that will replace this some day. We hope.
    if (text == "\t") {
        // Treat a tab like a number of spaces. This seems to be the HTML editing convention,
        // although the number of spaces varies (we choose four spaces). 
        // Note that there is no attempt to make this work like a real tab stop, it is merely 
        // a set number of spaces. This also seems to be the HTML editing convention.
        for (int i = 0; i < spacesPerTab; i++) {
            insertSpace(textNode, offset);
            rebalanceWhitespace();
            document()->updateLayout();
        }
        
        endPosition = Position(textNode, offset + spacesPerTab);

        m_charactersAdded += spacesPerTab;
    }
    else if (text == " ") {
        insertSpace(textNode, offset);
        endPosition = Position(textNode, offset + 1);

        m_charactersAdded++;
        rebalanceWhitespace();
    }
    else {
        const DOMString &existingText = textNode->data();
        if (textNode->length() >= 2 && offset >= 2 && isNBSP(existingText[offset - 1]) && !isCollapsibleWhitespace(existingText[offset - 2])) {
            // DOM looks like this:
            // character nbsp caret
            // As we are about to insert a non-whitespace character at the caret
            // convert the nbsp to a regular space.
            // EDIT FIXME: This needs to be improved some day to convert back only
            // those nbsp's added by the editor to make rendering come out right.
            replaceTextInNode(textNode, offset - 1, 1, " ");
        }
        insertTextIntoNode(textNode, offset, text);
        endPosition = Position(textNode, offset + text.length());

        m_charactersAdded += text.length();
    }

    setEndingSelection(Selection(startPosition, DOWNSTREAM, endPosition, SEL_DEFAULT_AFFINITY));

    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    if (typingStyle && typingStyle->length() > 0)
        applyStyle(typingStyle);

    if (!selectInsertedText)
        setEndingSelection(endingSelection().end(), endingSelection().endAffinity());
}

void InsertTextCommand::insertSpace(TextImpl *textNode, unsigned long offset)
{
    ASSERT(textNode);

    DOMString text(textNode->data());

    // count up all spaces and newlines in front of the caret
    // delete all collapsed ones
    // this will work out OK since the offset we have been passed has been upstream-ized 
    int count = 0;
    for (unsigned int i = offset; i < text.length(); i++) {
        if (isCollapsibleWhitespace(text[i]))
            count++;
        else 
            break;
    }
    if (count > 0) {
        // By checking the character at the downstream position, we can
        // check if there is a rendered WS at the caret
        Position pos(textNode, offset);
        Position downstream = pos.downstream();
        if (downstream.offset() < (long)text.length() && isCollapsibleWhitespace(text[downstream.offset()]))
            count--; // leave this WS in
        if (count > 0)
            deleteTextFromNode(textNode, offset, count);
    }

    if (offset > 0 && offset <= text.length() - 1 && !isCollapsibleWhitespace(text[offset]) && !isCollapsibleWhitespace(text[offset - 1])) {
        // insert a "regular" space
        insertTextIntoNode(textNode, offset, " ");
        return;
    }

    if (text.length() >= 2 && offset >= 2 && isNBSP(text[offset - 2]) && isNBSP(text[offset - 1])) {
        // DOM looks like this:
        // nbsp nbsp caret
        // insert a space between the two nbsps
        insertTextIntoNode(textNode, offset - 1, " ");
        return;
    }

    // insert an nbsp
    insertTextIntoNode(textNode, offset, nonBreakingSpaceString());
}

bool InsertTextCommand::isInsertTextCommand() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// JoinTextNodesCommand

JoinTextNodesCommand::JoinTextNodesCommand(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : EditCommand(document), m_text1(text1), m_text2(text2)
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);
    ASSERT(m_text1->length() > 0);
    ASSERT(m_text2->length() > 0);

    m_text1->ref();
    m_text2->ref();
}

JoinTextNodesCommand::~JoinTextNodesCommand()
{
    ASSERT(m_text1);
    m_text1->deref();
    ASSERT(m_text2);
    m_text2->deref();
}

void JoinTextNodesCommand::doApply()
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);

    int exceptionCode = 0;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->removeChild(m_text1, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_offset = m_text1->length();
}

void JoinTextNodesCommand::doUnapply()
{
    ASSERT(m_text2);
    ASSERT(m_offset > 0);

    int exceptionCode = 0;

    m_text2->deleteData(0, m_offset, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);
        
    ASSERT(m_text2->previousSibling()->isTextNode());
    ASSERT(m_text2->previousSibling() == m_text1);
}

//------------------------------------------------------------------------------------------
// MoveSelectionCommand

MoveSelectionCommand::MoveSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, Position &position, bool smartMove) 
    : CompositeEditCommand(document), m_fragment(fragment), m_position(position), m_smartMove(smartMove)
{
    ASSERT(m_fragment);
    m_fragment->ref();
}

MoveSelectionCommand::~MoveSelectionCommand()
{
    ASSERT(m_fragment);
    m_fragment->deref();
}

void MoveSelectionCommand::doApply()
{
    Selection selection = endingSelection();
    ASSERT(selection.isRange());

    Position pos = m_position;
    if (pos.isNull())
        return;
        
    // Update the position otherwise it may become invalid after the selection is deleted.
    NodeImpl *positionNode = m_position.node();
    long positionOffset = m_position.offset();
    Position selectionEnd = selection.end();
    long selectionEndOffset = selectionEnd.offset();    
    if (selectionEnd.node() == positionNode && selectionEndOffset < positionOffset) {
        positionOffset -= selectionEndOffset;
        Position selectionStart = selection.start();
        if (selectionStart.node() == positionNode) {
            positionOffset += selectionStart.offset();
        }
        pos = Position(positionNode, positionOffset);
    }

    deleteSelection(m_smartMove);

    // If the node for the destination has been removed as a result of the deletion,
    // set the destination to the ending point after the deletion.
    // Fixes: <rdar://problem/3910425> REGRESSION (Mail): Crash in ReplaceSelectionCommand; 
    //        selection is empty, leading to null deref
    if (!pos.node()->inDocument())
        pos = endingSelection().start();

    setEndingSelection(pos, endingSelection().startAffinity());
    EditCommandPtr cmd(new ReplaceSelectionCommand(document(), m_fragment, true, m_smartMove));
    applyCommandToComposite(cmd);
}

EditAction MoveSelectionCommand::editingAction() const
{
    return EditActionDrag;
}

//------------------------------------------------------------------------------------------
// RebalanceWhitespaceCommand

RebalanceWhitespaceCommand::RebalanceWhitespaceCommand(DocumentImpl *document, const Position &pos)
    : EditCommand(document), m_position(pos), m_upstreamOffset(InvalidOffset), m_downstreamOffset(InvalidOffset)
{
}

RebalanceWhitespaceCommand::~RebalanceWhitespaceCommand()
{
}

void RebalanceWhitespaceCommand::doApply()
{
    static DOMString space(" ");

    if (m_position.isNull() || !m_position.node()->isTextNode())
        return;
        
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    if (text.length() == 0)
        return;
    
    // find upstream offset
    long upstream = m_position.offset();
    while (upstream > 0 && isCollapsibleWhitespace(text[upstream - 1]) || isNBSP(text[upstream - 1])) {
        upstream--;
        m_upstreamOffset = upstream;
    }

    // find downstream offset
    long downstream = m_position.offset();
    while ((unsigned)downstream < text.length() && isCollapsibleWhitespace(text[downstream]) || isNBSP(text[downstream])) {
        downstream++;
        m_downstreamOffset = downstream;
    }

    if (m_upstreamOffset == InvalidOffset && m_downstreamOffset == InvalidOffset)
        return;
        
    m_upstreamOffset = upstream;
    m_downstreamOffset = downstream;
    long length = m_downstreamOffset - m_upstreamOffset;
    
    m_beforeString = text.substring(m_upstreamOffset, length);
    
    // The following loop figures out a "rebalanced" whitespace string for any length
    // string, and takes into account the special cases that need to handled for the
    // start and end of strings (i.e. first and last character must be an nbsp.
    long i = m_upstreamOffset;
    while (i < m_downstreamOffset) {
        long add = (m_downstreamOffset - i) % 3;
        switch (add) {
            case 0:
                m_afterString += nonBreakingSpaceString();
                m_afterString += space;
                m_afterString += nonBreakingSpaceString();
                add = 3;
                break;
            case 1:
                if (i == 0 || (unsigned)i + 1 == text.length()) // at start or end of string
                    m_afterString += nonBreakingSpaceString();
                else
                    m_afterString += space;
                break;
            case 2:
                if ((unsigned)i + 2 == text.length()) {
                     // at end of string
                    m_afterString += nonBreakingSpaceString();
                    m_afterString += nonBreakingSpaceString();
                }
                else {
                    m_afterString += nonBreakingSpaceString();
                    m_afterString += space;
                }
                break;
        }
        i += add;
    }
    
    text.remove(m_upstreamOffset, length);
    text.insert(m_afterString, m_upstreamOffset);
}

void RebalanceWhitespaceCommand::doUnapply()
{
    if (m_upstreamOffset == InvalidOffset && m_downstreamOffset == InvalidOffset)
        return;
    
    ASSERT(m_position.node()->isTextNode());
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    text.remove(m_upstreamOffset, m_afterString.length());
    text.insert(m_beforeString, m_upstreamOffset);
}

bool RebalanceWhitespaceCommand::preservesTypingStyle() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommand

RemoveCSSPropertyCommand::RemoveCSSPropertyCommand(DocumentImpl *document, CSSStyleDeclarationImpl *decl, int property)
    : EditCommand(document), m_decl(decl->makeMutable()), m_property(property), m_important(false)
{
    ASSERT(m_decl);
    m_decl->ref();
}

RemoveCSSPropertyCommand::~RemoveCSSPropertyCommand()
{
    ASSERT(m_decl);
    m_decl->deref();
}

void RemoveCSSPropertyCommand::doApply()
{
    ASSERT(m_decl);

    m_oldValue = m_decl->getPropertyValue(m_property);
    ASSERT(!m_oldValue.isNull());

    m_important = m_decl->getPropertyPriority(m_property);
    m_decl->removeProperty(m_property);
}

void RemoveCSSPropertyCommand::doUnapply()
{
    ASSERT(m_decl);
    ASSERT(!m_oldValue.isNull());

    m_decl->setProperty(m_property, m_oldValue, m_important);
}

//------------------------------------------------------------------------------------------
// RemoveNodeAttributeCommand

RemoveNodeAttributeCommand::RemoveNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute)
    : EditCommand(document), m_element(element), m_attribute(attribute)
{
    ASSERT(m_element);
    m_element->ref();
}

RemoveNodeAttributeCommand::~RemoveNodeAttributeCommand()
{
    ASSERT(m_element);
    m_element->deref();
}

void RemoveNodeAttributeCommand::doApply()
{
    ASSERT(m_element);

    m_oldValue = m_element->getAttribute(m_attribute);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->removeAttribute(m_attribute, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeAttributeCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->setAttribute(m_attribute, m_oldValue.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// RemoveNodeCommand

RemoveNodeCommand::RemoveNodeCommand(DocumentImpl *document, NodeImpl *removeChild)
    : EditCommand(document), m_parent(0), m_removeChild(removeChild), m_refChild(0)
{
    ASSERT(m_removeChild);
    m_removeChild->ref();

    m_parent = m_removeChild->parentNode();
    ASSERT(m_parent);
    m_parent->ref();
    
    m_refChild = m_removeChild->nextSibling();
    if (m_refChild)
        m_refChild->ref();
}

RemoveNodeCommand::~RemoveNodeCommand()
{
    ASSERT(m_parent);
    m_parent->deref();

    ASSERT(m_removeChild);
    m_removeChild->deref();

    if (m_refChild)
        m_refChild->deref();
}

void RemoveNodeCommand::doApply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->removeChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeCommand::doUnapply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->insertBefore(m_removeChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommand

RemoveNodePreservingChildrenCommand::RemoveNodePreservingChildrenCommand(DocumentImpl *document, NodeImpl *node)
    : CompositeEditCommand(document), m_node(node)
{
    ASSERT(m_node);
    m_node->ref();
}

RemoveNodePreservingChildrenCommand::~RemoveNodePreservingChildrenCommand()
{
    ASSERT(m_node);
    m_node->deref();
}

void RemoveNodePreservingChildrenCommand::doApply()
{
    while (NodeImpl* curr = node()->firstChild()) {
        removeNode(curr);
        insertNodeBefore(curr, node());
    }
    removeNode(node());
}

//------------------------------------------------------------------------------------------
// ReplaceSelectionCommand

ReplacementFragment::ReplacementFragment(DocumentImpl *document, DocumentFragmentImpl *fragment, bool matchStyle)
    : m_document(document), 
      m_fragment(fragment), 
      m_matchStyle(matchStyle), 
      m_hasInterchangeNewlineAtStart(false), 
      m_hasInterchangeNewlineAtEnd(false), 
      m_hasMoreThanOneBlock(false)
{
    if (!m_document)
        return;

    if (!m_fragment) {
        m_type = EmptyFragment;
        return;
    }

    m_document->ref();
    m_fragment->ref();

    NodeImpl *firstChild = m_fragment->firstChild();
    NodeImpl *lastChild = m_fragment->lastChild();

    if (!firstChild) {
        m_type = EmptyFragment;
        return;
    }

    if (firstChild == lastChild && firstChild->isTextNode()) {
        m_type = SingleTextNodeFragment;
        return;
    }
    
    m_type = TreeFragment;

    NodeImpl *node = m_fragment->firstChild();
    NodeImpl *newlineAtStartNode = 0;
    NodeImpl *newlineAtEndNode = 0;
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (isInterchangeNewlineNode(node)) {
            if (next || node == m_fragment->firstChild()) {
                m_hasInterchangeNewlineAtStart = true;
                newlineAtStartNode = node;
            }
            else {
                m_hasInterchangeNewlineAtEnd = true;
                newlineAtEndNode = node;
            }
        }
        else if (isInterchangeConvertedSpaceSpan(node)) {
            NodeImpl *n = 0;
            while ((n = node->firstChild())) {
                n->ref();
                removeNode(n);
                insertNodeBefore(n, node);
                n->deref();
            }
            removeNode(node);
            if (n)
                next = n->traverseNextNode();
        }
        node = next;
    }

    if (newlineAtStartNode)
        removeNode(newlineAtStartNode);
    if (newlineAtEndNode)
        removeNode(newlineAtEndNode);
    
    NodeImpl *holder = insertFragmentForTestRendering();
    if (holder)
        holder->ref();
    if (!m_matchStyle) {
        computeStylesUsingTestRendering(holder);
    }
    removeUnrenderedNodesUsingTestRendering(holder);
    m_hasMoreThanOneBlock = countRenderedBlocks(holder) > 1;
    restoreTestRenderingNodesToFragment(holder);
    removeNode(holder);
    holder->deref();
    removeStyleNodes();
}

ReplacementFragment::~ReplacementFragment()
{
    if (m_document)
        m_document->deref();
    if (m_fragment)
        m_fragment->deref();
}

NodeImpl *ReplacementFragment::firstChild() const 
{ 
    return m_fragment->firstChild(); 
}

NodeImpl *ReplacementFragment::lastChild() const 
{ 
    return  m_fragment->lastChild(); 
}

NodeImpl *ReplacementFragment::mergeStartNode() const
{
    NodeImpl *node = m_fragment->firstChild();
    while (node && isProbablyBlock(node) && !isMailPasteAsQuotationNode(node))
        node = node->traverseNextNode();
    return node;
}

void ReplacementFragment::pruneEmptyNodes()
{
    bool run = true;
    while (run) {
        run = false;
        NodeImpl *node = m_fragment->firstChild();
        while (node) {
            if ((node->isTextNode() && static_cast<TextImpl *>(node)->length() == 0) ||
                (isProbablyBlock(node) && !isProbablyTableStructureNode(node) && node->childNodeCount() == 0)) {
                NodeImpl *next = node->traverseNextSibling();
                removeNode(node);
                node = next;
                run = true;
            }
            else {
                node = node->traverseNextNode();
            }
         }
    }
}

bool ReplacementFragment::isInterchangeNewlineNode(const NodeImpl *node)
{
    static DOMString interchangeNewlineClassString(AppleInterchangeNewline);
    return node && node->id() == ID_BR && static_cast<const ElementImpl *>(node)->getAttribute(ATTR_CLASS) == interchangeNewlineClassString;
}

bool ReplacementFragment::isInterchangeConvertedSpaceSpan(const NodeImpl *node)
{
    static DOMString convertedSpaceSpanClassString(AppleConvertedSpace);
    return node->isHTMLElement() && static_cast<const HTMLElementImpl *>(node)->getAttribute(ATTR_CLASS) == convertedSpaceSpanClassString;
}

NodeImpl *ReplacementFragment::enclosingBlock(NodeImpl *node) const
{
    while (node && !isProbablyBlock(node))
        node = node->parentNode();    
    return node ? node : m_fragment;
}

void ReplacementFragment::removeNodePreservingChildren(NodeImpl *node)
{
    if (!node)
        return;

    while (NodeImpl *n = node->firstChild()) {
        n->ref();
        removeNode(n);
        insertNodeBefore(n, node);
        n->deref();
    }
    removeNode(node);
}

void ReplacementFragment::removeNode(NodeImpl *node)
{
    if (!node)
        return;
        
    NodeImpl *parent = node->parentNode();
    if (!parent)
        return;
        
    int exceptionCode = 0;
    parent->removeChild(node, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void ReplacementFragment::insertNodeBefore(NodeImpl *node, NodeImpl *refNode)
{
    if (!node || !refNode)
        return;
        
    NodeImpl *parent = refNode->parentNode();
    if (!parent)
        return;
        
    int exceptionCode = 0;
    parent->insertBefore(node, refNode, exceptionCode);
    ASSERT(exceptionCode == 0);
}

NodeImpl *ReplacementFragment::insertFragmentForTestRendering()
{
    NodeImpl *body = m_document->body();
    if (!body)
        return 0;

    ElementImpl *holder = createDefaultParagraphElement(m_document);
    holder->ref();
    
    int exceptionCode = 0;
    holder->appendChild(m_fragment, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    body->appendChild(holder, exceptionCode);
    ASSERT(exceptionCode == 0);
    holder->deref();
    
    m_document->updateLayout();
    
    return holder;
}

void ReplacementFragment::restoreTestRenderingNodesToFragment(NodeImpl *holder)
{
    if (!holder)
        return;

    int exceptionCode = 0;
    while (NodeImpl *node = holder->firstChild()) {
        node->ref();
        holder->removeChild(node, exceptionCode);
        ASSERT(exceptionCode == 0);
        m_fragment->appendChild(node, exceptionCode);
        ASSERT(exceptionCode == 0);
        node->deref();
    }
}

void ReplacementFragment::computeStylesUsingTestRendering(NodeImpl *holder)
{
    if (!holder)
        return;

    m_document->updateLayout();

    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder))
        computeAndStoreNodeDesiredStyle(node, m_styles);
}

void ReplacementFragment::removeUnrenderedNodesUsingTestRendering(NodeImpl *holder)
{
    if (!holder)
        return;

    QPtrList<NodeImpl> unrendered;

    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (!isNodeRendered(node) && !isTableStructureNode(node))
            unrendered.append(node);
    }

    for (QPtrListIterator<NodeImpl> it(unrendered); it.current(); ++it)
        removeNode(it.current());
}

int ReplacementFragment::countRenderedBlocks(NodeImpl *holder)
{
    if (!holder)
        return 0;
    
    int count = 0;
    NodeImpl *prev = 0;
    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (node->isBlockFlow()) {
            if (!prev) {
                count++;
                prev = node;
            }
        }
        else {
            NodeImpl *block = node->enclosingBlockFlowElement();
            if (block != prev) {
                count++;
                prev = block;
            }
        }
    }
    
    return count;
}

void ReplacementFragment::removeStyleNodes()
{
    // Since style information has been computed and cached away in
    // computeStylesForNodes(), these style nodes can be removed, since
    // the correct styles will be added back in fixupNodeStyles().
    NodeImpl *node = m_fragment->firstChild();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        // This list of tags change the appearance of content
        // in ways we can add back on later with CSS, if necessary.
        if (node->id() == ID_B || 
            node->id() == ID_BIG || 
            node->id() == ID_CENTER || 
            node->id() == ID_FONT || 
            node->id() == ID_I || 
            node->id() == ID_S || 
            node->id() == ID_SMALL || 
            node->id() == ID_STRIKE || 
            node->id() == ID_SUB || 
            node->id() == ID_SUP || 
            node->id() == ID_TT || 
            node->id() == ID_U || 
            isStyleSpan(node)) {
            removeNodePreservingChildren(node);
        }
        else if (node->isHTMLElement()) {
            HTMLElementImpl *elem = static_cast<HTMLElementImpl *>(node);
            CSSMutableStyleDeclarationImpl *inlineStyleDecl = elem->inlineStyleDecl();
            if (inlineStyleDecl) {
                inlineStyleDecl->removeBlockProperties();
                inlineStyleDecl->removeInheritableProperties();
            }
        }
        node = next;
    }
}

NodeDesiredStyle::NodeDesiredStyle(NodeImpl *node, CSSMutableStyleDeclarationImpl *style) 
    : m_node(node), m_style(style)
{
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
}

NodeDesiredStyle::NodeDesiredStyle(const NodeDesiredStyle &other)
    : m_node(other.node()), m_style(other.style())
{
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
}

NodeDesiredStyle::~NodeDesiredStyle()
{
    if (m_node)
        m_node->deref();
    if (m_style)
        m_style->deref();
}

NodeDesiredStyle &NodeDesiredStyle::operator=(const NodeDesiredStyle &other)
{
    NodeImpl *oldNode = m_node;
    CSSMutableStyleDeclarationImpl *oldStyle = m_style;

    m_node = other.node();
    m_style = other.style();
    
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
    
    if (oldNode)
        oldNode->deref();
    if (oldStyle)
        oldStyle->deref();
        
    return *this;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, bool selectReplacement, bool smartReplace, bool matchStyle) 
    : CompositeEditCommand(document), 
      m_fragment(document, fragment, matchStyle),
      m_firstNodeInserted(0),
      m_lastNodeInserted(0),
      m_lastTopNodeInserted(0),
      m_insertionStyle(0),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace),
      m_matchStyle(matchStyle)
{
}

ReplaceSelectionCommand::~ReplaceSelectionCommand()
{
    if (m_firstNodeInserted)
        m_firstNodeInserted->deref();
    if (m_lastNodeInserted)
        m_lastNodeInserted->deref();
    if (m_lastTopNodeInserted)
        m_lastTopNodeInserted->deref();
    if (m_insertionStyle)
        m_insertionStyle->deref();
}

void ReplaceSelectionCommand::doApply()
{
    // collect information about the current selection, prior to deleting the selection
    Selection selection = endingSelection();
    ASSERT(selection.isCaretOrRange());

    VisiblePosition visibleStart(selection.start(), selection.startAffinity());
    VisiblePosition visibleEnd(selection.end(), selection.endAffinity());
    bool startAtStartOfBlock = isStartOfBlock(visibleStart);
    bool startAtEndOfBlock = isEndOfBlock(visibleStart);
    bool startAtBlockBoundary = startAtStartOfBlock || startAtEndOfBlock;
    NodeImpl *startBlock = selection.start().node()->enclosingBlockFlowElement();
    NodeImpl *endBlock = selection.end().node()->enclosingBlockFlowElement();

    // decide whether to later merge content into the startBlock
    bool mergeStart = false;
    if (startBlock == startBlock->rootEditableElement() && startAtStartOfBlock && startAtEndOfBlock) {
        // empty editable subtree, need to mergeStart so that fragment ends up
        // inside the editable subtree rather than just before it
        mergeStart = false;
    } else {
        // merge if current selection starts inside a paragraph, or there is only one block and no interchange newline to add
        mergeStart = !m_fragment.hasInterchangeNewlineAtStart() && 
            (!isStartOfParagraph(visibleStart) || (!m_fragment.hasInterchangeNewlineAtEnd() && !m_fragment.hasMoreThanOneBlock())) &&
            !isLastVisiblePositionInSpecialElement(selection.start());
        
        // This is a workaround for this bug:
        // <rdar://problem/4013642> REGRESSION (Mail): Copied quoted word does not paste as a quote if pasted at the start of a line
        // We need more powerful logic in this whole mergeStart code for this case to come out right without
        // breaking other cases.
        if (isStartOfParagraph(visibleStart) && isMailBlockquote(m_fragment.firstChild()))
            mergeStart = false;
    }
    
    // decide whether to later append nodes to the end
    NodeImpl *beyondEndNode = 0;
    if (!isEndOfParagraph(visibleEnd) && !m_fragment.hasInterchangeNewlineAtEnd()) {
        Position beyondEndPos = selection.end().downstream();
        if (!isFirstVisiblePositionInSpecialElement(beyondEndPos))
            beyondEndNode = beyondEndPos.node();
    }
    bool moveNodesAfterEnd = beyondEndNode && (startBlock != endBlock || m_fragment.hasMoreThanOneBlock());

    Position startPos = selection.start();
    
    // delete the current range selection, or insert paragraph for caret selection, as needed
    if (selection.isRange()) {
        deleteSelection(false, !(m_fragment.hasInterchangeNewlineAtStart() || m_fragment.hasInterchangeNewlineAtEnd() || m_fragment.hasMoreThanOneBlock()));
        document()->updateLayout();
        visibleStart = VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY);
        if (m_fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            }
            else {
                insertParagraphSeparator();
                setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY));
            }
        }
        startPos = endingSelection().start();
    } 
    else {
        ASSERT(selection.isCaret());
        if (m_fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            }
            else {
                insertParagraphSeparator();
                setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY));
            }
        }
        if (!m_fragment.hasInterchangeNewlineAtEnd() && m_fragment.hasMoreThanOneBlock() && 
            !startAtBlockBoundary && !isEndOfParagraph(visibleEnd)) {
            // The start and the end need to wind up in separate blocks.
            // Insert a paragraph separator to make that happen.
            insertParagraphSeparator();
            setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY).previous());
        }
        startPos = endingSelection().start();
    }

    if (startAtStartOfBlock && startBlock->inDocument())
        startPos = Position(startBlock, 0);

    startPos = positionOutsideContainingSpecialElement(startPos);

    KHTMLPart *part = document()->part();
    if (m_matchStyle) {
        m_insertionStyle = styleAtPosition(startPos);
        m_insertionStyle->ref();
    }
    
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    part->clearTypingStyle();
    setTypingStyle(0);    
    
    // done if there is nothing to add
    if (!m_fragment.firstChild())
        return;
    
    // check for a line placeholder, and store it away for possible removal later.
    NodeImpl *block = startPos.node()->enclosingBlockFlowElement();
    NodeImpl *linePlaceholder = findBlockPlaceholder(block);
    if (!linePlaceholder) {
        Position downstream = startPos.downstream();
        downstream = positionOutsideContainingSpecialElement(downstream);
        if (downstream.node()->id() == ID_BR && downstream.offset() == 0 && 
            m_fragment.hasInterchangeNewlineAtEnd() &&
            isStartOfLine(VisiblePosition(downstream, VP_DEFAULT_AFFINITY)))
            linePlaceholder = downstream.node();
    }
    
    // check whether to "smart replace" needs to add leading and/or trailing space
    bool addLeadingSpace = false;
    bool addTrailingSpace = false;
    // FIXME: We need the affinity for startPos and endPos, but Position::downstream
    // and Position::upstream do not give it
    if (m_smartReplace) {
        VisiblePosition visiblePos = VisiblePosition(startPos, VP_DEFAULT_AFFINITY);
        assert(visiblePos.isNotNull());
        addLeadingSpace = startPos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isStartOfLine(visiblePos);
        if (addLeadingSpace) {
            QChar previousChar = visiblePos.previous().character();
            if (!previousChar.isNull()) {
                addLeadingSpace = !part->isCharacterSmartReplaceExempt(previousChar, true);
            }
        }
        addTrailingSpace = startPos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isEndOfLine(visiblePos);
        if (addTrailingSpace) {
            QChar thisChar = visiblePos.character();
            if (!thisChar.isNull()) {
                addTrailingSpace = !part->isCharacterSmartReplaceExempt(thisChar, false);
            }
        }
    }
    
    // There are five steps to adding the content: merge blocks at start, add remaining blocks,
    // add "smart replace" space, handle trailing newline, clean up.
    
    // initially, we say the insertion point is the start of selection
    document()->updateLayout();
    Position insertionPos = startPos;

    // step 1: merge content into the start block, if that is needed
    if (mergeStart && !isFirstVisiblePositionInSpecialElementInFragment(Position(m_fragment.mergeStartNode(), 0))) {
        NodeImpl *refNode = m_fragment.mergeStartNode();
        if (refNode) {
            NodeImpl *node = refNode ? refNode->nextSibling() : 0;
            insertNodeAtAndUpdateNodesInserted(refNode, startPos.node(), startPos.offset());
            while (node && !isProbablyBlock(node)) {
                NodeImpl *next = node->nextSibling();
                insertNodeAfterAndUpdateNodesInserted(node, refNode);
                refNode = node;
                node = next;
            }
        }
        
        // update insertion point to be at the end of the last block inserted
        if (m_lastNodeInserted) {
            document()->updateLayout();
            insertionPos = Position(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
        }
    }

    // prune empty nodes from fragment
    // NOTE: why was this not done earlier, before the mergeStart?
    m_fragment.pruneEmptyNodes();
    
    // step 2 : merge everything remaining in the fragment
    if (m_fragment.firstChild()) {
        NodeImpl *refNode = m_fragment.firstChild();
        NodeImpl *node = refNode ? refNode->nextSibling() : 0;
        NodeImpl *insertionBlock = insertionPos.node()->enclosingBlockFlowElement();
        bool insertionBlockIsRoot = insertionBlock == insertionBlock->rootEditableElement();
        VisiblePosition visiblePos(insertionPos, DOWNSTREAM);
        if (!insertionBlockIsRoot && isProbablyBlock(refNode) && isStartOfBlock(visiblePos))
            insertNodeBeforeAndUpdateNodesInserted(refNode, insertionBlock);
        else if (!insertionBlockIsRoot && isProbablyBlock(refNode) && isEndOfBlock(visiblePos)) {
            insertNodeAfterAndUpdateNodesInserted(refNode, insertionBlock);
        } else if (mergeStart && !isProbablyBlock(refNode)) {
            Position pos = visiblePos.next().deepEquivalent().downstream();
            insertNodeAtAndUpdateNodesInserted(refNode, pos.node(), pos.offset());
        } else {
            insertNodeAtAndUpdateNodesInserted(refNode, insertionPos.node(), insertionPos.offset());
        }
        
        while (node) {
            NodeImpl *next = node->nextSibling();
            insertNodeAfterAndUpdateNodesInserted(node, refNode);
            refNode = node;
            node = next;
        }
        document()->updateLayout();
        insertionPos = Position(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
    }

    // step 3 : handle "smart replace" whitespace
    if (addTrailingSpace && m_lastNodeInserted) {
        document()->updateLayout();
        Position pos(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
        bool needsTrailingSpace = pos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull();
        if (needsTrailingSpace) {
            if (m_lastNodeInserted->isTextNode()) {
                TextImpl *text = static_cast<TextImpl *>(m_lastNodeInserted);
                insertTextIntoNode(text, text->length(), nonBreakingSpaceString());
                insertionPos = Position(text, text->length());
            }
            else {
                NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeAfterAndUpdateNodesInserted(node, m_lastNodeInserted);
                insertionPos = Position(node, 1);
            }
        }
    }

    if (addLeadingSpace && m_firstNodeInserted) {
        document()->updateLayout();
        Position pos(m_firstNodeInserted, 0);
        bool needsLeadingSpace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull();
        if (needsLeadingSpace) {
            if (m_firstNodeInserted->isTextNode()) {
                TextImpl *text = static_cast<TextImpl *>(m_firstNodeInserted);
                insertTextIntoNode(text, 0, nonBreakingSpaceString());
            } else {
                NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeBeforeAndUpdateNodesInserted(node, m_firstNodeInserted);
            }
        }
    }
    
    Position lastPositionToSelect;

    // step 4 : handle trailing newline
    if (m_fragment.hasInterchangeNewlineAtEnd()) {
        removeLinePlaceholderIfNeeded(linePlaceholder);

        if (!m_lastNodeInserted) {
            lastPositionToSelect = endingSelection().end().downstream();
        }
        else {
            bool insertParagraph = false;
            VisiblePosition pos(insertionPos, VP_DEFAULT_AFFINITY);

            if (startBlock == endBlock && !isProbablyBlock(m_lastTopNodeInserted)) {
                insertParagraph = true;
            } else {
                // Handle end-of-document case.
                document()->updateLayout();
                if (isEndOfDocument(pos))
                    insertParagraph = true;
            }
            if (insertParagraph) {
                setEndingSelection(insertionPos, DOWNSTREAM);
                insertParagraphSeparator();
                VisiblePosition next = pos.next();

                // Select up to the paragraph separator that was added.
                lastPositionToSelect = next.deepEquivalent().downstream();
                updateNodesInserted(lastPositionToSelect.node());
            } else {
                // Select up to the preexising paragraph separator.
                VisiblePosition next = pos.next();
                lastPositionToSelect = next.deepEquivalent().downstream();
            }
        }
    } 
    else {
        if (m_lastNodeInserted && m_lastNodeInserted->id() == ID_BR && !document()->inStrictMode()) {
            document()->updateLayout();
            VisiblePosition pos(Position(m_lastNodeInserted, 1), DOWNSTREAM);
            if (isEndOfBlock(pos)) {
                NodeImpl *next = m_lastNodeInserted->traverseNextNode();
                bool hasTrailingBR = next && next->id() == ID_BR && m_lastNodeInserted->enclosingBlockFlowElement() == next->enclosingBlockFlowElement();
                if (!hasTrailingBR) {
                    // Insert an "extra" BR at the end of the block. 
                    insertNodeBefore(createBreakElement(document()), m_lastNodeInserted);
                }
            }
        }

        if (moveNodesAfterEnd && !isLastVisiblePositionInSpecialElement(Position(m_lastNodeInserted, maxRangeOffset(m_lastNodeInserted)))) {
            document()->updateLayout();
            QValueList<NodeDesiredStyle> styles;
            QPtrList<NodeImpl> blocks;
            NodeImpl *node = beyondEndNode;
            NodeImpl *refNode = m_lastNodeInserted;
            while (node) {
                RenderObject *renderer = node->renderer();
                // Stop at the first table or block.
                if (renderer && (renderer->isBlockFlow() || renderer->isTable()))
                    break;
                NodeImpl *next = node->nextSibling();
                blocks.append(node->enclosingBlockFlowElement());
                computeAndStoreNodeDesiredStyle(node, styles);
                removeNode(node);
                // No need to update inserted node variables.
                insertNodeAfter(node, refNode);
                refNode = node;
                // We want to move the first BR we see, so check for that here.
                if (node->id() == ID_BR)
                    break;
                node = next;
            }
            document()->updateLayout();
            for (QPtrListIterator<NodeImpl> it(blocks); it.current(); ++it) {
                NodeImpl *blockToRemove = it.current();
                if (!blockToRemove->inDocument())
                    continue;
                if (!blockToRemove->renderer() || !blockToRemove->renderer()->firstChild()) {
                    if (blockToRemove->parentNode())
                        blocks.append(blockToRemove->parentNode()->enclosingBlockFlowElement());
                    removeNode(blockToRemove);
                    document()->updateLayout();
                }
            }

            fixupNodeStyles(styles);
        }
    }
    
    if (!m_matchStyle)
        fixupNodeStyles(m_fragment.desiredStyles());
    completeHTMLReplacement(lastPositionToSelect);
    
    // step 5 : mop up
    removeLinePlaceholderIfNeeded(linePlaceholder);
}

void ReplaceSelectionCommand::removeLinePlaceholderIfNeeded(NodeImpl *linePlaceholder)
{
    if (!linePlaceholder)
        return;
        
    document()->updateLayout();
    if (linePlaceholder->inDocument()) {
        VisiblePosition placeholderPos(linePlaceholder, linePlaceholder->renderer()->caretMinOffset(), DOWNSTREAM);
        if (placeholderPos.next().isNull() ||
            !(isStartOfLine(placeholderPos) && isEndOfLine(placeholderPos))) {
            NodeImpl *block = linePlaceholder->enclosingBlockFlowElement();
            removeNode(linePlaceholder);
            document()->updateLayout();
            if (!block->renderer() || block->renderer()->height() == 0)
                removeNode(block);
        }
    }
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start;
    Position end;

    if (m_firstNodeInserted && m_firstNodeInserted->inDocument() &&
        m_lastNodeInserted && m_lastNodeInserted->inDocument()) {

        // Find the last leaf.
        NodeImpl *lastLeaf = m_lastNodeInserted;
        while (1) {
            NodeImpl *nextChild = lastLeaf->lastChild();
            if (!nextChild)
                break;
            lastLeaf = nextChild;
        }
    
        // Find the first leaf.
        NodeImpl *firstLeaf = m_firstNodeInserted;
        while (1) {
            NodeImpl *nextChild = firstLeaf->firstChild();
            if (!nextChild)
                break;
            firstLeaf = nextChild;
        }
        
        // Call updateLayout so caretMinOffset and caretMaxOffset return correct values.
        document()->updateLayout();
        start = Position(firstLeaf, firstLeaf->caretMinOffset());
        end = Position(lastLeaf, lastLeaf->caretMaxOffset());

        if (m_matchStyle) {
            assert(m_insertionStyle);
            setEndingSelection(Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY));
            applyStyle(m_insertionStyle);
        }    
        
        if (lastPositionToSelect.isNotNull())
            end = lastPositionToSelect;
    }
    else if (lastPositionToSelect.isNotNull()) {
        start = end = lastPositionToSelect;
    }
    else {
        return;
    }
    
    if (m_selectReplacement)
        setEndingSelection(Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY));
    else
        setEndingSelection(end, SEL_DEFAULT_AFFINITY);
    
    rebalanceWhitespace();
}

EditAction ReplaceSelectionCommand::editingAction() const
{
    return EditActionPaste;
}

void ReplaceSelectionCommand::insertNodeAfterAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild)
{
    insertNodeAfter(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeAtAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild, long offset)
{
    insertNodeAt(insertChild, refChild, offset);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeBeforeAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild)
{
    insertNodeBefore(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::updateNodesInserted(NodeImpl *node)
{
    if (!node)
        return;

    // update m_lastTopNodeInserted
    node->ref();
    if (m_lastTopNodeInserted)
        m_lastTopNodeInserted->deref();
    m_lastTopNodeInserted = node;
    
    // update m_firstNodeInserted
    if (!m_firstNodeInserted) {
        m_firstNodeInserted = node;
        m_firstNodeInserted->ref();
    }
    
    if (node == m_lastNodeInserted)
        return;
    
    // update m_lastNodeInserted
    NodeImpl *old = m_lastNodeInserted;
    m_lastNodeInserted = node->lastDescendent();
    m_lastNodeInserted->ref();
    if (old)
        old->deref();
}

void ReplaceSelectionCommand::fixupNodeStyles(const QValueList<NodeDesiredStyle> &list)
{
    // This function uses the mapped "desired style" to apply the additional style needed, if any,
    // to make the node have the desired style.

    document()->updateLayout();

    QValueListConstIterator<NodeDesiredStyle> it;
    for (it = list.begin(); it != list.end(); ++it) {
        NodeImpl *node = (*it).node();
        CSSMutableStyleDeclarationImpl *desiredStyle = (*it).style();
        ASSERT(desiredStyle);

        if (!node->inDocument())
            continue;

        // The desiredStyle declaration tells what style this node wants to be.
        // Compare that to the style that it is right now in the document.
        Position pos(node, 0);
        CSSComputedStyleDeclarationImpl *currentStyle = pos.computedStyle();
        currentStyle->ref();

        // Check for the special "match nearest blockquote color" property and resolve to the correct
        // color if necessary.
        DOMString matchColorCheck = desiredStyle->getPropertyValue(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);
        if (matchColorCheck == matchNearestBlockquoteColorString()) {
            NodeImpl *blockquote = nearestMailBlockquote(node);
            Position pos(blockquote ? blockquote : node->getDocument()->documentElement(), 0);
            CSSComputedStyleDeclarationImpl *style = pos.computedStyle();
            DOMString desiredColor = desiredStyle->getPropertyValue(CSS_PROP_COLOR);
            DOMString nearestColor = style->getPropertyValue(CSS_PROP_COLOR);
            if (desiredColor != nearestColor)
                desiredStyle->setProperty(CSS_PROP_COLOR, nearestColor);
        }
        desiredStyle->removeProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);

        currentStyle->diff(desiredStyle);
        
        // Only add in block properties if the node is at the start of a 
        // paragraph. This matches AppKit.
        if (!isStartOfParagraph(VisiblePosition(pos, DOWNSTREAM)))
            desiredStyle->removeBlockProperties();
        
        // If the desiredStyle is non-zero length, that means the current style differs
        // from the desired by the styles remaining in the desiredStyle declaration.
        if (desiredStyle->length() > 0) {
            DOM::RangeImpl *rangeAroundNode = document()->createRange();
            rangeAroundNode->ref();
            int exceptionCode = 0;
            rangeAroundNode->selectNode(node, exceptionCode);
            ASSERT(exceptionCode == 0);
            // affinity is not really important since this is a temp selection
            // just for calling applyStyle
            setEndingSelection(Selection(rangeAroundNode, SEL_DEFAULT_AFFINITY, SEL_DEFAULT_AFFINITY));
            applyStyle(desiredStyle);
            rangeAroundNode->deref();
        }

        currentStyle->deref();
    }
}

void computeAndStoreNodeDesiredStyle(DOM::NodeImpl *node, QValueList<NodeDesiredStyle> &list)
{
    if (!node || !node->inDocument())
        return;
        
    CSSComputedStyleDeclarationImpl *computedStyle = Position(node, 0).computedStyle();
    computedStyle->ref();
    CSSMutableStyleDeclarationImpl *style = computedStyle->copyInheritableProperties();
    list.append(NodeDesiredStyle(node, style));
    computedStyle->deref();

    // In either of the color-matching tests below, set the color to a pseudo-color that will
    // make the content take on the color of the nearest-enclosing blockquote (if any) after
    // being pasted in.
    if (NodeImpl *blockquote = nearestMailBlockquote(node)) {
        CSSComputedStyleDeclarationImpl *blockquoteStyle = Position(blockquote, 0).computedStyle();
        if (blockquoteStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR)) {
            style->setProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
            return;
        }
    }
    NodeImpl *documentElement = node->getDocument() ? node->getDocument()->documentElement() : 0;
    if (documentElement) {
        CSSComputedStyleDeclarationImpl *documentStyle = Position(documentElement, 0).computedStyle();
        if (documentStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR)) {
            style->setProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
        }
    }
}

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommand

SetNodeAttributeCommand::SetNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute, const DOMString &value)
    : EditCommand(document), m_element(element), m_attribute(attribute), m_value(value)
{
    ASSERT(m_element);
    m_element->ref();
    ASSERT(!m_value.isNull());
}

SetNodeAttributeCommand::~SetNodeAttributeCommand()
{
    ASSERT(m_element);
    m_element->deref();
}

void SetNodeAttributeCommand::doApply()
{
    ASSERT(m_element);
    ASSERT(!m_value.isNull());

    int exceptionCode = 0;
    m_oldValue = m_element->getAttribute(m_attribute);
    m_element->setAttribute(m_attribute, m_value.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

void SetNodeAttributeCommand::doUnapply()
{
    ASSERT(m_element);

    int exceptionCode = 0;
    if (m_oldValue.isNull())
        m_element->removeAttribute(m_attribute, exceptionCode);
    else
        m_element->setAttribute(m_attribute, m_oldValue.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// SplitTextNodeCommand

SplitTextNodeCommand::SplitTextNodeCommand(DocumentImpl *document, TextImpl *text, long offset)
    : EditCommand(document), m_text1(0), m_text2(text), m_offset(offset)
{
    ASSERT(m_text2);
    ASSERT(m_text2->length() > 0);

    m_text2->ref();
}

SplitTextNodeCommand::~SplitTextNodeCommand()
{
    if (m_text1)
        m_text1->deref();

    ASSERT(m_text2);
    m_text2->deref();
}

void SplitTextNodeCommand::doApply()
{
    ASSERT(m_text2);
    ASSERT(m_offset > 0);

    int exceptionCode = 0;

    // EDIT FIXME: This should use better smarts for figuring out which portion
    // of the split to copy (based on their comparitive sizes). We should also
    // just use the DOM's splitText function.
    
    if (!m_text1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_text1 = document()->createTextNode(m_text2->substringData(0, m_offset, exceptionCode));
        ASSERT(exceptionCode == 0);
        ASSERT(m_text1);
        m_text1->ref();
    }

    m_text2->deleteData(0, m_offset, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);
        
    ASSERT(m_text2->previousSibling()->isTextNode());
    ASSERT(m_text2->previousSibling() == m_text1);
}

void SplitTextNodeCommand::doUnapply()
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    
    ASSERT(m_text1->nextSibling() == m_text2);

    int exceptionCode = 0;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->removeChild(m_text1, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_offset = m_text1->length();
}

//------------------------------------------------------------------------------------------
// SplitElementCommand

SplitElementCommand::SplitElementCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element, DOM::NodeImpl *atChild)
    : EditCommand(document), m_element1(0), m_element2(element), m_atChild(atChild)
{
    ASSERT(m_element2);
    ASSERT(m_atChild);

    m_element2->ref();
    m_atChild->ref();
}

SplitElementCommand::~SplitElementCommand()
{
    if (m_element1)
        m_element1->deref();

    ASSERT(m_element2);
    m_element2->deref();
    ASSERT(m_atChild);
    m_atChild->deref();
}

void SplitElementCommand::doApply()
{
    ASSERT(m_element2);
    ASSERT(m_atChild);

    int exceptionCode = 0;

    if (!m_element1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_element1 = static_cast<ElementImpl *>(m_element2->cloneNode(false));
        ASSERT(m_element1);
        m_element1->ref();
    }

    m_element2->parent()->insertBefore(m_element1, m_element2, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    while (m_element2->firstChild() != m_atChild) {
        ASSERT(m_element2->firstChild());
        m_element1->appendChild(m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }
}

void SplitElementCommand::doUnapply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);
    ASSERT(m_atChild);

    ASSERT(m_element1->nextSibling() == m_element2);
    ASSERT(m_element2->firstChild() && m_element2->firstChild() == m_atChild);

    int exceptionCode = 0;

    while (m_element1->lastChild()) {
        m_element2->insertBefore(m_element1->lastChild(), m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element2->parentNode()->removeChild(m_element1, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// MergeIdenticalElementsCommand

MergeIdenticalElementsCommand::MergeIdenticalElementsCommand(DOM::DocumentImpl *document, DOM::ElementImpl *first, DOM::ElementImpl *second)
    : EditCommand(document), m_element1(first), m_element2(second), m_atChild(0)
{
    ASSERT(m_element1);
    ASSERT(m_element2);

    m_element1->ref();
    m_element2->ref();
}

MergeIdenticalElementsCommand::~MergeIdenticalElementsCommand()
{
    if (m_atChild)
        m_atChild->deref();

    ASSERT(m_element1);
    m_element1->deref();
    ASSERT(m_element2);
    m_element2->deref();
}

void MergeIdenticalElementsCommand::doApply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);
    ASSERT(m_element1->nextSibling() == m_element2);

    int exceptionCode = 0;

    if (!m_atChild) {
        m_atChild = m_element2->firstChild();
        m_atChild->ref();
    }

    while (m_element1->lastChild()) {
        m_element2->insertBefore(m_element1->lastChild(), m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element2->parentNode()->removeChild(m_element1, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void MergeIdenticalElementsCommand::doUnapply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);

    int exceptionCode = 0;

    m_element2->parent()->insertBefore(m_element1, m_element2, exceptionCode);
    ASSERT(exceptionCode == 0);

    while (m_element2->firstChild() != m_atChild) {
        ASSERT(m_element2->firstChild());
        m_element1->appendChild(m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }
}

//------------------------------------------------------------------------------------------
// WrapContentsInDummySpanCommand

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element)
    : EditCommand(document), m_element(element), m_dummySpan(0)
{
    ASSERT(m_element);

    m_element->ref();
}

WrapContentsInDummySpanCommand::~WrapContentsInDummySpanCommand()
{
    if (m_dummySpan)
        m_dummySpan->deref();

    ASSERT(m_element);
    m_element->deref();
}

void WrapContentsInDummySpanCommand::doApply()
{
    ASSERT(m_element);

    int exceptionCode = 0;

    if (!m_dummySpan) {
        m_dummySpan = createStyleSpanElement(document());
        m_dummySpan->ref();
    }

    while (m_element->firstChild()) {
        m_dummySpan->appendChild(m_element->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element->appendChild(m_dummySpan, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void WrapContentsInDummySpanCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(m_dummySpan);

    ASSERT(m_element->firstChild() == m_dummySpan);
    ASSERT(!m_element->firstChild()->nextSibling());

    int exceptionCode = 0;

    while (m_dummySpan->firstChild()) {
        m_element->appendChild(m_dummySpan->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element->removeChild(m_dummySpan, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// SplitTextNodeContainingElementCommand

SplitTextNodeContainingElementCommand::SplitTextNodeContainingElementCommand(DocumentImpl *document, TextImpl *text, long offset)
    : CompositeEditCommand(document), m_text(text), m_offset(offset)
{
    ASSERT(m_text);
    ASSERT(m_text->length() > 0);

    m_text->ref();
}

SplitTextNodeContainingElementCommand::~SplitTextNodeContainingElementCommand()
{
    ASSERT(m_text);
    m_text->deref();
}

void SplitTextNodeContainingElementCommand::doApply()
{
    ASSERT(m_text);
    ASSERT(m_offset > 0);

    splitTextNode(m_text, m_offset);
    
    NodeImpl *parentNode = m_text->parentNode();
    if (!parentNode->renderer() || !parentNode->renderer()->isInline()) {
        wrapContentsInDummySpan(static_cast<ElementImpl *>(parentNode));
        parentNode = parentNode->firstChild();
    }

    splitElement(static_cast<ElementImpl *>(parentNode), m_text);
}

//------------------------------------------------------------------------------------------
// TypingCommand

TypingCommand::TypingCommand(DocumentImpl *document, ETypingCommand commandType, const DOMString &textToInsert, bool selectInsertedText)
    : CompositeEditCommand(document), 
      m_commandType(commandType), 
      m_textToInsert(textToInsert), 
      m_openForMoreTyping(true), 
      m_applyEditing(false), 
      m_selectInsertedText(selectInsertedText),
      m_smartDelete(false)
{
}

void TypingCommand::deleteKeyPressed(DocumentImpl *document, bool smartDelete)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->deleteKeyPressed();
    }
    else {
        Selection selection = part->selection();
        if (selection.isCaret() && VisiblePosition(selection.start(), selection.startAffinity()).previous().isNull()) {
            // do nothing for a delete key at the start of an editable element.
        }
        else {
            TypingCommand *typingCommand = new TypingCommand(document, DeleteKey);
            typingCommand->setSmartDelete(smartDelete);
            EditCommandPtr cmd(typingCommand);
            cmd.apply();
        }
    }
}

void TypingCommand::forwardDeleteKeyPressed(DocumentImpl *document, bool smartDelete)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->forwardDeleteKeyPressed();
    }
    else {
        Selection selection = part->selection();
        if (selection.isCaret() && isEndOfDocument(VisiblePosition(selection.start(), selection.startAffinity()))) {
            // do nothing for a delete key at the start of an editable element.
        }
        else {
            TypingCommand *typingCommand = new TypingCommand(document, ForwardDeleteKey);
            typingCommand->setSmartDelete(smartDelete);
            EditCommandPtr cmd(typingCommand);
            cmd.apply();
        }
    }
}

void TypingCommand::insertText(DocumentImpl *document, const DOMString &text, bool selectInsertedText)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertText(text, selectInsertedText);
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertText, text, selectInsertedText));
        cmd.apply();
    }
}

void TypingCommand::insertLineBreak(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertLineBreak();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertLineBreak));
        cmd.apply();
    }
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertParagraphSeparatorInQuotedContent();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertParagraphSeparatorInQuotedContent));
        cmd.apply();
    }
}

void TypingCommand::insertParagraphSeparator(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertParagraphSeparator();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertParagraphSeparator));
        cmd.apply();
    }
}

bool TypingCommand::isOpenForMoreTypingCommand(const EditCommandPtr &cmd)
{
    return cmd.isTypingCommand() &&
        static_cast<const TypingCommand *>(cmd.get())->openForMoreTyping();
}

void TypingCommand::closeTyping(const EditCommandPtr &cmd)
{
    if (isOpenForMoreTypingCommand(cmd))
        static_cast<TypingCommand *>(cmd.get())->closeTyping();
}

void TypingCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    switch (m_commandType) {
        case DeleteKey:
            deleteKeyPressed();
            return;
        case ForwardDeleteKey:
            forwardDeleteKeyPressed();
            return;
        case InsertLineBreak:
            insertLineBreak();
            return;
        case InsertParagraphSeparator:
            insertParagraphSeparator();
            return;
        case InsertParagraphSeparatorInQuotedContent:
            insertParagraphSeparatorInQuotedContent();
            return;
        case InsertText:
            insertText(m_textToInsert, m_selectInsertedText);
            return;
    }

    ASSERT_NOT_REACHED();
}

EditAction TypingCommand::editingAction() const
{
    return EditActionTyping;
}

void TypingCommand::markMisspellingsAfterTyping()
{
    // Take a look at the selection that results after typing and determine whether we need to spellcheck. 
    // Since the word containing the current selection is never marked, this does a check to
    // see if typing made a new word that is not in the current selection. Basically, you
    // get this by being at the end of a word and typing a space.    
    VisiblePosition start(endingSelection().start(), endingSelection().startAffinity());
    VisiblePosition previous = start.previous();
    if (previous.isNotNull()) {
        VisiblePosition p1 = startOfWord(previous, LeftWordIfOnBoundary);
        VisiblePosition p2 = startOfWord(start, LeftWordIfOnBoundary);
        if (p1 != p2)
            KWQ(document()->part())->markMisspellingsInAdjacentWords(p1);
    }
}

void TypingCommand::typingAddedToOpenCommand()
{
    markMisspellingsAfterTyping();
    // Do not apply editing to the part on the first time through.
    // The part will get told in the same way as all other commands.
    // But since this command stays open and is used for additional typing, 
    // we need to tell the part here as other commands are added.
    if (m_applyEditing) {
        EditCommandPtr cmd(this);
        document()->part()->appliedEditing(cmd);
    }
    m_applyEditing = true;
}

void TypingCommand::insertText(const DOMString &text, bool selectInsertedText)
{
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    int offset = 0;
    int newline;
    while ((newline = text.find('\n', offset)) != -1) {
        if (newline != offset) {
            insertTextRunWithoutNewlines(text.substring(offset, newline - offset), false);
        }
        insertParagraphSeparator();
        offset = newline + 1;
    }
    if (offset == 0) {
        insertTextRunWithoutNewlines(text, selectInsertedText);
    } else {
        int length = text.length();
        if (length != offset) {
            insertTextRunWithoutNewlines(text.substring(offset, length - offset), selectInsertedText);
        }
    }
}

void TypingCommand::insertTextRunWithoutNewlines(const DOMString &text, bool selectInsertedText)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (document()->part()->typingStyle() || m_cmds.count() == 0) {
        InsertTextCommand *impl = new InsertTextCommand(document());
        EditCommandPtr cmd(impl);
        applyCommandToComposite(cmd);
        impl->input(text, selectInsertedText);
    }
    else {
        EditCommandPtr lastCommand = m_cmds.last();
        if (lastCommand.isInsertTextCommand()) {
            InsertTextCommand *impl = static_cast<InsertTextCommand *>(lastCommand.get());
            impl->input(text, selectInsertedText);
        }
        else {
            InsertTextCommand *impl = new InsertTextCommand(document());
            EditCommandPtr cmd(impl);
            applyCommandToComposite(cmd);
            impl->input(text, selectInsertedText);
        }
    }
    typingAddedToOpenCommand();
}

void TypingCommand::insertLineBreak()
{
    EditCommandPtr cmd(new InsertLineBreakCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparatorInQuotedContent()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorInQuotedContentCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::deleteKeyPressed()
{
    Selection selectionToDelete;
    
    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            break;
        case Selection::CARET: {
            // Handle delete at beginning-of-block case.
            // Do nothing in the case that the caret is at the start of a
            // root editable element or at the start of a document.
            Position pos(endingSelection().start());
            Position start = VisiblePosition(pos, endingSelection().startAffinity()).previous().deepEquivalent();
            Position end = VisiblePosition(pos, endingSelection().startAffinity()).deepEquivalent();
            if (start.isNotNull() && end.isNotNull() && start.node()->rootEditableElement() == end.node()->rootEditableElement())
                selectionToDelete = Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange()) {
        deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
}

void TypingCommand::forwardDeleteKeyPressed()
{
    Selection selectionToDelete;
    
    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            break;
        case Selection::CARET: {
            // Handle delete at beginning-of-block case.
            // Do nothing in the case that the caret is at the start of a
            // root editable element or at the start of a document.
            Position pos(endingSelection().start());
            Position start = VisiblePosition(pos, endingSelection().startAffinity()).next().deepEquivalent();
            Position end = VisiblePosition(pos, endingSelection().startAffinity()).deepEquivalent();
            if (start.isNotNull() && end.isNotNull() && start.node()->rootEditableElement() == end.node()->rootEditableElement())
                selectionToDelete = Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange()) {
        deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
}

bool TypingCommand::preservesTypingStyle() const
{
    switch (m_commandType) {
        case DeleteKey:
        case ForwardDeleteKey:
        case InsertParagraphSeparator:
        case InsertLineBreak:
            return true;
        case InsertParagraphSeparatorInQuotedContent:
        case InsertText:
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool TypingCommand::isTypingCommand() const
{
    return true;
}

ElementImpl *createDefaultParagraphElement(DocumentImpl *document)
{
    // We would need this margin-zeroing code back if we ever return to using <p> elements for default paragraphs.
    // static const DOMString defaultParagraphStyle("margin-top: 0; margin-bottom: 0");    
    int exceptionCode = 0;
    ElementImpl *element = document->createHTMLElement("div", exceptionCode);
    ASSERT(exceptionCode == 0);
    return element;
}

ElementImpl *createBreakElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *breakNode = document->createHTMLElement("br", exceptionCode);
    ASSERT(exceptionCode == 0);
    return breakNode;
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

bool isProbablyBlock(const NodeImpl *node)
{
    if (!node)
        return false;
    
    switch (node->id()) {
        case ID_BLOCKQUOTE:
        case ID_DD:
        case ID_DIV:
        case ID_DL:
        case ID_DT:
        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
        case ID_HR:
        case ID_LI:
        case ID_OL:
        case ID_P:
        case ID_PRE:
        case ID_TD:
        case ID_TH:
        case ID_UL:
            return true;
    }
    
    return false;
}

bool isProbablyTableStructureNode(const NodeImpl *node)
{
    if (!node)
        return false;
    
    switch (node->id()) {
        case ID_TABLE:
        case ID_TBODY:
        case ID_TD:
        case ID_TFOOT:
        case ID_THEAD:
        case ID_TR:
            return true;
    }
    return false;
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
    if (!node || !node->renderer() || !node->isElementNode() && node->id() != ID_BLOCKQUOTE)
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("type") == "cite";
}

bool isMailPasteAsQuotationNode(const NodeImpl *node)
{
    if (!node)
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("class") == ApplePasteAsQuotation;
}

} // namespace khtml
