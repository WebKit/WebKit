/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/WindowsKeyNames.h>
#include <windows.h>

namespace TestWebKitAPI {

using WebCore::WindowsKeyNames;

namespace {

// Keyboard layout identifiers, matching the ones Chromium uses for its
// equivalent AltGraph tests (ui/events/test/keyboard_layout_win.cc). French
// (France) defines the AltGraph modifier; US English does not.
constexpr auto englishUSLayoutName = L"00000409";
constexpr auto frenchLayoutName = L"0000040c";

// The Euro sign produced by AltGr+E on the French layout.
constexpr char16_t euroSign = 0x20AC;

// Activates the requested keyboard layout for the duration of a test, and
// restores both the previously active layout and the thread's keyboard input
// state on destruction so that tests don't perturb one another.
class ScopedKeyboardLayout {
public:
    explicit ScopedKeyboardLayout(const wchar_t* layoutName)
        : m_originalLayout(GetKeyboardLayout(0))
    {
        GetKeyboardState(m_originalState);
        m_layout = LoadKeyboardLayoutW(layoutName, KLF_ACTIVATE);
        if (m_layout)
            ActivateKeyboardLayout(m_layout, 0);
    }

    ~ScopedKeyboardLayout()
    {
        SetKeyboardState(m_originalState);
        ActivateKeyboardLayout(m_originalLayout, 0);
    }

    // The requested layout may not be installed on the test machine; callers
    // should skip when it could not be activated.
    bool isActive() const { return m_layout && GetKeyboardLayout(0) == m_layout; }

private:
    HKL m_originalLayout;
    HKL m_layout { nullptr };
    BYTE m_originalState[256] { };
};

// Marks the given virtual keys as held in the thread's keyboard input state, so
// that GetKeyState() (used by WindowsKeyNames) observes them as down.
void setKeysDown(std::initializer_list<int> virtualKeys)
{
    BYTE state[256] = { };
    for (int virtualKey : virtualKeys)
        state[virtualKey] = 0x80;
    SetKeyboardState(state);
}

// Builds the lParam Windows delivers for a key, with the extended-key bit set
// when |extended| is true (which is how right-Alt is expressed).
LPARAM lParamForKey(bool extended)
{
    return extended ? (1 << 24) : 0;
}

} // namespace

// US English defines no AltGraph modifier, so right-Alt is always a plain Alt
// and is never folded into AltGraph, regardless of the key.
TEST(WindowsKeyNames, AltGraphNotReportedOnLayoutWithoutAltGraph)
{
    ScopedKeyboardLayout layout(englishUSLayoutName);
    if (!layout.isActive())
        GTEST_SKIP() << "US English keyboard layout is not available.";

    WindowsKeyNames keyNames;
    setKeysDown({ VK_RMENU, VK_LCONTROL, VK_MENU, VK_CONTROL });

    EXPECT_FALSE(keyNames.shouldExposeLeftControlPlusRightAltAsAltGraph());
    EXPECT_FALSE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'C', lParamForKey(false)));
    EXPECT_FALSE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'E', lParamForKey(false)));
}

// On a layout that defines AltGraph (French), a held right-Alt is AltGraph for
// any key it modifies, matching Blink.
TEST(WindowsKeyNames, RightAltIsAltGraphForAnyKey)
{
    ScopedKeyboardLayout layout(frenchLayoutName);
    if (!layout.isActive())
        GTEST_SKIP() << "French keyboard layout is not available.";

    WindowsKeyNames keyNames;
    setKeysDown({ VK_RMENU, VK_LCONTROL, VK_MENU, VK_CONTROL });

    EXPECT_TRUE(keyNames.shouldExposeLeftControlPlusRightAltAsAltGraph());
    EXPECT_TRUE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'C', lParamForKey(false)));
    EXPECT_TRUE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'E', lParamForKey(false)));
}

// With left-Control + left-Alt held (no right-Alt), AltGraph is reported only
// for keys whose character requires the Control+Alt (AltGr) combination; plain
// Control+Alt shortcuts keep reporting Control and Alt.
TEST(WindowsKeyNames, ControlPlusAltIsAltGraphOnlyForAltGraphShiftedKeys)
{
    ScopedKeyboardLayout layout(frenchLayoutName);
    if (!layout.isActive())
        GTEST_SKIP() << "French keyboard layout is not available.";

    WindowsKeyNames keyNames;
    setKeysDown({ VK_LMENU, VK_LCONTROL, VK_MENU, VK_CONTROL });

    // AltGr+C produces no character on French, so this stays a plain Control+Alt.
    EXPECT_FALSE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'C', lParamForKey(false)));
    // AltGr+E produces the Euro sign on French, so it is AltGraph-shifted.
    EXPECT_TRUE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'E', lParamForKey(false)));
}

// A character message delivered while Control and Alt are held is, by
// definition, the product of the AltGr combination.
TEST(WindowsKeyNames, CharacterMessageUnderControlAndAltIsAltGraph)
{
    ScopedKeyboardLayout layout(frenchLayoutName);
    if (!layout.isActive())
        GTEST_SKIP() << "French keyboard layout is not available.";

    WindowsKeyNames keyNames;
    setKeysDown({ VK_LCONTROL, VK_CONTROL, VK_LMENU, VK_MENU });

    EXPECT_TRUE(keyNames.shouldExposeAltGraphForKeyEvent(WM_CHAR, euroSign, 0));
}

// AltGraph is not reported unless both Control and Alt are held.
TEST(WindowsKeyNames, NoAltGraphWithoutControlAndAlt)
{
    ScopedKeyboardLayout layout(frenchLayoutName);
    if (!layout.isActive())
        GTEST_SKIP() << "French keyboard layout is not available.";

    WindowsKeyNames keyNames;
    setKeysDown({ });

    EXPECT_FALSE(keyNames.shouldExposeAltGraphForKeyEvent(WM_KEYDOWN, 'E', lParamForKey(false)));
    EXPECT_FALSE(keyNames.shouldExposeAltGraphForKeyEvent(WM_CHAR, euroSign, 0));
}

// The DOM key string side mirrors the modifier side: the right-Alt key itself
// resolves to "AltGraph", and an AltGraph-shifted key resolves to its AltGr
// character.
TEST(WindowsKeyNames, DomKeyReflectsAltGraph)
{
    ScopedKeyboardLayout layout(frenchLayoutName);
    if (!layout.isActive())
        GTEST_SKIP() << "French keyboard layout is not available.";

    WindowsKeyNames keyNames;

    setKeysDown({ VK_RMENU, VK_LCONTROL, VK_MENU, VK_CONTROL });
    EXPECT_STREQ(keyNames.domKeyFromParams(VK_MENU, lParamForKey(true)).utf8().data(), "AltGraph");

    setKeysDown({ VK_LMENU, VK_LCONTROL, VK_MENU, VK_CONTROL });
    EXPECT_STREQ(keyNames.domKeyFromParams('E', lParamForKey(false)).utf8().data(), "\xE2\x82\xAC" /* euro, UTF-8 */);
}

} // namespace TestWebKitAPI
