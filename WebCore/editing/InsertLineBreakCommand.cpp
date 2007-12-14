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

// Whether we should insert a break element or a '\n'.
bool InsertLineBreakCommand::shouldUseBreakElement(const Position& insertionPos)
{
    // An editing position like [input, 0] actually refers to the position before
    // the input element, and in that case we need to check the input element's
    // parent's renderer.
    Position p(rangeCompliantEquivalent(insertionPos));
    return p.node()->renderer() && !p.node()->renderer()->style()->preserveNewline();
}

void InsertLineBreakCommand::doApply()
{
    deleteSelection();
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    VisiblePosition caret(selection.visibleStart());
    Position pos(caret.deepEquivalent());

    pos = positionAvoidingSpecialElementBoundary(pos);
    
    pos = positionOutsideTabSpan(pos);

    RefPtr<Node> nodeToInsert;
    if (shouldUseBreakElement(pos))
        nodeToInsert = createBreakElement(document());
    else
        nodeToInsert = document()->createTextNode("\n");
    
    // FIXME: Need to merge text nodes when inserting just after or before text.
    
    if (isEndOfParagraph(caret) && !lineBreakExistsAtPosition(caret)) {
        bool needExtraLineBreak = !pos.node()->hasTagName(hrTag) && !pos.node()->hasTagName(tableTag);
        
        insertNodeAt(nodeToInsert.get(), pos);
        
        if (needExtraLineBreak)
            insertNodeBefore(nodeToInsert->cloneNode(false).get(), nodeToInsert.get());
        
        VisiblePosition endingPosition(Position(nodeToInsert.get(), 0));
        setEndingSelection(Selection(endingPosition));
    } else if (pos.offset() <= caretMinOffset(pos.node())) {
        insertNodeAt(nodeToInsert.get(), pos);
        
        // Insert an extra br or '\n' if the just inserted one collapsed.
        if (!isStartOfParagraph(VisiblePosition(Position(nodeToInsert.get(), 0))))
            insertNodeBefore(nodeToInsert->cloneNode(false).get(), nodeToInsert.get());
        
        setEndingSelection(Selection(positionAfterNode(nodeToInsert.get()), DOWNSTREAM));
    } else if (pos.offset() >= caretMaxOffset(pos.node())) {
        insertNodeAt(nodeToInsert.get(), pos);
        setEndingSelection(Selection(positionAfterNode(nodeToInsert.get()), DOWNSTREAM));
    } else {
        // Split a text node
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
            Position positionBeforeTextNode(positionBeforeNode(textNode));
            // Clear out all whitespace and insert one non-breaking space
            deleteInsignificantTextDownstream(endingPosition);
            ASSERT(!textNode->renderer() || textNode->renderer()->style()->collapseWhiteSpace());
            // Deleting insignificant whitespace will remove textNode if it contains nothing but insignificant whitespace.
            if (textNode->inDocument())
                insertTextIntoNode(textNode, 0, nonBreakingSpaceString());
            else {
                RefPtr<Text> nbspNode = document()->createTextNode(nonBreakingSpaceString());
                insertNodeAt(nbspNode.get(), positionBeforeTextNode);
                endingPosition = Position(nbspNode.get(), 0);
            }
        }
        
        setEndingSelection(Selection(endingPosition, DOWNSTREAM));
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
