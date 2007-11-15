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
#include "InsertTextCommand.h"

#include "CharacterNames.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSComputedStyleDeclaration.h"
#include "Document.h"
#include "Element.h"
#include "EditingText.h"
#include "Editor.h"
#include "Frame.h"
#include "Logging.h"
#include "HTMLInterchange.h"
#include "htmlediting.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "visible_units.h"

namespace WebCore {

InsertTextCommand::InsertTextCommand(Document *document) 
    : CompositeEditCommand(document), m_charactersAdded(0)
{
}

void InsertTextCommand::doApply()
{
}

Position InsertTextCommand::prepareForTextInsertion(const Position& p)
{
    Position pos = p;
    // If an anchor was removed and the selection hasn't changed, we restore it.
    RefPtr<Node> anchor = document()->frame()->editor()->removedAnchor();
    if (anchor) {
        insertNodeAt(anchor.get(), pos);
        document()->frame()->editor()->setRemovedAnchor(0);
        pos = Position(anchor.get(), 0);
    }
    // Prepare for text input by looking at the specified position.
    // It may be necessary to insert a text node to receive characters.
    if (!pos.node()->isTextNode()) {
        RefPtr<Node> textNode = document()->createEditingTextNode("");
        insertNodeAt(textNode.get(), pos);
        return Position(textNode.get(), 0);
    }

    if (isTabSpanTextNode(pos.node())) {
        RefPtr<Node> textNode = document()->createEditingTextNode("");
        insertNodeAtTabSpanPosition(textNode.get(), pos);
        return Position(textNode.get(), 0);
    }

    return pos;
}

void InsertTextCommand::input(const String& originalText, bool selectInsertedText)
{
    String text = originalText;
    
    ASSERT(text.find('\n') == -1);

    if (endingSelection().isNone())
        return;
        
    if (RenderObject* renderer = endingSelection().start().node()->renderer())
        if (renderer->style()->collapseWhiteSpace())
            // Turn all spaces into non breaking spaces, to make sure that they are treated
            // literally, and aren't collapsed after insertion. They will be rebalanced 
            // (turned into a sequence of regular and non breaking spaces) below.
            text.replace(' ', noBreakSpace);
    
    // Delete the current selection.
    // FIXME: This delete operation blows away the typing style.
    if (endingSelection().isRange())
        deleteSelection(false, true, true, false);
    
    // Insert the character at the leftmost candidate.
    Position startPosition = endingSelection().start().upstream();
    // It is possible for the node that contains startPosition to contain only unrendered whitespace,
    // and so deleteInsignificantText could remove it.  Save the position before the node in case that happens.
    Position positionBeforeStartNode(positionBeforeNode(startPosition.node()));
    deleteInsignificantText(startPosition.upstream(), startPosition.downstream());
    if (!startPosition.node()->inDocument())
        startPosition = positionBeforeStartNode;
    if (!startPosition.isCandidate())
        startPosition = startPosition.downstream();
    
    // FIXME: This typing around anchor behavior doesn't exactly match TextEdit.  In TextEdit,
    // you won't be placed inside a link when typing after it if you've just placed the caret
    // there with the mouse.
    startPosition = positionAvoidingSpecialElementBoundary(startPosition, false);
    
    Position endPosition;
    
    if (text == "\t") {
        endPosition = insertTab(startPosition);
        startPosition = endPosition.previous();
        removePlaceholderAt(VisiblePosition(startPosition));
        m_charactersAdded += 1;
    } else {
        // Make sure the document is set up to receive text
        startPosition = prepareForTextInsertion(startPosition);
        removePlaceholderAt(VisiblePosition(startPosition));
        Text *textNode = static_cast<Text *>(startPosition.node());
        int offset = startPosition.offset();

        insertTextIntoNode(textNode, offset, text);
        endPosition = Position(textNode, offset + text.length());

        // The insertion may require adjusting adjacent whitespace, if it is present.
        rebalanceWhitespaceAt(endPosition);
        // Rebalancing on both sides isn't necessary if we've inserted a space.
        if (originalText != " ") 
            rebalanceWhitespaceAt(startPosition);
            
        m_charactersAdded += text.length();
    }

    // We could have inserted a part of composed character sequence,
    // so we are basically treating ending selection as a range to avoid validation.
    // <http://bugs.webkit.org/show_bug.cgi?id=15781>
    Selection forcedEndingSelection;
    forcedEndingSelection.setWithoutValidation(startPosition, endPosition);
    setEndingSelection(forcedEndingSelection);

    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclaration* typingStyle = document()->frame()->typingStyle();
    RefPtr<CSSComputedStyleDeclaration> endingStyle = endPosition.computedStyle();
    endingStyle->diff(typingStyle);
    if (typingStyle && typingStyle->length() > 0)
        applyStyle(typingStyle);

    if (!selectInsertedText)
        setEndingSelection(Selection(endingSelection().end(), endingSelection().affinity()));
}

Position InsertTextCommand::insertTab(const Position& pos)
{
    Position insertPos = VisiblePosition(pos, DOWNSTREAM).deepEquivalent();
        
    Node *node = insertPos.node();
    unsigned int offset = insertPos.offset();

    // keep tabs coalesced in tab span
    if (isTabSpanTextNode(node)) {
        insertTextIntoNode(static_cast<Text *>(node), offset, "\t");
        return Position(node, offset + 1);
    }
    
    // create new tab span
    RefPtr<Element> spanNode = createTabSpanElement(document());
    
    // place it
    if (!node->isTextNode()) {
        insertNodeAt(spanNode.get(), insertPos);
    } else {
        Text *textNode = static_cast<Text *>(node);
        if (offset >= textNode->length()) {
            insertNodeAfter(spanNode.get(), textNode);
        } else {
            // split node to make room for the span
            // NOTE: splitTextNode uses textNode for the
            // second node in the split, so we need to
            // insert the span before it.
            if (offset > 0)
                splitTextNode(textNode, offset);
            insertNodeBefore(spanNode.get(), textNode);
        }
    }
    
    // return the position following the new tab
    return Position(spanNode->lastChild(), caretMaxOffset(spanNode->lastChild()));
}

bool InsertTextCommand::isInsertTextCommand() const
{
    return true;
}

}
