/*
 * Copyright (C) 2018 Igalia S.L.
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
#include "WebPageProxy.h"

namespace WebKit {
using namespace WebCore;

static uint32_t modifiersToEventState(OptionSet<WebEvent::Modifier> modifiers)
{
    uint32_t state = 0;
    if (modifiers.contains(WebEvent::Modifier::ControlKey))
        state |= wpe_input_keyboard_modifier_control;
    if (modifiers.contains(WebEvent::Modifier::ShiftKey))
        state |= wpe_input_keyboard_modifier_shift;
    if (modifiers.contains(WebEvent::Modifier::AltKey))
        state |= wpe_input_keyboard_modifier_alt;
    return state;
}

static unsigned mouseButtonToWPEButton(WebMouseEvent::Button button)
{
    switch (button) {
    case WebMouseEvent::NoButton:
    case WebMouseEvent::LeftButton:
        return 1;
    case WebMouseEvent::MiddleButton:
        return 3;
    case WebMouseEvent::RightButton:
        return 2;
    }
    return 1;
}

static unsigned stateModifierForWPEButton(unsigned button)
{
    uint32_t state = 0;

    switch (button) {
    case 1:
        state |= wpe_input_pointer_modifier_button1;
        break;
    case 2:
        state |= wpe_input_pointer_modifier_button2;
        break;
    case 3:
        state |= wpe_input_pointer_modifier_button3;
        break;
    default:
        break;
    }

    return state;
}

static void doMouseEvent(struct wpe_view_backend* viewBackend, const WebCore::IntPoint& location, unsigned button, unsigned state, uint32_t modifiers)
{
    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, 0, location.x(), location.y(), button, static_cast<uint32_t>(state ? 1 : 0), modifiers };
    wpe_view_backend_dispatch_pointer_event(viewBackend, &event);
}

static void doMotionEvent(struct wpe_view_backend* viewBackend, const WebCore::IntPoint& location, uint32_t modifiers)
{
    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_motion, 0, location.x(), location.y(), 0, 0, modifiers };
    wpe_view_backend_dispatch_pointer_event(viewBackend, &event);
}

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, WebMouseEvent::Button button, const WebCore::IntPoint& locationInView, OptionSet<WebEvent::Modifier> keyModifiers)
{
    unsigned wpeButton = mouseButtonToWPEButton(button);
    auto modifier = stateModifierForWPEButton(wpeButton);
    uint32_t state = modifiersToEventState(keyModifiers) | m_currentModifiers;

    switch (interaction) {
    case MouseInteraction::Move:
        doMotionEvent(page.viewBackend(), locationInView, state);
        break;
    case MouseInteraction::Down:
        m_currentModifiers |= modifier;
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 1, state | modifier);
        break;
    case MouseInteraction::Up:
        m_currentModifiers &= ~modifier;
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 0, state & ~modifier);
        break;
    case MouseInteraction::SingleClick:
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 1, state | modifier);
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 0, state);
        break;
    case MouseInteraction::DoubleClick:
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 1, state | modifier);
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 0, state);
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 1, state | modifier);
        doMouseEvent(page.viewBackend(), locationInView, wpeButton, 0, state);
        break;
    }
}

static void doKeyStrokeEvent(struct wpe_view_backend* viewBackend, bool pressed, uint32_t keyCode, uint32_t modifiers, bool doReleaseAfterPress = false)
{
    struct wpe_input_xkb_keymap_entry* entries;
    uint32_t entriesCount;
    wpe_input_xkb_context_get_entries_for_key_code(wpe_input_xkb_context_get_default(), keyCode, &entries, &entriesCount);
    struct wpe_input_keyboard_event event = { 0, keyCode, entriesCount ? entries[0].hardware_key_code : 0, pressed, modifiers };
    wpe_view_backend_dispatch_keyboard_event(viewBackend, &event);
    free(entries);

    if (doReleaseAfterPress) {
        ASSERT(pressed);
        event.pressed = false;
        wpe_view_backend_dispatch_keyboard_event(viewBackend, &event);
    }
}

static uint32_t keyCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey key)
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        return WPE_KEY_Shift_R;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        return WPE_KEY_Control_R;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        return WPE_KEY_Alt_L;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        return WPE_KEY_Meta_R;
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return WPE_KEY_Execute;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        return WPE_KEY_Help;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        return WPE_KEY_BackSpace;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        return WPE_KEY_Tab;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        return WPE_KEY_Clear;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        return WPE_KEY_Return;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        return WPE_KEY_Pause;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        return WPE_KEY_Cancel;
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        return WPE_KEY_Escape;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        return WPE_KEY_Page_Up;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        return WPE_KEY_Page_Down;
    case Inspector::Protocol::Automation::VirtualKey::End:
        return WPE_KEY_End;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return WPE_KEY_Home;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        return WPE_KEY_Left;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        return WPE_KEY_Up;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        return WPE_KEY_Right;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return WPE_KEY_Down;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        return WPE_KEY_Insert;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return WPE_KEY_Delete;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        return WPE_KEY_space;
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        return WPE_KEY_semicolon;
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        return WPE_KEY_equal;
    case Inspector::Protocol::Automation::VirtualKey::Return:
        return WPE_KEY_Return;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        return WPE_KEY_KP_0;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        return WPE_KEY_KP_1;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        return WPE_KEY_KP_2;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        return WPE_KEY_KP_3;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        return WPE_KEY_KP_4;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        return WPE_KEY_KP_5;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        return WPE_KEY_KP_6;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        return WPE_KEY_KP_7;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        return WPE_KEY_KP_8;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        return WPE_KEY_KP_9;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        return WPE_KEY_KP_Multiply;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        return WPE_KEY_KP_Add;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        return WPE_KEY_KP_Subtract;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        return WPE_KEY_KP_Separator;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        return WPE_KEY_KP_Decimal;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        return WPE_KEY_KP_Divide;
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        return WPE_KEY_F1;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        return WPE_KEY_F2;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        return WPE_KEY_F3;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        return WPE_KEY_F4;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        return WPE_KEY_F5;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        return WPE_KEY_F6;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        return WPE_KEY_F7;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        return WPE_KEY_F8;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        return WPE_KEY_F9;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        return WPE_KEY_F10;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        return WPE_KEY_F11;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        return WPE_KEY_F12;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static uint32_t modifiersForKeyCode(unsigned keyCode)
{
    switch (keyCode) {
    case WPE_KEY_Shift_R:
        return wpe_input_keyboard_modifier_shift;
    case WPE_KEY_Control_R:
        return wpe_input_keyboard_modifier_control;
    case WPE_KEY_Alt_L:
        return wpe_input_keyboard_modifier_alt;
    case WPE_KEY_Meta_R:
        return wpe_input_keyboard_modifier_meta;
    }
    return 0;
}

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, WTF::Variant<VirtualKey, CharKey>&& key)
{
    uint32_t keyCode;
    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            keyCode = keyCodeForVirtualKey(virtualKey);
        },
        [&] (CharKey charKey) {
            keyCode = wpe_unicode_to_key_code(g_utf8_get_char(&charKey));
        }
    );
    uint32_t modifiers = modifiersForKeyCode(keyCode);

    switch (interaction) {
    case KeyboardInteraction::KeyPress:
        m_currentModifiers |= modifiers;
        doKeyStrokeEvent(page.viewBackend(), true, keyCode, m_currentModifiers);
        break;
    case KeyboardInteraction::KeyRelease:
        m_currentModifiers &= ~modifiers;
        doKeyStrokeEvent(page.viewBackend(), false, keyCode, m_currentModifiers);
        break;
    case KeyboardInteraction::InsertByKey:
        doKeyStrokeEvent(page.viewBackend(), true, keyCode, m_currentModifiers, true);
        break;
    }
}

void WebAutomationSession::platformSimulateKeySequence(WebPageProxy& page, const String& keySequence)
{
    CString keySequenceUTF8 = keySequence.utf8();
    const char* p = keySequenceUTF8.data();
    do {
        doKeyStrokeEvent(page.viewBackend(), true, wpe_unicode_to_key_code(g_utf8_get_char(p)), m_currentModifiers, true);
        p = g_utf8_next_char(p);
    } while (*p);
}

} // namespace WebKit

