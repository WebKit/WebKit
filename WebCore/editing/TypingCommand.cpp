/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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
#include "TypingCommand.h"

#include "BeforeTextInsertedEvent.h"
#include "BreakBlockquoteCommand.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "Frame.h"
#include "InsertLineBreakCommand.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "SelectionController.h"
#include "VisiblePosition.h"
#include "htmlediting.h"
#include "visible_units.h"

namespace WebCore {

TypingCommand::TypingCommand(Document *document, ETypingCommand commandType, const String &textToInsert, bool selectInsertedText, TextGranularity granularity)
    : CompositeEditCommand(document), 
      m_commandType(commandType), 
      m_textToInsert(textToInsert), 
      m_openForMoreTyping(true), 
      m_applyEditing(false), 
      m_selectInsertedText(selectInsertedText),
      m_smartDelete(false),
      m_granularity(granularity),
      m_openedByBackwardDelete(false)
{
}

void TypingCommand::deleteSelection(Document* document, bool smartDelete)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    if (!frame->selectionController()->isRange())
        return;
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->deleteSelection(smartDelete);
        return;
    }
    
    RefPtr<TypingCommand> typingCommand = new TypingCommand(document, DeleteSelection, "", false);
    typingCommand->setSmartDelete(smartDelete);
    typingCommand->apply();
}

void TypingCommand::deleteKeyPressed(Document *document, bool smartDelete, TextGranularity granularity)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->deleteKeyPressed(granularity);
        return;
    }
    
    RefPtr<TypingCommand> typingCommand = new TypingCommand(document, DeleteKey, "", false, granularity);
    typingCommand->setSmartDelete(smartDelete);
    typingCommand->apply();
}

void TypingCommand::forwardDeleteKeyPressed(Document *document, bool smartDelete, TextGranularity granularity)
{
    // FIXME: Forward delete in TextEdit appears to open and close a new typing command.
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->forwardDeleteKeyPressed(granularity);
        return;
    }

    RefPtr<TypingCommand> typingCommand = new TypingCommand(document, ForwardDeleteKey, "", false, granularity);
    typingCommand->setSmartDelete(smartDelete);
    typingCommand->apply();
}

void TypingCommand::insertText(Document* document, const String& text, bool selectInsertedText, bool insertedTextIsComposition)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);

    insertText(document, text, frame->selectionController()->selection(), selectInsertedText, insertedTextIsComposition);
}

void TypingCommand::insertText(Document* document, const String& text, const Selection& selectionForInsertion, bool selectInsertedText, bool insertedTextIsComposition)
{
    ASSERT(document);
    
    RefPtr<Frame> frame = document->frame();
    ASSERT(frame);
    
    Selection currentSelection = frame->selectionController()->selection();
    bool changeSelection = currentSelection != selectionForInsertion;
    
    String newText = text;
    Node* startNode = selectionForInsertion.start().node();
    
    if (startNode && startNode->rootEditableElement() && !insertedTextIsComposition) {        
        // Send BeforeTextInsertedEvent. The event handler will update text if necessary.
        ExceptionCode ec = 0;
        RefPtr<BeforeTextInsertedEvent> evt = new BeforeTextInsertedEvent(text);
        startNode->rootEditableElement()->dispatchEvent(evt, ec, true);
        newText = evt->text();
    }
    
    if (newText.isEmpty())
        return;
    
    // Set the starting and ending selection appropriately if we are using a selection
    // that is different from the current selection.  In the future, we should change EditCommand
    // to deal with custom selections in a general way that can be used by all of the commands.
    RefPtr<EditCommand> lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand.get())) {
        TypingCommand* lastTypingCommand = static_cast<TypingCommand*>(lastEditCommand.get());
        if (changeSelection) {
            lastTypingCommand->setStartingSelection(selectionForInsertion);
            lastTypingCommand->setEndingSelection(selectionForInsertion);
        }
        lastTypingCommand->insertText(newText, selectInsertedText);
        if (changeSelection) {
            lastTypingCommand->setEndingSelection(currentSelection);
            frame->selectionController()->setSelection(currentSelection);
        }
        return;
    }

    RefPtr<TypingCommand> cmd = new TypingCommand(document, InsertText, newText, selectInsertedText);
    if (changeSelection)  {
        cmd->setStartingSelection(selectionForInsertion);
        cmd->setEndingSelection(selectionForInsertion);
    }
    applyCommand(cmd);
    if (changeSelection) {
        cmd->setEndingSelection(currentSelection);
        frame->selectionController()->setSelection(currentSelection);
    }
}

void TypingCommand::insertLineBreak(Document *document)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->insertLineBreak();
        return;
    }

    applyCommand(new TypingCommand(document, InsertLineBreak));
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(Document *document)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->insertParagraphSeparatorInQuotedContent();
        return;
    }

    applyCommand(new TypingCommand(document, InsertParagraphSeparatorInQuotedContent));
}

void TypingCommand::insertParagraphSeparator(Document *document)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->insertParagraphSeparator();
        return;
    }

    applyCommand(new TypingCommand(document, InsertParagraphSeparator));
}

bool TypingCommand::isOpenForMoreTypingCommand(const EditCommand* cmd)
{
    return cmd && cmd->isTypingCommand() && static_cast<const TypingCommand*>(cmd)->isOpenForMoreTyping();
}

void TypingCommand::closeTyping(EditCommand* cmd)
{
    if (isOpenForMoreTypingCommand(cmd))
        static_cast<TypingCommand*>(cmd)->closeTyping();
}

void TypingCommand::doApply()
{
    if (endingSelection().isNone())
        return;
        
    if (m_commandType == DeleteKey)
        if (m_commands.isEmpty())
            m_openedByBackwardDelete = true;

    switch (m_commandType) {
        case DeleteSelection:
            deleteSelection(m_smartDelete);
            return;
        case DeleteKey:
            deleteKeyPressed(m_granularity);
            return;
        case ForwardDeleteKey:
            forwardDeleteKeyPressed(m_granularity);
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
    VisiblePosition start(endingSelection().start(), endingSelection().affinity());
    VisiblePosition previous = start.previous();
    if (previous.isNotNull()) {
        VisiblePosition p1 = startOfWord(previous, LeftWordIfOnBoundary);
        VisiblePosition p2 = startOfWord(start, LeftWordIfOnBoundary);
        if (p1 != p2)
            document()->frame()->editor()->markMisspellingsAfterTypingToPosition(p1);
    }
}

void TypingCommand::typingAddedToOpenCommand()
{
    markMisspellingsAfterTyping();
    // Do not apply editing to the frame on the first time through.
    // The frame will get told in the same way as all other commands.
    // But since this command stays open and is used for additional typing, 
    // we need to tell the frame here as other commands are added.
    if (m_applyEditing)
        document()->frame()->editor()->appliedEditing(this);
    m_applyEditing = true;
}

void TypingCommand::insertText(const String &text, bool selectInsertedText)
{
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    int offset = 0;
    int newline;
    while ((newline = text.find('\n', offset)) != -1) {
        if (newline != offset)
            insertTextRunWithoutNewlines(text.substring(offset, newline - offset), false);
        insertParagraphSeparator();
        offset = newline + 1;
    }
    if (offset == 0)
        insertTextRunWithoutNewlines(text, selectInsertedText);
    else {
        int length = text.length();
        if (length != offset) {
            insertTextRunWithoutNewlines(text.substring(offset, length - offset), selectInsertedText);
        }
    }
}

void TypingCommand::insertTextRunWithoutNewlines(const String &text, bool selectInsertedText)
{
    RefPtr<InsertTextCommand> command;
    if (!document()->frame()->typingStyle() && !m_commands.isEmpty()) {
        EditCommand* lastCommand = m_commands.last().get();
        if (lastCommand->isInsertTextCommand())
            command = static_cast<InsertTextCommand*>(lastCommand);
    }
    if (!command) {
        command = new InsertTextCommand(document());
        applyCommandToComposite(command);
    }
    command->input(text, selectInsertedText);
    typingAddedToOpenCommand();
}

void TypingCommand::insertLineBreak()
{
    applyCommandToComposite(new InsertLineBreakCommand(document()));
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparator()
{
    applyCommandToComposite(new InsertParagraphSeparatorCommand(document()));
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparatorInQuotedContent()
{
    applyCommandToComposite(new BreakBlockquoteCommand(document()));
    typingAddedToOpenCommand();
}

void TypingCommand::deleteKeyPressed(TextGranularity granularity)
{
    Selection selectionToDelete;
    Selection selectionAfterUndo;
    
    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            selectionAfterUndo = selectionToDelete;
            break;
        case Selection::CARET: {
            m_smartDelete = false;

            SelectionController selectionController;
            selectionController.setSelection(endingSelection());
            selectionController.modify(SelectionController::EXTEND, SelectionController::BACKWARD, granularity);
            
            // When the caret is at the start of the editable area in an empty list item, break out of the list item.
            if (endingSelection().visibleStart().previous(true).isNull()) {
                if (breakOutOfEmptyListItem()) {
                    typingAddedToOpenCommand();
                    return;
                }
            }
            
            VisiblePosition visibleStart(endingSelection().visibleStart());
            // If the caret is at the start of a paragraph after a table, move content into the last table cell.
            if (isStartOfParagraph(visibleStart) && isFirstPositionAfterTable(visibleStart.previous(true))) {
                // Unless the caret is just before a table.  We don't want to move a table into the last table cell.
                if (isLastPositionBeforeTable(visibleStart))
                    return;
                // Extend the selection backward into the last cell, then deletion will handle the move.
                selectionController.modify(SelectionController::EXTEND, SelectionController::BACKWARD, granularity);
            // If the caret is just after a table, select the table and don't delete anything.
            } else if (Node* table = isFirstPositionAfterTable(visibleStart)) {
                setEndingSelection(Selection(Position(table, 0), endingSelection().start(), DOWNSTREAM));
                typingAddedToOpenCommand();
                return;
            }

            selectionToDelete = selectionController.selection();
            if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start())
                selectionAfterUndo = selectionToDelete;
            else
                // It's a little tricky to compute what the starting selection would have been in the original document.
                // We can't let the Selection class's validation kick in or it'll adjust for us based on
                // the current state of the document and we'll get the wrong result.
                selectionAfterUndo.setWithoutValidation(startingSelection().end(), selectionToDelete.extent());
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange() && document()->frame()->shouldDeleteSelection(selectionToDelete)) {
        // Make undo select everything that has been deleted, unless an undo will undo more than just this deletion.
        // FIXME: This behaves like TextEdit except for the case where you open with text insertion and then delete
        // more text than you insert.  In that case all of the text that was around originally should be selected.
        if (m_openedByBackwardDelete)
            setStartingSelection(selectionAfterUndo);
        CompositeEditCommand::deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
}

void TypingCommand::forwardDeleteKeyPressed(TextGranularity granularity)
{
    Selection selectionToDelete;
    Selection selectionAfterUndo;

    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            selectionAfterUndo = selectionToDelete;
            break;
        case Selection::CARET: {
            m_smartDelete = false;

            // Handle delete at beginning-of-block case.
            // Do nothing in the case that the caret is at the start of a
            // root editable element or at the start of a document.
            SelectionController selectionController;
            selectionController.setSelection(endingSelection());
            selectionController.modify(SelectionController::EXTEND, SelectionController::FORWARD, granularity);
            Position downstreamEnd = endingSelection().end().downstream();
            VisiblePosition visibleEnd = endingSelection().visibleEnd();
            if (visibleEnd == endOfParagraph(visibleEnd))
                downstreamEnd = visibleEnd.next(true).deepEquivalent().downstream();
            // When deleting tables: Select the table first, then perform the deletion
            if (downstreamEnd.node() && downstreamEnd.node()->renderer() && downstreamEnd.node()->renderer()->isTable() && downstreamEnd.offset() == 0) {
                setEndingSelection(Selection(endingSelection().end(), Position(downstreamEnd.node(), maxDeepOffset(downstreamEnd.node())), DOWNSTREAM));
                typingAddedToOpenCommand();
                return;
            }

            // deleting to end of paragraph when at end of paragraph needs to merge the next paragraph (if any)
            if (granularity == ParagraphBoundary && selectionController.selection().isCaret() && isEndOfParagraph(selectionController.selection().visibleEnd()))
                selectionController.modify(SelectionController::EXTEND, SelectionController::FORWARD, CharacterGranularity);

            selectionToDelete = selectionController.selection();
            if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start())
                selectionAfterUndo = selectionToDelete;
            else {
                // It's a little tricky to compute what the starting selection would have been in the original document.
                // We can't let the Selection class's validation kick in or it'll adjust for us based on
                // the current state of the document and we'll get the wrong result.
                Position extent = startingSelection().end();
                if (extent.node() != selectionToDelete.end().node())
                    extent = selectionToDelete.extent();
                else {
                    int extraCharacters;
                    if (selectionToDelete.start().node() == selectionToDelete.end().node())
                        extraCharacters = selectionToDelete.end().offset() - selectionToDelete.start().offset();
                    else
                        extraCharacters = selectionToDelete.end().offset();
                    extent = Position(extent.node(), extent.offset() + extraCharacters);
                }
                selectionAfterUndo.setWithoutValidation(startingSelection().start(), extent);
            }
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange() && document()->frame()->shouldDeleteSelection(selectionToDelete)) {
        // make undo select what was deleted
        setStartingSelection(selectionAfterUndo);
        CompositeEditCommand::deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
}

void TypingCommand::deleteSelection(bool smartDelete)
{
    CompositeEditCommand::deleteSelection(smartDelete);
    typingAddedToOpenCommand();
}

bool TypingCommand::preservesTypingStyle() const
{
    switch (m_commandType) {
        case DeleteSelection:
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

} // namespace WebCore
