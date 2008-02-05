/*
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

using namespace WebCore;

namespace WebKit {

static void imContextCommitted(GtkIMContext* context, const char* str, EditorClient* client)
{
    Frame* frame = core(client->m_webView)->focusController()->focusedOrMainFrame();
    frame->editor()->insertTextWithoutSendingTextEvent(str, false);
}

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
    return webkit_web_view_get_editable(m_webView);
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

void EditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    Frame* frame = core(m_webView)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode())
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent || kevent->type() == PlatformKeyboardEvent::KeyUp)
        return;

    Node* start = frame->selectionController()->start().node();
    if (!start)
        return;

    // FIXME: Use GtkBindingSet instead of this hard-coded switch
    // http://bugs.webkit.org/show_bug.cgi?id=15911

    if (start->isContentEditable()) {
        switch (kevent->windowsVirtualKeyCode()) {
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
                frame->editor()->command("MovePageUp").execute();
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->command("MovePageDown").execute();
                break;
            case VK_HOME:
                if (kevent->ctrlKey() && kevent->shiftKey())
                    frame->editor()->command("MoveToBeginningOfDocumentAndModifySelection").execute();
                else if (kevent->ctrlKey())
                    frame->editor()->command("MoveToBeginningOfDocument").execute();
                else if (kevent->shiftKey())
                    frame->editor()->command("MoveToBeginningOfLineAndModifySelection").execute();
                else
                    frame->editor()->command("MoveToBeginningOfLine").execute();
                break;
            case VK_END:
                if (kevent->ctrlKey() && kevent->shiftKey())
                    frame->editor()->command("MoveToEndOfDocumentAndModifySelection").execute();
                else if (kevent->ctrlKey())
                    frame->editor()->command("MoveToEndOfDocument").execute();
                else if (kevent->shiftKey())
                    frame->editor()->command("MoveToEndOfLineAndModifySelection").execute();
                else
                    frame->editor()->command("MoveToEndOfLine").execute();
                break;
            case VK_RETURN:
                frame->editor()->command("InsertLineBreak").execute();
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
                        case VK_B:
                            frame->editor()->command("ToggleBold").execute();
                            break;
                        case VK_I:
                            frame->editor()->command("ToggleItalic").execute();
                            break;
                        case VK_Y:
                            frame->editor()->command("Redo").execute();
                            break;
                        case VK_Z:
                            frame->editor()->command("Undo").execute();
                            break;
                        default:
                            return;
                    }
                } else return;
        }
    } else {
        switch (kevent->windowsVirtualKeyCode()) {
            case VK_UP:
                frame->editor()->command("MoveUp").execute();
                break;
            case VK_DOWN:
                frame->editor()->command("MoveDown").execute();
                break;
            case VK_PRIOR:  // PageUp
                frame->editor()->command("MovePageUp").execute();
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->command("MovePageDown").execute();
                break;
            case VK_HOME:
                if (kevent->ctrlKey())
                    frame->editor()->command("MoveToBeginningOfDocument").execute();
                break;
            case VK_END:
                if (kevent->ctrlKey())
                    frame->editor()->command("MoveToEndOfDocument").execute();
                break;
            default:
                return;
        }
    }
    event->setDefaultHandled();
}


void EditorClient::handleInputMethodKeydown(KeyboardEvent*)
{
    notImplemented();
}

EditorClient::EditorClient(WebKitWebView* webView)
    : m_webView(webView)
{
    WebKitWebViewPrivate* priv = m_webView->priv;
    g_signal_connect(priv->imContext, "commit", G_CALLBACK(imContextCommitted), this);
}

EditorClient::~EditorClient()
{
    WebKitWebViewPrivate* priv = m_webView->priv;
    g_signal_handlers_disconnect_by_func(priv->imContext, (gpointer)imContextCommitted, this);
}

void EditorClient::textFieldDidBeginEditing(Element*)
{
    gtk_im_context_focus_in(WEBKIT_WEB_VIEW_GET_PRIVATE(m_webView)->imContext);
}

void EditorClient::textFieldDidEndEditing(Element*)
{
    WebKitWebViewPrivate* priv = m_webView->priv;

    gtk_im_context_focus_out(priv->imContext);
#ifdef MAEMO_CHANGES
    hildon_gtk_im_context_hide(priv->imContext);
#endif
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
