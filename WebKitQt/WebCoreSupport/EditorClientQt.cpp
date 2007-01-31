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
#include "EditorClientQt.h"

#include "qwebpage.h"
#include "qwebpage_p.h"

#include "EditCommand.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"

#include <stdio.h>


#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

bool EditorClientQt::shouldDeleteRange(Range*)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientQt::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientQt::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClientQt::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientQt::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldInsertText(String, Range*, EditorInsertAction)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplemented();
    return false;
}

void EditorClientQt::didBeginEditing()
{
    notImplemented();
}

void EditorClientQt::respondToChangedContents()
{
    notImplemented();
}

void EditorClientQt::didEndEditing()
{
    notImplemented();
}

void EditorClientQt::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientQt::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

bool EditorClientQt::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientQt::isEditable()
{
    notImplemented();
    return false;
}

void EditorClientQt::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientQt::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientQt::clearUndoRedoOperations()
{
    //notImplemented();
}

bool EditorClientQt::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientQt::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientQt::undo()
{
    notImplemented();
}

void EditorClientQt::redo()
{
    notImplemented();
}

bool EditorClientQt::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
}

void EditorClientQt::pageDestroyed()
{
    notImplemented();
}

bool EditorClientQt::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

void EditorClientQt::toggleContinuousSpellChecking()
{
    notImplemented();
}

void EditorClientQt::toggleGrammarChecking()
{
    notImplemented();
}

void EditorClientQt::handleKeyPress(KeyboardEvent* event)
{
    Frame* frame = m_page->d->page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent->isKeyUp()) {
        Node* start = frame->selectionController()->start().node();
        if (start && start->isContentEditable()) {
            switch(kevent->WindowsKeyCode()) {
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

EditorClientQt::EditorClientQt(QWebPage* page)
    : m_page(page)
{
}

}

// vim: ts=4 sw=4 et
