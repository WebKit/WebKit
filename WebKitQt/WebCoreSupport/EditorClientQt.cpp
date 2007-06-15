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

#include "Document.h"
#include "EditCommandQt.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "NotImplemented.h"

#include <stdio.h>

#include <QUndoStack>

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

void EditorClientQt::respondToChangedSelection()
{
    notImplemented();
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

void EditorClientQt::handleKeypress(KeyboardEvent* event)
{
    Frame* frame = m_page->d->page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode())
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (kevent->isKeyUp())
        return;

    Node* start = frame->selectionController()->start().node();
    if (!start)
        return;

    // FIXME: refactor all of this to use Actions or something like them
    if (start->isContentEditable()) {
        switch(kevent->WindowsKeyCode()) {
            case VK_RETURN:
                frame->editor()->execCommand("InsertLineBreak");
                break;
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
            case VK_PRIOR:  // PageUp
                frame->editor()->execCommand("MoveUpByPageAndModifyCaret");
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->execCommand("MoveDownByPageAndModifyCaret");
                break;
            case VK_TAB:
                return;
            default:
                if (!kevent->ctrlKey() && !kevent->altKey() && !kevent->text().isEmpty()) {
                    frame->editor()->insertText(kevent->text(), event);
                } else if (kevent->ctrlKey()) {
                    switch (kevent->WindowsKeyCode()) {
                        case VK_A:
                            frame->editor()->execCommand("SelectAll");
                            break;
                        case VK_B:
                            frame->editor()->execCommand("ToggleBold");
                            break;
                        case VK_C:
                            frame->editor()->execCommand("Copy");
                            break;
                        case VK_I:
                            frame->editor()->execCommand("ToggleItalic");
                            break;
                        case VK_V:
                            frame->editor()->execCommand("Paste");
                            break;
                        case VK_X:
                            frame->editor()->execCommand("Cut");
                            break;
                        case VK_Y:
                            frame->editor()->execCommand("Redo");
                            break;
                        case VK_Z:
                            frame->editor()->execCommand("Undo");
                            break;
                        default:
                            return;
                    }
                } else return;
        }
    } else {
        switch (kevent->WindowsKeyCode()) {
            case VK_UP:
                frame->editor()->execCommand("MoveUp");
                break;
            case VK_DOWN:
                frame->editor()->execCommand("MoveDown");
                break;
            case VK_PRIOR:  // PageUp
                frame->editor()->execCommand("MoveUpByPageAndModifyCaret");
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->execCommand("MoveDownByPageAndModifyCaret");
                break;
            case VK_HOME:
                if (kevent->ctrlKey())
                    frame->editor()->execCommand("MoveToBeginningOfDocument");
                break;
            case VK_END:
                if (kevent->ctrlKey())
                    frame->editor()->execCommand("MoveToEndOfDocument");
                break;
            default:
                if (kevent->ctrlKey()) {
                    switch(kevent->WindowsKeyCode()) {
                        case VK_A:
                            frame->editor()->execCommand("SelectAll");
                            break;
                        case VK_C: case VK_X:
                            frame->editor()->execCommand("Copy");
                            break;
                        default:
                            return;
                    }
                } else return;
        }
    }
    event->setDefaultHandled();
}

void EditorClientQt::handleInputMethodKeypress(KeyboardEvent*)
{
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

void EditorClientQt::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientQt::learnWord(const String&)
{
    notImplemented();
}

void EditorClientQt::checkSpellingOfString(const UChar*, int, int*, int*)
{
    notImplemented();
}

void EditorClientQt::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientQt::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientQt::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientQt::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientQt::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientQt::getGuessesForWord(const String&, Vector<String>&)
{
    notImplemented();
}

bool EditorClientQt::isEditing() const
{
    return m_editing;
}

}

// vim: ts=4 sw=4 et
