/*
 * Copyright (C) 2011 Samsung Electronics
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
#include "EflKeyboardUtilities.h"

#include "WindowsKeyboardCodes.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef HashMap<String, String> KeyMap;
typedef HashMap<String, int> WindowsKeyMap;

static KeyMap& keyMap()
{
    DEFINE_STATIC_LOCAL(KeyMap, keyMap, ());
    return keyMap;
}

static WindowsKeyMap& windowsKeyMap()
{
    DEFINE_STATIC_LOCAL(WindowsKeyMap, windowsKeyMap, ());
    return windowsKeyMap;
}

static void createKeyMap()
{
    for (unsigned int i = 1; i < 25; i++) {
        String key = "F" + String::number(i);
        keyMap().set(key, key);
    }
    keyMap().set("Alt_L", "Alt");
    keyMap().set("ISO_Level3_Shift", "Alt");
    keyMap().set("Menu", "Alt");
    keyMap().set("Shift_L", "Shift");
    keyMap().set("Shift_R", "Shift");
    keyMap().set("Down", "Down");
    keyMap().set("End", "End");
    keyMap().set("Return", "Enter");
    keyMap().set("KP_Enter", "Enter");
    keyMap().set("Home", "Home");
    keyMap().set("Insert", "Insert");
    keyMap().set("Left", "Left");
    keyMap().set("Down", "Down");
    keyMap().set("Next", "PageDown");
    keyMap().set("Prior", "PageUp");
    keyMap().set("Right", "Right");
    keyMap().set("Up", "Up");
    keyMap().set("Delete", "U+007F");
    keyMap().set("Tab", "U+0009");
    keyMap().set("ISO_Left_Tab", "U+0009");
    keyMap().set("BackSpace", "U+0008");
    keyMap().set("space", "U+0020");
}

static void createWindowsKeyMap()
{
    windowsKeyMap().set("Return", VK_RETURN);
    windowsKeyMap().set("KP_Return", VK_RETURN);
    windowsKeyMap().set("Alt_L", VK_MENU);
    windowsKeyMap().set("ISO_Level3_Shift", VK_MENU);
    windowsKeyMap().set("Menu", VK_MENU);
    windowsKeyMap().set("Shift_L", VK_SHIFT);
    windowsKeyMap().set("Shift_R", VK_SHIFT);
    windowsKeyMap().set("Control_L", VK_CONTROL);
    windowsKeyMap().set("Control_R", VK_CONTROL);
    windowsKeyMap().set("Pause", VK_PAUSE);
    windowsKeyMap().set("Break", VK_PAUSE);
    windowsKeyMap().set("Caps_Lock", VK_CAPITAL);
    windowsKeyMap().set("Scroll_Lock", VK_SCROLL);
    windowsKeyMap().set("Num_Lock", VK_NUMLOCK);
    windowsKeyMap().set("Escape", VK_ESCAPE);
    windowsKeyMap().set("Tab", VK_TAB);
    windowsKeyMap().set("ISO_Left_Tab", VK_TAB);
    windowsKeyMap().set("BackSpace", VK_BACK);
    windowsKeyMap().set("space", VK_SPACE);
    windowsKeyMap().set("Next", VK_NEXT);
    windowsKeyMap().set("Prior", VK_PRIOR);
    windowsKeyMap().set("Home", VK_HOME);
    windowsKeyMap().set("End", VK_END);
    windowsKeyMap().set("Right", VK_RIGHT);
    windowsKeyMap().set("Left", VK_LEFT);
    windowsKeyMap().set("Up", VK_UP);
    windowsKeyMap().set("Down", VK_DOWN);
    windowsKeyMap().set("Print", VK_PRINT);
    windowsKeyMap().set("Insert", VK_INSERT);
    windowsKeyMap().set("Delete", VK_DELETE);

    windowsKeyMap().set("comma", VK_OEM_COMMA);
    windowsKeyMap().set("less", VK_OEM_COMMA);
    windowsKeyMap().set("period", VK_OEM_PERIOD);
    windowsKeyMap().set("greater", VK_OEM_PERIOD);
    windowsKeyMap().set("semicolon", VK_OEM_1);
    windowsKeyMap().set("colon", VK_OEM_1);
    windowsKeyMap().set("slash", VK_OEM_2);
    windowsKeyMap().set("question", VK_OEM_2);
    windowsKeyMap().set("grave", VK_OEM_3);
    windowsKeyMap().set("asciitilde", VK_OEM_3);
    windowsKeyMap().set("bracketleft", VK_OEM_4);
    windowsKeyMap().set("braceleft", VK_OEM_4);
    windowsKeyMap().set("backslash", VK_OEM_5);
    windowsKeyMap().set("bar", VK_OEM_5);
    windowsKeyMap().set("bracketright", VK_OEM_6);
    windowsKeyMap().set("braceright", VK_OEM_6);
    windowsKeyMap().set("apostrophe", VK_OEM_7);
    windowsKeyMap().set("quotedbl", VK_OEM_7);

    // Set alphabet to the windowsKeyMap.
    const char* alphabet = "abcdefghijklmnopqrstuvwxyz";
    for (unsigned int i = 0; i < 26; i++) {
        String key(alphabet + i, 1);
        windowsKeyMap().set(key, VK_A + i);
    }

    // Set digits to the windowsKeyMap.
    for (unsigned int i = 0; i < 10; i++) {
        String key = String::number(i);
        windowsKeyMap().set(key, VK_0 + i);
    }

    // Set shifted digits to the windowsKeyMap.
    windowsKeyMap().set("exclam", VK_1);
    windowsKeyMap().set("at", VK_2);
    windowsKeyMap().set("numbersign", VK_3);
    windowsKeyMap().set("dollar", VK_4);
    windowsKeyMap().set("percent", VK_5);
    windowsKeyMap().set("asciicircum", VK_6);
    windowsKeyMap().set("ampersand", VK_7);
    windowsKeyMap().set("asterisk", VK_8);
    windowsKeyMap().set("parenleft", VK_9);
    windowsKeyMap().set("parenright", VK_0);
    windowsKeyMap().set("minus", VK_OEM_MINUS);
    windowsKeyMap().set("underscore", VK_OEM_MINUS);
    windowsKeyMap().set("equal", VK_OEM_PLUS);
    windowsKeyMap().set("plus", VK_OEM_PLUS);

    // Set F_XX keys to the windowsKeyMap.
    for (unsigned int i = 1; i < 25; i++) {
        String key = "F" + String::number(i);
        windowsKeyMap().set(key, VK_F1 + i - 1);
    }
}

String keyIdentifierForEvasKeyName(const String& keyName)
{
    if (keyMap().isEmpty())
        createKeyMap();

    if (keyMap().contains(keyName))
        return keyMap().get(keyName);

    return keyName;
}

String singleCharacterString(const String& keyName)
{
    if (keyName == "Return")
        return String("\r");
    if (keyName == "BackSpace")
        return String("\x8");
    if (keyName == "Tab")
        return String("\t");
    return keyName;
}

int windowsKeyCodeForEvasKeyName(const String& keyName)
{
    if (windowsKeyMap().isEmpty())
        createWindowsKeyMap();

    if (windowsKeyMap().contains(keyName))
        return windowsKeyMap().get(keyName);

    return 0;
}

} // namespace WebCore
