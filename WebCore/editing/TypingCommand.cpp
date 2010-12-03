/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc.  All rights reserved.
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
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "RenderObject.h"
#include "SelectionController.h"
#include "VisiblePosition.h"
#include "htmlediting.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

TypingCommand::TypingCommand(Document *document, ETypingCommand commandType, const String &textToInsert, bool selectInsertedText, TextGranularity granularity, bool killRing)
    : CompositeEditCommand(document), 
      m_commandType(commandType), 
      m_textToInsert(textToInsert), 
      m_openForMoreTyping(true), 
      m_selectInsertedText(selectInsertedText),
      m_smartDelete(false),
      m_granularity(granularity),
      m_killRing(killRing),
      m_openedByBackwardDelete(false)
{
    updatePreservesTypingStyle(m_commandType);
}

void TypingCommand::deleteSelection(Document* document, bool smartDelete)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    if (!frame->selection()->isRange())
        return;
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->deleteSelection(smartDelete);
        return;
    }
    
    RefPtr<TypingCommand> typingCommand = TypingCommand::create(document, DeleteSelection, "", false);
    typingCommand->setSmartDelete(smartDelete);
    typingCommand->apply();
}

void TypingCommand::deleteKeyPressed(Document *document, bool smartDelete, TextGranularity granularity, bool killRing)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (granularity == CharacterGranularity && isOpenForMoreTypingCommand(lastEditCommand)) {
        updateSelectionIfDifferentFromCurrentSelection(static_cast<TypingCommand*>(lastEditCommand), frame);
        static_cast<TypingCommand*>(lastEditCommand)->deleteKeyPressed(granularity, killRing);
        return;
    }
    
    RefPtr<TypingCommand> typingCommand = TypingCommand::create(document, DeleteKey, "", false, granularity, killRing);
    typingCommand->setSmartDelete(smartDelete);
    typingCommand->apply();
}

void TypingCommand::forwardDeleteKeyPressed(Document *document, bool smartDelete, TextGranularity granularity, bool killRing)
{
    // FIXME: Forward delete in TextEdit appears to open and close a new typing command.
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (granularity == CharacterGranularity && isOpenForMoreTypingCommand(lastEditCommand)) {
        updateSelectionIfDifferentFromCurrentSelection(static_cast<TypingCommand*>(lastEditCommand), frame);
        static_cast<TypingCommand*>(lastEditCommand)->forwardDeleteKeyPressed(granularity, killRing);
        return;
    }

    RefPtr<TypingCommand> typingCommand = TypingCommand::create(document, ForwardDeleteKey, "", false, granularity, killRing);
    typingCommand->setSmartDelete(smartDelete);
    typingCommand->apply();
}

void TypingCommand::updateSelectionIfDifferentFromCurrentSelection(TypingCommand* typingCommand, Frame* frame)
{
    ASSERT(frame);
    VisibleSelection currentSelection = frame->selection()->selection();
    if (currentSelection == typingCommand->endingSelection())
        return;
    
    typingCommand->setStartingSelection(currentSelection);
    typingCommand->setEndingSelection(currentSelection);
}
    

void TypingCommand::insertText(Document* document, const String& text, bool selectInsertedText, bool insertedTextIsComposition)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);

    insertText(document, text, frame->selection()->selection(), selectInsertedText, insertedTextIsComposition);
}

// FIXME: We shouldn't need to take selectionForInsertion. It should be identical to SelectionController's current selection.
void TypingCommand::insertText(Document* document, const String& text, const VisibleSelection& selectionForInsertion, bool selectInsertedText, bool insertedTextIsComposition)
{
#if REMOVE_MARKERS_UPON_EDITING
    if (!text.isEmpty())
        document->frame()->editor()->removeSpellAndCorrectionMarkersFromWordsToBeEdited(isSpaceOrNewline(text.characters()[0]));
#endif

    ASSERT(document);

    RefPtr<Frame> frame = document->frame();
    ASSERT(frame);

    VisibleSelection currentSelection = frame->selection()->selection();
    bool changeSelection = currentSelection != selectionForInsertion;
    String newText = text;
    Node* startNode = selectionForInsertion.start().node();
    
    if (startNode && startNode->rootEditableElement() && !insertedTextIsComposition) {        
        // Send BeforeTextInsertedEvent. The event handler will update text if necessary.
        ExceptionCode ec = 0;
        RefPtr<BeforeTextInsertedEvent> evt = BeforeTextInsertedEvent::create(text);
        startNode->rootEditableElement()->dispatchEvent(evt, ec);
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
        if (lastTypingCommand->endingSelection() != selectionForInsertion) {
            lastTypingCommand->setStartingSelection(selectionForInsertion);
            lastTypingCommand->setEndingSelection(selectionForInsertion);
        }
        lastTypingCommand->insertText(newText, selectInsertedText);
        return;
    }

    RefPtr<TypingCommand> cmd = TypingCommand::create(document, InsertText, newText, selectInsertedText);
    if (changeSelection)  {
        cmd->setStartingSelection(selectionForInsertion);
        cmd->setEndingSelection(selectionForInsertion);
    }
    applyCommand(cmd);
    if (changeSelection) {
        cmd->setEndingSelection(currentSelection);
        frame->selection()->setSelection(currentSelection);
    }
}

void TypingCommand::insertLineBreak(Document *document)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->insertLineBreak();
        return;
    }

    applyCommand(TypingCommand::create(document, InsertLineBreak));
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(Document *document)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->insertParagraphSeparatorInQuotedContent();
        return;
    }

    applyCommand(TypingCommand::create(document, InsertParagraphSeparatorInQuotedContent));
}

void TypingCommand::insertParagraphSeparator(Document *document)
{
    ASSERT(document);
    
    Frame* frame = document->frame();
    ASSERT(frame);
    
    EditCommand* lastEditCommand = frame->editor()->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand*>(lastEditCommand)->insertParagraphSeparator();
        return;
    }

    applyCommand(TypingCommand::create(document, InsertParagraphSeparator));
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
    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;
        
    if (m_commandType == DeleteKey)
        if (m_commands.isEmpty())
            m_openedByBackwardDelete = true;

    switch (m_commandType) {
    case DeleteSelection:
        deleteSelection(m_smartDelete);
        return;
    case DeleteKey:
        deleteKeyPressed(m_granularity, m_killRing);
        return;
    case ForwardDeleteKey:
        forwardDeleteKeyPressed(m_granularity, m_killRing);
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

void TypingCommand::markMisspellingsAfterTyping(ETypingCommand commandType)
{
#if PLATFORM(MAC) && !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    if (!document()->frame()->editor()->isContinuousSpellCheckingEnabled()
     && !document()->frame()->editor()->isAutomaticQuoteSubstitutionEnabled()
     && !document()->frame()->editor()->isAutomaticLinkDetectionEnabled()
     && !document()->frame()->editor()->isAutomaticDashSubstitutionEnabled()
     && !document()->frame()->editor()->isAutomaticTextReplacementEnabled())
        return;
#else
    if (!document()->frame()->editor()->isContinuousSpellCheckingEnabled())
        return;
#endif
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
            document()->frame()->editor()->markMisspellingsAfterTypingToWord(p1, endingSelection());
#if SUPPORT_AUTOCORRECTION_PANEL
        else if (commandType == TypingCommand::InsertText)
            document()->frame()->editor()->startCorrectionPanelTimer(CorrectionPanelInfo::PanelTypeCorrection);
#else
    UNUSED_PARAM(commandType);
#endif
    }
}

void TypingCommand::typingAddedToOpenCommand(ETypingCommand commandTypeForAddedTyping)
{
    updatePreservesTypingStyle(commandTypeForAddedTyping);

#if PLATFORM(MAC) && !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    document()->frame()->editor()->appliedEditing(this);
    // Since the spellchecking code may also perform corrections and other replacements, it should happen after the typing changes.
    markMisspellingsAfterTyping(commandTypeForAddedTyping);
#else
    // The old spellchecking code requires that checking be done first, to prevent issues like that in 6864072, where <doesn't> is marked as misspelled.
    markMisspellingsAfterTyping(commandTypeForAddedTyping);
    document()->frame()->editor()->appliedEditing(this);
#endif
}

void TypingCommand::insertText(const String &text, bool selectInsertedText)
{
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    unsigned offset = 0;
    size_t newline;
    while ((newline = text.find('\n', offset)) != notFound) {
        if (newline != offset)
            insertTextRunWithoutNewlines(text.substring(offset, newline - offset), false);
        insertParagraphSeparator();
        offset = newline + 1;
    }
    if (!offset)
        insertTextRunWithoutNewlines(text, selectInsertedText);
    else {
        unsigned length = text.length();
        if (length != offset)
            insertTextRunWithoutNewlines(text.substring(offset, length - offset), selectInsertedText);
    }
}

void TypingCommand::insertTextRunWithoutNewlines(const String &text, bool selectInsertedText)
{
    RefPtr<InsertTextCommand> command;
    if (!document()->frame()->selection()->typingStyle() && !m_commands.isEmpty()) {
        EditCommand* lastCommand = m_commands.last().get();
        if (lastCommand->isInsertTextCommand())
            command = static_cast<InsertTextCommand*>(lastCommand);
    }
    if (!command) {
        command = InsertTextCommand::create(document());
        applyCommandToComposite(command);
    }
    if (endingSelection() != command->endingSelection()) {
        command->setStartingSelection(endingSelection());
        command->setEndingSelection(endingSelection());
    }
    command->input(text, selectInsertedText);
    typingAddedToOpenCommand(InsertText);
}

void TypingCommand::insertLineBreak()
{
    applyCommandToComposite(InsertLineBreakCommand::create(document()));
    typingAddedToOpenCommand(InsertLineBreak);
}

void TypingCommand::insertParagraphSeparator()
{
    applyCommandToComposite(InsertParagraphSeparatorCommand::create(document()));
    typingAddedToOpenCommand(InsertParagraphSeparator);
}

void TypingCommand::insertParagraphSeparatorInQuotedContent()
{
    // If the selection starts inside a table, just insert the paragraph separator normally
    // Breaking the blockquote would also break apart the table, which is unecessary when inserting a newline
    if (enclosingNodeOfType(endingSelection().start(), &isTableStructureNode)) {
        insertParagraphSeparator();
        return;
    }
        
    applyCommandToComposite(BreakBlockquoteCommand::create(document()));
    typingAddedToOpenCommand(InsertParagraphSeparatorInQuotedContent);
}

bool TypingCommand::makeEditableRootEmpty()
{
    Element* root = endingSelection().rootEditableElement();
    if (!root->firstChild())
        return false;

    if (root->firstChild() == root->lastChild() && root->firstElementChild() && root->firstElementChild()->hasTagName(brTag)) {
        // If there is a single child and it could be a placeholder, leave it alone.
        if (root->renderer() && root->renderer()->isBlockFlow())
            return false;
    }

    while (Node* child = root->firstChild())
        removeNode(child);

    addBlockPlaceholderIfNeeded(root);
    setEndingSelection(VisibleSelection(Position(root, 0), DOWNSTREAM));

    return true;
}

void TypingCommand::deleteKeyPressed(TextGranularity granularity, bool killRing)
{
#if REMOVE_MARKERS_UPON_EDITING
    document()->frame()->editor()->removeSpellAndCorrectionMarkersFromWordsToBeEdited(false);
#endif
    VisibleSelection selectionToDelete;
    VisibleSelection selectionAfterUndo;

    switch (endingSelection().selectionType()) {
    case VisibleSelection::RangeSelection:
        selectionToDelete = endingSelection();
        selectionAfterUndo = selectionToDelete;
        break;
    case VisibleSelection::CaretSelection: {
        // After breaking out of an empty mail blockquote, we still want continue with the deletion
        // so actual content will get deleted, and not just the quote style.
        if (breakOutOfEmptyMailBlockquotedParagraph())
            typingAddedToOpenCommand(DeleteKey);

        m_smartDelete = false;

        SelectionController selection;
        selection.setSelection(endingSelection());
        selection.modify(SelectionController::AlterationExtend, SelectionController::DirectionBackward, granularity);
        if (killRing && selection.isCaret() && granularity != CharacterGranularity)
            selection.modify(SelectionController::AlterationExtend, SelectionController::DirectionBackward, CharacterGranularity);

        if (endingSelection().visibleStart().previous(true).isNull()) {
            // When the caret is at the start of the editable area in an empty list item, break out of the list item.
            if (breakOutOfEmptyListItem()) {
                typingAddedToOpenCommand(DeleteKey);
                return;
            }
            // When there are no visible positions in the editing root, delete its entire contents.
            if (endingSelection().visibleStart().next(true).isNull() && makeEditableRootEmpty()) {
                typingAddedToOpenCommand(DeleteKey);
                return;
            }
        }

        VisiblePosition visibleStart(endingSelection().visibleStart());
        // If we have a caret selection on an empty cell, we have nothing to do.
        if (isEmptyTableCell(visibleStart.deepEquivalent().node()))
            return;

        // If the caret is at the start of a paragraph after a table, move content into the last table cell.
        if (isStartOfParagraph(visibleStart) && isFirstPositionAfterTable(visibleStart.previous(true))) {
            // Unless the caret is just before a table.  We don't want to move a table into the last table cell.
            if (isLastPositionBeforeTable(visibleStart))
                return;
            // Extend the selection backward into the last cell, then deletion will handle the move.
            selection.modify(SelectionController::AlterationExtend, SelectionController::DirectionBackward, granularity);
        // If the caret is just after a table, select the table and don't delete anything.
        } else if (Node* table = isFirstPositionAfterTable(visibleStart)) {
            setEndingSelection(VisibleSelection(Position(table, 0), endingSelection().start(), DOWNSTREAM));
            typingAddedToOpenCommand(DeleteKey);
            return;
        }

        selectionToDelete = selection.selection();

        if (granularity == CharacterGranularity && selectionToDelete.end().node() == selectionToDelete.start().node() && selectionToDelete.end().deprecatedEditingOffset() - selectionToDelete.start().deprecatedEditingOffset() > 1) {
            // If there are multiple Unicode code points to be deleted, adjust the range to match platform conventions.
            selectionToDelete.setWithoutValidation(selectionToDelete.end(), selectionToDelete.end().previous(BackwardDeletion));
        }

        if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start())
            selectionAfterUndo = selectionToDelete;
        else
            // It's a little tricky to compute what the starting selection would have been in the original document.
            // We can't let the VisibleSelection class's validation kick in or it'll adjust for us based on
            // the current state of the document and we'll get the wrong result.
            selectionAfterUndo.setWithoutValidation(startingSelection().end(), selectionToDelete.extent());
        break;
    }
    case VisibleSelection::NoSelection:
        ASSERT_NOT_REACHED();
        break;
    }
    
    ASSERT(!selectionToDelete.isNone());
    if (selectionToDelete.isNone())
        return;
    
    if (selectionToDelete.isCaret() || !document()->frame()->selection()->shouldDeleteSelection(selectionToDelete))
        return;
    
    if (killRing)
        document()->frame()->editor()->addToKillRing(selectionToDelete.toNormalizedRange().get(), false);
    // Make undo select everything that has been deleted, unless an undo will undo more than just this deletion.
    // FIXME: This behaves like TextEdit except for the case where you open with text insertion and then delete
    // more text than you insert.  In that case all of the text that was around originally should be selected.
    if (m_openedByBackwardDelete)
        setStartingSelection(selectionAfterUndo);
    CompositeEditCommand::deleteSelection(selectionToDelete, m_smartDelete);
    setSmartDelete(false);
    typingAddedToOpenCommand(DeleteKey);
}

void TypingCommand::forwardDeleteKeyPressed(TextGranularity granularity, bool killRing)
{
#if REMOVE_MARKERS_UPON_EDITING
    document()->frame()->editor()->removeSpellAndCorrectionMarkersFromWordsToBeEdited(false);
#endif
    VisibleSelection selectionToDelete;
    VisibleSelection selectionAfterUndo;

    switch (endingSelection().selectionType()) {
    case VisibleSelection::RangeSelection:
        selectionToDelete = endingSelection();
        selectionAfterUndo = selectionToDelete;
        break;
    case VisibleSelection::CaretSelection: {
        m_smartDelete = false;

        // Handle delete at beginning-of-block case.
        // Do nothing in the case that the caret is at the start of a
        // root editable element or at the start of a document.
        SelectionController selection;
        selection.setSelection(endingSelection());
        selection.modify(SelectionController::AlterationExtend, SelectionController::DirectionForward, granularity);
        if (killRing && selection.isCaret() && granularity != CharacterGranularity)
            selection.modify(SelectionController::AlterationExtend, SelectionController::DirectionForward, CharacterGranularity);

        Position downstreamEnd = endingSelection().end().downstream();
        VisiblePosition visibleEnd = endingSelection().visibleEnd();
        if (visibleEnd == endOfParagraph(visibleEnd))
            downstreamEnd = visibleEnd.next(true).deepEquivalent().downstream();
        // When deleting tables: Select the table first, then perform the deletion
        if (downstreamEnd.node() && downstreamEnd.node()->renderer() && downstreamEnd.node()->renderer()->isTable() && !downstreamEnd.deprecatedEditingOffset()) {
            setEndingSelection(VisibleSelection(endingSelection().end(), lastDeepEditingPositionForNode(downstreamEnd.node()), DOWNSTREAM));
            typingAddedToOpenCommand(ForwardDeleteKey);
            return;
        }

        // deleting to end of paragraph when at end of paragraph needs to merge the next paragraph (if any)
        if (granularity == ParagraphBoundary && selection.selection().isCaret() && isEndOfParagraph(selection.selection().visibleEnd()))
            selection.modify(SelectionController::AlterationExtend, SelectionController::DirectionForward, CharacterGranularity);

        selectionToDelete = selection.selection();
        if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start())
            selectionAfterUndo = selectionToDelete;
        else {
            // It's a little tricky to compute what the starting selection would have been in the original document.
            // We can't let the VisibleSelection class's validation kick in or it'll adjust for us based on
            // the current state of the document and we'll get the wrong result.
            Position extent = startingSelection().end();
            if (extent.node() != selectionToDelete.end().node())
                extent = selectionToDelete.extent();
            else {
                int extraCharacters;
                if (selectionToDelete.start().node() == selectionToDelete.end().node())
                    extraCharacters = selectionToDelete.end().deprecatedEditingOffset() - selectionToDelete.start().deprecatedEditingOffset();
                else
                    extraCharacters = selectionToDelete.end().deprecatedEditingOffset();
                extent = Position(extent.node(), extent.deprecatedEditingOffset() + extraCharacters);
            }
            selectionAfterUndo.setWithoutValidation(startingSelection().start(), extent);
        }
        break;
    }
    case VisibleSelection::NoSelection:
        ASSERT_NOT_REACHED();
        break;
    }
    
    ASSERT(!selectionToDelete.isNone());
    if (selectionToDelete.isNone())
        return;
    
    if (selectionToDelete.isCaret() || !document()->frame()->selection()->shouldDeleteSelection(selectionToDelete))
        return;
        
    if (killRing)
        document()->frame()->editor()->addToKillRing(selectionToDelete.toNormalizedRange().get(), false);
    // make undo select what was deleted
    setStartingSelection(selectionAfterUndo);
    CompositeEditCommand::deleteSelection(selectionToDelete, m_smartDelete);
    setSmartDelete(false);
    typingAddedToOpenCommand(ForwardDeleteKey);
}

void TypingCommand::deleteSelection(bool smartDelete)
{
    CompositeEditCommand::deleteSelection(smartDelete);
    typingAddedToOpenCommand(DeleteSelection);
}

void TypingCommand::updatePreservesTypingStyle(ETypingCommand commandType)
{
    switch (commandType) {
    case DeleteSelection:
    case DeleteKey:
    case ForwardDeleteKey:
    case InsertParagraphSeparator:
    case InsertLineBreak:
        m_preservesTypingStyle = true;
        return;
    case InsertParagraphSeparatorInQuotedContent:
    case InsertText:
        m_preservesTypingStyle = false;
        return;
    }
    ASSERT_NOT_REACHED();
    m_preservesTypingStyle = false;
}

bool TypingCommand::isTypingCommand() const
{
    return true;
}

} // namespace WebCore
