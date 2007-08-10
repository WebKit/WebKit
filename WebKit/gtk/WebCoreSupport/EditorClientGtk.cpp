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
#include "EditorClientGtk.h"

#include "EditCommand.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "webkitgtkprivate.h"

#include <stdio.h>

using namespace WebKitGtk;

namespace WebCore {

bool EditorClientGtk::shouldDeleteRange(Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGtk::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientGtk::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientGtk::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClientGtk::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientGtk::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGtk::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGtk::shouldInsertText(String, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientGtk::shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting)
{
    notImplemented();
    return true;
}

bool EditorClientGtk::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientGtk::shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*)
{
    notImplemented();
    return true;
}

void EditorClientGtk::didBeginEditing()
{
    notImplemented();
}

void EditorClientGtk::respondToChangedContents()
{
    notImplemented();
}

void EditorClientGtk::respondToChangedSelection()
{
    notImplemented();
}

void EditorClientGtk::didEndEditing()
{
    notImplemented();
}

void EditorClientGtk::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientGtk::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

bool EditorClientGtk::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientGtk::isEditable()
{
    notImplemented();
    return false;
}

void EditorClientGtk::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientGtk::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientGtk::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientGtk::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientGtk::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientGtk::undo()
{
    notImplemented();
}

void EditorClientGtk::redo()
{
    notImplemented();
}

bool EditorClientGtk::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
    return false;
}

void EditorClientGtk::pageDestroyed()
{
    notImplemented();
}

bool EditorClientGtk::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

void EditorClientGtk::toggleContinuousSpellChecking()
{
    notImplemented();
}

void EditorClientGtk::toggleGrammarChecking()
{
}

void EditorClientGtk::handleKeypress(KeyboardEvent* event)
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
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


void EditorClientGtk::handleInputMethodKeypress(KeyboardEvent*)
{
    notImplemented();
}

EditorClientGtk::EditorClientGtk(WebKitGtkPage* page)
    : m_page(page)
{
}

void EditorClientGtk::textFieldDidBeginEditing(Element*)
{
    notImplemented();
}

void EditorClientGtk::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClientGtk::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClientGtk::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplemented();
    return false;
}

void EditorClientGtk::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientGtk::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientGtk::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientGtk::learnWord(const String&)
{
    notImplemented();
}

void EditorClientGtk::checkSpellingOfString(const UChar*, int, int*, int*)
{
    notImplemented();
}

void EditorClientGtk::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientGtk::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientGtk::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientGtk::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientGtk::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientGtk::getGuessesForWord(const String&, Vector<String>&)
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
