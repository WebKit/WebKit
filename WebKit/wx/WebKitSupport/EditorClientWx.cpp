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
#include "HostWindow.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "SelectionController.h"
#include "WebFrame.h"
#include "WebFramePrivate.h"
#include "WebView.h"
#include "WebViewPrivate.h"
#include "WindowsKeyboardCodes.h"

#include <stdio.h>

namespace WebCore {

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
    //FIXME: this'll never happen. We can trash it or make it a normal period
    { VK_OEM_PERIOD, CtrlKey,        "Cancel"                                      },
    { VK_TAB,    0,                  "InsertTab"                                   },
    { VK_TAB,    ShiftKey,           "InsertBacktab"                               },
    { VK_RETURN, 0,                  "InsertNewline"                               },
    { VK_RETURN, CtrlKey,            "InsertNewline"                               },
    { VK_RETURN, AltKey,             "InsertNewline"                               },
    { VK_RETURN, ShiftKey,           "InsertLineBreak"                               },
    { 'A',       CtrlKey,            "SelectAll"                                   },
    { 'Z',       CtrlKey,            "Undo"                                        },
    { 'Z',       CtrlKey | ShiftKey, "Redo"                                        },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                   },
    { '\t',   ShiftKey,           "InsertBacktab"                               },
    { '\r',   0,                  "InsertNewline"                               },
    { '\r',   CtrlKey,            "InsertNewline"                               },
    { '\r',   AltKey,             "InsertNewline"                               },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

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
    delete this;
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
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
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
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    
    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            wxWebViewContentsChangedEvent wkEvent(webKitWin);
            webKitWin->GetEventHandler()->ProcessEvent(wkEvent);
        }
    }
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
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            webKitWin->m_impl->undoStack.append(EditCommandWx(command));
        }
    }
}

void EditorClientWx::registerCommandForRedo(PassRefPtr<EditCommand> command)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            webKitWin->m_impl->redoStack.insert(0, EditCommandWx(command));
        }
    }
}

void EditorClientWx::clearUndoRedoOperations()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    
    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            webKitWin->m_impl->redoStack.clear();
            webKitWin->m_impl->undoStack.clear();
        }
    }
}

bool EditorClientWx::canUndo() const
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            return webKitWin->m_impl->undoStack.size() != 0;
        }
    }
    return false;
}

bool EditorClientWx::canRedo() const
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin && webKitWin) {
            return webKitWin->m_impl->redoStack.size() != 0;
        }
    }
    return false;
}

void EditorClientWx::undo()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            webKitWin->m_impl->undoStack.last().editCommand()->unapply();
            webKitWin->m_impl->undoStack.removeLast();
        }
    }
}

void EditorClientWx::redo()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame) {    
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            webKitWin->m_impl->redoStack.last().editCommand()->reapply();
            webKitWin->m_impl->redoStack.removeLast();
        }
    }
}

bool EditorClientWx::handleEditingKeyboardEvent(KeyboardEvent* event)
{
    Node* node = event->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = event->keyEvent();

    //NB: this is what windows does, but they also have a keypress event for Alt+Enter which clearly won't get hit with this
    if (!keyEvent || keyEvent->altKey())  // do not treat this as text input if Alt is down
        return false;

    Editor::Command command = frame->editor()->command(interpretKeyEvent(event));

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or if not to let a CHAR event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(event);
    }

     if (command.execute(event))
        return true;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (event->charCode() < ' ')
        return false;

    return frame->editor()->insertText(event->keyEvent()->text(), event);
}

const char* EditorClientWx::interpretKeyEvent(const KeyboardEvent* evt)
{
    ASSERT(evt->keyEvent()->type() == PlatformKeyboardEvent::RawKeyDown || evt->keyEvent()->type() == PlatformKeyboardEvent::Char);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < WXSIZEOF(keyDownEntries); i++)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (unsigned i = 0; i < WXSIZEOF(keyPressEntries); i++)
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (evt->shiftKey())
        modifiers |= ShiftKey;
    if (evt->altKey())
        modifiers |= AltKey;
    if (evt->ctrlKey())
        modifiers |= CtrlKey;

    if (evt->keyEvent()->type() == PlatformKeyboardEvent::RawKeyDown) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}


void EditorClientWx::handleInputMethodKeydown(KeyboardEvent* event)
{
// NOTE: we don't currently need to handle this. When key events occur,
// both this method and handleKeyboardEvent get a chance at handling them.
// We might use this method later on for IME-specific handling.
}

void EditorClientWx::handleKeyboardEvent(KeyboardEvent* event)
{
    if (handleEditingKeyboardEvent(event))
        event->setDefaultHandled();
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
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (frame) {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->hostWindow()->platformPageClient());
        if (webKitWin) {
            wxWebViewSelectionChangedEvent wkEvent(webKitWin);
            webKitWin->GetEventHandler()->ProcessEvent(wkEvent);
        }
    }
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

String EditorClientWx::getAutoCorrectSuggestionForMisspelledWord(const WTF::String&)
{
    notImplemented();
    return String();
}

void EditorClientWx::willSetInputMethodState()
{
    notImplemented();
}

void EditorClientWx::setInputMethodState(bool enabled)
{
    notImplemented();
}

}
