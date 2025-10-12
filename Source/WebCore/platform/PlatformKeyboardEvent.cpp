/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformKeyboardEvent.h"

#include "WindowsKeyboardCodes.h"
#include <wtf/HexNumber.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformKeyboardEvent);

std::optional<OptionSet<PlatformEvent::Modifier>> PlatformKeyboardEvent::s_currentModifiers;    

bool PlatformKeyboardEvent::currentCapsLockState()
{
    return currentStateOfModifierKeys().contains(PlatformEvent::Modifier::CapsLockKey);
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    auto currentModifiers = currentStateOfModifierKeys();
    shiftKey = currentModifiers.contains(PlatformEvent::Modifier::ShiftKey);
    ctrlKey = currentModifiers.contains(PlatformEvent::Modifier::ControlKey);
    altKey = currentModifiers.contains(PlatformEvent::Modifier::AltKey);
    metaKey = currentModifiers.contains(PlatformEvent::Modifier::MetaKey);
}

void PlatformKeyboardEvent::setCurrentModifierState(OptionSet<Modifier> modifiers)
{
    ASSERT(isMainThread());
    s_currentModifiers = modifiers;
}

struct KeyEventData {
    String text;
    int keyCode { 0 };
    String keyIdentifier;
    int virtualKey { 0 };
    String code;
    std::optional<std::pair<String, String>> editCommandAndText { };
};

using KeyToEventDataMap = MemoryCompactLookupOnlyRobinHoodHashMap<String, KeyEventData>;
static const KeyToEventDataMap& nonAlphaNumericKeys()
{
    static MainThreadNeverDestroyed<KeyToEventDataMap> table = [&] {
        char16_t arrowLeftCharacter = u'\uF702';
        char16_t arrowRightCharacter = u'\uF703';
        char16_t arrowUpCharacter = u'\uF700';
        char16_t arrowDownCharacter = u'\uF701';
        char16_t deleteCharacter = u'\uF728';
        return KeyToEventDataMap {
            { "Escape"_s,       { emptyString(),                                27,     "U+001B"_s,     VK_ESCAPE,      "Escape"_s } },
            { "Backspace"_s,    { emptyString(),                                8,      "U+0008"_s,     VK_BACK,        "Backspace"_s,        { { "deleteBackward"_s, { } } } } },
            { "Enter"_s,        { "\r"_s,                                       13,     "Enter"_s,      VK_RETURN,      "Enter"_s,            { { "insertNewline"_s, { } } } } },
            { "Tab"_s,          { "\t"_s,                                       9,      "U+0009"_s,     VK_TAB,         "Tab"_s,              { { "insertTab"_s, { } } } } },
            { "Shift"_s,        { emptyString(),                                0,      "Shift"_s,      VK_SHIFT,       "Shift"_s } },
            { "ShiftLeft"_s,    { emptyString(),                                0,      "Shift"_s,      VK_LSHIFT,      "ShiftLeft"_s } },
            { "ShiftRight"_s,   { emptyString(),                                0,      "Shift"_s,      VK_RSHIFT,      "ShiftRight"_s } },
            { "Control"_s,      { emptyString(),                                0,      "Control"_s,    VK_CONTROL,     "Control"_s } },
            { "ControlLeft"_s,  { emptyString(),                                0,      "Control"_s,    VK_LCONTROL,    "ControlLeft"_s } },
            { "ControlRight"_s, { emptyString(),                                0,      "Control"_s,    VK_RCONTROL,    "ControlRight"_s } },
            { "Alt"_s,          { emptyString(),                                0,      "Alt"_s,        VK_MENU,        "Alt"_s } },
            { "AltLeft"_s,      { emptyString(),                                0,      "Alt"_s,        VK_LMENU,       "AltLeft"_s } },
            { "AltRight"_s,     { emptyString(),                                0,      "Alt"_s,        VK_RMENU,       "AltRight"_s } },
            { "Meta"_s,         { emptyString(),                                0,      "Meta"_s,       VK_UNKNOWN,     "Meta"_s } },
            { "MetaLeft"_s,     { emptyString(),                                0,      "Meta"_s,       VK_LWIN,        "MetaLeft"_s } },
            { "MetaRight"_s,    { emptyString(),                                0,      "Meta"_s,       VK_APPS,        "MetaRight"_s } },
            { "ArrowLeft"_s,    { { std::span { &arrowLeftCharacter, 1 } },     63234,  "Left"_s,       VK_LEFT,        "ArrowLeft"_s,        { { "moveLeft"_s, { } } } } },
            { "ArrowRight"_s,   { { std::span { &arrowRightCharacter, 1 } },    63235,  "Right"_s,      VK_RIGHT,       "ArrowRight"_s,       { { "moveRight"_s, { } } } } },
            { "ArrowUp"_s,      { { std::span { &arrowUpCharacter, 1 } },       63232,  "Up"_s,         VK_UP,          "ArrowUp"_s,          { { "moveUp"_s, { } } } } },
            { "ArrowDown"_s,    { { std::span { &arrowDownCharacter, 1 } },     63233,  "Down"_s,       VK_DOWN,        "ArrowDown"_s,        { { "moveDown"_s, { } } } } },
            { "Delete"_s,       { { std::span { &deleteCharacter, 1 } },        63272,  "U+007F"_s,     VK_DELETE,      "Delete"_s,           { { "deleteForward"_s, { } } } } },
            { " "_s,            { " "_s,                                        32,     "U+0020"_s,     VK_SPACE,       "Space"_s,            { { "insertText"_s, " "_s } } } },
            { "`"_s,            { "`"_s,                                        96,     "U+0060"_s,     VK_OEM_3,       "Backquote"_s,        { { "insertText"_s, "`"_s } } } },
            { "~"_s,            { "~"_s,                                        126,    "U+007E"_s,     VK_OEM_3,       "Backquote"_s,        { { "insertText"_s, "~"_s } } } },
            { "-"_s,            { "-"_s,                                        45,     "U+002D"_s,     VK_OEM_MINUS,   "Minus"_s,            { { "insertText"_s, "-"_s } } } },
            { "_"_s,            { "_"_s,                                        95,     "U+005F"_s,     VK_OEM_MINUS,   "Minus"_s,            { { "insertText"_s, "_"_s } } } },
            { "="_s,            { "="_s,                                        61,     "U+003D"_s,     VK_OEM_PLUS,    "Equal"_s,            { { "insertText"_s, "="_s } } } },
            { "+"_s,            { "+"_s,                                        43,     "U+002B"_s,     VK_OEM_PLUS,    "Equal"_s,            { { "insertText"_s, "+"_s } } } },
            { "\\"_s,           { "\\"_s,                                       92,     "U+005C"_s,     VK_OEM_5,       "Backslash"_s,        { { "insertText"_s, "\\"_s } } } },
            { "|"_s,            { "|"_s,                                        124,    "U+007C"_s,     VK_OEM_5,       "Backslash"_s,        { { "insertText"_s, "|"_s } } } },
            { "["_s,            { "["_s,                                        91,     "U+005B"_s,     VK_OEM_4,       "BracketLeft"_s,      { { "insertText"_s, "["_s } } } },
            { "{ "_s,           { "{ "_s,                                       123,    "U+007B"_s,     VK_OEM_4,       "BracketLeft"_s,      { { "insertText"_s, "{"_s } } } },
            { "]"_s,            { "]"_s,                                        93,     "U+005D"_s,     VK_OEM_6,       "BracketRight"_s,     { { "insertText"_s, "]"_s } } } },
            { "}"_s,            { "}"_s,                                        125,    "U+007D"_s,     VK_OEM_6,       "BracketRight"_s,     { { "insertText"_s, "}"_s } } } },
            { ";"_s,            { ";"_s,                                        59,     "U+003B"_s,     VK_OEM_1,       "Semicolon"_s,        { { "insertText"_s, ";"_s } } } },
            { ":"_s,            { ":"_s,                                        58,     "U+003A"_s,     VK_OEM_1,       "Semicolon"_s,        { { "insertText"_s, ":"_s } } } },
            { "'"_s,            { "'"_s,                                        39,     "U+0027"_s,     VK_OEM_7,       "Quote"_s,            { { "insertText"_s, "'"_s } } } },
            { "\""_s,           { "\""_s,                                       34,     "U+0022"_s,     VK_OEM_7,       "Quote"_s,            { { "insertText"_s, "\""_s } } } },
            { ","_s,            { ","_s,                                        44,     "U+002C"_s,     VK_OEM_COMMA,   "Comma"_s,            { { "insertText"_s, ","_s } } } },
            { "<"_s,            { "<"_s,                                        60,     "U+003C"_s,     VK_OEM_COMMA,   "Comma"_s,            { { "insertText"_s, "<"_s } } } },
            { "."_s,            { "."_s,                                        46,     "U+002E"_s,     VK_OEM_PERIOD,  "Period"_s,           { { "insertText"_s, "."_s } } } },
            { ">"_s,            { ">"_s,                                        62,     "U+003E"_s,     VK_OEM_PERIOD,  "Period"_s,           { { "insertText"_s, ">"_s } } } },
            { "/"_s,            { "/"_s,                                        47,     "U+002F"_s,     VK_OEM_2,       "Slash"_s,            { { "insertText"_s, "/"_s } } } },
            { "?"_s,            { "?"_s,                                        63,     "U+003F"_s,     VK_OEM_2,       "Slash"_s,            { { "insertText"_s, "?"_s } } } },
            { "0"_s,            { "0"_s,                                        48,     "U+0030"_s,     VK_0,           "Digit0"_s,           { { "insertText"_s, "0"_s } } } },
            { "1"_s,            { "1"_s,                                        49,     "U+0031"_s,     VK_1,           "Digit1"_s,           { { "insertText"_s, "1"_s } } } },
            { "2"_s,            { "2"_s,                                        50,     "U+0032"_s,     VK_2,           "Digit2"_s,           { { "insertText"_s, "2"_s } } } },
            { "3"_s,            { "3"_s,                                        51,     "U+0033"_s,     VK_3,           "Digit3"_s,           { { "insertText"_s, "3"_s } } } },
            { "4"_s,            { "4"_s,                                        52,     "U+0034"_s,     VK_4,           "Digit4"_s,           { { "insertText"_s, "4"_s } } } },
            { "5"_s,            { "5"_s,                                        53,     "U+0035"_s,     VK_5,           "Digit5"_s,           { { "insertText"_s, "5"_s } } } },
            { "6"_s,            { "6"_s,                                        54,     "U+0036"_s,     VK_6,           "Digit6"_s,           { { "insertText"_s, "6"_s } } } },
            { "7"_s,            { "7"_s,                                        55,     "U+0037"_s,     VK_7,           "Digit7"_s,           { { "insertText"_s, "7"_s } } } },
            { "8"_s,            { "8"_s,                                        56,     "U+0038"_s,     VK_8,           "Digit8"_s,           { { "insertText"_s, "8"_s } } } },
            { "9"_s,            { "9"_s,                                        57,     "U+0039"_s,     VK_9,           "Digit9"_s,           { { "insertText"_s, "9"_s } } } },
            { "!"_s,            { "!"_s,                                        33,     "U+0021"_s,     VK_1,           "Digit1"_s,           { { "insertText"_s, "!"_s } } } },
            { "@"_s,            { "@"_s,                                        64,     "U+0040"_s,     VK_2,           "Digit2"_s,           { { "insertText"_s, "@"_s } } } },
            { "#"_s,            { "#"_s,                                        35,     "U+0023"_s,     VK_3,           "Digit3"_s,           { { "insertText"_s, "#"_s } } } },
            { "$"_s,            { "$"_s,                                        36,     "U+0024"_s,     VK_4,           "Digit4"_s,           { { "insertText"_s, "$"_s } } } },
            { "%"_s,            { "%"_s,                                        37,     "U+0025"_s,     VK_5,           "Digit5"_s,           { { "insertText"_s, "%"_s } } } },
            { "^"_s,            { "^"_s,                                        94,     "U+005E"_s,     VK_6,           "Digit6"_s,           { { "insertText"_s, "^"_s } } } },
            { "&"_s,            { "&"_s,                                        38,     "U+0026"_s,     VK_7,           "Digit7"_s,           { { "insertText"_s, "&"_s } } } },
            { "*"_s,            { "*"_s,                                        42,     "U+002A"_s,     VK_8,           "Digit8"_s,           { { "insertText"_s, "*"_s } } } },
            { "("_s,            { "("_s,                                        40,     "U+0028"_s,     VK_9,           "Digit9"_s,           { { "insertText"_s, "("_s } } } },
            { ")"_s,            { ")"_s,                                        41,     "U+0029"_s,     VK_0,           "Digit0"_s,           { { "insertText"_s, ")"_s } } } },
        };
    }();
    return table.get();
}

static std::optional<KeyEventData> lookup(const String& key)
{
    if (key.isEmpty())
        return { };

    if (auto result = nonAlphaNumericKeys().getOptional(key))
        return result;

    if (key.length() != 1)
        return { };

    auto character = key.characterAt(0);
    if (character >= 'A' && character <= 'Z') {
        return { {
            key,
            static_cast<int>(character),
            makeString("U+00"_s, hex(static_cast<unsigned>(character), 2, WTF::Uppercase)),
            static_cast<int>(VK_A + (character - 'A')),
            makeString("Key"_s, key),
            { { "insertText"_s, key } },
        } };
    }

    if (character >= 'a' && character <= 'z') {
        int uppercaseCharacter = character - 'a' + 'A';
        return { {
            key,
            static_cast<int>(character),
            makeString("U+00"_s, hex(static_cast<unsigned>(uppercaseCharacter), 2, WTF::Uppercase)),
            static_cast<int>(VK_A + (uppercaseCharacter - 'A')),
            makeString("Key"_s, key.convertToASCIIUppercase()),
            { { "insertText"_s, key } },
        } };
    }

    if (character >= '0' && character <= '9') {
        return { {
            key,
            static_cast<int>(character),
            makeString("U+00"_s, hex(static_cast<unsigned>(character), 2, WTF::Uppercase)),
            static_cast<int>(VK_0 + (character - '0')),
            makeString("Digit"_s, key),
            { { "insertText"_s, key } },
        } };
    }

    return { };
}

std::optional<PlatformKeyboardEvent> PlatformKeyboardEvent::syntheticEventFromText(Type type, const String& key)
{
    auto info = lookup(key);
    if (!info)
        return { };

    auto [text, keyCode, keyIdentifier, virtualKey, code, commandAndText] = *info;
    PlatformKeyboardEvent event { type, text, text, key, code, keyIdentifier, virtualKey, false, false, false, { }, MonotonicTime::now() };
#if USE(APPKIT)
    if (commandAndText) {
        auto [editCommandName, text] = *commandAndText;
        auto commandName = makeString(editCommandName, ':');
        if (text.isEmpty())
            event.m_commands = { KeypressCommand { WTFMove(commandName) } };
        else
            event.m_commands = { { WTFMove(commandName), WTFMove(text) } };
    }
#else
    UNUSED_VARIABLE(commandAndText);
#endif

    return { WTFMove(event) };
}

} // namespace WebCore
