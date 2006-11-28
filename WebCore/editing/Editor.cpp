/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Editor.h"

#include "ApplyStyleCommand.h"
#include "AXObjectCache.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "DeleteButtonController.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "IndentOutdentCommand.h"
#include "Page.h"
#include "Range.h"
#include "ReplaceSelectionCommand.h"
#include "SelectionController.h"
#include "Sound.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "markup.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

// implement as platform-specific
static Pasteboard generalPasteboard()
{
    return 0;
}

EditorClient* Editor::client() const
{
    if (Page* page = m_frame->page())
        return page->editorClient();
    return 0;
}

bool Editor::canEdit() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isCaretOrRange() && selectionController->isContentEditable();
}

bool Editor::canEditRichly() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return canEdit() && selectionController->isContentRichlyEditable();
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

bool Editor::canCopy() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && !selectionController->isInPasswordField();
}

bool Editor::canPaste() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isCaretOrRange() && selectionController->isContentEditable();
}

bool Editor::canDelete() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && selectionController->isContentEditable();
}

bool Editor::canDeleteRange(Range* range) const
{
    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    Node* endContainer = range->endContainer(ec);
    if (!startContainer || !endContainer)
        return false;
    
    if (!startContainer->isContentEditable() || !endContainer->isContentEditable())
        return false;
    
    if (range->collapsed(ec)) {
        VisiblePosition start(startContainer, range->startOffset(ec), DOWNSTREAM);
        VisiblePosition previous = start.previous();
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().node()->rootEditableElement() != startContainer->rootEditableElement())
            return false;
    }
    return true;
}

bool Editor::canSmartCopyOrDelete()
{
    return false;
}

void Editor::deleteSelection()
{
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete)
{
    if (m_frame->selectionController()->isNone())
        return;
    
    applyCommand(new DeleteSelectionCommand(m_frame->document(), smartDelete));
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard pasteboard)
{
}

void Editor::pasteWithPasteboard(Pasteboard pasteboard, bool allowPlainText)
{
}

Range* Editor::selectedRange()
{
    return 0;
}

bool Editor::shouldDeleteRange(Range* range) const
{
    ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return false;
    
    if (!canDeleteRange(range))
        return false;

    if (client())
        return client()->shouldDeleteRange(range);
    return false;
 }

bool Editor::tryDHTMLCopy()
{
    bool handled = false;
    return handled;
}

bool Editor::tryDHTMLCut()
{
    bool handled = false;
    return handled;
}

bool Editor::tryDHTMLPaste()
{
    bool handled = false;
    return handled;
}

void Editor::writeSelectionToPasteboard(Pasteboard pasteboard)
{
}

bool Editor::shouldInsertText(String text, Range* range, EditorInsertAction action) const
{
    if (client())
        return client()->shouldInsertText(text, range, action);
    return false;
}

bool Editor::shouldShowDeleteInterface(HTMLElement* element) const
{
    if (client())
        return client()->shouldShowDeleteInterface(element);
    return false;
}

void Editor::respondToChangedSelection(const Selection& oldSelection)
{
    m_deleteButtonController->respondToChangedSelection(oldSelection);
}

void Editor::respondToChangedContents(const Selection& endingSelection)
{
    if (AXObjectCache::accessibilityEnabled()) {
        Node* node = endingSelection.start().node();
        if (node)
            m_frame->renderer()->document()->axObjectCache()->postNotification(node->renderer(), "AXValueChanged");
    }
    
    if (client())
        client()->respondToChangedContents();  
    m_deleteButtonController->respondToChangedContents();
}

Frame::TriState Editor::selectionUnorderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        Node* selectionNode = m_frame->selectionController()->selection().start().node();
        if (enclosingNodeWithTag(selectionNode, ulTag))
            return Frame::trueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start().node(), ulTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end().node(), ulTag);
        if (startNode && endNode && startNode == endNode)
            return Frame::trueTriState;
    }

    return Frame::falseTriState;
}

Frame::TriState Editor::selectionOrderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        Node* selectionNode = m_frame->selectionController()->selection().start().node();
        if (enclosingNodeWithTag(selectionNode, olTag))
            return Frame::trueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start().node(), olTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end().node(), olTag);
        if (startNode && endNode && startNode == endNode)
            return Frame::trueTriState;
    }

    return Frame::falseTriState;
}

void Editor::removeFormattingAndStyle()
{
    Document* document = m_frame->document();
    
    // Make a plain text string from the selection to remove formatting like tables and lists.
    RefPtr<DocumentFragment> text = createFragmentFromText(m_frame->selectionController()->toRange().get(), m_frame->selectionController()->toString());
    
    // Put the fragment made from that string into a style span with the document's
    // default style to make sure that it is unstyled regardless of where it is inserted.
    Position pos(document->documentElement(), 0);
    RefPtr<CSSComputedStyleDeclaration> computedStyle = pos.computedStyle();
    RefPtr<CSSMutableStyleDeclaration> defaultStyle = computedStyle->copyInheritableProperties();
    
    RefPtr<Element> span = createStyleSpanElement(document);
    span->setAttribute(styleAttr, defaultStyle->cssText());
    
    ExceptionCode ec;
    
    while (text->lastChild())
        span->appendChild(text->lastChild(), ec);
    
    RefPtr<DocumentFragment> fragment = new DocumentFragment(document);
    fragment->appendChild(span, ec);
    
    applyCommand(new ReplaceSelectionCommand(document, fragment, false, false, false, true, EditActionUnspecified));
}

void Editor::setLastEditCommand(PassRefPtr<EditCommand> lastEditCommand) 
{
    m_lastEditCommand = lastEditCommand;
}

void Editor::applyStyle(CSSStyleDeclaration *style, EditAction editingAction)
{
    switch (m_frame->selectionController()->state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET: {
            m_frame->computeAndSetTypingStyle(style, editingAction);
            break;
        }
        case Selection::RANGE:
            if (m_frame->document() && style)
                applyCommand(new ApplyStyleCommand(m_frame->document(), style, editingAction));
            break;
    }
}

void Editor::applyParagraphStyle(CSSStyleDeclaration *style, EditAction editingAction)
{
    switch (m_frame->selectionController()->state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET:
        case Selection::RANGE:
            if (m_frame->document() && style)
                applyCommand(new ApplyStyleCommand(m_frame->document(), style, editingAction, ApplyStyleCommand::ForceBlockProperties));
            break;
    }
}

void Editor::applyStyleToSelection(CSSStyleDeclaration* style, EditAction editingAction)
{
    if (!style || style->length() == 0 || !canEditRichly())
        return;

    if (client() && client()->shouldApplyStyle(style, m_frame->selectionController()->toRange().get()))
        applyStyle(style, editingAction);
}

void Editor::applyParagraphStyleToSelection(CSSStyleDeclaration* style, EditAction editingAction)
{
    if (!style || style->length() == 0 || !canEditRichly())
        return;
    
    if (client() && client()->shouldApplyStyle(style, m_frame->selectionController()->toRange().get()))
        applyParagraphStyle(style, editingAction);
}

bool Editor::selectWordBeforeMenuEvent() const
{
    if (client())
        return client()->selectWordBeforeMenuEvent();
    return false;
}

bool Editor::clientIsEditable() const
{
    if (client())
        return client()->isEditable();
    return false;
}

bool Editor::selectionStartHasStyle(CSSStyleDeclaration* style) const
{
    Node* nodeToRemove;
    RefPtr<CSSStyleDeclaration> selectionStyle = m_frame->selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return false;
    
    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();
    
    bool match = true;
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = mutableStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        if (!equalIgnoringCase(mutableStyle->getPropertyValue(propertyID), selectionStyle->getPropertyValue(propertyID))) {
            match = false;
            break;
        }
    }
    
    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        assert(ec == 0);
    }
    
    return match;
}

void Editor::indent()
{
    applyCommand(new IndentOutdentCommand(m_frame->document(), IndentOutdentCommand::Indent));
}

void Editor::outdent()
{
    applyCommand(new IndentOutdentCommand(m_frame->document(), IndentOutdentCommand::Outdent));
}

static void dispatchEditableContentChangedEvents(const EditCommand& command)
{
    Element* startRoot = command.startingRootEditableElement();
    Element* endRoot = command.endingRootEditableElement();
    ExceptionCode ec;
    if (startRoot)
        startRoot->dispatchEvent(new Event(khtmlEditableContentChangedEvent, false, false), ec, true);
    if (endRoot && endRoot != startRoot)
        endRoot->dispatchEvent(new Event(khtmlEditableContentChangedEvent, false, false), ec, true);
}

void Editor::appliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
    
    // FIXME: We shouldn't tell setSelection to clear the typing style or removed anchor here.
    // If we didn't, we wouldn't have to save/restore the removedAnchor, and we wouldn't have to have
    // the typing style stored in two places (the Frame and commands).
    RefPtr<Node> anchor = m_frame->editor()->removedAnchor();
    
    Selection newSelection(cmd->endingSelection());
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, false);
    
    m_frame->editor()->setRemovedAnchor(anchor);
    
    // Now set the typing style from the command. Clear it when done.
    // This helps make the case work where you completely delete a piece
    // of styled text and then type a character immediately after.
    // That new character needs to take on the style of the just-deleted text.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (cmd->typingStyle()) {
        m_frame->setTypingStyle(cmd->typingStyle());
        cmd->setTypingStyle(0);
    }
    
    // Command will be equal to last edit command only in the case of typing
    if (m_lastEditCommand.get() == cmd)
        ASSERT(cmd->isTypingCommand());
    else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        m_lastEditCommand = cmd;
        if (client())
            client()->registerCommandForUndo(m_lastEditCommand);
    }
    respondToChangedContents(newSelection);    
}

void Editor::unappliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
    
    Selection newSelection(cmd->startingSelection());
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, true);
    
    m_lastEditCommand = 0;
    if (client())
        client()->registerCommandForRedo(cmd);
    respondToChangedContents(newSelection);    
}

void Editor::reappliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
    
    Selection newSelection(cmd->endingSelection());
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, true);
    
    m_lastEditCommand = 0;
    if (client())
        client()->registerCommandForUndo(cmd);
    respondToChangedContents(newSelection);    
}

// Execute command functions

bool execCopy(Frame* frame)
{
    frame->copyToPasteboard();
    return true;
}

bool execCut(Frame* frame)
{
    frame->cutToPasteboard();
    return true;
}

static bool execDelete(Frame* frame)
{
    TypingCommand::deleteKeyPressed(frame->document(), frame->selectionGranularity() == WordGranularity);
    return true;
}

static bool execForwardDelete(Frame* frame)
{
    TypingCommand::forwardDeleteKeyPressed(frame->document());
    return true;
}

static bool execMoveBackward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveBackwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveDown(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

static bool execMoveDownAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

static bool execMoveForward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveForwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveLeft(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

static bool execMoveLeftAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

static bool execMoveRight(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

static bool execMoveRightAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

static bool execMoveToBeginningOfDocument(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToBeginningOfDocumentAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToBeginningOfSentence(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToBeginningOfSentenceAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToBeginningOfLine(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineBoundary, true);
    return true;
}

static bool execMoveToBeginningOfLineAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineBoundary, true);
    return true;
}

static bool execMoveToBeginningOfParagraph(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveToBeginningOfParagraphAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveToEndOfDocument(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToEndOfDocumentAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToEndOfSentence(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToEndOfSentenceAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToEndOfLine(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineBoundary, true);
    return true;
}

static bool execMoveToEndOfLineAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineBoundary, true);
    return true;
}

static bool execMoveToEndOfParagraph(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveToEndOfParagraphAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveParagraphBackwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, ParagraphGranularity, true);
    return true;
}

static bool execMoveParagraphForwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, ParagraphGranularity, true);
    return true;
}

static bool execMoveUp(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

static bool execMoveUpAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

static bool execMoveWordBackward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordBackwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordForward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordForwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordLeft(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::LEFT, WordGranularity, true);
    return true;
}

static bool execMoveWordLeftAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::LEFT, WordGranularity, true);
    return true;
}

static bool execMoveWordRight(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::RIGHT, WordGranularity, true);
    return true;
}

static bool execMoveWordRightAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::RIGHT, WordGranularity, true);
    return true;
}

static bool execPaste(Frame* frame)
{
    frame->pasteFromPasteboard();
    return true;
}

static bool execSelectAll(Frame* frame)
{
    frame->selectionController()->selectAll();
    return true;
}

static bool execToggleBold(Frame* frame)
{
    ExceptionCode ec;
    
    RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
    style->setProperty(CSS_PROP_FONT_WEIGHT, "bold", false, ec);
    if (frame->editor()->selectionStartHasStyle(style.get()))
        style->setProperty(CSS_PROP_FONT_WEIGHT, "normal", false, ec);
    frame->editor()->applyStyleToSelection(style.get(), EditActionSetFont);
    return true;
}

static bool execToggleItalic(Frame* frame)
{
    ExceptionCode ec;
    
    RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
    style->setProperty(CSS_PROP_FONT_STYLE, "italic", false, ec);
    if (frame->editor()->selectionStartHasStyle(style.get()))
        style->setProperty(CSS_PROP_FONT_STYLE, "normal", false, ec);
    frame->editor()->applyStyleToSelection(style.get(), EditActionSetFont);
    return true;
}

static bool execRedo(Frame* frame)
{
    frame->editor()->client()->redo();
    return true;
}

static bool execUndo(Frame* frame)
{
    frame->editor()->client()->undo();
    return true;
}

// Enabled functions

static bool enabled(Frame*)
{
    return true;
}

static bool canPaste(Frame* frame)
{
    return frame->editor()->canPaste();
}

static bool hasEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentEditable();
}

static bool hasEditableRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange() && frame->selectionController()->isContentEditable();
}

static bool hasRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange();
}

static bool hasRichlyEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentRichlyEditable();
}

static bool canRedo(Frame* frame)
{
    return frame->editor()->client()->canRedo();
}

static bool canUndo(Frame* frame)
{
    return frame->editor()->client()->canUndo();
}

struct Command {
    bool (*enabled)(Frame* frame);
    bool (*exec)(Frame* frame);
};

typedef HashMap<String, const Command*> CommandMap;

static CommandMap* createCommandMap()
{
    struct CommandEntry { const char* name; Command command; };
    
    static const CommandEntry commands[] = {
        { "Copy", { hasRangeSelection, execCopy } },
        { "Cut", { hasEditableRangeSelection, execCut } },
        { "Delete", { hasEditableSelection, execDelete } },
        { "ForwardDelete", { hasEditableSelection, execForwardDelete } },
        { "MoveBackward", { hasEditableSelection, execMoveBackward } },
        { "MoveBackwardAndModifySelection", { hasEditableSelection, execMoveBackwardAndModifySelection } },
        { "MoveDown", { hasEditableSelection, execMoveDown } },
        { "MoveDownAndModifySelection", { hasEditableSelection, execMoveDownAndModifySelection } },
        { "MoveForward", { hasEditableSelection, execMoveForward } },
        { "MoveForwardAndModifySelection", { hasEditableSelection, execMoveForwardAndModifySelection } },
        { "MoveLeft", { hasEditableSelection, execMoveLeft } },
        { "MoveLeftAndModifySelection", { hasEditableSelection, execMoveLeftAndModifySelection } },
        { "MoveRight", { hasEditableSelection, execMoveRight } },
        { "MoveRightAndModifySelection", { hasEditableSelection, execMoveRightAndModifySelection } },
        { "MoveToBeginningOfDocument", { hasEditableSelection, execMoveToBeginningOfDocument } },
        { "MoveToBeginningOfDocumentAndModifySelection", { hasEditableSelection, execMoveToBeginningOfDocumentAndModifySelection } },
        { "MoveToBeginningOfSentence", { hasEditableSelection, execMoveToBeginningOfSentence } },
        { "MoveToBeginningOfSentenceAndModifySelection", { hasEditableSelection, execMoveToBeginningOfSentenceAndModifySelection } },
        { "MoveToBeginningOfLine", { hasEditableSelection, execMoveToBeginningOfLine } },
        { "MoveToBeginningOfLineAndModifySelection", { hasEditableSelection, execMoveToBeginningOfLineAndModifySelection } },
        { "MoveToBeginningOfParagraph", { hasEditableSelection, execMoveToBeginningOfParagraph } },
        { "MoveToBeginningOfLineAndModifySelection", { hasEditableSelection, execMoveToBeginningOfParagraphAndModifySelection } },
        { "MoveToEndOfDocument", { hasEditableSelection, execMoveToEndOfDocument } },
        { "MoveToEndOfDocumentAndModifySelection", { hasEditableSelection, execMoveToEndOfDocumentAndModifySelection } },
        { "MoveToEndOfSentence", { hasEditableSelection, execMoveToEndOfSentence } },
        { "MoveToEndOfSentenceAndModifySelection", { hasEditableSelection, execMoveToEndOfSentenceAndModifySelection } },
        { "MoveToEndOfLine", { hasEditableSelection, execMoveToEndOfLine } },
        { "MoveToEndOfLineAndModifySelection", { hasEditableSelection, execMoveToEndOfLineAndModifySelection } },
        { "MoveToEndOfParagraph", { hasEditableSelection, execMoveToEndOfParagraph } },
        { "MoveToEndOfLineAndModifySelection", { hasEditableSelection, execMoveToEndOfParagraphAndModifySelection } },
        { "MoveParagraphBackwardAndModifySelection", { hasEditableSelection, execMoveParagraphBackwardAndModifySelection } },
        { "MoveParagraphForwardAndModifySelection", { hasEditableSelection, execMoveParagraphForwardAndModifySelection } },
        { "MoveUp", { hasEditableSelection, execMoveUp } },
        { "MoveUpAndModifySelection", { hasEditableSelection, execMoveUpAndModifySelection } },
        { "MoveWordBackward", { hasEditableSelection, execMoveWordBackward } },
        { "MoveWordBackwardAndModifySelection", { hasEditableSelection, execMoveWordBackwardAndModifySelection } },
        { "MoveWordForward", { hasEditableSelection, execMoveWordForward } },
        { "MoveWordForwardAndModifySelection", { hasEditableSelection, execMoveWordForwardAndModifySelection } },
        { "MoveWordLeft", { hasEditableSelection, execMoveWordLeft } },
        { "MoveWordLeftAndModifySelection", { hasEditableSelection, execMoveWordLeftAndModifySelection } },
        { "MoveWordRight", { hasEditableSelection, execMoveWordRight } },
        { "MoveWordRightAndModifySelection", { hasEditableSelection, execMoveWordRightAndModifySelection } },
        { "Paste", { canPaste, execPaste } },
        { "Redo", { canRedo, execRedo } },
        { "SelectAll", { enabled, execSelectAll } },
        { "ToggleBold", { hasRichlyEditableSelection, execToggleBold } },
        { "ToggleItalic", { hasRichlyEditableSelection, execToggleItalic } },
        { "Undo", { canUndo, execUndo } }
    };
    
    CommandMap* commandMap = new CommandMap;
    
    const unsigned numCommands = sizeof(commands) / sizeof(commands[0]);
    for (unsigned i = 0; i < numCommands; i++)
        commandMap->set(commands[i].name, &commands[i].command);
    return commandMap;
}

// =============================================================================
//
// public editing commands
//
// =============================================================================

Editor::Editor(Frame* frame)
    : m_frame(frame)
    , m_deleteButtonController(new DeleteButtonController(frame))
{ 
}

Editor::~Editor()
{
}

bool Editor::execCommand(const String& command)
{
    static CommandMap* commandMap;
    if (!commandMap)
        commandMap = createCommandMap();
    
    const Command* c = commandMap->get(command);
    ASSERT(c);
    
    bool handled = false;
    
    if (c->enabled(m_frame)) {
        m_frame->document()->updateLayoutIgnorePendingStylesheets();
        handled = c->exec(m_frame);
    }
    
    return handled;
}

void Editor::cut()
{
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut()) {
        systemBeep();
        return;
    }
    
    if (shouldDeleteRange(selectedRange())) {
        writeSelectionToPasteboard(generalPasteboard());
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
    }
}

void Editor::copy()
{
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy()) {
        systemBeep();
        return;
    }
    writeSelectionToPasteboard(generalPasteboard());
}

void Editor::paste()
{
    if (tryDHTMLPaste())
        return;     // DHTML did the whole operation
    if (!canPaste())
        return;
    if (canEditRichly())
        pasteWithPasteboard(generalPasteboard(), true);
    else
        pasteAsPlainTextWithPasteboard(generalPasteboard());
}

void Editor::performDelete()
{
    if (!canDelete()) {
        systemBeep();
        return;
    }
    deleteSelection();
}

} // namespace WebCore
