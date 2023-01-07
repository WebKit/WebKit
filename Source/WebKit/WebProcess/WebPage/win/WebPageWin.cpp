/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPage.h"

#include "EditorState.h"
#include "WebFrame.h"
#include "WebKeyboardEvent.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/BackForwardController.h>
#include <WebCore/Editor.h>
#include <WebCore/EventHandler.h>
#include <WebCore/EventNames.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PointerCharacteristics.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/UserAgent.h>
#include <WebCore/WindowsKeyboardCodes.h>

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitialize(const WebPageCreationParameters&)
{
}

void WebPage::platformReinitialize()
{
}

void WebPage::platformDetach()
{
}

void WebPage::getPlatformEditorState(Frame&, EditorState&) const
{
}

bool WebPage::platformCanHandleRequest(const ResourceRequest&)
{
    notImplemented();
    return false;
}

String WebPage::platformUserAgent(const URL&) const
{
    return { };
}

bool WebPage::hoverSupportedByPrimaryPointingDevice() const
{
    return true;
}

bool WebPage::hoverSupportedByAnyAvailablePointingDevice() const
{
    return true;
}

std::optional<PointerCharacteristics> WebPage::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    return PointerCharacteristics::Fine;
}

OptionSet<PointerCharacteristics> WebPage::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    return PointerCharacteristics::Fine;
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
    { VK_LEFT,   0,                  "MoveLeft" },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection" },
    { VK_LEFT,   CtrlKey,            "MoveWordLeft" },
    { VK_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection" },
    { VK_RIGHT,  0,                  "MoveRight" },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection" },
    { VK_RIGHT,  CtrlKey,            "MoveWordRight" },
    { VK_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection" },
    { VK_UP,     0,                  "MoveUp" },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection" },
    { VK_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection" },
    { VK_DOWN,   0,                  "MoveDown" },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection" },
    { VK_NEXT,   ShiftKey,           "MovePageDownAndModifySelection" },
    { VK_PRIOR,  0,                  "MovePageUp" },
    { VK_NEXT,   0,                  "MovePageDown" },
    { VK_HOME,   0,                  "MoveToBeginningOfLine" },
    { VK_HOME,   ShiftKey,           "MoveToBeginningOfLineAndModifySelection" },
    { VK_HOME,   CtrlKey,            "MoveToBeginningOfDocument" },
    { VK_HOME,   CtrlKey | ShiftKey, "MoveToBeginningOfDocumentAndModifySelection" },

    { VK_END,    0,                  "MoveToEndOfLine" },
    { VK_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection" },
    { VK_END,    CtrlKey,            "MoveToEndOfDocument" },
    { VK_END,    CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection" },

    { VK_BACK,   0,                  "DeleteBackward" },
    { VK_BACK,   ShiftKey,           "DeleteBackward" },
    { VK_DELETE, 0,                  "DeleteForward" },
    { VK_BACK,   CtrlKey,            "DeleteWordBackward" },
    { VK_DELETE, CtrlKey,            "DeleteWordForward" },

    { 'B',       CtrlKey,            "ToggleBold" },
    { 'I',       CtrlKey,            "ToggleItalic" },

    { VK_ESCAPE, 0,                  "Cancel" },
    { VK_OEM_PERIOD, CtrlKey,        "Cancel" },
    { VK_TAB,    0,                  "InsertTab" },
    { VK_TAB,    ShiftKey,           "InsertBacktab" },
    { VK_RETURN, 0,                  "InsertNewline" },
    { VK_RETURN, CtrlKey,            "InsertNewline" },
    { VK_RETURN, AltKey,             "InsertNewline" },
    { VK_RETURN, ShiftKey,           "InsertNewline" },
    { VK_RETURN, AltKey | ShiftKey,  "InsertNewline" },

    // It's not quite clear whether clipboard shortcuts and Undo/Redo should be handled
    // in the application or in WebKit. We chose WebKit.
    { 'C',       CtrlKey,            "Copy" },
    { 'V',       CtrlKey,            "Paste" },
    { 'X',       CtrlKey,            "Cut" },
    { 'A',       CtrlKey,            "SelectAll" },
    { VK_INSERT, CtrlKey,            "Copy" },
    { VK_DELETE, ShiftKey,           "Cut" },
    { VK_INSERT, ShiftKey,           "Paste" },
    { 'Z',       CtrlKey,            "Undo" },
    { 'Z',       CtrlKey | ShiftKey, "Redo" },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab" },
    { '\t',   ShiftKey,           "InsertBacktab" },
    { '\r',   0,                  "InsertNewline" },
    { '\r',   CtrlKey,            "InsertNewline" },
    { '\r',   AltKey,             "InsertNewline" },
    { '\r',   ShiftKey,           "InsertNewline" },
    { '\r',   AltKey | ShiftKey,  "InsertNewline" },
};

const char* WebPage::interpretKeyEvent(const WebCore::KeyboardEvent* evt)
{
    ASSERT(evt->type() == eventNames().keydownEvent || evt->type() == eventNames().keypressEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (size_t i = 0; i < std::size(keyDownEntries); ++i)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (size_t i = 0; i < std::size(keyPressEntries); ++i)
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

bool WebPage::handleEditingKeyboardEvent(WebCore::KeyboardEvent& event)
{
    auto* frame = downcast<Node>(event.target())->document().frame();
    ASSERT(frame);

    auto* keyEvent = event.underlyingPlatformEvent();
    if (!keyEvent || keyEvent->isSystemKey()) // Do not treat this as text input if it's a system key event.
        return false;

    if (event.type() != eventNames().keydownEvent && event.type() != eventNames().keypressEvent)
        return false;

    auto command = frame->editor().command(String::fromLatin1(interpretKeyEvent(&event)));

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide
        // how commands that just insert text if executed via Editor
        // should be treated, so we leave it upon WebCore to either
        // handle them immediately (e.g. Tab that changes focus) or
        // let a keypress event be generated (e.g. Tab that inserts a
        // Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(&event);
    }

    if (command.execute(&event))
        return true;

    // Don't insert null or control characters as they can result in unexpected behaviour.
    if (event.charCode() < ' ')
        return false;

    return frame->editor().insertText(keyEvent->text(), &event);
}

} // namespace WebKit
