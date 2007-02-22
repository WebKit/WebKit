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
#include "Page.h"
#include "PlatformKeyboardEvent.h"

#include <stdio.h>

#define notImplementedGdk() do { \
    static int count; \
    if (!count && !getenv("DISABLE_NI_WARNING")) \
        fprintf(stderr, "FIXME: UNIMPLEMENTED %s %s:%d\n", WTF_PRETTY_FUNCTION, __FILE__, __LINE__); \
    ++count; \
 } while(0)

namespace WebCore {

bool EditorClientGdk::shouldDeleteRange(Range*)
{
    notImplementedGdk();
    return false;
}

bool EditorClientGdk::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientGdk::isContinuousSpellCheckingEnabled()
{
    notImplementedGdk();
    return false;
}

bool EditorClientGdk::isGrammarCheckingEnabled()
{
    notImplementedGdk();
    return false;
}

int EditorClientGdk::spellCheckerDocumentTag()
{
    notImplementedGdk();
    return 0;
}

bool EditorClientGdk::shouldBeginEditing(WebCore::Range*)
{
    notImplementedGdk();
    return false;
}

bool EditorClientGdk::shouldEndEditing(WebCore::Range*)
{
    notImplementedGdk();
    return false;
}

bool EditorClientGdk::shouldInsertText(String, Range*, EditorInsertAction)
{
    notImplementedGdk();
    return true;
}

bool EditorClientGdk::shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting)
{
    notImplementedGdk();
    return true;
}

bool EditorClientGdk::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplementedGdk();
    return false;
}

void EditorClientGdk::didBeginEditing()
{
    notImplementedGdk();
}

void EditorClientGdk::respondToChangedContents()
{
    notImplementedGdk();
}

void EditorClientGdk::didEndEditing()
{
    notImplementedGdk();
}

void EditorClientGdk::didWriteSelectionToPasteboard()
{
    notImplementedGdk();
}

void EditorClientGdk::didSetSelectionTypesForPasteboard()
{
    notImplementedGdk();
}

bool EditorClientGdk::selectWordBeforeMenuEvent()
{
    notImplementedGdk();
    return false;
}

bool EditorClientGdk::isEditable()
{
    notImplementedGdk();
    return true;
}

void EditorClientGdk::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplementedGdk();
}

void EditorClientGdk::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplementedGdk();
}

void EditorClientGdk::clearUndoRedoOperations()
{
    notImplementedGdk();
}

bool EditorClientGdk::canUndo() const
{
    notImplementedGdk();
    return false;
}

bool EditorClientGdk::canRedo() const
{
    notImplementedGdk();
    return false;
}

void EditorClientGdk::undo()
{
    notImplementedGdk();
}

void EditorClientGdk::redo()
{
    notImplementedGdk();
}

bool EditorClientGdk::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplementedGdk();
    return false;
}

void EditorClientGdk::pageDestroyed()
{
    notImplementedGdk();
}

bool EditorClientGdk::smartInsertDeleteEnabled()
{
    notImplementedGdk();
    return false;
}

void EditorClientGdk::toggleContinuousSpellChecking()
{
    notImplementedGdk();
}

void EditorClientGdk::toggleGrammarChecking()
{
    EditorClientGdk();
}

void EditorClientGdk::handleKeyPress(KeyboardEvent* event)
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
                frame->editor()->insertText(kevent->text(), false, event);
            }
            event->setDefaultHandled();
        }
    }
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
    notImplementedGdk();
}

void EditorClientGdk::textFieldDidEndEditing(Element*)
{
    notImplementedGdk();
}

void EditorClientGdk::textDidChangeInTextField(Element*)
{
    notImplementedGdk();
}

bool EditorClientGdk::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplementedGdk();
    return false;
}

void EditorClientGdk::textWillBeDeletedInTextField(Element*)
{
    notImplementedGdk();
}

void EditorClientGdk::textDidChangeInTextArea(Element*)
{
    notImplementedGdk();
}

}

// vim: ts=4 sw=4 et
