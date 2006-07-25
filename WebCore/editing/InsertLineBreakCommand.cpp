/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "InsertLineBreakCommand.h"

#include "CSSMutableStyleDeclaration.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "Logging.h"
#include "Text.h"
#include "VisiblePosition.h"
#include "Range.h"
#include "htmlediting.h"
#include "HTMLNames.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

InsertLineBreakCommand::InsertLineBreakCommand(Document* document) 
    : CompositeEditCommand(document)
{
}

bool InsertLineBreakCommand::preservesTypingStyle() const
{
    return true;
}

void InsertLineBreakCommand::insertNodeAfterPosition(Node *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *after* the block.
    Position upstream(pos.upstream());
    Node *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(node, cb);
    else
        insertNodeAfter(node, pos.node());
}

void InsertLineBreakCommand::insertNodeBeforePosition(Node *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *before* the block.
    Position upstream(pos.upstream());
    Node *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(node, cb);
    else
        insertNodeBefore(node, pos.node());
}

void InsertLineBreakCommand::doApply()
{
    deleteSelection();
    Selection selection = endingSelection();
    if (selection.isNone())
        return;

    Position pos(selection.start().upstream());

    pos = positionAvoidingSpecialElementBoundary(pos);

    Node* styleNode = pos.node();
    bool isTabSpan = isTabSpanTextNode(styleNode);
    if (isTabSpan)
        styleNode = styleNode->parentNode()->parentNode();
    RenderObject* styleRenderer = styleNode->renderer();
    bool useBreakElement = !styleRenderer || !styleRenderer->style()->preserveNewline();

    RefPtr<Node> nodeToInsert;
    if (useBreakElement)
        nodeToInsert = createBreakElement(document());
    else
        nodeToInsert = document()->createTextNode("\n");
        // FIXME: Need to merge text nodes when inserting just after or before text.
    
    if (isTabSpan) {
        insertNodeAtTabSpanPosition(nodeToInsert.get(), pos);
        setEndingSelection(Position(nodeToInsert->traverseNextNode(), 0), DOWNSTREAM);
    } else if (isEndOfBlock(VisiblePosition(pos, selection.affinity()))) {
        Node* block = pos.node()->enclosingBlockFlowElement();
        
        // Insert an extra break element so that there will be a blank line after the last
        // inserted line break. In HTML, a line break at the end of a block ends the last
        // line in the block, while in editable text, a line break at the end of block
        // creates a last blank line. We need an extra break element to get HTML to act
        // the way editable text would.
        bool haveBreak = pos.downstream().node()->hasTagName(brTag) && pos.downstream().offset() == 0;
        insertNodeAt(nodeToInsert.get(), pos.node(), pos.offset());
        if (!haveBreak)
            insertNodeAfter(createBreakElement(document()).get(), nodeToInsert.get());
            
        setEndingSelection(Position(block, maxDeepOffset(block)), DOWNSTREAM);
    } else if (pos.offset() <= pos.node()->caretMinOffset()) {
        LOG(Editing, "input newline case 2");
        // Insert node before downstream position, and place caret there as well. 
        Position endingPosition = pos.downstream();
        insertNodeBeforePosition(nodeToInsert.get(), endingPosition);
        setEndingSelection(endingPosition, DOWNSTREAM);
    } else if (pos.offset() >= pos.node()->caretMaxOffset()) {
        LOG(Editing, "input newline case 3");
        // Insert BR after this node. Place caret in the position that is downstream
        // of the current position, reckoned before inserting the BR in between.
        Position endingPosition = pos.downstream();
        insertNodeAfterPosition(nodeToInsert.get(), pos);
        setEndingSelection(endingPosition, DOWNSTREAM);
    } else {
        // Split a text node
        LOG(Editing, "input newline case 4");
        ASSERT(pos.node()->isTextNode());
        
        // Do the split
        ExceptionCode ec = 0;
        Text *textNode = static_cast<Text *>(pos.node());
        RefPtr<Text> textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.start().offset(), ec));
        deleteTextFromNode(textNode, 0, pos.offset());
        insertNodeBefore(textBeforeNode.get(), textNode);
        insertNodeBefore(nodeToInsert.get(), textNode);
        Position endingPosition = Position(textNode, 0);
        
        // Handle whitespace that occurs after the split
        updateLayout();
        if (!endingPosition.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            deleteInsignificantTextDownstream(endingPosition);
            ASSERT(!textNode->renderer() || textNode->renderer()->style()->collapseWhiteSpace());
            insertTextIntoNode(textNode, 0, nonBreakingSpaceString());
        }
        
        setEndingSelection(endingPosition, DOWNSTREAM);
    }

    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    
    CSSMutableStyleDeclaration* typingStyle = document()->frame()->typingStyle();
    
    if (typingStyle && typingStyle->length() > 0) {
        Selection selectionBeforeStyle = endingSelection();
        applyStyle(typingStyle, Position(nodeToInsert.get(), 0),
            Position(nodeToInsert.get(), maxDeepOffset(nodeToInsert.get())));
        setEndingSelection(selectionBeforeStyle);
    }

    rebalanceWhitespace();
}

}
