/*
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2008 Nuanti Ltd.
 *  Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 *  Copyright (C) 2009-2010 ProFUSION embedded systems
 *  Copyright (C) 2009-2010 Samsung Electronics
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
#include "EditorClientEfl.h"

#include "Editor.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "Settings.h"
#include "UndoStep.h"
#include "WindowsKeyboardCodes.h"
#include "ewk_private.h"

using namespace WebCore;

namespace WebCore {

void EditorClientEfl::willSetInputMethodState()
{
    notImplemented();
}

void EditorClientEfl::setInputMethodState(bool active)
{
    ewk_view_input_method_state_set(m_view, active);
}

bool EditorClientEfl::shouldDeleteRange(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientEfl::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientEfl::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientEfl::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClientEfl::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientEfl::shouldBeginEditing(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientEfl::shouldEndEditing(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientEfl::shouldInsertText(const String&, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientEfl::shouldChangeSelectedRange(Range*, Range*, EAffinity, bool)
{
    notImplemented();
    return true;
}

bool EditorClientEfl::shouldApplyStyle(CSSStyleDeclaration*, Range*)
{
    notImplemented();
    return true;
}

bool EditorClientEfl::shouldMoveRangeAfterDelete(Range*, Range*)
{
    notImplemented();
    return true;
}

void EditorClientEfl::didBeginEditing()
{
    notImplemented();
}

void EditorClientEfl::respondToChangedContents()
{
    Evas_Object* frame = ewk_view_frame_focused_get(m_view);

    if (!frame)
        frame = ewk_view_frame_main_get(m_view);

    if (!frame)
        return;

    ewk_frame_editor_client_contents_changed(frame);
}

void EditorClientEfl::respondToChangedSelection(Frame* coreFrame)
{
    if (!coreFrame)
        return;

    if (coreFrame->editor() && coreFrame->editor()->ignoreCompositionSelectionChange())
        return;

    Evas_Object* webFrame = EWKPrivate::kitFrame(coreFrame);
    ewk_frame_editor_client_selection_changed(webFrame);
}

void EditorClientEfl::didEndEditing()
{
    notImplemented();
}

void EditorClientEfl::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientEfl::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

void EditorClientEfl::registerCommandForUndo(WTF::PassRefPtr<UndoStep> step)
{
    if (!m_isInRedo)
        redoStack.clear();
    undoStack.prepend(step);
}

void EditorClientEfl::registerCommandForRedo(WTF::PassRefPtr<UndoStep> step)
{
    redoStack.prepend(step);
}

void EditorClientEfl::clearUndoRedoOperations()
{
    undoStack.clear();
    redoStack.clear();
}

bool EditorClientEfl::canCopyCut(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientEfl::canPaste(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientEfl::canUndo() const
{
    return !undoStack.isEmpty();
}

bool EditorClientEfl::canRedo() const
{
    return !redoStack.isEmpty();
}

void EditorClientEfl::undo()
{
    if (canUndo()) {
        RefPtr<WebCore::UndoStep> step = undoStack.takeFirst();
        step->unapply();
    }
}

void EditorClientEfl::redo()
{
    if (canRedo()) {
        RefPtr<WebCore::UndoStep> step = redoStack.takeFirst();

        ASSERT(!m_isInRedo);
        m_isInRedo = true;
        step->reapply();
        m_isInRedo = false;
    }
}

bool EditorClientEfl::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

void EditorClientEfl::pageDestroyed()
{
    delete this;
}

bool EditorClientEfl::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientEfl::isSelectTrailingWhitespaceEnabled()
{
    notImplemented();
    return false;
}

void EditorClientEfl::toggleContinuousSpellChecking()
{
    notImplemented();
}

void EditorClientEfl::toggleGrammarChecking()
{
    notImplemented();
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

const char* EditorClientEfl::interpretKeyEvent(const KeyboardEvent* event)
{
    ASSERT(event->type() == eventNames().keydownEvent || event->type() == eventNames().keypressEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyDownEntries); ++i)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyPressEntries); ++i)
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (event->shiftKey())
        modifiers |= ShiftKey;
    if (event->altKey())
        modifiers |= AltKey;
    if (event->ctrlKey())
        modifiers |= CtrlKey;

    if (event->type() == eventNames().keydownEvent) {
        int mapKey = modifiers << 16 | event->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | event->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool EditorClientEfl::handleEditingKeyboardEvent(KeyboardEvent* event)
{
    Node* node = event->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = event->keyEvent();
    if (!keyEvent)
        return false;

    if (!frame->settings())
        return false;

    bool caretBrowsing = frame->settings()->caretBrowsingEnabled();
    if (caretBrowsing) {
        switch (keyEvent->windowsVirtualKeyCode()) {
        case VK_LEFT:
            frame->selection()->modify(keyEvent->shiftKey() ? FrameSelection::AlterationExtend : FrameSelection::AlterationMove,
                                       DirectionLeft,
                                       keyEvent->ctrlKey() ? WordGranularity : CharacterGranularity,
                                       UserTriggered);
            return true;
        case VK_RIGHT:
            frame->selection()->modify(keyEvent->shiftKey() ? FrameSelection::AlterationExtend : FrameSelection::AlterationMove,
                                       DirectionRight,
                                       keyEvent->ctrlKey() ? WordGranularity : CharacterGranularity,
                                       UserTriggered);
            return true;
        case VK_UP:
            frame->selection()->modify(keyEvent->shiftKey() ? FrameSelection::AlterationExtend : FrameSelection::AlterationMove,
                                       DirectionBackward,
                                       keyEvent->ctrlKey() ? ParagraphGranularity : LineGranularity,
                                       UserTriggered);
            return true;
        case VK_DOWN:
            frame->selection()->modify(keyEvent->shiftKey() ? FrameSelection::AlterationExtend : FrameSelection::AlterationMove,
                                       DirectionForward,
                                       keyEvent->ctrlKey() ? ParagraphGranularity : LineGranularity,
                                       UserTriggered);
            return true;
        }
    }

    Editor::Command command = frame->editor()->command(interpretKeyEvent(event));

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(event);
    }

    if (command.execute(event))
        return true;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (event->charCode() < ' ')
        return false;

    // Don't insert anything if a modifier is pressed
    if (keyEvent->ctrlKey() || keyEvent->altKey())
        return false;

    return frame->editor()->insertText(event->keyEvent()->text(), event);
}

void EditorClientEfl::handleKeyboardEvent(KeyboardEvent* event)
{
    if (handleEditingKeyboardEvent(event))
        event->setDefaultHandled();
}

void EditorClientEfl::handleInputMethodKeydown(KeyboardEvent* event)
{
}

EditorClientEfl::EditorClientEfl(Evas_Object* view)
    : m_isInRedo(false)
    , m_view(view)
{
    notImplemented();
}

EditorClientEfl::~EditorClientEfl()
{
    notImplemented();
}

void EditorClientEfl::textFieldDidBeginEditing(Element*)
{
}

void EditorClientEfl::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClientEfl::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClientEfl::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    return false;
}

void EditorClientEfl::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientEfl::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientEfl::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientEfl::learnWord(const String&)
{
    notImplemented();
}

void EditorClientEfl::checkSpellingOfString(const UChar*, int, int*, int*)
{
    notImplemented();
}

String EditorClientEfl::getAutoCorrectSuggestionForMisspelledWord(const String&)
{
    notImplemented();
    return String();
}

void EditorClientEfl::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientEfl::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientEfl::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientEfl::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientEfl::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientEfl::getGuessesForWord(const String& word, const String& context, Vector<String>& guesses)
{
    notImplemented();
}

}
