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

#include "EditCommandQt.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"

#include <stdio.h>

#include <QUndoStack>

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

bool EditorClientQt::shouldDeleteRange(Range*)
{
    return true;
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
    return true;
}

bool EditorClientQt::shouldEndEditing(WebCore::Range*)
{
    return true;
}

bool EditorClientQt::shouldInsertText(String, Range*, EditorInsertAction)
{
    return true;
}

bool EditorClientQt::shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting)
{
    return true;
}

bool EditorClientQt::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    return true;
}

void EditorClientQt::didBeginEditing()
{
    m_editing = true;
}

void EditorClientQt::respondToChangedContents()
{
    m_page->d->modified = true;
}

void EditorClientQt::didEndEditing()
{
    m_editing = false;
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
    // FIXME: should be controllable by a setting in QWebPage
    return false;
}

void EditorClientQt::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand> cmd)
{
    Frame* frame = m_page->d->page->focusController()->focusedOrMainFrame();
    if (m_inUndoRedo || (frame && !frame->editor()->lastEditCommand() /* HACK!! Don't recreate undos */)) {
        return;
    }
    m_page->undoStack()->push(new EditCommandQt(cmd));
}

void EditorClientQt::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
}

void EditorClientQt::clearUndoRedoOperations()
{
    return m_page->undoStack()->clear();
}

bool EditorClientQt::canUndo() const
{
    return m_page->undoStack()->canUndo();
}

bool EditorClientQt::canRedo() const
{
    return m_page->undoStack()->canRedo();
}

void EditorClientQt::undo()
{
    m_inUndoRedo = true;
    m_page->undoStack()->undo();
    m_inUndoRedo = false;
}

void EditorClientQt::redo()
{
    m_inUndoRedo = true;
    m_page->undoStack()->redo();
    m_inUndoRedo = false;
}

bool EditorClientQt::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    return true;
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
                if (kevent->shiftKey())
                    frame->editor()->execCommand("MoveLeftAndModifySelection");
                else frame->editor()->execCommand("MoveLeft");
                break;
            case VK_RIGHT:
                if (kevent->shiftKey())
                    frame->editor()->execCommand("MoveRightAndModifySelection");
                else frame->editor()->execCommand("MoveRight");
                break;
            case VK_UP:
                if (kevent->shiftKey())
                    frame->editor()->execCommand("MoveUpAndModifySelection");
                else frame->editor()->execCommand("MoveUp");
                break;
            case VK_DOWN:
                if (kevent->shiftKey())
                    frame->editor()->execCommand("MoveDownAndModifySelection");
                else frame->editor()->execCommand("MoveDown");
                break;
            case VK_RETURN:
                frame->editor()->insertLineBreak();
                break;
            default:
                if (!kevent->ctrlKey() && !kevent->altKey())
                    frame->editor()->insertText(kevent->text(), false, event);
            }
            event->setDefaultHandled();
        }
    }
}

EditorClientQt::EditorClientQt(QWebPage* page)
    : m_page(page), m_editing(false), m_inUndoRedo(false)
{
}

void EditorClientQt::textFieldDidBeginEditing(Element*)
{
    m_editing = true;
}

void EditorClientQt::textFieldDidEndEditing(Element*)
{
    m_editing = false;
}

void EditorClientQt::textDidChangeInTextField(Element*)
{
}

bool EditorClientQt::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    return false;
}

void EditorClientQt::textWillBeDeletedInTextField(Element*)
{
}

void EditorClientQt::textDidChangeInTextArea(Element*)
{
}

bool EditorClientQt::isEditing() const
{
    return m_editing;
}

}

// vim: ts=4 sw=4 et
