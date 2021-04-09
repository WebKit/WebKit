// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2019 Sony Interactive Entertainment Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "WindowsKeyNames.h"

namespace WebCore {

enum class WindowsKeyNames::KeyModifier : uint8_t {
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    CapsLock = 1 << 3,
};

static void setModifierState(BYTE* keyboardState, WindowsKeyNames::KeyModifierSet modifiers)
{
    // According to MSDN GetKeyState():
    // 1. If the high-order bit is 1, the key is down; otherwise, it is up.
    // 2. If the low-order bit is 1, the key is toggled. A key, such as the
    //    CAPS LOCK key, is toggled if it is turned on. The key is off and
    //    untoggled if the low-order bit is 0.
    // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301.aspx
    if (modifiers.contains(WindowsKeyNames::KeyModifier::Shift))
        keyboardState[VK_SHIFT] |= 0x80;

    if (modifiers.contains(WindowsKeyNames::KeyModifier::Control))
        keyboardState[VK_CONTROL] |= 0x80;

    if (modifiers.contains(WindowsKeyNames::KeyModifier::Alt))
        keyboardState[VK_MENU] |= 0x80;

    if (modifiers.contains(WindowsKeyNames::KeyModifier::CapsLock))
        keyboardState[VK_CAPITAL] |= 0x01;
}

static bool hasControlAndAlt(WindowsKeyNames::KeyModifierSet modifiers)
{
    return modifiers.containsAll({ WindowsKeyNames::KeyModifier::Control, WindowsKeyNames::KeyModifier::Alt });
}

// This table must be sorted by 'virtualKey' for binary search.
constexpr struct NonPrintableKeyEntry {
    int virtualKey;
    ASCIILiteral domKey;
} cNonPrintableKeyMap[] = {
    { VK_CANCEL, "Cancel"_s },
    { VK_BACK, "Backspace"_s },
    { VK_TAB, "Tab"_s },
    { VK_CLEAR, "Clear"_s },
    { VK_RETURN, "Enter"_s },
    { VK_SHIFT, "Shift"_s },
    { VK_CONTROL, "Control"_s },
    { VK_MENU, "Alt"_s },
    { VK_PAUSE, "Pause"_s },
    { VK_CAPITAL, "CapsLock"_s },
    // VK_KANA == VK_HANGUL
    { VK_JUNJA, "JunjaMode"_s },
    { VK_FINAL, "FINAL_MODE"_s },
    // VK_HANJA == VK_KANJI
    { VK_ESCAPE, "Escape"_s },
    { VK_CONVERT, "Convert"_s },
    { VK_NONCONVERT, "NonConvert"_s },
    { VK_ACCEPT, "Accept"_s },
    { VK_MODECHANGE, "ModeChange"_s },
    // VK_SPACE
    { VK_PRIOR, "PageUp"_s },
    { VK_NEXT, "PageDown"_s },
    { VK_END, "End"_s },
    { VK_HOME, "Home"_s },
    { VK_LEFT, "ArrowLeft"_s },
    { VK_UP, "ArrowUp"_s },
    { VK_RIGHT, "ArrowRight"_s },
    { VK_DOWN, "ArrowDown"_s },
    { VK_SELECT, "Select"_s },
    { VK_PRINT, "Print"_s },
    { VK_EXECUTE, "Execute"_s },
    { VK_SNAPSHOT, "PrintScreen"_s },
    { VK_INSERT, "Insert"_s },
    { VK_DELETE, "Delete"_s },
    { VK_HELP, "Help"_s },
    // VK_0..9
    // VK_A..Z
    { VK_LWIN, "Meta"_s },
    // VK_COMMAND == VK_LWIN
    { VK_RWIN, "Meta"_s },
    { VK_APPS, "ContextMenu"_s },
    { VK_SLEEP, "Standby"_s },
    // VK_NUMPAD0..9
    // VK_MULTIPLY, VK_ADD, VK_SEPARATOR, VK_SUBTRACT, VK_DECIMAL,
    // VK_DIVIDE
    { VK_F1, "F1"_s },
    { VK_F2, "F2"_s },
    { VK_F3, "F3"_s },
    { VK_F4, "F4"_s },
    { VK_F5, "F5"_s },
    { VK_F6, "F6"_s },
    { VK_F7, "F7"_s },
    { VK_F8, "F8"_s },
    { VK_F9, "F9"_s },
    { VK_F10, "F10"_s },
    { VK_F11, "F11"_s },
    { VK_F12, "F12"_s },
    { VK_F13, "F13"_s },
    { VK_F14, "F14"_s },
    { VK_F15, "F15"_s },
    { VK_F16, "F16"_s },
    { VK_F17, "F17"_s },
    { VK_F18, "F18"_s },
    { VK_F19, "F19"_s },
    { VK_F20, "F20"_s },
    { VK_F21, "F21"_s },
    { VK_F22, "F22"_s },
    { VK_F23, "F23"_s },
    { VK_F24, "F24"_s },
    { VK_NUMLOCK, "NumLock"_s },
    { VK_SCROLL, "ScrollLock"_s },
    { VK_LSHIFT, "Shift"_s },
    { VK_RSHIFT, "Shift"_s },
    { VK_LCONTROL, "Control"_s },
    { VK_RCONTROL, "Control"_s },
    { VK_LMENU, "Alt"_s },
    { VK_RMENU, "Alt"_s },
    { VK_BROWSER_BACK, "BrowserBack"_s },
    { VK_BROWSER_FORWARD, "BrowserForward"_s },
    { VK_BROWSER_REFRESH, "BrowserRefresh"_s },
    { VK_BROWSER_STOP, "BrowserStop"_s },
    { VK_BROWSER_SEARCH, "BrowserSearch"_s },
    { VK_BROWSER_FAVORITES, "BrowserFavorites"_s },
    { VK_BROWSER_HOME, "BrowserHome"_s },
    { VK_VOLUME_MUTE, "AudioVolume"_s },
    { VK_VOLUME_DOWN, "AudioVolumeDown"_s },
    { VK_VOLUME_UP, "AudioVolumeUp"_s },
    { VK_MEDIA_NEXT_TRACK, "MediaTrackNext"_s },
    { VK_MEDIA_PREV_TRACK, "MediaTrackPrevious"_s },
    { VK_MEDIA_STOP, "MediaStop"_s },
    { VK_MEDIA_PLAY_PAUSE, "MediaPlayPause"_s },
    // VK_OEM_1..8, 102, PLUS, COMMA, MINUS, PERIOD
    { VK_PROCESSKEY, "Process"_s },
    // VK_PACKET - Used to pass Unicode char, considered as printable key.
    { VK_ATTN, "Attn"_s },
    { VK_CRSEL, "CrSel"_s },
    { VK_EXSEL, "ExSel"_s },
    { VK_EREOF, "EraseEof"_s },
    { VK_PLAY, "Play"_s },
    { VK_ZOOM, "ZoomToggle"_s },
    { VK_OEM_CLEAR, "Clear"_s },
};

// Disambiguates the meaning of certain non-printable keys which have different
// meanings under different languages, but use the same VKEY code.
static String languageSpecificOemVirtualKeyToDomKey(int virtualKey, HKL layout)
{
    WORD language = LOWORD(layout);
    WORD primaryLanguage = PRIMARYLANGID(language);
    if (primaryLanguage == LANG_KOREAN) {
        switch (virtualKey) {
        case VK_HANGUL:
            return "HangulMode"_s;
        case VK_HANJA:
            return "HanjaMode"_s;
        default:
            return String();
        }
    } else if (primaryLanguage == LANG_JAPANESE) {
        switch (virtualKey) {
        // VK_KANA isn't generated by any modern layouts but is a listed value
        // that third-party apps might synthesize, so we handle it anyway.
        case VK_KANA:
        case VK_ATTN:
            return "KanaMode"_s;
        case VK_KANJI:
            return "KanjiMode"_s;
        case VK_OEM_ATTN:
            return "Alphanumeric"_s;
        case VK_OEM_FINISH:
            return "Katakana"_s;
        case VK_OEM_COPY:
            return "Hiragana"_s;
        case VK_OEM_BACKTAB:
            return "Romaji"_s;
        case VK_OEM_AUTO:
            return "Hankaku"_s;
        case VK_OEM_ENLW:
            return "Zenkaku"_s;
        default:
            return String();
        }
    }
    return String();
}

static String nonPrintableVirtualKeyToDomKey(int virtualKey, HKL layout)
{
    // 1. Check if |virtualKey| has a |layout|-specific meaning.
    const String key = languageSpecificOemVirtualKeyToDomKey(virtualKey, layout);
    if (!key.isNull())
        return key;

    // 2. Most |virtualKeys| have the same meaning regardless of |layout|.
    const NonPrintableKeyEntry* result = std::lower_bound(
        std::begin(cNonPrintableKeyMap), std::end(cNonPrintableKeyMap), virtualKey,
        [](const NonPrintableKeyEntry& entry, int needle) {
            return entry.virtualKey < needle;
        });
    if (result != std::end(cNonPrintableKeyMap) && result->virtualKey == virtualKey)
        return result->domKey;

    return String();
}

WindowsKeyNames::WindowsKeyNames()
{
    updateLayout();
}

String WindowsKeyNames::domKeyFromParams(WPARAM virtualKey, LPARAM lParam)
{
    bool extended = lParam & 0x01000000;
    KeyModifierSet modifiers;
    if (GetKeyState(VK_SHIFT) < 0)
        modifiers.add(KeyModifier::Shift);
    if (GetKeyState(VK_CONTROL) < 0)
        modifiers.add(KeyModifier::Control);
    if (GetKeyState(VK_MENU ) < 0)
        modifiers.add(KeyModifier::Alt);
    if (GetKeyState(VK_CAPITAL) & 1)
        modifiers.add(KeyModifier::CapsLock);

    updateLayout();
    // Windows expresses right-Alt as VK_MENU with the extended flag set.
    // This key should generate AltGraph under layouts which use that modifier.
    if (virtualKey == VK_MENU && extended && m_hasAltGraph)
        return "AltGraph"_s;

    String key = nonPrintableVirtualKeyToDomKey(virtualKey, m_keyboardLayout);
    if (!key.isNull())
        return key;

    const KeyModifierSet modifiersToTry[] = {
        // Trying to match Firefox's behavior and UIEvents DomKey guidelines.
        // If the combination doesn't produce a printable character, the key value
        // should be the key with no modifiers except for Shift and AltGr.
        // See https://w3c.github.io/uievents/#keys-guidelines
        modifiers,
        KeyModifierSet({ KeyModifier::Shift, KeyModifier::CapsLock }) & modifiers,
    };

    for (auto tryModifiers : modifiersToTry) {
        const auto& it = m_printableKeyCodeToKey.find(std::make_pair(virtualKey, tryModifiers));
        if (it != m_printableKeyCodeToKey.end()) {
            key = it->value;
            if (!key.isNull())
                return key;
        }
    }

    return "Unidentified"_s;
}

String WindowsKeyNames::domKeyFromChar(UChar c)
{
    switch (c) {
    case '\x1b':
        return "Escape"_s;
    case '\t':
        return "Tab"_s;
    case '\r':
        return "Enter"_s;
    default:
        break;
    }
    return makeString(c);
}

// This table must be sorted by 'scanCode' for binary search.
constexpr struct {
    unsigned scanCode;
    ASCIILiteral domCode;
} cDomCodeMap[] = {
    { 0x0001, "Escape"_s },
    { 0x0002, "Digit1"_s },
    { 0x0003, "Digit2"_s },
    { 0x0004, "Digit3"_s },
    { 0x0005, "Digit4"_s },
    { 0x0006, "Digit5"_s },
    { 0x0007, "Digit6"_s },
    { 0x0008, "Digit7"_s },
    { 0x0009, "Digit8"_s },
    { 0x000a, "Digit9"_s },
    { 0x000b, "Digit0"_s },
    { 0x000c, "Minus"_s },
    { 0x000d, "Equal"_s },
    { 0x000e, "Backspace"_s },
    { 0x000f, "Tab"_s },
    { 0x0010, "KeyQ"_s },
    { 0x0011, "KeyW"_s },
    { 0x0012, "KeyE"_s },
    { 0x0013, "KeyR"_s },
    { 0x0014, "KeyT"_s },
    { 0x0015, "KeyY"_s },
    { 0x0016, "KeyU"_s },
    { 0x0017, "KeyI"_s },
    { 0x0018, "KeyO"_s },
    { 0x0019, "KeyP"_s },
    { 0x001a, "BracketLeft"_s },
    { 0x001b, "BracketRight"_s },
    { 0x001c, "Enter"_s },
    { 0x001d, "ControlLeft"_s },
    { 0x001e, "KeyA"_s },
    { 0x001f, "KeyS"_s },
    { 0x0020, "KeyD"_s },
    { 0x0021, "KeyF"_s },
    { 0x0022, "KeyG"_s },
    { 0x0023, "KeyH"_s },
    { 0x0024, "KeyJ"_s },
    { 0x0025, "KeyK"_s },
    { 0x0026, "KeyL"_s },
    { 0x0027, "Semicolon"_s },
    { 0x0028, "Quote"_s },
    { 0x0029, "Backquote"_s },
    { 0x002a, "ShiftLeft"_s },
    { 0x002b, "Backslash"_s },
    { 0x002c, "KeyZ"_s },
    { 0x002d, "KeyX"_s },
    { 0x002e, "KeyC"_s },
    { 0x002f, "KeyV"_s },
    { 0x0030, "KeyB"_s },
    { 0x0031, "KeyN"_s },
    { 0x0032, "KeyM"_s },
    { 0x0033, "Comma"_s },
    { 0x0034, "Period"_s },
    { 0x0035, "Slash"_s },
    { 0x0036, "ShiftRight"_s },
    { 0x0037, "NumpadMultiply"_s },
    { 0x0038, "AltLeft"_s },
    { 0x0039, "Space"_s },
    { 0x003a, "CapsLock"_s },
    { 0x003b, "F1"_s },
    { 0x003c, "F2"_s },
    { 0x003d, "F3"_s },
    { 0x003e, "F4"_s },
    { 0x003f, "F5"_s },
    { 0x0040, "F6"_s },
    { 0x0041, "F7"_s },
    { 0x0042, "F8"_s },
    { 0x0043, "F9"_s },
    { 0x0044, "F10"_s },
    { 0x0045, "Pause"_s },
    { 0x0046, "ScrollLock"_s },
    { 0x0047, "Numpad7"_s },
    { 0x0048, "Numpad8"_s },
    { 0x0049, "Numpad9"_s },
    { 0x004a, "NumpadSubtract"_s },
    { 0x004b, "Numpad4"_s },
    { 0x004c, "Numpad5"_s },
    { 0x004d, "Numpad6"_s },
    { 0x004e, "NumpadAdd"_s },
    { 0x004f, "Numpad1"_s },
    { 0x0050, "Numpad2"_s },
    { 0x0051, "Numpad3"_s },
    { 0x0052, "Numpad0"_s },
    { 0x0053, "NumpadDecimal"_s },
    { 0x0056, "IntlBackslash"_s },
    { 0x0057, "F11"_s },
    { 0x0058, "F12"_s },
    { 0x0059, "NumpadEqual"_s },
    { 0x0064, "F13"_s },
    { 0x0065, "F14"_s },
    { 0x0066, "F15"_s },
    { 0x0067, "F16"_s },
    { 0x0068, "F17"_s },
    { 0x0069, "F18"_s },
    { 0x006a, "F19"_s },
    { 0x006b, "F20"_s },
    { 0x006c, "F21"_s },
    { 0x006d, "F22"_s },
    { 0x006e, "F23"_s },
    { 0x0070, "KanaMode"_s },
    { 0x0071, "Lang2"_s },
    { 0x0072, "Lang1"_s },
    { 0x0073, "IntlRo"_s },
    { 0x0076, "F24"_s },
    { 0x0077, "Lang4"_s },
    { 0x0078, "Lang3"_s },
    { 0x0079, "Convert"_s },
    { 0x007b, "NonConvert"_s },
    { 0x007d, "IntlYen"_s },
    { 0x007e, "NumpadComma"_s },
    { 0x0108, "Undo"_s },
    { 0x010a, "Paste"_s },
    { 0x0110, "MediaTrackPrevious"_s },
    { 0x0117, "Cut"_s },
    { 0x0118, "Copy"_s },
    { 0x0119, "MediaTrackNext"_s },
    { 0x011c, "NumpadEnter"_s },
    { 0x011d, "ControlRight"_s },
    { 0x0120, "AudioVolumeMute"_s },
    { 0x0121, "LaunchApp2"_s },
    { 0x0122, "MediaPlayPause"_s },
    { 0x0124, "MediaStop"_s },
    { 0x012c, "Eject"_s },
    { 0x012e, "AudioVolumeDown"_s },
    { 0x0130, "AudioVolumeUp"_s },
    { 0x0132, "BrowserHome"_s },
    { 0x0135, "NumpadDivide"_s },
    { 0x0137, "PrintScreen"_s },
    { 0x0138, "AltRight"_s },
    { 0x013b, "Help"_s },
    { 0x0145, "NumLock"_s },
    { 0x0147, "Home"_s },
    { 0x0148, "ArrowUp"_s },
    { 0x0149, "PageUp"_s },
    { 0x014b, "ArrowLeft"_s },
    { 0x014d, "ArrowRight"_s },
    { 0x014f, "End"_s },
    { 0x0150, "ArrowDown"_s },
    { 0x0151, "PageDown"_s },
    { 0x0152, "Insert"_s },
    { 0x0153, "Delete"_s },
    { 0x015b, "MetaLeft"_s },
    { 0x015c, "MetaRight"_s },
    { 0x015d, "ContextMenu"_s },
    { 0x015e, "Power"_s },
    { 0x015f, "Sleep"_s },
    { 0x0163, "WakeUp"_s },
    { 0x0165, "BrowserSearch"_s },
    { 0x0166, "BrowserFavorites"_s },
    { 0x0167, "BrowserRefresh"_s },
    { 0x0168, "BrowserStop"_s },
    { 0x0169, "BrowserForward"_s },
    { 0x016a, "BrowserBack"_s },
    { 0x016b, "LaunchApp1"_s },
    { 0x016c, "LaunchMail"_s },
    { 0x016d, "MediaSelect"_s },
};

String WindowsKeyNames::domCodeFromLParam(LPARAM lParam)
{
    unsigned extendedScanCode = (lParam >> 16) & 0x1ff;
    const auto* result = std::lower_bound(
        std::begin(cDomCodeMap), std::end(cDomCodeMap), extendedScanCode,
        [](const auto& entry, int needle) {
            return entry.scanCode < needle;
        });
    if (result != std::end(cDomCodeMap) && result->scanCode == extendedScanCode)
        return result->domCode;

    return "Unidentified"_s;
}

void WindowsKeyNames::updateLayout()
{
    HKL currentLayout = GetKeyboardLayout(0);
    if (currentLayout == m_keyboardLayout)
        return;

    BYTE keyboardStateToRestore[256];
    if (!GetKeyboardState(keyboardStateToRestore))
        return;

    m_keyboardLayout = currentLayout;
    m_printableKeyCodeToKey.clear();
    m_hasAltGraph = false;

    // Map size for some sample keyboard layouts:
    // US: 476, French: 602, Persian: 482, Vietnamese: 1436
    m_printableKeyCodeToKey.reserveInitialCapacity(1500);

    KeyModifierSet allCombination { KeyModifier::Shift, KeyModifier::Control, KeyModifier::Alt, KeyModifier::CapsLock };

    for (int combination = 0; combination <= allCombination.toRaw(); ++combination) {
        BYTE keyboardState[256] = { };

        // Setting up keyboard state for modifiers.
        auto modifiers = KeyModifierSet::fromRaw(combination);
        setModifierState(keyboardState, modifiers);

        for (unsigned virtualKey = 0; virtualKey <= 0xFF; ++virtualKey) {
            wchar_t translatedChars[5];
            int rv = ToUnicodeEx(virtualKey, 0, keyboardState, translatedChars,
                WTF_ARRAY_LENGTH(translatedChars), 0, m_keyboardLayout);

            if (rv == -1) {
                // Dead key, injecting VK_SPACE to get character representation.
                BYTE emptyState[256] = { };
                rv = ToUnicodeEx(VK_SPACE, 0, emptyState, translatedChars,
                    WTF_ARRAY_LENGTH(translatedChars), 0, m_keyboardLayout);
                // Expecting a dead key character (not followed by a space).
                if (rv == 1)
                    m_printableKeyCodeToKey.set(std::make_pair(virtualKey, modifiers), "Dead"_s);
            } else if (rv == 1) {
                if (translatedChars[0] >= 0x20) {
                    m_printableKeyCodeToKey.set(std::make_pair(virtualKey, modifiers), String(translatedChars, 1));

                    // Detect whether the layout makes use of AltGraph.
                    if (hasControlAndAlt(modifiers))
                        m_hasAltGraph = true;
                }
            }
        }
    }
    SetKeyboardState(keyboardStateToRestore);
}

} // namespace WebCore
