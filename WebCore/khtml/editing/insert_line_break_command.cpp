/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "insert_line_break_command.h"

#include "htmlediting.h"
#include "visible_position.h"
#include "visible_units.h"

#include "htmlnames.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_textimpl.h"
#include "Frame.h"

#include <kxmlcore/Assertions.h>
#include "KWQLogging.h"

using namespace DOM::HTMLNames;

using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::TextImpl;
using DOM::CSSMutableStyleDeclarationImpl;

namespace khtml {

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
    SelectionController selection = endingSelection();

    ElementImpl *breakNode = createBreakElement(document());
    NodeImpl *nodeToInsert = breakNode;
    
    Position pos(selection.start().upstream());

    pos = positionOutsideContainingSpecialElement(pos);

    if (isTabSpanTextNode(pos.node())) {
        insertNodeAtTabSpanPosition(nodeToInsert, pos);
        setEndingSelection(Position(nodeToInsert->traverseNextNode(), 0), DOWNSTREAM);
    } else if (isEndOfBlock(VisiblePosition(pos, selection.startAffinity()))) {
        LOG(Editing, "input newline case 1");
        // Check for a trailing BR. If there isn't one, we'll need to insert an "extra" one.
        // This makes the "real" BR we want to insert appear in the rendering without any 
        // significant side effects (and no real worries either since you can't arrow past 
        // this extra one.
        if (pos.node()->hasTagName(brTag) && pos.offset() == 0) {
            // Already placed in a trailing BR. Insert "real" BR before it and leave the selection alone.
            insertNodeBefore(nodeToInsert, pos.node());
        } else {
            NodeImpl *next = pos.node()->traverseNextNode();
            bool hasTrailingBR = next && next->hasTagName(brTag) && pos.node()->enclosingBlockFlowElement() == next->enclosingBlockFlowElement();
            insertNodeAfterPosition(nodeToInsert, pos);
            if (hasTrailingBR)
                setEndingSelection(SelectionController(Position(next, 0), DOWNSTREAM));
            else if (!document()->inStrictMode()) {
                // Insert an "extra" BR at the end of the block. 
                ElementImpl *extraBreakNode = createBreakElement(document());
                insertNodeAfter(extraBreakNode, nodeToInsert);
                setEndingSelection(Position(extraBreakNode, 0), DOWNSTREAM);
            }
        }
    }
    else if (pos.offset() <= pos.node()->caretMinOffset()) {
        LOG(Editing, "input newline case 2");
        // Insert node before downstream position, and place caret there as well. 
        Position endingPosition = pos.downstream();
        insertNodeBeforePosition(nodeToInsert, endingPosition);
        setEndingSelection(endingPosition, DOWNSTREAM);
    } else if (pos.offset() >= pos.node()->caretMaxOffset()) {
        LOG(Editing, "input newline case 3");
        // Insert BR after this node. Place caret in the position that is downstream
        // of the current position, reckoned before inserting the BR in between.
        Position endingPosition = pos.downstream();
        insertNodeAfterPosition(nodeToInsert, pos);
        setEndingSelection(endingPosition, DOWNSTREAM);
    } else {
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
        updateLayout();
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
    
    CSSMutableStyleDeclarationImpl *typingStyle = document()->frame()->typingStyle();
    
    if (typingStyle && typingStyle->length() > 0) {
        SelectionController selectionBeforeStyle = endingSelection();
        applyStyle(typingStyle, Position(nodeToInsert, 0), Position(nodeToInsert, nodeToInsert->maxDeepOffset()));
        setEndingSelection(selectionBeforeStyle);
    }

    rebalanceWhitespace();
}

} // namespace khtml
