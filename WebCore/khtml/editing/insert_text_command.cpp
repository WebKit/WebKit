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

#include "insert_text_command.h"

#include "khtml_part.h"
#include "htmlediting.h"
#include "html_interchange.h"
#include "visible_position.h"
#include "visible_text.h"
#include "visible_units.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_position.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) assert(assertion)
#define LOG(channel, formatAndArgs...) ((void)0)
#endif

using DOM::DocumentImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::TextImpl;
using DOM::DOMString;
using DOM::CSSMutableStyleDeclarationImpl;

namespace khtml {

InsertTextCommand::InsertTextCommand(DocumentImpl *document) 
    : CompositeEditCommand(document), m_charactersAdded(0)
{
}

void InsertTextCommand::doApply()
{
}

Position InsertTextCommand::prepareForTextInsertion(const Position& pos)
{
    // Prepare for text input by looking at the specified position.
    // It may be necessary to insert a text node to receive characters.
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
        
        return Position(textNode, 0);
    }

    if (isTabSpanTextNode(pos.node())) {
        Position tempPos = pos;
//#ifndef COALESCE_TAB_SPANS
#if 0
        NodeImpl *node = pos.node()->parentNode();
        if (pos.offset() > pos.node()->caretMinOffset())
            tempPos = Position(node->parentNode(), node->nodeIndex() + 1);
        else
            tempPos = Position(node->parentNode(), node->nodeIndex());
#endif        
        NodeImpl *textNode = document()->createEditingTextNode("");
        NodeImpl *originalTabSpan = tempPos.node()->parent();
        if (tempPos.offset() <= tempPos.node()->caretMinOffset()) {
            insertNodeBefore(textNode, originalTabSpan);
        } else if (tempPos.offset() >= tempPos.node()->caretMaxOffset()) {
            insertNodeAfter(textNode, originalTabSpan);
        } else {
            splitTextNodeContainingElement(static_cast<TextImpl *>(tempPos.node()), tempPos.offset());
            insertNodeBefore(textNode, originalTabSpan);
        }
        return Position(textNode, 0);
    }

    return pos;
}

static inline bool isNBSP(const QChar &c)
{
    return c.unicode() == 0xa0;
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

    // Figure out the startPosition
    Position startPosition = selection.start();
    Position endPosition;
    if (adjustDownstream)
        startPosition = startPosition.downstream();
    else
        startPosition = startPosition.upstream();
    startPosition = positionOutsideContainingSpecialElement(startPosition);
    
    if (text == "\t") {
        endPosition = insertTab(startPosition);
        startPosition = endPosition.previous();
        removeBlockPlaceholder(startPosition.node()->enclosingBlockFlowElement());
        m_charactersAdded += 1;
    } else {
        // Make sure the document is set up to receive text
        startPosition = prepareForTextInsertion(startPosition);
        removeBlockPlaceholder(startPosition.node()->enclosingBlockFlowElement());
        TextImpl *textNode = static_cast<TextImpl *>(startPosition.node());
        int offset = startPosition.offset();

        if (text == " ") {
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
            unsigned int len = text.length();
            
#if APPLE_CHANGES
            // When the user hits space to finish marked sequence, the string that
            // we receive ends with a normal space, not a non breaking space.  This code
            // ensures that the right kind of space is produced.
            if (KWQ(document()->part())->markedTextRange() && text[len-1] == ' ') {
                DOMString textWithoutTrailingSpace(text.unicode(), len-1);
                insertTextIntoNode(textNode, offset, textWithoutTrailingSpace);
                insertSpace(textNode, offset + len-1);
            } else
                insertTextIntoNode(textNode, offset, text);
#else
            insertTextIntoNode(textNode, offset, text);
#endif
            m_charactersAdded += len;
            endPosition = Position(textNode, offset + len);
        }
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

DOM::Position InsertTextCommand::insertTab(Position pos)
{
    Position insertPos = VisiblePosition(pos, DOWNSTREAM).deepEquivalent();
    NodeImpl *node = insertPos.node();
    unsigned int offset = insertPos.offset();

//#ifdef COALESCE_TAB_SPANS
#if 1
    // keep tabs coalesced in tab span
    if (isTabSpanTextNode(node)) {
        insertTextIntoNode(static_cast<TextImpl *>(node), offset, "\t");
        return Position(node, offset + 1);
    }
#else
    if (isTabSpanTextNode(node)) {
        node = node->parentNode();
        if (offset > (unsigned int) node->caretMinOffset())
            insertPos = Position(node->parentNode(), node->nodeIndex() + 1);
        else
            insertPos = Position(node->parentNode(), node->nodeIndex());
        node = insertPos.node();
        offset = insertPos.offset();
    }
#endif
    
    // create new tab span
    DOM::ElementImpl * spanNode = createTabSpanElement(document());
    
    // place it
    if (!node->isTextNode()) {
        insertNodeAt(spanNode, node, offset);
    } else {
        TextImpl *textNode = static_cast<TextImpl *>(node);
        if (offset >= textNode->length()) {
            insertNodeAfter(spanNode, textNode);
        } else {
            // split node to make room for the span
            // NOTE: splitTextNode uses textNode for the
            // second node in the split, so we need to
            // insert the span before it.
            if (offset > 0)
                splitTextNode(textNode, offset);
            insertNodeBefore(spanNode, textNode);
        }
    }
    
    // return the position following the new tab
    return Position(spanNode->lastChild(), spanNode->lastChild()->caretMaxOffset());
}

void InsertTextCommand::insertSpace(TextImpl *textNode, unsigned offset)
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
        if (downstream.offset() < (int)text.length() && isCollapsibleWhitespace(text[downstream.offset()]))
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

} // namespace khtml
