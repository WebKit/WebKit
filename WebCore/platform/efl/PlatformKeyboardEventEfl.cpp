/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Diego Hidalgo C. Gonzalez
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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

#include "PlatformKeyboardEvent.h"

#include "NotImplemented.h"
#include "StringHash.h"
#include "TextEncoding.h"
#include "WindowsKeyboardCodes.h"

#include <stdio.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

namespace WebCore {

typedef HashMap<String, String> KeyMap;
typedef HashMap<String, int> WindowsKeyMap;

static KeyMap gKeyMap;
static WindowsKeyMap gWindowsKeyMap;

static void createKeyMap()
{
    for (unsigned int i = 1; i < 25; i++) {
        String key = String::format("F%d", i);
        gKeyMap.set(key, key);
    }
    gKeyMap.set("Alt_L", "Alt");
    gKeyMap.set("ISO_Level3_Shift", "Alt");
    gKeyMap.set("Menu", "Alt");
    gKeyMap.set("Shift_L", "Shift");
    gKeyMap.set("Shift_R", "Shift");
    gKeyMap.set("Down", "Down");
    gKeyMap.set("End", "End");
    gKeyMap.set("Return", "Enter");
    gKeyMap.set("KP_Enter", "Enter");
    gKeyMap.set("Home", "Home");
    gKeyMap.set("Insert", "Insert");
    gKeyMap.set("Left", "Left");
    gKeyMap.set("Down", "Down");
    gKeyMap.set("Next", "PageDown");
    gKeyMap.set("Prior", "PageUp");
    gKeyMap.set("Right", "Right");
    gKeyMap.set("Up", "Up");
    gKeyMap.set("Delete", "U+007F");
    gKeyMap.set("Tab", "U+0009");
    gKeyMap.set("ISO_Left_Tab", "U+0009");
}

static void createWindowsKeyMap()
{
    gWindowsKeyMap.set("Return",     VK_RETURN);
    gWindowsKeyMap.set("KP_Return",  VK_RETURN);
    gWindowsKeyMap.set("Alt_L",      VK_MENU);
    gWindowsKeyMap.set("ISO_Level3_Shift", VK_MENU);
    gWindowsKeyMap.set("Menu",       VK_MENU);
    gWindowsKeyMap.set("Shift_L",    VK_SHIFT);
    gWindowsKeyMap.set("Shift_R",    VK_SHIFT);
    gWindowsKeyMap.set("Control_L",  VK_CONTROL);
    gWindowsKeyMap.set("Control_R",  VK_CONTROL);
    gWindowsKeyMap.set("Pause",      VK_PAUSE);
    gWindowsKeyMap.set("Break",      VK_PAUSE);
    gWindowsKeyMap.set("Caps_Lock",  VK_CAPITAL);
    gWindowsKeyMap.set("Scroll_Lock", VK_SCROLL);
    gWindowsKeyMap.set("Num_Lock",   VK_NUMLOCK);
    gWindowsKeyMap.set("Escape",     VK_ESCAPE);
    gWindowsKeyMap.set("Tab",        VK_TAB);
    gWindowsKeyMap.set("ISO_Left_Tab", VK_TAB);
    gWindowsKeyMap.set("BackSpace",  VK_BACK);
    gWindowsKeyMap.set("Space",      VK_SPACE);
    gWindowsKeyMap.set("Next",       VK_NEXT);
    gWindowsKeyMap.set("Prior",      VK_PRIOR);
    gWindowsKeyMap.set("Home",       VK_HOME);
    gWindowsKeyMap.set("End",        VK_END);
    gWindowsKeyMap.set("Right",      VK_RIGHT);
    gWindowsKeyMap.set("Left",       VK_LEFT);
    gWindowsKeyMap.set("Up",         VK_UP);
    gWindowsKeyMap.set("Down",       VK_DOWN);
    gWindowsKeyMap.set("Print",      VK_PRINT);
    gWindowsKeyMap.set("Insert",     VK_INSERT);
    gWindowsKeyMap.set("Delete",     VK_DELETE);

    gWindowsKeyMap.set("comma",        VK_OEM_COMMA);
    gWindowsKeyMap.set("less",         VK_OEM_COMMA);
    gWindowsKeyMap.set("period",       VK_OEM_PERIOD);
    gWindowsKeyMap.set("greater",      VK_OEM_PERIOD);
    gWindowsKeyMap.set("semicolon",    VK_OEM_1);
    gWindowsKeyMap.set("colon",        VK_OEM_1);
    gWindowsKeyMap.set("slash",        VK_OEM_2);
    gWindowsKeyMap.set("question",     VK_OEM_2);
    gWindowsKeyMap.set("grave",        VK_OEM_3);
    gWindowsKeyMap.set("asciitilde",   VK_OEM_3);
    gWindowsKeyMap.set("bracketleft",  VK_OEM_4);
    gWindowsKeyMap.set("braceleft",    VK_OEM_4);
    gWindowsKeyMap.set("backslash",    VK_OEM_5);
    gWindowsKeyMap.set("bar",          VK_OEM_5);
    gWindowsKeyMap.set("bracketright", VK_OEM_6);
    gWindowsKeyMap.set("braceright",   VK_OEM_6);
    gWindowsKeyMap.set("apostrophe",   VK_OEM_7);
    gWindowsKeyMap.set("quotedbl",     VK_OEM_7);

    // Alphabet
    String alphabet = "abcdefghijklmnopqrstuvwxyz";
    for (unsigned int i = 0; i < 26; i++) {
        String key = String::format("%c", alphabet[i]);
        gWindowsKeyMap.set(key, VK_A + i);
    }

    // Digits
    for (unsigned int i = 0; i < 10; i++) {
        String key = String::format("%d", i);
        gWindowsKeyMap.set(key, VK_0 + i);
    }

    // Shifted digits
    gWindowsKeyMap.set("exclam",    VK_1);
    gWindowsKeyMap.set("at",        VK_2);
    gWindowsKeyMap.set("numbersign", VK_3);
    gWindowsKeyMap.set("dollar",    VK_4);
    gWindowsKeyMap.set("percent",   VK_5);
    gWindowsKeyMap.set("asciicircum", VK_6);
    gWindowsKeyMap.set("ampersand", VK_7);
    gWindowsKeyMap.set("asterisk",  VK_8);
    gWindowsKeyMap.set("parenleft", VK_9);
    gWindowsKeyMap.set("parenright", VK_0);
    gWindowsKeyMap.set("minus",     VK_OEM_MINUS);
    gWindowsKeyMap.set("underscore", VK_OEM_MINUS);
    gWindowsKeyMap.set("equal",     VK_OEM_PLUS);
    gWindowsKeyMap.set("plus",      VK_OEM_PLUS);

    // F_XX
    for (unsigned int i = 1; i < 25; i++) {
        String key = String::format("F%d", i);
        gWindowsKeyMap.set(key, VK_F1 + i);
    }
}

static String keyIdentifierForEvasKeyName(String& keyName)
{
    if (gKeyMap.isEmpty())
        createKeyMap();

    if (gKeyMap.contains(keyName))
        return gKeyMap.get(keyName);

    return keyName;
}

static int windowsKeyCodeForEvasKeyName(String& keyName)
{
    if (gWindowsKeyMap.isEmpty())
        createWindowsKeyMap();

    if (gWindowsKeyMap.contains(keyName))
        return gWindowsKeyMap.get(keyName);

    return 0;
}

PlatformKeyboardEvent::PlatformKeyboardEvent(const Evas_Event_Key_Down* ev)
    : m_type(KeyDown)
    , m_metaKey(evas_key_modifier_is_set(ev->modifiers, "Meta"))
    , m_shiftKey(evas_key_modifier_is_set(ev->modifiers, "Shift"))
    , m_ctrlKey(evas_key_modifier_is_set(ev->modifiers, "Control"))
    , m_altKey(evas_key_modifier_is_set(ev->modifiers, "Alt"))
    , m_text(String::fromUTF8(ev->string))
{
    String keyName = String(ev->key);
    m_keyIdentifier = keyIdentifierForEvasKeyName(keyName);
    m_windowsVirtualKeyCode = windowsKeyCodeForEvasKeyName(keyName);

    // FIXME:
    m_isKeypad = false;
    m_autoRepeat = false;
}

PlatformKeyboardEvent::PlatformKeyboardEvent(const Evas_Event_Key_Up* ev)
    : m_type(KeyUp)
    , m_metaKey(evas_key_modifier_is_set(ev->modifiers, "Meta"))
    , m_shiftKey(evas_key_modifier_is_set(ev->modifiers, "Shift"))
    , m_ctrlKey(evas_key_modifier_is_set(ev->modifiers, "Control"))
    , m_altKey(evas_key_modifier_is_set(ev->modifiers, "Alt"))
    , m_text(String::fromUTF8(ev->string))
{
    String keyName = String(ev->key);
    m_keyIdentifier = keyIdentifierForEvasKeyName(keyName);
    m_windowsVirtualKeyCode = windowsKeyCodeForEvasKeyName(keyName);

    // FIXME:
    m_isKeypad = false;
    m_autoRepeat = false;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool)
{
    ASSERT(m_type == KeyDown);
    m_type = type;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    notImplemented();
    return false;
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    notImplemented();
    shiftKey = false;
    ctrlKey = false;
    altKey = false;
    metaKey = false;
}

}
