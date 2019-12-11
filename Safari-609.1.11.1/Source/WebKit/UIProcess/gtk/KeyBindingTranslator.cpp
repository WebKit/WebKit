/*
 * Copyright (C) 2010, 2011 Igalia S.L.
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
#include "KeyBindingTranslator.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

namespace WebKit {

static void backspaceCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "backspace");
    translator->addPendingEditorCommand("DeleteBackward");
}

static void selectAllCallback(GtkWidget* widget, gboolean select, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "select-all");
    translator->addPendingEditorCommand(select ? "SelectAll" : "Unselect");
}

static void cutClipboardCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "cut-clipboard");
    translator->addPendingEditorCommand("Cut");
}

static void copyClipboardCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "copy-clipboard");
    translator->addPendingEditorCommand("Copy");
}

static void pasteClipboardCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "paste-clipboard");
    translator->addPendingEditorCommand("Paste");
}

static void toggleOverwriteCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "toggle-overwrite");
    translator->addPendingEditorCommand("OverWrite");
}

#if GTK_CHECK_VERSION(3, 24, 0)
static void insertEmojiCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "insert-emoji");
    translator->addPendingEditorCommand("GtkInsertEmoji");
}
#endif

// GTK+ will still send these signals to the web view. So we can safely stop signal
// emission without breaking accessibility.
static void popupMenuCallback(GtkWidget* widget, KeyBindingTranslator*)
{
    g_signal_stop_emission_by_name(widget, "popup-menu");
}

static void showHelpCallback(GtkWidget* widget, KeyBindingTranslator*)
{
    g_signal_stop_emission_by_name(widget, "show-help");
}

static const char* const gtkDeleteCommands[][2] = {
    { "DeleteBackward",               "DeleteForward"                        }, // Characters
    { "DeleteWordBackward",           "DeleteWordForward"                    }, // Word ends
    { "DeleteWordBackward",           "DeleteWordForward"                    }, // Words
    { "DeleteToBeginningOfLine",      "DeleteToEndOfLine"                    }, // Lines
    { "DeleteToBeginningOfLine",      "DeleteToEndOfLine"                    }, // Line ends
    { "DeleteToBeginningOfParagraph", "DeleteToEndOfParagraph"               }, // Paragraph ends
    { "DeleteToBeginningOfParagraph", "DeleteToEndOfParagraph"               }, // Paragraphs
    { 0,                              0                                      } // Whitespace (M-\ in Emacs)
};

static void deleteFromCursorCallback(GtkWidget* widget, GtkDeleteType deleteType, gint count, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "delete-from-cursor");
    int direction = count > 0 ? 1 : 0;

    // Ensuring that deleteType <= G_N_ELEMENTS here results in a compiler warning
    // that the condition is always true.

    if (deleteType == GTK_DELETE_WORDS) {
        if (!direction) {
            translator->addPendingEditorCommand("MoveWordForward");
            translator->addPendingEditorCommand("MoveWordBackward");
        } else {
            translator->addPendingEditorCommand("MoveWordBackward");
            translator->addPendingEditorCommand("MoveWordForward");
        }
    } else if (deleteType == GTK_DELETE_DISPLAY_LINES) {
        if (!direction)
            translator->addPendingEditorCommand("MoveToBeginningOfLine");
        else
            translator->addPendingEditorCommand("MoveToEndOfLine");
    } else if (deleteType == GTK_DELETE_PARAGRAPHS) {
        if (!direction)
            translator->addPendingEditorCommand("MoveToBeginningOfParagraph");
        else
            translator->addPendingEditorCommand("MoveToEndOfParagraph");
    }

    const char* rawCommand = gtkDeleteCommands[deleteType][direction];
    if (!rawCommand)
        return;

    for (int i = 0; i < abs(count); i++)
        translator->addPendingEditorCommand(rawCommand);
}

static const char* const gtkMoveCommands[][4] = {
    { "MoveBackward",                                   "MoveForward",
      "MoveBackwardAndModifySelection",                 "MoveForwardAndModifySelection"             }, // Forward/backward grapheme
    { "MoveLeft",                                       "MoveRight",
      "MoveBackwardAndModifySelection",                 "MoveForwardAndModifySelection"             }, // Left/right grapheme
    { "MoveWordBackward",                               "MoveWordForward",
      "MoveWordBackwardAndModifySelection",             "MoveWordForwardAndModifySelection"         }, // Forward/backward word
    { "MoveUp",                                         "MoveDown",
      "MoveUpAndModifySelection",                       "MoveDownAndModifySelection"                }, // Up/down line
    { "MoveToBeginningOfLine",                          "MoveToEndOfLine",
      "MoveToBeginningOfLineAndModifySelection",        "MoveToEndOfLineAndModifySelection"         }, // Up/down line ends
    { 0,                                                0,
      "MoveParagraphBackwardAndModifySelection",        "MoveParagraphForwardAndModifySelection"    }, // Up/down paragraphs
    { "MoveToBeginningOfParagraph",                     "MoveToEndOfParagraph",
      "MoveToBeginningOfParagraphAndModifySelection",   "MoveToEndOfParagraphAndModifySelection"    }, // Up/down paragraph ends.
    { "MovePageUp",                                     "MovePageDown",
      "MovePageUpAndModifySelection",                   "MovePageDownAndModifySelection"            }, // Up/down page
    { "MoveToBeginningOfDocument",                      "MoveToEndOfDocument",
      "MoveToBeginningOfDocumentAndModifySelection",    "MoveToEndOfDocumentAndModifySelection"     }, // Begin/end of buffer
    { 0,                                                0,
      0,                                                0                                           } // Horizontal page movement
};

static void moveCursorCallback(GtkWidget* widget, GtkMovementStep step, gint count, gboolean extendSelection, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "move-cursor");
    int direction = count > 0 ? 1 : 0;
    if (extendSelection)
        direction += 2;

    if (static_cast<unsigned>(step) >= G_N_ELEMENTS(gtkMoveCommands))
        return;

    const char* rawCommand = gtkMoveCommands[step][direction];
    if (!rawCommand)
        return;

    for (int i = 0; i < abs(count); i++)
        translator->addPendingEditorCommand(rawCommand);
}

KeyBindingTranslator::KeyBindingTranslator()
    : m_nativeWidget(gtk_text_view_new())
{
    g_signal_connect(m_nativeWidget.get(), "backspace", G_CALLBACK(backspaceCallback), this);
    g_signal_connect(m_nativeWidget.get(), "cut-clipboard", G_CALLBACK(cutClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "copy-clipboard", G_CALLBACK(copyClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "paste-clipboard", G_CALLBACK(pasteClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "select-all", G_CALLBACK(selectAllCallback), this);
    g_signal_connect(m_nativeWidget.get(), "move-cursor", G_CALLBACK(moveCursorCallback), this);
    g_signal_connect(m_nativeWidget.get(), "delete-from-cursor", G_CALLBACK(deleteFromCursorCallback), this);
    g_signal_connect(m_nativeWidget.get(), "toggle-overwrite", G_CALLBACK(toggleOverwriteCallback), this);
    g_signal_connect(m_nativeWidget.get(), "popup-menu", G_CALLBACK(popupMenuCallback), this);
    g_signal_connect(m_nativeWidget.get(), "show-help", G_CALLBACK(showHelpCallback), this);
#if GTK_CHECK_VERSION(3, 24, 0)
    g_signal_connect(m_nativeWidget.get(), "insert-emoji", G_CALLBACK(insertEmojiCallback), this);
#endif
}

struct KeyCombinationEntry {
    unsigned gdkKeyCode;
    unsigned state;
    const char* name;
};

static const KeyCombinationEntry customKeyBindings[] = {
    { GDK_KEY_b,         GDK_CONTROL_MASK,               "ToggleBold"    },
    { GDK_KEY_i,         GDK_CONTROL_MASK,               "ToggleItalic"  },
    { GDK_KEY_Escape,    0,                              "Cancel"        },
    { GDK_KEY_greater,   GDK_CONTROL_MASK,               "Cancel"        },
    { GDK_KEY_Tab,       0,                              "InsertTab"     },
    { GDK_KEY_Tab,       GDK_SHIFT_MASK,                 "InsertBacktab" },
};

Vector<String> KeyBindingTranslator::commandsForKeyEvent(GdkEventKey* event)
{
    ASSERT(m_pendingEditorCommands.isEmpty());

    guint keyval;
    GdkModifierType state;
    gdk_event_get_keyval(reinterpret_cast<GdkEvent*>(event), &keyval);
    gdk_event_get_state(reinterpret_cast<GdkEvent*>(event), &state);

    gtk_bindings_activate_event(G_OBJECT(m_nativeWidget.get()), event);
    if (!m_pendingEditorCommands.isEmpty())
        return WTFMove(m_pendingEditorCommands);

    // Special-case enter keys for we want them to work regardless of modifier.
    if ((keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter || keyval == GDK_KEY_ISO_Enter))
        return { "InsertNewLine" };

    // For keypress events, we want charCode(), but keyCode() does that.
    unsigned mapKey = state << 16 | keyval;
    if (!mapKey)
        return { };

    for (unsigned i = 0; i < G_N_ELEMENTS(customKeyBindings); ++i) {
        if (mapKey == (customKeyBindings[i].state << 16 | customKeyBindings[i].gdkKeyCode))
            return { customKeyBindings[i].name };
    }

    return { };
}

} // namespace WebKit
