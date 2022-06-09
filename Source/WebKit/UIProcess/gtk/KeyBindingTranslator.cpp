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

#include <WebCore/GtkVersioning.h>
#include <gdk/gdkkeysyms.h>

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

#if !USE(GTK4)
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
#endif

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
    g_signal_connect(m_nativeWidget, "backspace", G_CALLBACK(backspaceCallback), this);
    g_signal_connect(m_nativeWidget, "cut-clipboard", G_CALLBACK(cutClipboardCallback), this);
    g_signal_connect(m_nativeWidget, "copy-clipboard", G_CALLBACK(copyClipboardCallback), this);
    g_signal_connect(m_nativeWidget, "paste-clipboard", G_CALLBACK(pasteClipboardCallback), this);
    g_signal_connect(m_nativeWidget, "select-all", G_CALLBACK(selectAllCallback), this);
    g_signal_connect(m_nativeWidget, "move-cursor", G_CALLBACK(moveCursorCallback), this);
    g_signal_connect(m_nativeWidget, "delete-from-cursor", G_CALLBACK(deleteFromCursorCallback), this);
    g_signal_connect(m_nativeWidget, "toggle-overwrite", G_CALLBACK(toggleOverwriteCallback), this);
#if !USE(GTK4)
    g_signal_connect(m_nativeWidget, "popup-menu", G_CALLBACK(popupMenuCallback), this);
    g_signal_connect(m_nativeWidget, "show-help", G_CALLBACK(showHelpCallback), this);
#endif
#if GTK_CHECK_VERSION(3, 24, 0)
    g_signal_connect(m_nativeWidget, "insert-emoji", G_CALLBACK(insertEmojiCallback), this);
#endif
}

KeyBindingTranslator::~KeyBindingTranslator()
{
    ASSERT(!m_nativeWidget);
}

struct KeyCombinationEntry {
    unsigned gdkKeyCode;
    unsigned state;
    const char* name;
};

static const KeyCombinationEntry customKeyBindings[] = {
    { GDK_KEY_b,         GDK_CONTROL_MASK,                  "ToggleBold"      },
    { GDK_KEY_i,         GDK_CONTROL_MASK,                  "ToggleItalic"    },
    { GDK_KEY_Escape,    0,                                 "Cancel"          },
    { GDK_KEY_greater,   GDK_CONTROL_MASK,                  "Cancel"          },
    { GDK_KEY_Tab,       0,                                 "InsertTab"       },
    { GDK_KEY_Tab,       GDK_SHIFT_MASK,                    "InsertBacktab"   },
    { GDK_KEY_Return,    0,                                 "InsertNewLine"   },
    { GDK_KEY_KP_Enter,  0,                                 "InsertNewLine"   },
    { GDK_KEY_ISO_Enter, 0,                                 "InsertNewLine"   },
    { GDK_KEY_Return,    GDK_SHIFT_MASK,                    "InsertLineBreak" },
    { GDK_KEY_KP_Enter,  GDK_SHIFT_MASK,                    "InsertLineBreak" },
    { GDK_KEY_ISO_Enter, GDK_SHIFT_MASK,                    "InsertLineBreak" },
    { GDK_KEY_V,         GDK_CONTROL_MASK | GDK_SHIFT_MASK, "PasteAsPlainText" },
};

static Vector<String> handleKeyBindingsForMap(const KeyCombinationEntry* map, unsigned mapSize, unsigned keyval, GdkModifierType state)
{
    // For keypress events, we want charCode(), but keyCode() does that.
    unsigned mapKey = (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) << 16 | keyval;
    if (!mapKey)
        return { };

    for (unsigned i = 0; i < mapSize; ++i) {
        if (mapKey == (map[i].state << 16 | map[i].gdkKeyCode))
            return { map[i].name };
    }

    return { };
}

static Vector<String> handleCustomKeyBindings(unsigned keyval, GdkModifierType state)
{
    return handleKeyBindingsForMap(customKeyBindings, G_N_ELEMENTS(customKeyBindings), keyval, state);
}

#if USE(GTK4)
Vector<String> KeyBindingTranslator::commandsForKeyEvent(GtkEventControllerKey* controller)
{
    ASSERT(m_pendingEditorCommands.isEmpty());

    gtk_event_controller_key_forward(GTK_EVENT_CONTROLLER_KEY(controller), m_nativeWidget);
    if (!m_pendingEditorCommands.isEmpty())
        return WTFMove(m_pendingEditorCommands);

    auto* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    return handleCustomKeyBindings(gdk_key_event_get_keyval(event), gdk_event_get_modifier_state(event));
}
#else
Vector<String> KeyBindingTranslator::commandsForKeyEvent(GdkEventKey* event)
{
    ASSERT(m_pendingEditorCommands.isEmpty());

    gtk_bindings_activate_event(G_OBJECT(m_nativeWidget), event);
    if (!m_pendingEditorCommands.isEmpty())
        return WTFMove(m_pendingEditorCommands);

    guint keyval;
    gdk_event_get_keyval(reinterpret_cast<GdkEvent*>(event), &keyval);
    GdkModifierType state;
    gdk_event_get_state(reinterpret_cast<GdkEvent*>(event), &state);
    return handleCustomKeyBindings(keyval, state);
}
#endif

static const KeyCombinationEntry predefinedKeyBindings[] = {
    { GDK_KEY_Left,         0,                                 "MoveLeft" },
    { GDK_KEY_KP_Left,      0,                                 "MoveLeft" },
    { GDK_KEY_Left,         GDK_SHIFT_MASK,                    "MoveBackwardAndModifySelection" },
    { GDK_KEY_KP_Left,      GDK_SHIFT_MASK,                    "MoveBackwardAndModifySelection" },
    { GDK_KEY_Left,         GDK_CONTROL_MASK,                  "MoveWordBackward" },
    { GDK_KEY_KP_Left,      GDK_CONTROL_MASK,                  "MoveWordBackward" },
    { GDK_KEY_Left,         GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordBackwardAndModifySelection" },
    { GDK_KEY_KP_Left,      GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordBackwardAndModifySelectio" },
    { GDK_KEY_Right,        0,                                 "MoveRight" },
    { GDK_KEY_KP_Right,     0,                                 "MoveRight" },
    { GDK_KEY_Right,        GDK_SHIFT_MASK,                    "MoveForwardAndModifySelection" },
    { GDK_KEY_KP_Right,     GDK_SHIFT_MASK,                    "MoveForwardAndModifySelection" },
    { GDK_KEY_Right,        GDK_CONTROL_MASK,                  "MoveWordForward" },
    { GDK_KEY_KP_Right,     GDK_CONTROL_MASK,                  "MoveWordForward" },
    { GDK_KEY_Right,        GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordForwardAndModifySelection" },
    { GDK_KEY_KP_Right,     GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordForwardAndModifySelection" },
    { GDK_KEY_Up,           0,                                 "MoveUp" },
    { GDK_KEY_KP_Up,        0,                                 "MoveUp" },
    { GDK_KEY_Up,           GDK_SHIFT_MASK,                    "MoveUpAndModifySelection" },
    { GDK_KEY_KP_Up,        GDK_SHIFT_MASK,                    "MoveUpAndModifySelection" },
    { GDK_KEY_Down,         0,                                 "MoveDown" },
    { GDK_KEY_KP_Down,      0,                                 "MoveDown" },
    { GDK_KEY_Down,         GDK_SHIFT_MASK,                    "MoveDownAndModifySelection" },
    { GDK_KEY_KP_Down,      GDK_SHIFT_MASK,                    "MoveDownAndModifySelection" },
    { GDK_KEY_Home,         0,                                 "MoveToBeginningOfLine" },
    { GDK_KEY_KP_Home,      0,                                 "MoveToBeginningOfLine" },
    { GDK_KEY_Home,         GDK_SHIFT_MASK,                    "MoveToBeginningOfLineAndModifySelection" },
    { GDK_KEY_KP_Home,      GDK_SHIFT_MASK,                    "MoveToBeginningOfLineAndModifySelection" },
    { GDK_KEY_Home,         GDK_CONTROL_MASK,                  "MoveToBeginningOfDocument" },
    { GDK_KEY_KP_Home,      GDK_CONTROL_MASK,                  "MoveToBeginningOfDocument" },
    { GDK_KEY_Home,         GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToBeginningOfDocumentAndModifySelection" },
    { GDK_KEY_KP_Home,      GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToBeginningOfDocumentAndModifySelection" },
    { GDK_KEY_End,          0,                                 "MoveToEndOfLine" },
    { GDK_KEY_KP_End,       0,                                 "MoveToEndOfLine" },
    { GDK_KEY_End,          GDK_SHIFT_MASK,                    "MoveToEndOfLineAndModifySelection" },
    { GDK_KEY_KP_End,       GDK_SHIFT_MASK,                    "MoveToEndOfLineAndModifySelection" },
    { GDK_KEY_End,          GDK_CONTROL_MASK,                  "MoveToEndOfDocument" },
    { GDK_KEY_KP_End,       GDK_CONTROL_MASK,                  "MoveToEndOfDocument" },
    { GDK_KEY_End,          GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToEndOfDocumentAndModifySelection" },
    { GDK_KEY_KP_End,       GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToEndOfDocumentAndModifySelection" },
    { GDK_KEY_Page_Up,      0,                                 "MovePageUp" },
    { GDK_KEY_KP_Page_Up,   0,                                 "MovePageUp" },
    { GDK_KEY_Page_Up,      GDK_SHIFT_MASK,                    "MovePageUpAndModifySelection" },
    { GDK_KEY_KP_Page_Up,   GDK_SHIFT_MASK,                    "MovePageUpAndModifySelection" },
    { GDK_KEY_Page_Down,    0,                                 "MovePageDown" },
    { GDK_KEY_KP_Page_Down, 0,                                 "MovePageDown" },
    { GDK_KEY_Page_Down,    GDK_SHIFT_MASK,                    "MovePageDownAndModifySelection" },
    { GDK_KEY_KP_Page_Down, GDK_SHIFT_MASK,                    "MovePageDownAndModifySelection" },
    { GDK_KEY_Delete,       0,                                 "DeleteForward" },
    { GDK_KEY_KP_Delete,    0,                                 "DeleteForward" },
    { GDK_KEY_Delete,       GDK_CONTROL_MASK,                  "DeleteWordForward" },
    { GDK_KEY_KP_Delete,    GDK_CONTROL_MASK,                  "DeleteWordForward" },
    { GDK_KEY_BackSpace,    0,                                 "DeleteBackward" },
    { GDK_KEY_BackSpace,    GDK_SHIFT_MASK,                    "DeleteBackward" },
    { GDK_KEY_BackSpace,    GDK_CONTROL_MASK,                  "DeleteWordBackward" },
    { GDK_KEY_a,            GDK_CONTROL_MASK,                  "SelectAll" },
    { GDK_KEY_a,            GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Unselect" },
    { GDK_KEY_slash,        GDK_CONTROL_MASK,                  "SelectAll" },
    { GDK_KEY_backslash,    GDK_CONTROL_MASK,                  "Unselect" },
    { GDK_KEY_x,            GDK_CONTROL_MASK,                  "Cut" },
    { GDK_KEY_c,            GDK_CONTROL_MASK,                  "Copy" },
    { GDK_KEY_v,            GDK_CONTROL_MASK,                  "Paste" },
    { GDK_KEY_KP_Delete,    GDK_SHIFT_MASK,                    "Cut" },
    { GDK_KEY_KP_Insert,    GDK_CONTROL_MASK,                  "Copy" },
    { GDK_KEY_KP_Insert,    GDK_SHIFT_MASK,                    "Paste" }
};

Vector<String> KeyBindingTranslator::commandsForKeyval(unsigned keyval, unsigned modifiers)
{
    auto commands = handleKeyBindingsForMap(predefinedKeyBindings, G_N_ELEMENTS(predefinedKeyBindings), keyval, static_cast<GdkModifierType>(modifiers));
    if (!commands.isEmpty())
        return commands;

    return handleCustomKeyBindings(keyval, static_cast<GdkModifierType>(modifiers));
}

} // namespace WebKit
