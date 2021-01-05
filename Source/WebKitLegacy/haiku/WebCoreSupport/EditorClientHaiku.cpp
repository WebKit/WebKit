/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2007 Andrea Anzani <andrea.anzani@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
#include "EditorClientHaiku.h"

#include "Document.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "Settings.h"
#include "WebFrame.h"
#include "WebViewConstants.h"
#include "WebPage.h"
#include "WindowsKeyboardCodes.h"

namespace WebCore {

EditorClientHaiku::EditorClientHaiku(BWebPage* page)
    : m_page(page)
    , m_isInRedo(false)
{
}

bool EditorClientHaiku::shouldDeleteRange(const Optional<SimpleRange>& range)
{
    BMessage message(EDITOR_DELETE_RANGE);
    if (range)
        message.AddPointer("range", &*range);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientHaiku::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientHaiku::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool EditorClientHaiku::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientHaiku::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientHaiku::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientHaiku::shouldBeginEditing(const SimpleRange& range)
{
    BMessage message(EDITOR_BEGIN_EDITING);
    message.AddPointer("range", &range);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::shouldEndEditing(const SimpleRange& range)
{
    BMessage message(EDITOR_END_EDITING);
    message.AddPointer("range", &range);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::shouldInsertNode(Node& node, const Optional<SimpleRange>& range, EditorInsertAction action)
{
    BMessage message(EDITOR_INSERT_NODE);
    message.AddPointer("node", &node);
    if (range)
        message.AddPointer("range", &*range);
    message.AddInt32("action", (int32)action);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::shouldInsertText(const String& text, const Optional<SimpleRange>& range, EditorInsertAction action)
{
    BMessage message(EDITOR_INSERT_TEXT);
    message.AddString("text", text);
    if (range)
        message.AddPointer("range", &*range);
    message.AddInt32("action", (int32)action);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::shouldChangeSelectedRange(const Optional<SimpleRange>& fromRange, const Optional<SimpleRange>& toRange,
    Affinity affinity, bool stillSelecting)
{
    BMessage message(EDITOR_CHANGE_SELECTED_RANGE);
    if (fromRange)
        message.AddPointer("from", &*fromRange);
    if (toRange)
        message.AddPointer("to", &*toRange);
    message.AddInt32("affinity", (int32)affinity);
    message.AddInt32("stillEditing", stillSelecting);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::shouldApplyStyle(const WebCore::StyleProperties& style,
                                      const Optional<SimpleRange>& range)
{
    BMessage message(EDITOR_APPLY_STYLE);
    message.AddPointer("style", &style);
    if (range)
        message.AddPointer("range", &*range);
    dispatchMessage(message);
    return true;
}

bool EditorClientHaiku::shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&)
{
    notImplemented();
    return true;
}

void EditorClientHaiku::didBeginEditing()
{
    BMessage message(EDITOR_EDITING_BEGAN);
    dispatchMessage(message);
}

void EditorClientHaiku::respondToChangedContents()
{
    BMessage message(EDITOR_CONTENTS_CHANGED);
    dispatchMessage(message);
}

void EditorClientHaiku::respondToChangedSelection(Frame* frame)
{
    if (!frame)
        return;

    BMessage message(EDITOR_SELECTION_CHANGED);
    dispatchMessage(message);
}

void EditorClientHaiku::didEndEditing()
{
    BMessage message(EDITOR_EDITING_ENDED);
    dispatchMessage(message);
}

void EditorClientHaiku::didWriteSelectionToPasteboard()
{
    notImplemented();
}

bool EditorClientHaiku::isSelectTrailingWhitespaceEnabled() const
{
    notImplemented();
	return false;
}

void EditorClientHaiku::willWriteSelectionToPasteboard(const Optional<SimpleRange>&)
{
}

void EditorClientHaiku::getClientPasteboardData(const Optional<SimpleRange>&, Vector<String>&, Vector<RefPtr<WebCore::SharedBuffer> >&)
{
}

void EditorClientHaiku::discardedComposition(Frame*)
{
}

void EditorClientHaiku::canceledComposition()
{
}

void EditorClientHaiku::registerUndoStep(WebCore::UndoStep& step)
{
    if (!m_isInRedo)
        m_redoStack.clear();
    m_undoStack.append(&step);
}

void EditorClientHaiku::registerRedoStep(WebCore::UndoStep& step)
{
    m_redoStack.append(&step);
}

void EditorClientHaiku::clearUndoRedoOperations()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

bool EditorClientHaiku::canCopyCut(WebCore::Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientHaiku::canPaste(WebCore::Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientHaiku::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool EditorClientHaiku::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void EditorClientHaiku::undo()
{
    if (canUndo()) {
        RefPtr<WebCore::UndoStep> step(m_undoStack.takeLast());
        // unapply will call us back to push this step onto the redo stack.
        step->unapply();
    }
}

void EditorClientHaiku::redo()
{
    if (canRedo()) {
        RefPtr<WebCore::UndoStep> step(m_redoStack.takeLast());

        ASSERT(!m_isInRedo);
        m_isInRedo = true;
        // reapply will call us back to push this step onto the undo stack.
        step->reapply();
        m_isInRedo = false;
    }
}

void EditorClientHaiku::handleKeyboardEvent(KeyboardEvent& event)
{
    const PlatformKeyboardEvent* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent || platformEvent->type() == PlatformKeyboardEvent::KeyUp)
        return;

    if (handleEditingKeyboardEvent(&event, platformEvent)) {
        event.setDefaultHandled();
    }
}

void EditorClientHaiku::handleInputMethodKeydown(KeyboardEvent&)
{
    notImplemented();
}

void EditorClientHaiku::textFieldDidBeginEditing(Element*)
{
}

void EditorClientHaiku::textFieldDidEndEditing(Element*)
{
}

void EditorClientHaiku::textDidChangeInTextField(Element*)
{
}

bool EditorClientHaiku::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    return false;
}

void EditorClientHaiku::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientHaiku::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientHaiku::overflowScrollPositionChanged()
{
    notImplemented();
}

bool EditorClientHaiku::shouldEraseMarkersAfterChangeSelection(TextCheckingType) const
{
    return true;
}

void EditorClientHaiku::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientHaiku::learnWord(const String&)
{
    notImplemented();
}

void EditorClientHaiku::checkSpellingOfString(StringView, int*, int*)
{
    notImplemented();
}

String EditorClientHaiku::getAutoCorrectSuggestionForMisspelledWord(const String& /*misspelledWord*/)
{
    notImplemented();
    return String();
}

void EditorClientHaiku::checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientHaiku::getGuessesForWord(const String&, const String&, const WebCore::VisibleSelection&, Vector<String>&)
{
    notImplemented();
}

void EditorClientHaiku::requestCheckingOfString(TextCheckingRequest&, const WebCore::VisibleSelection&)
{
    notImplemented();
}

void EditorClientHaiku::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientHaiku::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientHaiku::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientHaiku::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientHaiku::willSetInputMethodState()
{
    notImplemented();
}

void EditorClientHaiku::setInputMethodState(Element*)
{
    notImplemented();
}

bool EditorClientHaiku::performTwoStepDrop(DocumentFragment&, const SimpleRange& destination, bool isMove)
{
	notImplemented();
	return false;
}

// #pragma mark -

bool EditorClientHaiku::handleEditingKeyboardEvent(KeyboardEvent* event,
    const PlatformKeyboardEvent* platformEvent)
{
    Frame& frame = m_page->page()->focusController().focusedOrMainFrame();
    if (!frame.document())
        return false;

    // TODO be more specific when filtering events here. Some of the keys are
    // relevant only when there is a range-selection (shift+arrows to edit it),
    // but others are only relevant when there is an edition going on
    // (backspace, delete). We should check these different cases depending on
    // the key, and decide to swallow the event (return true) or not, in which
    // case the BWebFrame code can handle it for scrolling or other keyboard
    // shortcuts.
    if (!frame.selection().isRange() && !frame.editor().canEdit())
        return false;

    switch (platformEvent->windowsVirtualKeyCode()) {
    case VK_BACK:
        frame.editor().deleteWithDirection(SelectionDirection::Backward,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            false, true);
        break;
    case VK_DELETE:
        frame.editor().deleteWithDirection(SelectionDirection::Forward,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            false, true);
        break;
    case VK_LEFT:
        frame.selection().modify(platformEvent->shiftKey()
                ? FrameSelection::AlterationExtend : FrameSelection::AlterationMove,
            SelectionDirection::Left,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            UserTriggered);
        break;
    case VK_RIGHT:
        frame.selection().modify(platformEvent->shiftKey() ? FrameSelection::AlterationExtend
                : FrameSelection::AlterationMove,
            SelectionDirection::Right,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            UserTriggered);
        break;
    case VK_UP:
        frame.selection().modify(platformEvent->shiftKey() ? FrameSelection::AlterationExtend
                : FrameSelection::AlterationMove,
            SelectionDirection::Backward,
            platformEvent->controlKey() ? TextGranularity::ParagraphGranularity
                : TextGranularity::LineGranularity,
            UserTriggered);
        break;
    case VK_DOWN:
        frame.selection().modify(platformEvent->shiftKey() ? FrameSelection::AlterationExtend
                : FrameSelection::AlterationMove,
            SelectionDirection::Forward,
            platformEvent->controlKey() ? TextGranularity::ParagraphGranularity
                : TextGranularity::LineGranularity,
            UserTriggered);
        break;
    case VK_HOME:
        if (platformEvent->shiftKey() && platformEvent->controlKey())
            frame.editor().command("MoveToBeginningOfDocumentAndModifySelection").execute();
        else if (platformEvent->shiftKey())
            frame.editor().command("MoveToBeginningOfLineAndModifySelection").execute();
        else if (platformEvent->controlKey())
            frame.editor().command("MoveToBeginningOfDocument").execute();
        else
            frame.editor().command("MoveToBeginningOfLine").execute();
        break;
    case VK_END:
        if (platformEvent->shiftKey() && platformEvent->controlKey())
            frame.editor().command("MoveToEndOfDocumentAndModifySelection").execute();
        else if (platformEvent->shiftKey())
            frame.editor().command("MoveToEndOfLineAndModifySelection").execute();
        else if (platformEvent->controlKey())
            frame.editor().command("MoveToEndOfDocument").execute();
        else
            frame.editor().command("MoveToEndOfLine").execute();
        break;
    case VK_PRIOR:  // PageUp
        if (platformEvent->shiftKey())
            frame.editor().command("MovePageUpAndModifySelection").execute();
        else
            frame.editor().command("MovePageUp").execute();
        break;
    case VK_NEXT:  // PageDown
        if (platformEvent->shiftKey())
            frame.editor().command("MovePageDownAndModifySelection").execute();
        else
            frame.editor().command("MovePageDown").execute();
        break;
    case VK_RETURN:
        if (platformEvent->shiftKey())
            frame.editor().command("InsertLineBreak").execute();
        else
            frame.editor().command("InsertNewline").execute();
        break;
    case VK_TAB:
        return false;
    default:
        if (!platformEvent->controlKey() && !platformEvent->altKey() && !platformEvent->text().isEmpty()) {
            if (platformEvent->text().length() == 1) {
                UChar ch = platformEvent->text()[0];
                // Don't insert null or control characters as they can result in unexpected behaviour
                if (ch < ' ')
                    break;
            }
            frame.editor().insertText(platformEvent->text(), event);
        } else if (platformEvent->controlKey()) {
            switch (platformEvent->windowsVirtualKeyCode()) {
            case VK_B:
                frame.editor().command("ToggleBold").execute();
                break;
            case VK_I:
                frame.editor().command("ToggleItalic").execute();
                break;
            case VK_V:
                frame.editor().command("Paste").execute();
                break;
            case VK_X:
                frame.editor().command("Cut").execute();
                break;
            case VK_Y:
            case VK_Z:
                if (platformEvent->shiftKey())
                    frame.editor().command("Redo").execute();
                else
                    frame.editor().command("Undo").execute();
                break;
		    case VK_A:
	            frame.editor().command("SelectAll").execute();
		        break;
		    case VK_C:
	            frame.editor().command("Copy").execute();
		        break;
            default:
                return false;
            }
        } else
            return false;
    }

    return true;
}

void EditorClientHaiku::setPendingComposition(const char* newComposition)
{
    m_pendingComposition = newComposition;
}

void EditorClientHaiku::setPendingPreedit(const char* newPreedit)
{
    m_pendingPreedit = newPreedit;
}

void EditorClientHaiku::clearPendingIMData()
{
    setPendingComposition(0);
    setPendingPreedit(0);
}

void EditorClientHaiku::imContextCommitted(const char* str, EditorClient* /*client*/)
{
    // This signal will fire during a keydown event. We want the contents of the
    // field to change right before the keyup event, so we wait until then to actually
    // commit this composition.
    setPendingComposition(str);
}

void EditorClientHaiku::imContextPreeditChanged(EditorClient* /*client*/)
{
    const char* newPreedit = 0;
    // FIXME: get pre edit
    setPendingPreedit(newPreedit);
}


void EditorClientHaiku::dispatchMessage(BMessage& message)
{
    BHandler* handler = m_page->fListener.Target(NULL);
    if (!handler)
        return;
    handler->MessageReceived(&message);
        // We need the message to be delivered synchronously, otherwise the
        // pointers passed through it would not be valid anymore.
}

} // namespace WebCore

