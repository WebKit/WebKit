/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2010 Google, Inc.  All rights reserved.
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
#include "EditorClientImpl.h"

#include "Document.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "RenderObject.h"

#include "DOMUtilitiesPrivate.h"
#include "WebEditingAction.h"
#include "WebElement.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebInputElement.h"
#include "WebInputEventConversion.h"
#include "WebNode.h"
#include "WebPasswordAutocompleteListener.h"
#include "WebRange.h"
#include "WebTextAffinity.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory.  This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action.
static const size_t maximumUndoStackDepth = 1000;

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
static const size_t maximumTextSizeForAutofill = 1000;

EditorClientImpl::EditorClientImpl(WebViewImpl* webview)
    : m_webView(webview)
    , m_inRedo(false)
    , m_backspaceOrDeletePressed(false)
    , m_spellCheckThisFieldStatus(SpellCheckAutomatic)
    , m_autofillTimer(this, &EditorClientImpl::doAutofill)
{
}

EditorClientImpl::~EditorClientImpl()
{
}

void EditorClientImpl::pageDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

bool EditorClientImpl::shouldShowDeleteInterface(HTMLElement* elem)
{
    // Normally, we don't care to show WebCore's deletion UI, so we only enable
    // it if in testing mode and the test specifically requests it by using this
    // magic class name.
    return layoutTestMode()
           && elem->getAttribute(HTMLNames::classAttr) == "needsDeletionUI";
}

bool EditorClientImpl::smartInsertDeleteEnabled()
{
    if (m_webView->client())
        return m_webView->client()->isSmartInsertDeleteEnabled();
    return true;
}

bool EditorClientImpl::isSelectTrailingWhitespaceEnabled()
{
    if (m_webView->client())
        return m_webView->client()->isSelectTrailingWhitespaceEnabled();
#if OS(WINDOWS)
    return true;
#else
    return false;
#endif
}

bool EditorClientImpl::shouldSpellcheckByDefault()
{
    // Spellcheck should be enabled for all editable areas (such as textareas,
    // contentEditable regions, and designMode docs), except text inputs.
    const Frame* frame = m_webView->focusedWebCoreFrame();
    if (!frame)
        return false;
    const Editor* editor = frame->editor();
    if (!editor)
        return false;
    if (editor->isSpellCheckingEnabledInFocusedNode())
        return true;
    const Document* document = frame->document();
    if (!document)
        return false;
    const Node* node = document->focusedNode();
    // If |node| is null, we default to allowing spellchecking. This is done in
    // order to mitigate the issue when the user clicks outside the textbox, as a
    // result of which |node| becomes null, resulting in all the spell check
    // markers being deleted. Also, the Frame will decide not to do spellchecking
    // if the user can't edit - so returning true here will not cause any problems
    // to the Frame's behavior.
    if (!node)
        return true;
    const RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;

    return !renderer->isTextField();
}

bool EditorClientImpl::isContinuousSpellCheckingEnabled()
{
    if (m_spellCheckThisFieldStatus == SpellCheckForcedOff)
        return false;
    if (m_spellCheckThisFieldStatus == SpellCheckForcedOn)
        return true;
    return shouldSpellcheckByDefault();
}

void EditorClientImpl::toggleContinuousSpellChecking()
{
    if (isContinuousSpellCheckingEnabled())
        m_spellCheckThisFieldStatus = SpellCheckForcedOff;
    else
        m_spellCheckThisFieldStatus = SpellCheckForcedOn;
}

bool EditorClientImpl::isGrammarCheckingEnabled()
{
    return false;
}

void EditorClientImpl::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientImpl::spellCheckerDocumentTag()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool EditorClientImpl::isEditable()
{
    return false;
}

bool EditorClientImpl::shouldBeginEditing(Range* range)
{
    if (m_webView->client())
        return m_webView->client()->shouldBeginEditing(WebRange(range));
    return true;
}

bool EditorClientImpl::shouldEndEditing(Range* range)
{
    if (m_webView->client())
        return m_webView->client()->shouldEndEditing(WebRange(range));
    return true;
}

bool EditorClientImpl::shouldInsertNode(Node* node,
                                        Range* range,
                                        EditorInsertAction action)
{
    if (m_webView->client()) {
        return m_webView->client()->shouldInsertNode(WebNode(node),
                                                     WebRange(range),
                                                     static_cast<WebEditingAction>(action));
    }
    return true;
}

bool EditorClientImpl::shouldInsertText(const String& text,
                                        Range* range,
                                        EditorInsertAction action)
{
    if (m_webView->client()) {
        return m_webView->client()->shouldInsertText(WebString(text),
                                                     WebRange(range),
                                                     static_cast<WebEditingAction>(action));
    }
    return true;
}


bool EditorClientImpl::shouldDeleteRange(Range* range)
{
    if (m_webView->client())
        return m_webView->client()->shouldDeleteRange(WebRange(range));
    return true;
}

bool EditorClientImpl::shouldChangeSelectedRange(Range* fromRange,
                                                 Range* toRange,
                                                 EAffinity affinity,
                                                 bool stillSelecting)
{
    if (m_webView->client()) {
        return m_webView->client()->shouldChangeSelectedRange(WebRange(fromRange),
                                                              WebRange(toRange),
                                                              static_cast<WebTextAffinity>(affinity),
                                                              stillSelecting);
    }
    return true;
}

bool EditorClientImpl::shouldApplyStyle(CSSStyleDeclaration* style,
                                        Range* range)
{
    if (m_webView->client()) {
        // FIXME: Pass a reference to the CSSStyleDeclaration somehow.
        return m_webView->client()->shouldApplyStyle(WebString(),
                                                     WebRange(range));
    }
    return true;
}

bool EditorClientImpl::shouldMoveRangeAfterDelete(Range* range,
                                                  Range* rangeToBeReplaced)
{
    return true;
}

void EditorClientImpl::didBeginEditing()
{
    if (m_webView->client())
        m_webView->client()->didBeginEditing();
}

void EditorClientImpl::respondToChangedSelection()
{
    if (m_webView->client()) {
        Frame* frame = m_webView->focusedWebCoreFrame();
        if (frame)
            m_webView->client()->didChangeSelection(!frame->selection()->isRange());
    }
}

void EditorClientImpl::respondToChangedContents()
{
    if (m_webView->client())
        m_webView->client()->didChangeContents();
}

void EditorClientImpl::didEndEditing()
{
    if (m_webView->client())
        m_webView->client()->didEndEditing();
}

void EditorClientImpl::didWriteSelectionToPasteboard()
{
}

void EditorClientImpl::didSetSelectionTypesForPasteboard()
{
}

void EditorClientImpl::registerCommandForUndo(PassRefPtr<EditCommand> command)
{
    if (m_undoStack.size() == maximumUndoStackDepth)
        m_undoStack.removeFirst(); // drop oldest item off the far end
    if (!m_inRedo)
        m_redoStack.clear();
    m_undoStack.append(command);
}

void EditorClientImpl::registerCommandForRedo(PassRefPtr<EditCommand> command)
{
    m_redoStack.append(command);
}

void EditorClientImpl::clearUndoRedoOperations()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

bool EditorClientImpl::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool EditorClientImpl::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void EditorClientImpl::undo()
{
    if (canUndo()) {
        EditCommandStack::iterator back = --m_undoStack.end();
        RefPtr<EditCommand> command(*back);
        m_undoStack.remove(back);
        command->unapply();
        // unapply will call us back to push this command onto the redo stack.
    }
}

void EditorClientImpl::redo()
{
    if (canRedo()) {
        EditCommandStack::iterator back = --m_redoStack.end();
        RefPtr<EditCommand> command(*back);
        m_redoStack.remove(back);

        ASSERT(!m_inRedo);
        m_inRedo = true;
        command->reapply();
        // reapply will call us back to push this command onto the undo stack.
        m_inRedo = false;
    }
}

//
// The below code was adapted from the WebKit file webview.cpp
//

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;
static const unsigned MetaKey = 1 << 3;
#if OS(DARWIN)
// Aliases for the generic key defintions to make kbd shortcuts definitions more
// readable on OS X.
static const unsigned OptionKey  = AltKey;

// Do not use this constant for anything but cursor movement commands. Keys
// with cmd set have their |isSystemKey| bit set, so chances are the shortcut
// will not be executed. Another, less important, reason is that shortcuts
// defined in the renderer do not blink the menu item that they triggered.  See
// http://crbug.com/25856 and the bugs linked from there for details.
static const unsigned CommandKey = MetaKey;
#endif

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
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
    { VKEY_LEFT,   0,                  "MoveLeft"                             },
    { VKEY_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"           },
#if OS(DARWIN)
    { VKEY_LEFT,   OptionKey,          "MoveWordLeft"                         },
    { VKEY_LEFT,   OptionKey | ShiftKey,
        "MoveWordLeftAndModifySelection"                                      },
#else
    { VKEY_LEFT,   CtrlKey,            "MoveWordLeft"                         },
    { VKEY_LEFT,   CtrlKey | ShiftKey,
        "MoveWordLeftAndModifySelection"                                      },
#endif
    { VKEY_RIGHT,  0,                  "MoveRight"                            },
    { VKEY_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"          },
#if OS(DARWIN)
    { VKEY_RIGHT,  OptionKey,          "MoveWordRight"                        },
    { VKEY_RIGHT,  OptionKey | ShiftKey,
      "MoveWordRightAndModifySelection"                                       },
#else
    { VKEY_RIGHT,  CtrlKey,            "MoveWordRight"                        },
    { VKEY_RIGHT,  CtrlKey | ShiftKey,
      "MoveWordRightAndModifySelection"                                       },
#endif
    { VKEY_UP,     0,                  "MoveUp"                               },
    { VKEY_UP,     ShiftKey,           "MoveUpAndModifySelection"             },
    { VKEY_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"         },
    { VKEY_DOWN,   0,                  "MoveDown"                             },
    { VKEY_DOWN,   ShiftKey,           "MoveDownAndModifySelection"           },
    { VKEY_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"       },
#if !OS(DARWIN)
    { VKEY_PRIOR,  0,                  "MovePageUp"                           },
    { VKEY_NEXT,   0,                  "MovePageDown"                         },
#endif
    { VKEY_HOME,   0,                  "MoveToBeginningOfLine"                },
    { VKEY_HOME,   ShiftKey,
        "MoveToBeginningOfLineAndModifySelection"                             },
#if OS(DARWIN)
    { VKEY_LEFT,   CommandKey,         "MoveToBeginningOfLine"                },
    { VKEY_LEFT,   CommandKey | ShiftKey,
      "MoveToBeginningOfLineAndModifySelection"                               },
    { VKEY_PRIOR,  OptionKey,          "MovePageUp"                           },
    { VKEY_NEXT,   OptionKey,          "MovePageDown"                         },
#endif
#if OS(DARWIN)
    { VKEY_UP,     CommandKey,         "MoveToBeginningOfDocument"            },
    { VKEY_UP,     CommandKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },
#else
    { VKEY_HOME,   CtrlKey,            "MoveToBeginningOfDocument"            },
    { VKEY_HOME,   CtrlKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },
#endif
    { VKEY_END,    0,                  "MoveToEndOfLine"                      },
    { VKEY_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"    },
#if OS(DARWIN)
    { VKEY_DOWN,   CommandKey,         "MoveToEndOfDocument"                  },
    { VKEY_DOWN,   CommandKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#else
    { VKEY_END,    CtrlKey,            "MoveToEndOfDocument"                  },
    { VKEY_END,    CtrlKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#endif
#if OS(DARWIN)
    { VKEY_RIGHT,  CommandKey,         "MoveToEndOfLine"                      },
    { VKEY_RIGHT,  CommandKey | ShiftKey,
        "MoveToEndOfLineAndModifySelection"                                   },
#endif
    { VKEY_BACK,   0,                  "DeleteBackward"                       },
    { VKEY_BACK,   ShiftKey,           "DeleteBackward"                       },
    { VKEY_DELETE, 0,                  "DeleteForward"                        },
#if OS(DARWIN)
    { VKEY_BACK,   OptionKey,          "DeleteWordBackward"                   },
    { VKEY_DELETE, OptionKey,          "DeleteWordForward"                    },
#else
    { VKEY_BACK,   CtrlKey,            "DeleteWordBackward"                   },
    { VKEY_DELETE, CtrlKey,            "DeleteWordForward"                    },
#endif
    { 'B',         CtrlKey,            "ToggleBold"                           },
    { 'I',         CtrlKey,            "ToggleItalic"                         },
    { 'U',         CtrlKey,            "ToggleUnderline"                      },
    { VKEY_ESCAPE, 0,                  "Cancel"                               },
    { VKEY_OEM_PERIOD, CtrlKey,        "Cancel"                               },
    { VKEY_TAB,    0,                  "InsertTab"                            },
    { VKEY_TAB,    ShiftKey,           "InsertBacktab"                        },
    { VKEY_RETURN, 0,                  "InsertNewline"                        },
    { VKEY_RETURN, CtrlKey,            "InsertNewline"                        },
    { VKEY_RETURN, AltKey,             "InsertNewline"                        },
    { VKEY_RETURN, AltKey | ShiftKey,  "InsertNewline"                        },
    { VKEY_RETURN, ShiftKey,           "InsertLineBreak"                      },
    { VKEY_INSERT, CtrlKey,            "Copy"                                 },
    { VKEY_INSERT, ShiftKey,           "Paste"                                },
    { VKEY_DELETE, ShiftKey,           "Cut"                                  },
#if !OS(DARWIN)
    // On OS X, we pipe these back to the browser, so that it can do menu item
    // blinking.
    { 'C',         CtrlKey,            "Copy"                                 },
    { 'V',         CtrlKey,            "Paste"                                },
    { 'V',         CtrlKey | ShiftKey, "PasteAndMatchStyle"                   },
    { 'X',         CtrlKey,            "Cut"                                  },
    { 'A',         CtrlKey,            "SelectAll"                            },
    { 'Z',         CtrlKey,            "Undo"                                 },
    { 'Z',         CtrlKey | ShiftKey, "Redo"                                 },
    { 'Y',         CtrlKey,            "Redo"                                 },
#endif
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                 },
    { '\t',   ShiftKey,           "InsertBacktab"                             },
    { '\r',   0,                  "InsertNewline"                             },
    { '\r',   CtrlKey,            "InsertNewline"                             },
    { '\r',   ShiftKey,           "InsertLineBreak"                           },
    { '\r',   AltKey,             "InsertNewline"                             },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                             },
};

const char* EditorClientImpl::interpretKeyEvent(const KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return "";

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < arraysize(keyDownEntries); i++) {
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey,
                                    keyDownEntries[i].name);
        }

        for (unsigned i = 0; i < arraysize(keyPressEntries); i++) {
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode,
                                     keyPressEntries[i].name);
        }
    }

    unsigned modifiers = 0;
    if (keyEvent->shiftKey())
        modifiers |= ShiftKey;
    if (keyEvent->altKey())
        modifiers |= AltKey;
    if (keyEvent->ctrlKey())
        modifiers |= CtrlKey;
    if (keyEvent->metaKey())
        modifiers |= MetaKey;

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool EditorClientImpl::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    // do not treat this as text input if it's a system key event
    if (!keyEvent || keyEvent->isSystemKey())
        return false;

    Frame* frame = evt->target()->toNode()->document()->frame();
    if (!frame)
        return false;

    String commandName = interpretKeyEvent(evt);
    Editor::Command command = frame->editor()->command(commandName);

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how
        // commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately
        // (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        if (command.isTextInsertion() || commandName.isEmpty())
            return false;
        if (command.execute(evt)) {
            if (m_webView->client())
                m_webView->client()->didExecuteCommand(WebString(commandName));
            return true;
        }
        return false;
    }

    if (command.execute(evt)) {
        if (m_webView->client())
            m_webView->client()->didExecuteCommand(WebString(commandName));
        return true;
    }

    // Here we need to filter key events.
    // On Gtk/Linux, it emits key events with ASCII text and ctrl on for ctrl-<x>.
    // In Webkit, EditorClient::handleKeyboardEvent in
    // WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp drop such events.
    // On Mac, it emits key events with ASCII text and meta on for Command-<x>.
    // These key events should not emit text insert event.
    // Alt key would be used to insert alternative character, so we should let
    // through. Also note that Ctrl-Alt combination equals to AltGr key which is
    // also used to insert alternative character.
    // http://code.google.com/p/chromium/issues/detail?id=10846
    // Windows sets both alt and meta are on when "Alt" key pressed.
    // http://code.google.com/p/chromium/issues/detail?id=2215
    // Also, we should not rely on an assumption that keyboards don't
    // send ASCII characters when pressing a control key on Windows,
    // which may be configured to do it so by user.
    // See also http://en.wikipedia.org/wiki/Keyboard_Layout
    // FIXME(ukai): investigate more detail for various keyboard layout.
    if (evt->keyEvent()->text().length() == 1) {
        UChar ch = evt->keyEvent()->text()[0U];

        // Don't insert null or control characters as they can result in
        // unexpected behaviour
        if (ch < ' ')
            return false;
#if !OS(WINDOWS)
        // Don't insert ASCII character if ctrl w/o alt or meta is on.
        // On Mac, we should ignore events when meta is on (Command-<x>).
        if (ch < 0x80) {
            if (evt->keyEvent()->ctrlKey() && !evt->keyEvent()->altKey())
                return false;
#if OS(DARWIN)
            if (evt->keyEvent()->metaKey())
            return false;
#endif
        }
#endif
    }

    if (!frame->editor()->canEdit())
        return false;

    return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}

void EditorClientImpl::handleKeyboardEvent(KeyboardEvent* evt)
{
    if (evt->keyCode() == VKEY_DOWN
        || evt->keyCode() == VKEY_UP) {
        ASSERT(evt->target()->toNode());
        showFormAutofillForNode(evt->target()->toNode());
    }

    // Give the embedder a chance to handle the keyboard event.
    if ((m_webView->client()
         && m_webView->client()->handleCurrentKeyboardEvent())
        || handleEditingKeyboardEvent(evt))
        evt->setDefaultHandled();
}

void EditorClientImpl::handleInputMethodKeydown(KeyboardEvent* keyEvent)
{
    // We handle IME within chrome.
}

void EditorClientImpl::textFieldDidBeginEditing(Element* element)
{
    HTMLInputElement* inputElement = toHTMLInputElement(element);
    if (m_webView->client() && inputElement)
        m_webView->client()->textFieldDidBeginEditing(WebInputElement(inputElement));
}

void EditorClientImpl::textFieldDidEndEditing(Element* element)
{
    HTMLInputElement* inputElement = toHTMLInputElement(element);
    if (m_webView->client() && inputElement)
        m_webView->client()->textFieldDidEndEditing(WebInputElement(inputElement));

    // Notification that focus was lost.  Be careful with this, it's also sent
    // when the page is being closed.

    // Cancel any pending DoAutofill call.
    m_autofillArgs.clear();
    m_autofillTimer.stop();

    // Hide any showing popup.
    m_webView->hideAutoFillPopup();

    if (!m_webView->client())
        return; // The page is getting closed, don't fill the password.

    // Notify any password-listener of the focus change.
    if (!inputElement)
        return;

    WebFrameImpl* webframe = WebFrameImpl::fromFrame(inputElement->document()->frame());
    if (!webframe)
        return;

    WebPasswordAutocompleteListener* listener = webframe->getPasswordListener(inputElement);
    if (!listener)
        return;

    listener->didBlurInputElement(inputElement->value());
}

void EditorClientImpl::textDidChangeInTextField(Element* element)
{
    ASSERT(element->hasLocalName(HTMLNames::inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
    if (m_webView->client())
        m_webView->client()->textFieldDidChange(WebInputElement(inputElement));

    // Note that we only show the autofill popup in this case if the caret is at
    // the end.  This matches FireFox and Safari but not IE.
    autofill(inputElement, false, false, true);
}

bool EditorClientImpl::showFormAutofillForNode(Node* node)
{
    HTMLInputElement* inputElement = toHTMLInputElement(node);
    if (inputElement)
        return autofill(inputElement, true, true, false);
    return false;
}

bool EditorClientImpl::autofill(HTMLInputElement* inputElement,
                                bool autofillFormOnly,
                                bool autofillOnEmptyValue,
                                bool requireCaretAtEnd)
{
    // Cancel any pending DoAutofill call.
    m_autofillArgs.clear();
    m_autofillTimer.stop();

    // FIXME: Remove the extraneous isEnabledFormControl call below.
    // Let's try to trigger autofill for that field, if applicable.
    if (!inputElement->isEnabledFormControl() || !inputElement->isTextField()
        || inputElement->isPasswordField() || !inputElement->autoComplete()
        || !inputElement->isEnabledFormControl()
        || inputElement->isReadOnlyFormControl())
        return false;

    WebString name = WebInputElement(inputElement).nameForAutofill();
    if (name.isEmpty()) // If the field has no name, then we won't have values.
        return false;

    // Don't attempt to autofill with values that are too large.
    if (inputElement->value().length() > maximumTextSizeForAutofill)
        return false;

    m_autofillArgs = new AutofillArgs();
    m_autofillArgs->inputElement = inputElement;
    m_autofillArgs->autofillFormOnly = autofillFormOnly;
    m_autofillArgs->autofillOnEmptyValue = autofillOnEmptyValue;
    m_autofillArgs->requireCaretAtEnd = requireCaretAtEnd;
    m_autofillArgs->backspaceOrDeletePressed = m_backspaceOrDeletePressed;

    if (!requireCaretAtEnd)
        doAutofill(0);
    else {
        // We post a task for doing the autofill as the caret position is not set
        // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976)
        // and we need it to determine whether or not to trigger autofill.
        m_autofillTimer.startOneShot(0.0);
    }
    return true;
}

void EditorClientImpl::doAutofill(Timer<EditorClientImpl>* timer)
{
    OwnPtr<AutofillArgs> args(m_autofillArgs.release());
    HTMLInputElement* inputElement = args->inputElement.get();

    const String& value = inputElement->value();

    // Enforce autofill_on_empty_value and caret_at_end.

    bool isCaretAtEnd = true;
    if (args->requireCaretAtEnd)
        isCaretAtEnd = inputElement->selectionStart() == inputElement->selectionEnd()
                       && inputElement->selectionEnd() == static_cast<int>(value.length());

    if ((!args->autofillOnEmptyValue && value.isEmpty()) || !isCaretAtEnd) {
        m_webView->hideAutoFillPopup();
        return;
    }

    // First let's see if there is a password listener for that element.
    // We won't trigger form autofill in that case, as having both behavior on
    // a node would be confusing.
    WebFrameImpl* webframe = WebFrameImpl::fromFrame(inputElement->document()->frame());
    if (!webframe)
        return;
    WebPasswordAutocompleteListener* listener = webframe->getPasswordListener(inputElement);
    if (listener) {
        if (args->autofillFormOnly)
            return;

        listener->performInlineAutocomplete(value,
                                            args->backspaceOrDeletePressed,
                                            true);
        return;
    }

    // Then trigger form autofill.
    WebString name = WebInputElement(inputElement).nameForAutofill();
    ASSERT(static_cast<int>(name.length()) > 0);

    if (m_webView->client())
        m_webView->client()->queryAutofillSuggestions(WebNode(inputElement),
                                                      name, WebString(value));
}

void EditorClientImpl::cancelPendingAutofill()
{
    m_autofillArgs.clear();
    m_autofillTimer.stop();
}

void EditorClientImpl::onAutocompleteSuggestionAccepted(HTMLInputElement* textField)
{
    if (m_webView->client())
        m_webView->client()->didAcceptAutocompleteSuggestion(WebInputElement(textField));

    WebFrameImpl* webframe = WebFrameImpl::fromFrame(textField->document()->frame());
    if (!webframe)
        return;

    webframe->notifiyPasswordListenerOfAutocomplete(WebInputElement(textField));
}

bool EditorClientImpl::doTextFieldCommandFromEvent(Element* element,
                                                   KeyboardEvent* event)
{
    HTMLInputElement* inputElement = toHTMLInputElement(element);
    if (m_webView->client() && inputElement) {
        m_webView->client()->textFieldDidReceiveKeyDown(WebInputElement(inputElement),
                                                        WebKeyboardEventBuilder(*event));
    }

    // Remember if backspace was pressed for the autofill.  It is not clear how to
    // find if backspace was pressed from textFieldDidBeginEditing and
    // textDidChangeInTextField as when these methods are called the value of the
    // input element already contains the type character.
    m_backspaceOrDeletePressed = event->keyCode() == VKEY_BACK || event->keyCode() == VKEY_DELETE;

    // The Mac code appears to use this method as a hook to implement special
    // keyboard commands specific to Safari's auto-fill implementation.  We
    // just return false to allow the default action.
    return false;
}

void EditorClientImpl::textWillBeDeletedInTextField(Element*)
{
}

void EditorClientImpl::textDidChangeInTextArea(Element*)
{
}

void EditorClientImpl::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientImpl::learnWord(const String&)
{
    notImplemented();
}

void EditorClientImpl::checkSpellingOfString(const UChar* text, int length,
                                             int* misspellingLocation,
                                             int* misspellingLength)
{
    // SpellCheckWord will write (0, 0) into the output vars, which is what our
    // caller expects if the word is spelled correctly.
    int spellLocation = -1;
    int spellLength = 0;

    // Check to see if the provided text is spelled correctly.
    if (isContinuousSpellCheckingEnabled() && m_webView->client())
        m_webView->client()->spellCheck(WebString(text, length), spellLocation, spellLength);
    else {
        spellLocation = 0;
        spellLength = 0;
    }

    // Note: the Mac code checks if the pointers are null before writing to them,
    // so we do too.
    if (misspellingLocation)
        *misspellingLocation = spellLocation;
    if (misspellingLength)
        *misspellingLength = spellLength;
}

String EditorClientImpl::getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord)
{
    if (!(isContinuousSpellCheckingEnabled() && m_webView->client()))
        return String();

    // Do not autocorrect words with capital letters in it except the
    // first letter. This will remove cases changing "IMB" to "IBM".
    for (size_t i = 1; i < misspelledWord.length(); i++) {
        if (u_isupper(static_cast<UChar32>(misspelledWord[i])))
            return String();
    }

    return m_webView->client()->autoCorrectWord(WebString(misspelledWord));
}

void EditorClientImpl::checkGrammarOfString(const UChar*, int length,
                                            WTF::Vector<GrammarDetail>&,
                                            int* badGrammarLocation,
                                            int* badGrammarLength)
{
    notImplemented();
    if (badGrammarLocation)
        *badGrammarLocation = 0;
    if (badGrammarLength)
        *badGrammarLength = 0;
}

void EditorClientImpl::updateSpellingUIWithGrammarString(const String&,
                                                         const GrammarDetail& detail)
{
    notImplemented();
}

void EditorClientImpl::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    if (m_webView->client())
        m_webView->client()->updateSpellingUIWithMisspelledWord(WebString(misspelledWord));
}

void EditorClientImpl::showSpellingUI(bool show)
{
    if (m_webView->client())
        m_webView->client()->showSpellingUI(show);
}

bool EditorClientImpl::spellingUIIsShowing()
{
    if (m_webView->client())
        return m_webView->client()->isShowingSpellingUI();
    return false;
}

void EditorClientImpl::getGuessesForWord(const String&,
                                         WTF::Vector<String>& guesses)
{
    notImplemented();
}

void EditorClientImpl::willSetInputMethodState()
{
    if (m_webView->client())
        m_webView->client()->resetInputMethod();
}

void EditorClientImpl::setInputMethodState(bool)
{
}

} // namesace WebKit
