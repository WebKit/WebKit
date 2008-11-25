/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
#include "EditorClientWx.h"

#include "EditCommand.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "KeyboardEvent.h"
#include "KeyboardCodes.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "SelectionController.h"

#include "WebFrame.h"
#include "WebFramePrivate.h"
#include "WebView.h"
#include "WebViewPrivate.h"

#include <stdio.h>

namespace WebCore {

EditorClientWx::~EditorClientWx()
{
    m_page = NULL;
}

void EditorClientWx::setPage(Page* page)
{
    m_page = page;
}

void EditorClientWx::pageDestroyed()
{
    notImplemented();
}

bool EditorClientWx::shouldDeleteRange(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldShowDeleteInterface(HTMLElement*)
{
    notImplemented();
    return false;
}

bool EditorClientWx::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientWx::isSelectTrailingWhitespaceEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientWx::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWx::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool EditorClientWx::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWx::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientWx::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientWx::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientWx::isEditable()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin) 
            return webKitWin->IsEditable();
    }
    return false;
}

bool EditorClientWx::shouldBeginEditing(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldEndEditing(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldInsertNode(Node*, Range*,
                                       EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldInsertText(const String&, Range*,
                                       EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldApplyStyle(CSSStyleDeclaration*,
                                       Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldMoveRangeAfterDelete(Range*, Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldChangeSelectedRange(Range* fromRange, Range* toRange, 
                                EAffinity, bool stillSelecting)
{
    notImplemented();
    return true;
}

void EditorClientWx::didBeginEditing()
{
    notImplemented();
}

void EditorClientWx::respondToChangedContents()
{
    notImplemented();
}

void EditorClientWx::didEndEditing()
{
    notImplemented();
}

void EditorClientWx::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientWx::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

void EditorClientWx::registerCommandForUndo(PassRefPtr<EditCommand> command)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin) {
            webKitWin->GetMainFrame()->m_impl->undoStack.append(EditCommandWx(command));
        }
    }
}

void EditorClientWx::registerCommandForRedo(PassRefPtr<EditCommand> command)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin) {
            webKitWin->GetMainFrame()->m_impl->redoStack.insert(0, EditCommandWx(command));
        }
    }
}

void EditorClientWx::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientWx::canUndo() const
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin && webKitWin->GetMainFrame()) {
            return webKitWin->GetMainFrame()->m_impl->undoStack.size() != 0;
        }
    }
    return false;
}

bool EditorClientWx::canRedo() const
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin && webKitWin->GetMainFrame()) {
            return webKitWin->GetMainFrame()->m_impl->redoStack.size() != 0;
        }
    }
    return false;
}

void EditorClientWx::undo()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin && webKitWin->GetMainFrame()) {
            webKitWin->GetMainFrame()->m_impl->undoStack.last().editCommand()->unapply();
            webKitWin->GetMainFrame()->m_impl->undoStack.removeLast();
        }
    }
}

void EditorClientWx::redo()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->platformWidget());
        if (webKitWin && webKitWin->GetMainFrame()) {
            webKitWin->GetMainFrame()->m_impl->redoStack.first().editCommand()->reapply();
            webKitWin->GetMainFrame()->m_impl->redoStack.remove(0);
        }
    }
}

void EditorClientWx::handleInputMethodKeydown(KeyboardEvent* event)
{
// NOTE: we don't currently need to handle this. When key events occur,
// both this method and handleKeyboardEvent get a chance at handling them.
// We might use this method later on for IME-specific handling.
}

void EditorClientWx::handleKeyboardEvent(KeyboardEvent* event)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (kevent->type() != PlatformKeyboardEvent::KeyUp) {
        Node* start = frame->selection()->start().node();
        if (!start || !start->isContentEditable())
            return; 
        
        if (kevent->type() == PlatformKeyboardEvent::Char && !kevent->ctrlKey() && !kevent->altKey()) {
            switch (kevent->windowsVirtualKeyCode()) {
                // we handled these on key down, ignore them for char events
                case VK_BACK:
                case VK_DELETE:
                case VK_LEFT:
                case VK_RIGHT:
                case VK_UP:
                case VK_DOWN:
                case VK_RETURN:
                    break;
                default:
                    frame->editor()->insertText(kevent->text(), event);
            }
            event->setDefaultHandled();
            return; 
        }
        
        switch (kevent->windowsVirtualKeyCode()) {
            case VK_BACK:
                frame->editor()->deleteWithDirection(SelectionController::BACKWARD,
                                                     CharacterGranularity, false, true);
                break;
            case VK_DELETE:
                frame->editor()->deleteWithDirection(SelectionController::FORWARD,
                                                     CharacterGranularity, false, true);
                break;
            case VK_LEFT:
                frame->editor()->command("MoveLeft").execute();
                break;
            case VK_RIGHT:
                frame->editor()->command("MoveRight").execute();
                break;
            case VK_UP:
                frame->editor()->command("MoveUp").execute();
                break;
            case VK_DOWN:
                frame->editor()->command("MoveDown").execute();
                break;
            case VK_RETURN:
                frame->editor()->command("InsertLineBreak").execute();
            default:
                break;
        }
            
        event->setDefaultHandled();
    }
}

void EditorClientWx::textFieldDidBeginEditing(Element*)
{
    notImplemented();
}

void EditorClientWx::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClientWx::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClientWx::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplemented();
    return false;
}

void EditorClientWx::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientWx::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientWx::respondToChangedSelection()
{
    notImplemented();
}

void EditorClientWx::ignoreWordInSpellDocument(const String&) 
{ 
    notImplemented(); 
}

void EditorClientWx::learnWord(const String&) 
{ 
    notImplemented(); 
}

void EditorClientWx::checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength) 
{ 
    notImplemented(); 
}

void EditorClientWx::checkGrammarOfString(const UChar*, int length, Vector<GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) 
{ 
    notImplemented(); 
}

void EditorClientWx::updateSpellingUIWithGrammarString(const String&, const GrammarDetail& detail) 
{ 
    notImplemented(); 
}

void EditorClientWx::updateSpellingUIWithMisspelledWord(const String&) 
{ 
    notImplemented(); 
}

void EditorClientWx::showSpellingUI(bool show) 
{ 
    notImplemented(); 
}

bool EditorClientWx::spellingUIIsShowing() 
{ 
    notImplemented(); 
    return false;
}

void EditorClientWx::getGuessesForWord(const String&, Vector<String>& guesses) 
{ 
    notImplemented(); 
}

void EditorClientWx::setInputMethodState(bool enabled)
{
    notImplemented();
}

}
