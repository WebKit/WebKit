/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2007 Andrea Anzani <andrea.anzani@gmail.com>
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
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"

#include "NotImplemented.h"


namespace WebCore {

EditorClientHaiku::EditorClientHaiku()
    : m_editing(false)
    , m_inUndoRedo(false)
{
}

void EditorClientHaiku::setPage(Page* page)
{
    m_page = page;
}

void EditorClientHaiku::pageDestroyed()
{
    notImplemented();
}

bool EditorClientHaiku::shouldDeleteRange(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldShowDeleteInterface(HTMLElement*)
{
    notImplemented();
    return false;
}

bool EditorClientHaiku::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientHaiku::isSelectTrailingWhitespaceEnabled()
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

bool EditorClientHaiku::isEditable()
{
    // FIXME: should be controllable
    return false;
}

bool EditorClientHaiku::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldInsertText(const String&, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldChangeSelectedRange(Range* fromRange, Range* toRange,
                                                  EAffinity, bool stillSelecting)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClientHaiku::shouldMoveRangeAfterDelete(Range*, Range*)
{
    notImplemented();
    return true;
}

void EditorClientHaiku::didBeginEditing()
{
    notImplemented();
    m_editing = true;
}

void EditorClientHaiku::respondToChangedContents()
{
    notImplemented();
}

void EditorClientHaiku::respondToChangedSelection()
{
    notImplemented();
}

void EditorClientHaiku::didEndEditing()
{
    notImplemented();
    m_editing = false;
}

void EditorClientHaiku::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientHaiku::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

void EditorClientHaiku::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand> cmd)
{
    notImplemented();
}

void EditorClientHaiku::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientHaiku::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientHaiku::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientHaiku::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientHaiku::undo()
{
    notImplemented();
    m_inUndoRedo = true;
    m_inUndoRedo = false;
}

void EditorClientHaiku::redo()
{
    notImplemented();
    m_inUndoRedo = true;
    m_inUndoRedo = false;
}

void EditorClientHaiku::handleKeyboardEvent(KeyboardEvent* event)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode())
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent || kevent->type() == PlatformKeyboardEvent::KeyUp)
        return;

    Node* start = frame->selection()->start().node();
    if (!start)
        return;

    if (start->isContentEditable()) {
        switch(kevent->windowsVirtualKeyCode()) {
        case VK_BACK:
            frame->editor()->deleteWithDirection(SelectionController::BACKWARD,
                                                 kevent->ctrlKey() ? WordGranularity : CharacterGranularity,
                                                 false, true);
            break;
        case VK_DELETE:
            frame->editor()->deleteWithDirection(SelectionController::FORWARD,
                                                 kevent->ctrlKey() ? WordGranularity : CharacterGranularity,
                                                 false, true);
            break;
        case VK_LEFT:
            frame->selection()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                                       SelectionController::LEFT,
                                       kevent->ctrlKey() ? WordGranularity : CharacterGranularity,
                                       true);
            break;
        case VK_RIGHT:
            frame->selection()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                                       SelectionController::RIGHT,
                                       kevent->ctrlKey() ? WordGranularity : CharacterGranularity,
                                       true);
            break;
        case VK_UP:
            frame->selection()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                                       SelectionController::BACKWARD,
                                       kevent->ctrlKey() ? ParagraphGranularity : LineGranularity,
                                       true);
            break;
        case VK_DOWN:
            frame->selection()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                                       SelectionController::FORWARD,
                                       kevent->ctrlKey() ? ParagraphGranularity : LineGranularity,
                                       true);
            break;
        case VK_PRIOR:  // PageUp
            frame->editor()->command("MoveUpByPageAndModifyCaret");
            break;
        case VK_NEXT:  // PageDown
            frame->editor()->command("MoveDownByPageAndModifyCaret");
            break;
        case VK_RETURN:
            frame->editor()->command("InsertLineBreak");
            break;
        case VK_TAB:
            return;
        default:
            if (!kevent->ctrlKey() && !kevent->altKey() && !kevent->text().isEmpty()) {
                if (kevent->text().length() == 1) {
                    UChar ch = kevent->text()[0];
                    // Don't insert null or control characters as they can result in unexpected behaviour
                    if (ch < ' ')
                        break;
                }
                frame->editor()->insertText(kevent->text(), event);
            } else if (kevent->ctrlKey()) {
                switch (kevent->windowsVirtualKeyCode()) {
                case VK_A:
                    frame->editor()->command("SelectAll");
                    break;
                case VK_B:
                    frame->editor()->command("ToggleBold");
                    break;
                case VK_C:
                    frame->editor()->command("Copy");
                    break;
                case VK_I:
                    frame->editor()->command("ToggleItalic");
                    break;
                case VK_V:
                    frame->editor()->command("Paste");
                    break;
                case VK_X:
                    frame->editor()->command("Cut");
                    break;
                case VK_Y:
                    frame->editor()->command("Redo");
                    break;
                case VK_Z:
                    frame->editor()->command("Undo");
                    break;
                default:
                    return;
                }
            } else return;
        }
    } else {
        switch (kevent->windowsVirtualKeyCode()) {
        case VK_UP:
            frame->editor()->command("MoveUp");
            break;
        case VK_DOWN:
            frame->editor()->command("MoveDown");
            break;
        case VK_PRIOR:  // PageUp
            frame->editor()->command("MoveUpByPageAndModifyCaret");
            break;
        case VK_NEXT:  // PageDown
            frame->editor()->command("MoveDownByPageAndModifyCaret");
            break;
        case VK_HOME:
            if (kevent->ctrlKey())
                frame->editor()->command("MoveToBeginningOfDocument");
            break;
        case VK_END:
            if (kevent->ctrlKey())
                frame->editor()->command("MoveToEndOfDocument");
            break;
        default:
            if (kevent->ctrlKey()) {
                switch(kevent->windowsVirtualKeyCode()) {
                    case VK_A:
                        frame->editor()->command("SelectAll");
                        break;
                    case VK_C: case VK_X:
                        frame->editor()->command("Copy");
                        break;
                    default:
                        return;
                }
            } else return;
        }
    }
    event->setDefaultHandled();
}

void EditorClientHaiku::handleInputMethodKeydown(KeyboardEvent*)
{
    notImplemented();
}

void EditorClientHaiku::textFieldDidBeginEditing(Element*)
{
    m_editing = true;
}

void EditorClientHaiku::textFieldDidEndEditing(Element*)
{
    m_editing = false;
}

void EditorClientHaiku::textDidChangeInTextField(Element*)
{
    notImplemented();
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

void EditorClientHaiku::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientHaiku::learnWord(const String&)
{
    notImplemented();
}

void EditorClientHaiku::checkSpellingOfString(const UChar*, int, int*, int*)
{
    notImplemented();
}

String EditorClientHaiku::getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord)
{
    notImplemented();
    return String();
}

void EditorClientHaiku::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
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

void EditorClientHaiku::getGuessesForWord(const String&, Vector<String>&)
{
    notImplemented();
}

void EditorClientHaiku::setInputMethodState(bool enabled)
{
    notImplemented();
}

bool EditorClientHaiku::isEditing() const
{
    return m_editing;
}

} // namespace WebCore

