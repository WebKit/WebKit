/*
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2008 Nuanti Ltd.
 *  Copyright (C) 2009 Diego Escalante Urrelo <diegoe@gnome.org>
 *  Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *  Copyright (C) 2009, Igalia S.L.
 *  Copyright (C) 2010, Martin Robinson <mrobinson@webkit.org>
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

#include "DataObjectGtk.h"
#include "EditCommand.h"
#include "Editor.h"
#include <enchant.h>
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include <glib.h>
#include "KeyboardEvent.h"
#include "markup.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PasteboardHelperGtk.h"
#include "PlatformKeyboardEvent.h"
#include "WindowsKeyboardCodes.h"
#include "webkitmarshal.h"
#include "webkitprivate.h"
#include <wtf/text/CString.h>

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory.  This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action.
#define maximumUndoStackDepth 1000

using namespace WebCore;

namespace WebKit {

static void imContextCommitted(GtkIMContext* context, const gchar* compositionString, EditorClient* client)
{
    Frame* frame = core(client->webView())->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;

    // If this signal fires during a keydown event when we are not in the middle
    // of a composition, then treat this 'commit' as a normal key event and just
    // change the editable area right before the keypress event.
    if (client->treatContextCommitAsKeyEvent()) {
        client->updatePendingComposition(compositionString);
        return;
    }

    frame->editor()->confirmComposition(String::fromUTF8(compositionString));
    client->clearPendingComposition();
}

static void imContextPreeditChanged(GtkIMContext* context, EditorClient* client)
{
    Frame* frame = core(client->webView())->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;

    // We ignore the provided PangoAttrList for now.
    GOwnPtr<gchar> newPreedit(0);
    gtk_im_context_get_preedit_string(context, &newPreedit.outPtr(), 0, 0);

    String preeditString = String::fromUTF8(newPreedit.get());
    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, preeditString.length(), Color(0, 0, 0), false));
    frame->editor()->setComposition(preeditString, underlines, 0, 0);
}

void EditorClient::updatePendingComposition(const gchar* newComposition)
{
    // The IMContext may signal more than one completed composition in a row,
    // in which case we want to append them, rather than overwrite the old one.
    if (!m_pendingComposition)
        m_pendingComposition.set(g_strdup(newComposition));
    else
        m_pendingComposition.set(g_strconcat(m_pendingComposition.get(), newComposition, NULL));
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
    clearPendingComposition();

    notImplemented();
    return true;
}

bool EditorClient::shouldEndEditing(WebCore::Range*)
{
    clearPendingComposition();

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

static WebKitWebView* viewSettingClipboard = 0;
static void collapseSelection(GtkClipboard* clipboard, WebKitWebView* webView)
{
    if (viewSettingClipboard && viewSettingClipboard == webView)
        return;

    WebCore::Page* corePage = core(webView);
    if (!corePage || !corePage->focusController())
        return;

    Frame* frame = corePage->focusController()->focusedOrMainFrame();

    // Collapse the selection without clearing it
    ASSERT(frame);
    frame->selection()->setBase(frame->selection()->extent(), frame->selection()->affinity());
}

void EditorClient::respondToChangedSelection()
{
    WebKitWebViewPrivate* priv = m_webView->priv;
    WebCore::Page* corePage = core(m_webView);
    Frame* targetFrame = corePage->focusController()->focusedOrMainFrame();

    if (!targetFrame)
        return;

    if (targetFrame->editor()->ignoreCompositionSelectionChange())
        return;

#if PLATFORM(X11)
    GtkClipboard* clipboard = gtk_widget_get_clipboard(GTK_WIDGET(m_webView), GDK_SELECTION_PRIMARY);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);

    if (targetFrame->selection()->isRange()) {
        dataObject->clear();
        dataObject->setRange(targetFrame->selection()->toNormalizedRange());

        viewSettingClipboard = m_webView;
        GClosure* callback = g_cclosure_new_object(G_CALLBACK(collapseSelection), G_OBJECT(m_webView));
        g_closure_set_marshal(callback, g_cclosure_marshal_VOID__VOID);
        pasteboardHelperInstance()->writeClipboardContents(clipboard, callback);
        viewSettingClipboard = 0;
    }
#endif

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

void EditorClient::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand> command)
{
    if (undoStack.size() == maximumUndoStackDepth)
        undoStack.removeFirst();
    if (!m_isInRedo)
        redoStack.clear();
    undoStack.append(command);
}

void EditorClient::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand> command)
{
    redoStack.append(command);
}

void EditorClient::clearUndoRedoOperations()
{
    undoStack.clear();
    redoStack.clear();
}

bool EditorClient::canUndo() const
{
    return !undoStack.isEmpty();
}

bool EditorClient::canRedo() const
{
    return !redoStack.isEmpty();
}

void EditorClient::undo()
{
    if (canUndo()) {
        RefPtr<WebCore::EditCommand> command(*(--undoStack.end()));
        undoStack.remove(--undoStack.end());
        // unapply will call us back to push this command onto the redo stack.
        command->unapply();
    }
}

void EditorClient::redo()
{
    if (canRedo()) {
        RefPtr<WebCore::EditCommand> command(*(--redoStack.end()));
        redoStack.remove(--redoStack.end());

        ASSERT(!m_isInRedo);
        m_isInRedo = true;
        // reapply will call us back to push this command onto the undo stack.
        command->reapply();
        m_isInRedo = false;
    }
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

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;

struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

struct KeyPressEntry {
    unsigned charCode;
    unsigned modifiers;
    const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
    { VK_LEFT,   0,                  "MoveLeft"                                    },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"                  },
    { VK_LEFT,   CtrlKey,            "MoveWordLeft"                                },
    { VK_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"              },
    { VK_RIGHT,  0,                  "MoveRight"                                   },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"                 },
    { VK_RIGHT,  CtrlKey,            "MoveWordRight"                               },
    { VK_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"             },
    { VK_UP,     0,                  "MoveUp"                                      },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection"                    },
    { VK_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"                },
    { VK_DOWN,   0,                  "MoveDown"                                    },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection"                  },
    { VK_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"              },
    { VK_PRIOR,  0,                  "MovePageUp"                                  },
    { VK_NEXT,   0,                  "MovePageDown"                                },
    { VK_HOME,   0,                  "MoveToBeginningOfLine"                       },
    { VK_HOME,   ShiftKey,           "MoveToBeginningOfLineAndModifySelection"     },
    { VK_HOME,   CtrlKey,            "MoveToBeginningOfDocument"                   },
    { VK_HOME,   CtrlKey | ShiftKey, "MoveToBeginningOfDocumentAndModifySelection" },

    { VK_END,    0,                  "MoveToEndOfLine"                             },
    { VK_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"           },
    { VK_END,    CtrlKey,            "MoveToEndOfDocument"                         },
    { VK_END,    CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection"       },

    { VK_BACK,   0,                  "DeleteBackward"                              },
    { VK_BACK,   ShiftKey,           "DeleteBackward"                              },
    { VK_DELETE, 0,                  "DeleteForward"                               },
    { VK_BACK,   CtrlKey,            "DeleteWordBackward"                          },
    { VK_DELETE, CtrlKey,            "DeleteWordForward"                           },

    { 'B',       CtrlKey,            "ToggleBold"                                  },
    { 'I',       CtrlKey,            "ToggleItalic"                                },

    { VK_ESCAPE, 0,                  "Cancel"                                      },
    { VK_OEM_PERIOD, CtrlKey,        "Cancel"                                      },
    { VK_TAB,    0,                  "InsertTab"                                   },
    { VK_TAB,    ShiftKey,           "InsertBacktab"                               },
    { VK_RETURN, 0,                  "InsertNewline"                               },
    { VK_RETURN, CtrlKey,            "InsertNewline"                               },
    { VK_RETURN, AltKey,             "InsertNewline"                               },
    { VK_RETURN, AltKey | ShiftKey,  "InsertNewline"                               },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                   },
    { '\t',   ShiftKey,           "InsertBacktab"                               },
    { '\r',   0,                  "InsertNewline"                               },
    { '\r',   CtrlKey,            "InsertNewline"                               },
    { '\r',   AltKey,             "InsertNewline"                               },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

static const char* interpretEditorCommandKeyEvent(const KeyboardEvent* evt)
{
    ASSERT(evt->type() == eventNames().keydownEvent || evt->type() == eventNames().keypressEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < G_N_ELEMENTS(keyDownEntries); i++)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (unsigned i = 0; i < G_N_ELEMENTS(keyPressEntries); i++)
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (evt->shiftKey())
        modifiers |= ShiftKey;
    if (evt->altKey())
        modifiers |= AltKey;
    if (evt->ctrlKey())
        modifiers |= CtrlKey;

    if (evt->type() == eventNames().keydownEvent) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

void EditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    Node* node = event->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    if (!platformEvent)
        return;

    // Don't allow editor commands or text insertion for nodes that
    // cannot edit, unless we are in caret mode.
    if (!frame->editor()->canEdit() && !(frame->settings() && frame->settings()->caretBrowsingEnabled()))
        return;

    const gchar* editorCommandString = interpretEditorCommandKeyEvent(event);
    if (editorCommandString) {
        Editor::Command command = frame->editor()->command(editorCommandString);

        // On editor commands from key down events, we only want to let the event bubble up to
        // the DOM if it inserts text. If it doesn't insert text (e.g. Tab that changes focus)
        // we just want WebKit to handle it immediately without a DOM event.
        if (platformEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
            if (!command.isTextInsertion() && command.execute(event))
                event->setDefaultHandled();

            clearPendingComposition();
            return;
        }

        if (command.execute(event)) {
            clearPendingComposition();
            event->setDefaultHandled();
            return;
        }
    }

    // This is just a normal text insertion, so wait to execute the insertion
    // until a keypress event happens. This will ensure that the insertion will not
    // be reflected in the contents of the field until the keyup DOM event.
    if (event->type() == eventNames().keypressEvent) {

        // If we have a pending composition at this point, it happened while
        // filtering a keypress, so we treat it as a normal text insertion.
        // This will also ensure that if the keypress event handler changed the
        // currently focused node, the text is still inserted into the original
        // node (insertText() has this logic, but confirmComposition() does not).
        if (m_pendingComposition) {
            frame->editor()->insertText(String::fromUTF8(m_pendingComposition.get()), event);
            clearPendingComposition();
            event->setDefaultHandled();

        } else {
            // Don't insert null or control characters as they can result in unexpected behaviour
            if (event->charCode() < ' ')
                return;

            // Don't insert anything if a modifier is pressed
            if (platformEvent->ctrlKey() || platformEvent->altKey())
                return;

            if (frame->editor()->insertText(platformEvent->text(), event))
                event->setDefaultHandled();
        }
    }
}

void EditorClient::handleInputMethodKeydown(KeyboardEvent* event)
{
    Frame* targetFrame = core(m_webView)->focusController()->focusedOrMainFrame();
    if (!targetFrame || !targetFrame->editor()->canEdit())
        return;

    WebKitWebViewPrivate* priv = m_webView->priv;


    // Some IM contexts (e.g. 'simple') will act as if they filter every
    // keystroke and just issue a 'commit' signal during handling. In situations
    // where the 'commit' signal happens during filtering and there is no active
    // composition, act as if the keystroke was not filtered. The one exception to
    // this is when the keyval parameter of the GdkKeyEvent is 0, which is often
    // a key event sent by the IM context for committing the current composition.

    // Here is a typical sequence of events for the 'simple' context:
    // 1. GDK key press event -> webkit_web_view_key_press_event
    // 2. Keydown event -> EditorClient::handleInputMethodKeydown
    //     gtk_im_context_filter_keypress returns true, but there is a pending
    //     composition so event->preventDefault is not called (below).
    // 3. Keydown event bubbles through the DOM
    // 4. Keydown event -> EditorClient::handleKeyboardEvent
    //     No action taken.
    // 4. GDK key release event -> webkit_web_view_key_release_event
    // 5. gtk_im_context_filter_keypress is called on the release event.
    //     Simple does not filter most key releases, so the event continues.
    // 6. Keypress event bubbles through the DOM.
    // 7. Keypress event -> EditorClient::handleKeyboardEvent
    //     pending composition is inserted.
    // 8. Keyup event bubbles through the DOM.
    // 9. Keyup event -> EditorClient::handleKeyboardEvent
    //     No action taken.

    // There are two situations where we do filter the keystroke:
    // 1. The IMContext instructed us to filter and we have no pending composition.
    // 2. The IMContext did not instruct us to filter, but the keystroke caused a
    //    composition in progress to finish. It seems that sometimes SCIM will finish
    //    a composition and not mark the keystroke as filtered.
    m_treatContextCommitAsKeyEvent = (!targetFrame->editor()->hasComposition())
         && event->keyEvent()->gdkEventKey()->keyval;
    clearPendingComposition();
    if ((gtk_im_context_filter_keypress(priv->imContext, event->keyEvent()->gdkEventKey()) && !m_pendingComposition)
        || (!m_treatContextCommitAsKeyEvent && !targetFrame->editor()->hasComposition()))
        event->preventDefault();

    m_treatContextCommitAsKeyEvent = false;
}

EditorClient::EditorClient(WebKitWebView* webView)
    : m_isInRedo(false)
    , m_webView(webView)
    , m_treatContextCommitAsKeyEvent(false)
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

void EditorClient::ignoreWordInSpellDocument(const String& text)
{
    GSList* dicts = webkit_web_settings_get_enchant_dicts(m_webView);

    for (; dicts; dicts = dicts->next) {
        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);

        enchant_dict_add_to_session(dict, text.utf8().data(), -1);
    }
}

void EditorClient::learnWord(const String& text)
{
    GSList* dicts = webkit_web_settings_get_enchant_dicts(m_webView);

    for (; dicts; dicts = dicts->next) {
        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);

        enchant_dict_add_to_personal(dict, text.utf8().data(), -1);
    }
}

void EditorClient::checkSpellingOfString(const UChar* text, int length, int* misspellingLocation, int* misspellingLength)
{
    GSList* dicts = webkit_web_settings_get_enchant_dicts(m_webView);
    if (!dicts)
        return;

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

            while (attrs[end].is_word_end < 1)
                end++;

            wordLength = end - start;
            // Set the iterator to be at the current word end, so we don't
            // check characters twice.
            i = end;

            for (; dicts; dicts = dicts->next) {
                EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);
                gchar* cstart = g_utf8_offset_to_pointer(ctext, start);
                gint bytes = static_cast<gint>(g_utf8_offset_to_pointer(ctext, end) - cstart);
                gchar* word = g_new0(gchar, bytes+1);
                int result;

                g_utf8_strncpy(word, cstart, end - start);

                result = enchant_dict_check(dict, word, -1);
                g_free(word);
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

    g_free(attrs);
    g_free(ctext);
}

String EditorClient::getAutoCorrectSuggestionForMisspelledWord(const String& inputWord)
{
    // This method can be implemented using customized algorithms for the particular browser.
    // Currently, it computes an empty string.
    return String();
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

void EditorClient::getGuessesForWord(const String& word, WTF::Vector<String>& guesses)
{
    GSList* dicts = webkit_web_settings_get_enchant_dicts(m_webView);
    guesses.clear();

    for (; dicts; dicts = dicts->next) {
        size_t numberOfSuggestions;
        size_t i;

        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);
        gchar** suggestions = enchant_dict_suggest(dict, word.utf8().data(), -1, &numberOfSuggestions);

        for (i = 0; i < numberOfSuggestions && i < 10; i++)
            guesses.append(String::fromUTF8(suggestions[i]));

        if (numberOfSuggestions > 0)
            enchant_dict_free_suggestions(dict, suggestions);
    }
}

}
