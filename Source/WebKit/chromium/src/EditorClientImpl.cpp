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
#include "SpellChecker.h"
#include "UndoStep.h"

#include "DOMUtilitiesPrivate.h"
#include "WebAutofillClient.h"
#include "WebEditingAction.h"
#include "WebElement.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebInputElement.h"
#include "WebInputEventConversion.h"
#include "WebNode.h"
#include "WebPermissionClient.h"
#include "WebRange.h"
#include "WebSpellCheckClient.h"
#include "WebTextAffinity.h"
#include "WebTextCheckingCompletionImpl.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory.  This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action.
static const size_t maximumUndoStackDepth = 1000;

EditorClientImpl::EditorClientImpl(WebViewImpl* webview)
    : m_webView(webview)
    , m_inRedo(false)
    , m_spellCheckThisFieldStatus(SpellCheckAutomatic)
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

void EditorClientImpl::respondToChangedSelection(Frame* frame)
{
    if (m_webView->client()) {
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

void EditorClientImpl::registerUndoStep(PassRefPtr<UndoStep> step)
{
    if (m_undoStack.size() == maximumUndoStackDepth)
        m_undoStack.removeFirst(); // drop oldest item off the far end
    if (!m_inRedo)
        m_redoStack.clear();
    m_undoStack.append(step);
}

void EditorClientImpl::registerRedoStep(PassRefPtr<UndoStep> step)
{
    m_redoStack.append(step);
}

void EditorClientImpl::clearUndoRedoOperations()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

bool EditorClientImpl::canCopyCut(Frame* frame, bool defaultValue) const
{
    if (!m_webView->permissionClient())
        return defaultValue;
    return m_webView->permissionClient()->allowWriteToClipboard(WebFrameImpl::fromFrame(frame), defaultValue);
}

bool EditorClientImpl::canPaste(Frame* frame, bool defaultValue) const
{
    if (!m_webView->permissionClient())
        return defaultValue;
    return m_webView->permissionClient()->allowReadFromClipboard(WebFrameImpl::fromFrame(frame), defaultValue);
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
        UndoManagerStack::iterator back = --m_undoStack.end();
        RefPtr<UndoStep> step(*back);
        m_undoStack.remove(back);
        step->unapply();
        // unapply will call us back to push this command onto the redo stack.
    }
}

void EditorClientImpl::redo()
{
    if (canRedo()) {
        UndoManagerStack::iterator back = --m_redoStack.end();
        RefPtr<UndoStep> step(*back);
        m_redoStack.remove(back);

        ASSERT(!m_inRedo);
        m_inRedo = true;
        step->reapply();
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

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
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

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
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
}

void EditorClientImpl::textFieldDidEndEditing(Element* element)
{
    HTMLInputElement* inputElement = toHTMLInputElement(element);
    if (m_webView->autofillClient() && inputElement)
        m_webView->autofillClient()->textFieldDidEndEditing(WebInputElement(inputElement));

    // Notification that focus was lost.  Be careful with this, it's also sent
    // when the page is being closed.

    // Hide any showing popup.
    m_webView->hideAutofillPopup();
}

void EditorClientImpl::textDidChangeInTextField(Element* element)
{
    ASSERT(element->hasLocalName(HTMLNames::inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
    if (m_webView->autofillClient())
        m_webView->autofillClient()->textFieldDidChange(WebInputElement(inputElement));
}

bool EditorClientImpl::doTextFieldCommandFromEvent(Element* element,
                                                   KeyboardEvent* event)
{
    HTMLInputElement* inputElement = toHTMLInputElement(element);
    if (m_webView->autofillClient() && inputElement) {
        m_webView->autofillClient()->textFieldDidReceiveKeyDown(WebInputElement(inputElement),
                                                                WebKeyboardEventBuilder(*event));
    }

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
    if (isContinuousSpellCheckingEnabled() && m_webView->spellCheckClient())
        m_webView->spellCheckClient()->spellCheck(WebString(text, length), spellLocation, spellLength, 0);
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

void EditorClientImpl::requestCheckingOfString(SpellChecker* sender, int identifier, TextCheckingTypeMask, const String& text)
{
    if (m_webView->spellCheckClient())
        m_webView->spellCheckClient()->requestCheckingOfText(text, new WebTextCheckingCompletionImpl(identifier, sender));
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

    if (m_webView->spellCheckClient())
        return m_webView->spellCheckClient()->autoCorrectWord(WebString(misspelledWord));
    return String();
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
    if (m_webView->spellCheckClient())
        m_webView->spellCheckClient()->updateSpellingUIWithMisspelledWord(WebString(misspelledWord));
}

void EditorClientImpl::showSpellingUI(bool show)
{
    if (m_webView->spellCheckClient())
        m_webView->spellCheckClient()->showSpellingUI(show);
}

bool EditorClientImpl::spellingUIIsShowing()
{
    if (m_webView->spellCheckClient())
        return m_webView->spellCheckClient()->isShowingSpellingUI();
    return false;
}

void EditorClientImpl::getGuessesForWord(const String& word,
                                         const String& context,
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
