/*
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2008 Nuanti Ltd.
 *  Copyright (C) 2009 Diego Escalante Urrelo <diegoe@gnome.org>
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

#include "CString.h"
#include "EditCommand.h"
#include "Editor.h"
#include <enchant.h>
#include "FocusController.h"
#include "Frame.h"
#include <glib.h>
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "markup.h"
#include "webkitprivate.h"

using namespace WebCore;

namespace WebKit {

static void imContextCommitted(GtkIMContext* context, const gchar* str, EditorClient* client)
{
    Frame* targetFrame = core(client->m_webView)->focusController()->focusedOrMainFrame();

    if (!targetFrame || !targetFrame->editor()->canEdit())
        return;

    Editor* editor = targetFrame->editor();

    String commitString = String::fromUTF8(str);
    editor->confirmComposition(commitString);
}

static void imContextPreeditChanged(GtkIMContext* context, EditorClient* client)
{
    Frame* frame = core(client->m_webView)->focusController()->focusedOrMainFrame();
    Editor* editor = frame->editor();

    gchar* preedit = NULL;
    gint cursorPos = 0;
    // We ignore the provided PangoAttrList for now.
    gtk_im_context_get_preedit_string(context, &preedit, NULL, &cursorPos);
    String preeditString = String::fromUTF8(preedit);
    g_free(preedit);

    // setComposition() will replace the user selection if passed an empty
    // preedit. We don't want this to happen.
    if (preeditString.isEmpty() && !editor->hasComposition())
        return;

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, preeditString.length(), Color(0, 0, 0), false));
    editor->setComposition(preeditString, underlines, cursorPos, 0);
}

void EditorClient::setInputMethodState(bool active)
{
    WebKitWebViewPrivate* priv = m_webView->priv;

    if (active)
        gtk_im_context_focus_in(priv->imContext);
    else
        gtk_im_context_focus_out(priv->imContext);

#ifdef MAEMO_CHANGES
    if (active)
        hildon_gtk_im_context_show(priv->imContext);
    else
        hildon_gtk_im_context_hide(priv->imContext);
#endif
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
    WebKitWebSettings* settings = webkit_web_view_get_settings(m_webView);

    gboolean enabled;
    g_object_get(settings, "enable-spell-checking", &enabled, NULL);

    return enabled;
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

bool EditorClient::shouldInsertText(const String&, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldChangeSelectedRange(Range*, Range*, EAffinity, bool)
{
    notImplemented();
    return true;
}

bool EditorClient::shouldApplyStyle(WebCore::CSSStyleDeclaration*, WebCore::Range*)
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

static void clipboard_get_contents_cb(GtkClipboard* clipboard, GtkSelectionData* selection_data, guint info, gpointer data)
{
    WebKitWebView* webView = reinterpret_cast<WebKitWebView*>(data);
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    PassRefPtr<Range> selectedRange = frame->selection()->toNormalizedRange();

    if (static_cast<gint>(info) == WEBKIT_WEB_VIEW_TARGET_INFO_HTML) {
        String markup = createMarkup(selectedRange.get(), 0, AnnotateForInterchange);
        gtk_selection_data_set(selection_data, selection_data->target, 8,
                               reinterpret_cast<const guchar*>(markup.utf8().data()), markup.utf8().length());
    } else {
        String text = selectedRange->text();
        gtk_selection_data_set_text(selection_data, text.utf8().data(), text.utf8().length());
    }
}

static void clipboard_clear_contents_cb(GtkClipboard* clipboard, gpointer data)
{
    WebKitWebView* webView = reinterpret_cast<WebKitWebView*>(data);
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();

    // Collapse the selection without clearing it
    frame->selection()->setBase(frame->selection()->extent(), frame->selection()->affinity());
}

void EditorClient::respondToChangedSelection()
{
    WebKitWebViewPrivate* priv = m_webView->priv;
    Frame* targetFrame = core(m_webView)->focusController()->focusedOrMainFrame();

    if (!targetFrame)
        return;

    if (targetFrame->editor()->ignoreCompositionSelectionChange())
        return;

    GtkClipboard* clipboard = gtk_widget_get_clipboard(GTK_WIDGET(m_webView), GDK_SELECTION_PRIMARY);
    if (targetFrame->selection()->isRange()) {
        GtkTargetList* targetList = webkit_web_view_get_copy_target_list(m_webView);
        gint targetCount;
        GtkTargetEntry* targets = gtk_target_table_new_from_list(targetList, &targetCount);
        gtk_clipboard_set_with_owner(clipboard, targets, targetCount,
                                     clipboard_get_contents_cb, clipboard_clear_contents_cb, G_OBJECT(m_webView));
        gtk_target_table_free(targets, targetCount);
    } else if (gtk_clipboard_get_owner(clipboard) == G_OBJECT(m_webView))
        gtk_clipboard_clear(clipboard);

    if (!targetFrame->editor()->hasComposition())
        return;

    unsigned start;
    unsigned end;
    if (!targetFrame->editor()->getCompositionSelection(start, end)) {
        // gtk_im_context_reset() clears the composition for us.
        gtk_im_context_reset(priv->imContext);
        targetFrame->editor()->confirmCompositionWithoutDisturbingSelection();
    }
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

bool EditorClient::isSelectTrailingWhitespaceEnabled()
{
    notImplemented();
    return false;
}

void EditorClient::toggleContinuousSpellChecking()
{
    WebKitWebSettings* settings = webkit_web_view_get_settings(m_webView);

    gboolean enabled;
    g_object_get(settings, "enable-spell-checking", &enabled, NULL);

    g_object_set(settings, "enable-spell-checking", !enabled, NULL);
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

    Node* start = frame->selection()->start().node();
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
                frame->editor()->command(kevent->shiftKey() ? "MovePageUpAndModifySelection" : "MovePageUp").execute();
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->command(kevent->shiftKey() ? "MovePageDownAndModifySelection" : "MovePageDown").execute();
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

void EditorClient::handleInputMethodKeydown(KeyboardEvent* event)
{
    Frame* targetFrame = core(m_webView)->focusController()->focusedOrMainFrame();
    if (!targetFrame || !targetFrame->editor()->canEdit())
        return;

    WebKitWebViewPrivate* priv = m_webView->priv;
    // TODO: Dispatch IE-compatible text input events for IM events.
    if (gtk_im_context_filter_keypress(priv->imContext, event->keyEvent()->gdkEventKey()))
        event->setDefaultHandled();
}

EditorClient::EditorClient(WebKitWebView* webView)
    : m_webView(webView)
{
    WebKitWebViewPrivate* priv = m_webView->priv;
    g_signal_connect(priv->imContext, "commit", G_CALLBACK(imContextCommitted), this);
    g_signal_connect(priv->imContext, "preedit-changed", G_CALLBACK(imContextPreeditChanged), this);
}

EditorClient::~EditorClient()
{
    WebKitWebViewPrivate* priv = m_webView->priv;
    g_signal_handlers_disconnect_by_func(priv->imContext, (gpointer)imContextCommitted, this);
    g_signal_handlers_disconnect_by_func(priv->imContext, (gpointer)imContextPreeditChanged, this);
}

void EditorClient::textFieldDidBeginEditing(Element*)
{
}

void EditorClient::textFieldDidEndEditing(Element*)
{
}

void EditorClient::textDidChangeInTextField(Element*)
{
}

bool EditorClient::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
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

void EditorClient::checkSpellingOfString(const UChar* text, int length, int* misspellingLocation, int* misspellingLength)
{
    gchar* ctext = g_utf16_to_utf8(const_cast<gunichar2*>(text), length, 0, 0, 0);
    int utflen = g_utf8_strlen(ctext, -1);

    PangoLanguage* language = pango_language_get_default();
    PangoLogAttr* attrs = g_new(PangoLogAttr, utflen+1);

    // pango_get_log_attrs uses an aditional position at the end of the text.
    pango_get_log_attrs(ctext, -1, -1, language, attrs, utflen+1);

    for (int i = 0; i < length+1; i++) {
        // We go through each character until we find an is_word_start,
        // then we get into an inner loop to find the is_word_end corresponding
        // to it.
        if (attrs[i].is_word_start) {
            int start = i;
            int end = i;
            int wordLength;
            GSList* langs = webkit_web_settings_get_spell_languages(m_webView);

            while (attrs[end].is_word_end < 1)
                end++;

            wordLength = end - start;
            // Set the iterator to be at the current word end, so we don't
            // check characters twice.
            i = end;

            for (; langs; langs = langs->next) {
                SpellLanguage* lang = static_cast<SpellLanguage*>(langs->data);
                gchar* cstart = g_utf8_offset_to_pointer(ctext, start);
                gint bytes = static_cast<gint>(g_utf8_offset_to_pointer(ctext, end) - cstart);
                gchar* word = g_new0(gchar, bytes+1);
                int result;

                g_utf8_strncpy(word, cstart, end - start);

                result = enchant_dict_check(lang->speller, word, -1);
                if (result) {
                    *misspellingLocation = start;
                    *misspellingLength = wordLength;
                } else {
                    // Stop checking, this word is ok in at least one dict.
                    *misspellingLocation = -1;
                    *misspellingLength = 0;
                    break;
                }
            }
        }
    }
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

}
