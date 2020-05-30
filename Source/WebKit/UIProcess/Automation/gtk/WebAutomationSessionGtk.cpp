/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebAutomationSession.h"

#include "WebAutomationSessionMacros.h"
#include "WebKitWebViewBaseInternal.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
static unsigned modifiersToEventState(OptionSet<WebEvent::Modifier> modifiers)
{
    unsigned state = 0;
    if (modifiers.contains(WebEvent::Modifier::ControlKey))
        state |= GDK_CONTROL_MASK;
    if (modifiers.contains(WebEvent::Modifier::ShiftKey))
        state |= GDK_SHIFT_MASK;
    if (modifiers.contains(WebEvent::Modifier::AltKey))
        state |= GDK_META_MASK;
    if (modifiers.contains(WebEvent::Modifier::CapsLockKey))
        state |= GDK_LOCK_MASK;
    return state;
}

static unsigned mouseButtonToGdkButton(MouseButton button)
{
    switch (button) {
    case MouseButton::None:
    case MouseButton::Left:
        return GDK_BUTTON_PRIMARY;
    case MouseButton::Middle:
        return GDK_BUTTON_MIDDLE;
    case MouseButton::Right:
        return GDK_BUTTON_SECONDARY;
    }
    return GDK_BUTTON_PRIMARY;
}

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, MouseButton button, const WebCore::IntPoint& locationInView, OptionSet<WebEvent::Modifier> keyModifiers)
{
    unsigned gdkButton = mouseButtonToGdkButton(button);
    auto modifier = stateModifierForGdkButton(gdkButton);
    unsigned state = modifiersToEventState(keyModifiers) | m_currentModifiers;
    auto* viewWidget = reinterpret_cast<WebKitWebViewBase*>(page.viewWidget());

    switch (interaction) {
    case MouseInteraction::Move:
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Motion, 0, m_currentModifiers, locationInView.x(), locationInView.y(), state, 0);
        break;
    case MouseInteraction::Down: {
        int doubleClickTime, doubleClickDistance;
        g_object_get(gtk_widget_get_settings(page.viewWidget()), "gtk-double-click-time", &doubleClickTime, "gtk-double-click-distance", &doubleClickDistance, nullptr);
        updateClickCount(button, locationInView, Seconds::fromMilliseconds(doubleClickTime), doubleClickDistance);
        m_currentModifiers |= modifier;
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Press, gdkButton, m_currentModifiers, locationInView.x(), locationInView.y(), state, m_clickCount);
        break;
    }
    case MouseInteraction::Up:
        m_currentModifiers &= ~modifier;
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Release, gdkButton, m_currentModifiers, locationInView.x(), locationInView.y(), state, 0);
        break;
    case MouseInteraction::SingleClick:
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Press, gdkButton, m_currentModifiers | modifier, locationInView.x(), locationInView.y(), state, 1);
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Release, gdkButton, m_currentModifiers, locationInView.x(), locationInView.y(), state, 0);
        break;
    case MouseInteraction::DoubleClick:
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Press, gdkButton, m_currentModifiers | modifier, locationInView.x(), locationInView.y(), state, 1);
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Release, gdkButton, m_currentModifiers, locationInView.x(), locationInView.y(), state, 0);
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Press, gdkButton, m_currentModifiers | modifier, locationInView.x(), locationInView.y(), state, 2);
        webkitWebViewBaseSynthesizeMouseEvent(viewWidget, MouseEventType::Release, gdkButton, m_currentModifiers, locationInView.x(), locationInView.y(), state, 0);
        break;
    }
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
static int keyCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey key)
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        return GDK_KEY_Shift_R;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        return GDK_KEY_Control_R;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        return GDK_KEY_Alt_L;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        return GDK_KEY_Meta_R;
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return GDK_KEY_Execute;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        return GDK_KEY_Help;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        return GDK_KEY_BackSpace;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        return GDK_KEY_Tab;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        return GDK_KEY_Clear;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        return GDK_KEY_Return;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        return GDK_KEY_Pause;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        return GDK_KEY_Cancel;
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        return GDK_KEY_Escape;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        return GDK_KEY_Page_Up;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        return GDK_KEY_Page_Down;
    case Inspector::Protocol::Automation::VirtualKey::End:
        return GDK_KEY_End;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return GDK_KEY_Home;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        return GDK_KEY_Left;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        return GDK_KEY_Up;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        return GDK_KEY_Right;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return GDK_KEY_Down;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        return GDK_KEY_Insert;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return GDK_KEY_Delete;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        return GDK_KEY_space;
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        return GDK_KEY_semicolon;
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        return GDK_KEY_equal;
    case Inspector::Protocol::Automation::VirtualKey::Return:
        return GDK_KEY_Return;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        return GDK_KEY_KP_0;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        return GDK_KEY_KP_1;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        return GDK_KEY_KP_2;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        return GDK_KEY_KP_3;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        return GDK_KEY_KP_4;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        return GDK_KEY_KP_5;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        return GDK_KEY_KP_6;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        return GDK_KEY_KP_7;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        return GDK_KEY_KP_8;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        return GDK_KEY_KP_9;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        return GDK_KEY_KP_Multiply;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        return GDK_KEY_KP_Add;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        return GDK_KEY_KP_Subtract;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        return GDK_KEY_KP_Separator;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        return GDK_KEY_KP_Decimal;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        return GDK_KEY_KP_Divide;
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        return GDK_KEY_F1;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        return GDK_KEY_F2;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        return GDK_KEY_F3;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        return GDK_KEY_F4;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        return GDK_KEY_F5;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        return GDK_KEY_F6;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        return GDK_KEY_F7;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        return GDK_KEY_F8;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        return GDK_KEY_F9;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        return GDK_KEY_F10;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        return GDK_KEY_F11;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        return GDK_KEY_F12;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static unsigned modifiersForKeyCode(unsigned keyCode)
{
    switch (keyCode) {
    case GDK_KEY_Shift_R:
        return GDK_SHIFT_MASK;
    case GDK_KEY_Control_R:
        return GDK_CONTROL_MASK;
    case GDK_KEY_Alt_L:
        return GDK_MOD1_MASK;
    case GDK_KEY_Meta_R:
        return GDK_META_MASK;
    }
    return 0;
}

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, WTF::Variant<VirtualKey, CharKey>&& key)
{
    unsigned keyCode;
    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            keyCode = keyCodeForVirtualKey(virtualKey);
        },
        [&] (CharKey charKey) {
            keyCode = gdk_unicode_to_keyval(g_utf8_get_char(&charKey));
        }
    );
    unsigned modifiers = modifiersForKeyCode(keyCode);
    auto* viewWidget = reinterpret_cast<WebKitWebViewBase*>(page.viewWidget());

    KeyEventType type;
    switch (interaction) {
    case KeyboardInteraction::KeyPress:
        type = KeyEventType::Press;
        m_currentModifiers |= modifiers;
        break;
    case KeyboardInteraction::KeyRelease:
        type = KeyEventType::Release;
        m_currentModifiers &= ~modifiers;
        break;
    case KeyboardInteraction::InsertByKey:
        type = KeyEventType::Insert;
        break;
    }

    webkitWebViewBaseSynthesizeKeyEvent(viewWidget, type, keyCode, m_currentModifiers, ShouldTranslateKeyboardState::Yes);
}

void WebAutomationSession::platformSimulateKeySequence(WebPageProxy& page, const String& keySequence)
{
    CString keySequenceUTF8 = keySequence.utf8();
    const char* p = keySequenceUTF8.data();
    auto* viewWidget = reinterpret_cast<WebKitWebViewBase*>(page.viewWidget());
    do {
        webkitWebViewBaseSynthesizeKeyEvent(viewWidget, KeyEventType::Insert, gdk_unicode_to_keyval(g_utf8_get_char(p)), m_currentModifiers, ShouldTranslateKeyboardState::Yes);
        p = g_utf8_next_char(p);
    } while (*p);
}
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

} // namespace WebKit
