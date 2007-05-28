/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org> 
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "EditorClientGdk.h"

#include "EditCommand.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"

#include <stdio.h>

namespace WebCore {

bool EditorClientGdk::shouldDeleteRange(Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGdk::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientGdk::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientGdk::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClientGdk::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientGdk::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGdk::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGdk::shouldInsertText(String, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientGdk::shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting)
{
    notImplemented();
    return true;
}

bool EditorClientGdk::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplemented();
    return false;
}

void EditorClientGdk::didBeginEditing()
{
    notImplemented();
}

void EditorClientGdk::respondToChangedContents()
{
    notImplemented();
}

void EditorClientGdk::respondToChangedSelection()
{
    notImplemented();
}

void EditorClientGdk::didEndEditing()
{
    notImplemented();
}

void EditorClientGdk::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientGdk::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

bool EditorClientGdk::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientGdk::isEditable()
{
    notImplemented();
    return true;
}

void EditorClientGdk::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientGdk::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientGdk::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientGdk::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientGdk::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientGdk::undo()
{
    notImplemented();
}

void EditorClientGdk::redo()
{
    notImplemented();
}

bool EditorClientGdk::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
    return false;
}

void EditorClientGdk::pageDestroyed()
{
    notImplemented();
}

bool EditorClientGdk::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

void EditorClientGdk::toggleContinuousSpellChecking()
{
    notImplemented();
}

void EditorClientGdk::toggleGrammarChecking()
{
    EditorClientGdk();
}

void EditorClientGdk::handleKeypress(KeyboardEvent* event)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent->isKeyUp()) {
        Node* start = frame->selectionController()->start().node();
        if (start && start->isContentEditable()) {
            switch (kevent->WindowsKeyCode()) {
            case VK_BACK:
                frame->editor()->deleteWithDirection(SelectionController::BACKWARD,
                                                     CharacterGranularity, false, true);
                break;
            case VK_DELETE:
                frame->editor()->deleteWithDirection(SelectionController::FORWARD,
                                                     CharacterGranularity, false, true);
                break;
            case VK_LEFT:
                frame->editor()->execCommand("MoveLeft");
                break;
            case VK_RIGHT:
                frame->editor()->execCommand("MoveRight");
                break;
            case VK_UP:
                frame->editor()->execCommand("MoveUp");
                break;
            case VK_DOWN:
                frame->editor()->execCommand("MoveDown");
                break;
            default:
                frame->editor()->insertText(kevent->text(), event);
            }
            event->setDefaultHandled();
        }
    }
}


void EditorClientGdk::handleInputMethodKeypress(KeyboardEvent*)
{
    notImplemented();
}

EditorClientGdk::EditorClientGdk()
{
}

void EditorClientGdk::setPage(Page* page)
{
    m_page = page;
}

void EditorClientGdk::textFieldDidBeginEditing(Element*)
{
    notImplemented();
}

void EditorClientGdk::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClientGdk::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClientGdk::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplemented();
    return false;
}

void EditorClientGdk::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientGdk::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientGdk::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientGdk::learnWord(const String&)
{
    notImplemented();
}

void EditorClientGdk::checkSpellingOfString(const UChar*, int, int*, int*)
{
    notImplemented();
}

void EditorClientGdk::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientGdk::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientGdk::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientGdk::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientGdk::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientGdk::getGuessesForWord(const String&, Vector<String>&)
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
