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

static const int spacesPerTab = 4;

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

} // namespace khtml
