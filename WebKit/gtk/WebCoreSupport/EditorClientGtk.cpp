/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org> 
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "webkitprivate.h"

#include <stdio.h>

using namespace WebCore;

namespace WebKit {

bool EditorClient::shouldDeleteRange(Range*)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClient::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClient::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClient::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClient::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldInsertText(String, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldChangeSelectedRange(Range*, Range*, EAffinity, bool)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*)
{
    notImplemented();
    return true;
}

void EditorClient::didBeginEditing()
{
    notImplemented();
}

void EditorClient::respondToChangedContents()
{
    notImplemented();
}

void EditorClient::respondToChangedSelection()
{
    notImplemented();
}

void EditorClient::didEndEditing()
{
    notImplemented();
}

void EditorClient::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClient::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

bool EditorClient::isEditable()
{
    return webkit_web_view_get_editable(m_page);
}

void EditorClient::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClient::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClient::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClient::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClient::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClient::undo()
{
    notImplemented();
}

void EditorClient::redo()
{
    notImplemented();
}

bool EditorClient::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

void EditorClient::pageDestroyed()
{
    delete this;
}

bool EditorClient::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

void EditorClient::toggleContinuousSpellChecking()
{
    notImplemented();
}

void EditorClient::toggleGrammarChecking()
{
}

void EditorClient::handleKeypress(KeyboardEvent* event)
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode())
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent || kevent->isKeyUp())
        return;

    Node* start = frame->selectionController()->start().node();
    if (!start)
        return;

    // FIXME: Use GtkBindingSet instead of this hard-coded switch
    // http://bugs.webkit.org/show_bug.cgi?id=15911

    if (start->isContentEditable()) {
        switch(kevent->WindowsKeyCode()) {
            case VK_BACK:
                frame->editor()->deleteWithDirection(SelectionController::BACKWARD,
                        kevent->ctrlKey() ? WordGranularity : CharacterGranularity, false, true);
                break;
            case VK_DELETE:
                frame->editor()->deleteWithDirection(SelectionController::FORWARD,
                        kevent->ctrlKey() ? WordGranularity : CharacterGranularity, false, true);
                break;
            case VK_LEFT:
                frame->selectionController()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                        SelectionController::LEFT,
                        kevent->ctrlKey() ? WordGranularity : CharacterGranularity,
                        true);
                break;
            case VK_RIGHT:
                frame->selectionController()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                        SelectionController::RIGHT,
                        kevent->ctrlKey() ? WordGranularity : CharacterGranularity,
                        true);
                break;
            case VK_UP:
                frame->selectionController()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                        SelectionController::BACKWARD,
                        kevent->ctrlKey() ? ParagraphGranularity : LineGranularity,
                        true);
                break;
            case VK_DOWN:
                frame->selectionController()->modify(kevent->shiftKey() ? SelectionController::EXTEND : SelectionController::MOVE,
                        SelectionController::FORWARD,
                        kevent->ctrlKey() ? ParagraphGranularity : LineGranularity,
                        true);
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
                else if (kevent->shiftKey())
                    frame->editor()->execCommand("MoveToBeginningOfLineAndModifySelection");
                else
                    frame->editor()->execCommand("MoveToBeginningOfLine");
                break;
            case VK_END:
                if (kevent->ctrlKey())
                    frame->editor()->execCommand("MoveToEndOfDocument");
                else if (kevent->shiftKey())
                    frame->editor()->execCommand("MoveToEndOfLineAndModifySelection");
                else
                    frame->editor()->execCommand("MoveToEndOfLine");
                break;
            case VK_RETURN:
                frame->editor()->execCommand("InsertLineBreak");
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
                    switch (kevent->WindowsKeyCode()) {
                        case VK_B:
                            frame->editor()->execCommand("ToggleBold");
                            break;
                        case VK_I:
                            frame->editor()->execCommand("ToggleItalic");
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
                return;
        }
    }
    event->setDefaultHandled();
}


void EditorClient::handleInputMethodKeypress(KeyboardEvent*)
{
    notImplemented();
}

EditorClient::EditorClient(WebKitWebView* page)
    : m_page(page)
{
}

void EditorClient::textFieldDidBeginEditing(Element*)
{
    notImplemented();
}

void EditorClient::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClient::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClient::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplemented();
    return false;
}

void EditorClient::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClient::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClient::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClient::learnWord(const String&)
{
    notImplemented();
}

void EditorClient::checkSpellingOfString(const UChar*, int, int*, int*)
{
    notImplemented();
}

void EditorClient::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClient::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClient::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClient::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClient::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClient::getGuessesForWord(const String&, Vector<String>&)
{
    notImplemented();
}

void EditorClient::setInputMethodState(bool)
{
}

}

// vim: ts=4 sw=4 et
