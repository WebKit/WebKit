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
#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
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

// This avoids the expense of a full fledged delete operation, and avoids a layout that typically results
// from text removal.
bool InsertTextCommand::performTrivialReplace(const String& text, bool selectInsertedText)
{
    if (!endingSelection().isRange())
        return false;
    
    if (text.contains('\t') || text.contains(' ') || text.contains('\n'))
        return false;
    
    Position start = endingSelection().start();
    Position end = endingSelection().end();
    
    if (start.node() != end.node() || !start.node()->isTextNode() || isTabSpanTextNode(start.node()))
        return false;
        
    replaceTextInNode(static_cast<Text*>(start.node()), start.deprecatedEditingOffset(), end.deprecatedEditingOffset() - start.deprecatedEditingOffset(), text);
    
    Position endPosition(start.node(), start.deprecatedEditingOffset() + text.length());
    
    // We could have inserted a part of composed character sequence,
    // so we are basically treating ending selection as a range to avoid validation.
    // <http://bugs.webkit.org/show_bug.cgi?id=15781>
    VisibleSelection forcedEndingSelection;
    forcedEndingSelection.setWithoutValidation(start, endPosition);
    setEndingSelection(forcedEndingSelection);
    
    if (!selectInsertedText)
        setEndingSelection(VisibleSelection(endingSelection().visibleEnd()));
    
    return true;
}

void InsertTextCommand::input(const String& text, bool selectInsertedText)
{
    
    ASSERT(text.find('\n') == notFound);

    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;

    // Delete the current selection.
    // FIXME: This delete operation blows away the typing style.
    if (endingSelection().isRange()) {
        if (performTrivialReplace(text, selectInsertedText))
            return;
        deleteSelection(false, true, true, false);
    }

    Position startPosition(endingSelection().start());
    
    Position placeholder;
    // We want to remove preserved newlines and brs that will collapse (and thus become unnecessary) when content 
    // is inserted just before them.
    // FIXME: We shouldn't really have to do this, but removing placeholders is a workaround for 9661.
    // If the caret is just before a placeholder, downstream will normalize the caret to it.
    Position downstream(startPosition.downstream());
    if (lineBreakExistsAtPosition(downstream)) {
        // FIXME: This doesn't handle placeholders at the end of anonymous blocks.
        VisiblePosition caret(startPosition);
        if (isEndOfBlock(caret) && isStartOfParagraph(caret))
            placeholder = downstream;
        // Don't remove the placeholder yet, otherwise the block we're inserting into would collapse before
        // we get a chance to insert into it.  We check for a placeholder now, though, because doing so requires
        // the creation of a VisiblePosition, and if we did that post-insertion it would force a layout.
    }
    
    // Insert the character at the leftmost candidate.
    startPosition = startPosition.upstream();
    
    // It is possible for the node that contains startPosition to contain only unrendered whitespace,
    // and so deleteInsignificantText could remove it.  Save the position before the node in case that happens.
    Position positionBeforeStartNode(positionInParentBeforeNode(startPosition.node()));
    deleteInsignificantText(startPosition.upstream(), startPosition.downstream());
    if (!startPosition.node()->inDocument())
        startPosition = positionBeforeStartNode;
    if (!startPosition.isCandidate())
        startPosition = startPosition.downstream();
    
    startPosition = positionAvoidingSpecialElementBoundary(startPosition);
    
    Position endPosition;
    
    if (text == "\t") {
        endPosition = insertTab(startPosition);
        startPosition = endPosition.previous();
        if (placeholder.isNotNull())
            removePlaceholderAt(placeholder);
        m_charactersAdded += 1;
    } else {
        // Make sure the document is set up to receive text
        startPosition = prepareForTextInsertion(startPosition);
        if (placeholder.isNotNull())
            removePlaceholderAt(placeholder);
        Text *textNode = static_cast<Text *>(startPosition.node());
        int offset = startPosition.deprecatedEditingOffset();

        insertTextIntoNode(textNode, offset, text);
        endPosition = Position(textNode, offset + text.length());

        // The insertion may require adjusting adjacent whitespace, if it is present.
        rebalanceWhitespaceAt(endPosition);
        // Rebalancing on both sides isn't necessary if we've inserted a space.
        if (text != " ") 
            rebalanceWhitespaceAt(startPosition);
            
        m_charactersAdded += text.length();
    }

    // We could have inserted a part of composed character sequence,
    // so we are basically treating ending selection as a range to avoid validation.
    // <http://bugs.webkit.org/show_bug.cgi?id=15781>
    VisibleSelection forcedEndingSelection;
    forcedEndingSelection.setWithoutValidation(startPosition, endPosition);
    setEndingSelection(forcedEndingSelection);

    // Handle the case where there is a typing style.
    if (RefPtr<EditingStyle> typingStyle = document()->frame()->selection()->typingStyle()) {
        typingStyle->prepareToApplyAt(endPosition, EditingStyle::PreserveWritingDirection);
        if (!typingStyle->isEmpty())
            applyStyle(typingStyle->style());
    }

    if (!selectInsertedText)
        setEndingSelection(VisibleSelection(endingSelection().end(), endingSelection().affinity()));
}

Position InsertTextCommand::insertTab(const Position& pos)
{
    Position insertPos = VisiblePosition(pos, DOWNSTREAM).deepEquivalent();
        
    Node *node = insertPos.node();
    unsigned int offset = insertPos.deprecatedEditingOffset();

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
            insertNodeBefore(spanNode, textNode);
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
