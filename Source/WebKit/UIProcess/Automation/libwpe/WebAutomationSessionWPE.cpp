/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "AutomationProtocolObjects.h"
#include "SimulatedInputDispatcher.h"
#include "WebAutomationSession.h"

#include "WebAutomationSessionLibWPE.h"
#include "WebAutomationSessionMacros.h"
#include "WebEventModifier.h"
#include "WebPageProxy.h"
#include <optional>
#include <wpe/wpe.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>


#if ENABLE(WPE_PLATFORM)
#include "Logging.h"
#include <wpe/GRefPtrWPE.h>
#include <wpe/wpe-platform.h>
#endif

namespace WebKit {

#if ENABLE(WEBDRIVER) && (ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) || ENABLE(WEBDRIVER_TOUCH_INTERACTIONS) || ENABLE(WEBDRIVER_WHEEL_INTERACTIONS))
static WebCore::IntPoint deviceScaleLocationInView(WebPageProxy& page, const WebCore::IntPoint& locationInView)
{
    WebCore::IntPoint deviceScaleLocationInView(locationInView);
    deviceScaleLocationInView.scale(page.deviceScaleFactor());
    return deviceScaleLocationInView;
}
#endif

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

// Called by platform-indendent code to convert the current platform-dependent raw modifiers into generic WebEventModifiers,
// which will be passed to the platformSimulateFooBarInteraction methods.
OptionSet<WebEventModifier> WebAutomationSession::platformWebModifiersFromRaw(WebPageProxy& page, unsigned modifiers)
{
    OptionSet<WebEventModifier> webModifiers;

#if ENABLE(WPE_PLATFORM)
    if (page.wpeView()) {
        if (modifiers & WPE_MODIFIER_KEYBOARD_ALT)
            webModifiers.add(WebEventModifier::AltKey);
        if (modifiers & WPE_MODIFIER_KEYBOARD_META)
            webModifiers.add(WebEventModifier::MetaKey);
        if (modifiers & WPE_MODIFIER_KEYBOARD_CONTROL)
            webModifiers.add(WebEventModifier::ControlKey);
        if (modifiers & WPE_MODIFIER_KEYBOARD_SHIFT)
            webModifiers.add(WebEventModifier::ShiftKey);
        if (modifiers & WPE_MODIFIER_KEYBOARD_CAPS_LOCK)
            webModifiers.add(WebEventModifier::CapsLockKey);
        return webModifiers;
    }
#endif

    if (modifiers & wpe_input_keyboard_modifier_alt)
        webModifiers.add(WebEventModifier::AltKey);
    if (modifiers & wpe_input_keyboard_modifier_meta)
        webModifiers.add(WebEventModifier::MetaKey);
    if (modifiers & wpe_input_keyboard_modifier_control)
        webModifiers.add(WebEventModifier::ControlKey);
    if (modifiers & wpe_input_keyboard_modifier_shift)
        webModifiers.add(WebEventModifier::ShiftKey);
    // libWPE has no Caps Lock modifier.
    return webModifiers;
}

#if ENABLE(WPE_PLATFORM)
static unsigned libWPEStateModifierForWPEButton(unsigned button)
{
    uint32_t state = 0;

    switch (button) {
    case 1:
        state |= WPE_MODIFIER_POINTER_BUTTON1;
        break;
    case 2:
        state |= WPE_MODIFIER_POINTER_BUTTON2;
        break;
    case 3:
        state |= WPE_MODIFIER_POINTER_BUTTON3;
        break;
    case 4:
        state |= WPE_MODIFIER_POINTER_BUTTON4;
        break;
    case 5:
        state |= WPE_MODIFIER_POINTER_BUTTON5;
        break;
    default:
        break;
    }

    return state;
}

static void doMouseEvent(WebPageProxy& page, const WebCore::IntPoint& location, unsigned button, bool isPressed, uint32_t modifiers)
{
    auto* view = page.wpeView();
    auto buttonModifiers =  libWPEStateModifierForWPEButton(button);
    if (isPressed)
        modifiers |= buttonModifiers;
    else
        modifiers &= ~buttonModifiers;
    unsigned pressCount = isPressed ? wpe_view_compute_press_count(view, location.x(), location.y(), button, 0) : 0;
    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_pointer_button_new(isPressed ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP, view, WPE_INPUT_SOURCE_MOUSE,
        0, static_cast<WPEModifiers>(modifiers), button, location.x(), location.y(), pressCount));
    wpe_view_event(view, event.get());
}

static void doMotionEvent(WebPageProxy& page, const WebCore::IntPoint& location, uint32_t modifiers)
{
    auto* view = page.wpeView();
    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, WPE_INPUT_SOURCE_MOUSE, 0, static_cast<WPEModifiers>(modifiers), location.x(), location.y(), 0, 0));
    wpe_view_event(view, event.get());
}

static uint32_t modifiersToEventState(OptionSet<WebEventModifier> modifiers)
{
    uint32_t state = 0;
    if (modifiers.contains(WebEventModifier::ControlKey))
        state |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (modifiers.contains(WebEventModifier::ShiftKey))
        state |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (modifiers.contains(WebEventModifier::AltKey))
        state |= WPE_MODIFIER_KEYBOARD_ALT;
    if (modifiers.contains(WebEventModifier::MetaKey))
        state |= WPE_MODIFIER_KEYBOARD_META;
    if (modifiers.contains(WebEventModifier::CapsLockKey))
        state |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
    return state;
}

static unsigned libWPEMouseButtonToWPEButton(MouseButton button)
{
    switch (button) {
    case MouseButton::None:
    case MouseButton::Left:
        return 1;
    case MouseButton::Middle:
        return 3;
    case MouseButton::Right:
        return 2;
    }
    return 1;
}
#endif // ENABLE(WPE_PLATFORM)

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, MouseButton button, const WebCore::IntPoint& locationInView, OptionSet<WebEventModifier> keyModifiers, const String& pointerType)
{
    UNUSED_PARAM(pointerType);

    auto location = deviceScaleLocationInView(page, locationInView);

    if (page.viewBackend()) {
        platformSimulateMouseInteractionLibWPE(page, interaction, button, location, keyModifiers, pointerType, m_currentModifiers);
        return;
    }

#if ENABLE(WPE_PLATFORM)
    unsigned wpeButton = libWPEMouseButtonToWPEButton(button);
    auto modifier = libWPEStateModifierForWPEButton(wpeButton);
    uint32_t state = modifiersToEventState(keyModifiers) | m_currentModifiers;

    switch (interaction) {
    case MouseInteraction::Move:
        doMotionEvent(page, location, state);
        break;
    case MouseInteraction::Down:
        m_currentModifiers |= modifier;
        doMouseEvent(page, location, wpeButton, true, state | modifier);
        break;
    case MouseInteraction::Up:
        m_currentModifiers &= ~modifier;
        doMouseEvent(page, location, wpeButton, false, state & ~modifier);
        break;
    case MouseInteraction::SingleClick:
        doMouseEvent(page, location, wpeButton, true, state | modifier);
        doMouseEvent(page, location, wpeButton, false, state);
        break;
    case MouseInteraction::DoubleClick:
        doMouseEvent(page, location, wpeButton, true, state | modifier);
        doMouseEvent(page, location, wpeButton, false, state);
        doMouseEvent(page, location, wpeButton, true, state | modifier);
        doMouseEvent(page, location, wpeButton, false, state);
        break;
    }
#endif // ENABLE(WPE_PLATFORM)
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WPE_PLATFORM)
static void doKeyStrokeEvent(WebPageProxy &page, bool pressed, uint32_t keyVal, uint32_t modifiers, bool doReleaseAfterPress = false)
{
    auto* view = page.wpeView();
    auto* display = wpe_view_get_display(view);
    GUniqueOutPtr<GError> error;
    auto* keymap = WPE_KEYMAP(wpe_display_get_keymap(display));

    GUniqueOutPtr<WPEKeymapEntry> entries;
    guint entriesCount;
    if (!wpe_keymap_get_entries_for_keyval(keymap, keyVal, &entries.outPtr(), &entriesCount)) {
        LOG(Automation, "WebAutomationSession::doKeyStrokeEvent: Failed to get keymap entries for keyval %u. Ignoring event.", keyVal);
        return;
    }
    unsigned keyCode = entries.get()[0].keycode;

    WPEModifiers consumedModifiers;
    if (!wpe_keymap_translate_keyboard_state(keymap, keyCode, static_cast<WPEModifiers>(modifiers), entries.get()[0].group, &keyVal, nullptr, nullptr, &consumedModifiers)) {
        LOG(Automation, "WebAutomationSession::doKeyStrokeEvent: Failed to translate keyboard state for keycode %u. Ignoring event.", keyCode);
        return;
    }

    auto remainingModifiers = static_cast<WPEModifiers>(modifiers & ~consumedModifiers);

    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_keyboard_new(pressed ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP, view, WPE_INPUT_SOURCE_KEYBOARD, 0,
    remainingModifiers, keyCode, keyVal));
    wpe_view_event(view, event.get());

    if (doReleaseAfterPress) {
        event = adoptGRef(wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP, view, WPE_INPUT_SOURCE_KEYBOARD, 0, static_cast<WPEModifiers>(modifiers), keyCode, keyVal));
        wpe_view_event(view, event.get());
    }
}

static uint32_t keyValForVirtualKey(Inspector::Protocol::Automation::VirtualKey key)
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        return WPE_KEY_Shift_L;
    case Inspector::Protocol::Automation::VirtualKey::ShiftRight:
        return WPE_KEY_Shift_R;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        return WPE_KEY_Control_L;
    case Inspector::Protocol::Automation::VirtualKey::ControlRight:
        return WPE_KEY_Control_R;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        return WPE_KEY_Alt_L;
    case Inspector::Protocol::Automation::VirtualKey::AlternateRight:
        return WPE_KEY_Alt_R;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        return WPE_KEY_Meta_L;
    case Inspector::Protocol::Automation::VirtualKey::MetaRight:
        return WPE_KEY_Meta_R;
    case Inspector::Protocol::Automation::VirtualKey::Command:
    case Inspector::Protocol::Automation::VirtualKey::CommandRight:
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
    case Inspector::Protocol::Automation::VirtualKey::PageUpRight:
        return WPE_KEY_KP_Page_Up;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        return WPE_KEY_Page_Down;
    case Inspector::Protocol::Automation::VirtualKey::PageDownRight:
        return WPE_KEY_KP_Page_Down;
    case Inspector::Protocol::Automation::VirtualKey::End:
        return WPE_KEY_End;
    case Inspector::Protocol::Automation::VirtualKey::EndRight:
        return WPE_KEY_KP_End;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return WPE_KEY_Home;
    case Inspector::Protocol::Automation::VirtualKey::HomeRight:
        return WPE_KEY_KP_Home;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        return WPE_KEY_Left;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrowRight:
        return WPE_KEY_KP_Left;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        return WPE_KEY_Up;
    case Inspector::Protocol::Automation::VirtualKey::UpArrowRight:
        return WPE_KEY_KP_Up;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        return WPE_KEY_Right;
    case Inspector::Protocol::Automation::VirtualKey::RightArrowRight:
        return WPE_KEY_KP_Right;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return WPE_KEY_Down;
    case Inspector::Protocol::Automation::VirtualKey::DownArrowRight:
        return WPE_KEY_KP_Down;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        return WPE_KEY_Insert;
    case Inspector::Protocol::Automation::VirtualKey::InsertRight:
        return WPE_KEY_KP_Insert;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return WPE_KEY_Delete;
    case Inspector::Protocol::Automation::VirtualKey::DeleteRight:
        return WPE_KEY_KP_Delete;
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

static uint32_t modifiersForKeyVal(unsigned keyVal)
{
    switch (keyVal) {
    case WPE_KEY_Shift_L:
    case WPE_KEY_Shift_R:
        return WPE_MODIFIER_KEYBOARD_SHIFT;
    case WPE_KEY_Control_L:
    case WPE_KEY_Control_R:
        return WPE_MODIFIER_KEYBOARD_CONTROL;
    case WPE_KEY_Alt_L:
    case WPE_KEY_Alt_R:
        return WPE_MODIFIER_KEYBOARD_ALT;
    case WPE_KEY_Meta_L:
    case WPE_KEY_Meta_R:
        return WPE_MODIFIER_KEYBOARD_META;
    }
    return 0;
}
#endif // ENABLE(WPE_PLATFORM)

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, Variant<VirtualKey, CharKey>&& key)
{
    if (page.viewBackend()) {
        platformSimulateKeyboardInteractionLibWPE(page, interaction, WTFMove(key), m_currentModifiers);
        return;
    }
#if ENABLE(WPE_PLATFORM)
    uint32_t keyVal;
    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            keyVal = keyValForVirtualKey(virtualKey);
        },
        [&] (CharKey charKey) {
            keyVal = wpe_unicode_to_keyval(charKey);
        }
    );
    uint32_t modifiers = modifiersForKeyVal(keyVal);

    switch (interaction) {
    case KeyboardInteraction::KeyPress:
        m_currentModifiers |= modifiers;
        doKeyStrokeEvent(page, true, keyVal, m_currentModifiers);
        break;
    case KeyboardInteraction::KeyRelease:
        m_currentModifiers &= ~modifiers;
        doKeyStrokeEvent(page, false, keyVal, m_currentModifiers);
        break;
    case KeyboardInteraction::InsertByKey:
        doKeyStrokeEvent(page, true, keyVal, m_currentModifiers, true);
        break;
    }
#endif // ENABLE(WPE_PLATFORM)
}
void WebAutomationSession::platformSimulateKeySequence(WebPageProxy& page, const String& keySequence)
{
    if (page.viewBackend()) {
        platformSimulateKeySequenceLibWPE(page, keySequence, m_currentModifiers);
        return;
    }
#if ENABLE(WPE_PLATFORM)
    for (auto codePoint : StringView(keySequence).codePoints())
        doKeyStrokeEvent(page, true, wpe_unicode_to_keyval(codePoint), m_currentModifiers, true);
#endif
}
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
void WebAutomationSession::platformSimulateWheelInteraction(WebPageProxy& page, const WebCore::IntPoint& locationInView, const WebCore::IntSize& delta)
{
    auto location = deviceScaleLocationInView(page, locationInView);

    if (page.viewBackend()) {
        platformSimulateWheelInteractionLibWPE(page, location, delta);
        return;
    }

#if ENABLE(WPE_PLATFORM)
    auto* view = page.wpeView();
    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_scroll_new(view, WPE_INPUT_SOURCE_MOUSE, 0, static_cast<WPEModifiers>(0), delta.width(), delta.height(), false, false, location.x(), location.y()));
    wpe_view_event(view, event.get());
#endif
}
#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)

void WebAutomationSession::platformSimulateTouchInteraction(WebPageProxy&page, TouchInteraction interaction, const WebCore::IntPoint& locationInView, std::optional<Seconds> duration, AutomationCompletionHandler&& completionHandler)
{

    if (page.viewBackend()) {
        LOG(Automation, "Touch event emulation is not supported for the legacy libwpe API");
        completionHandler(AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage::InternalError, "Touch event emulation is not supported for the legacy libwpe API"_s));
        return;
    }

    auto location = deviceScaleLocationInView(page, locationInView);

#if ENABLE(WPE_PLATFORM)
    GRefPtr<WPEEvent> event;

    switch (interaction) {
    case TouchInteraction::TouchDown:
        event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_DOWN, page.wpeView(), WPE_INPUT_SOURCE_TOUCHSCREEN, 0,
            static_cast<WPEModifiers>(m_currentModifiers), 0, location.x(), location.y()));
        break;
    case TouchInteraction::LiftUp:
        event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_UP, page.wpeView(), WPE_INPUT_SOURCE_TOUCHSCREEN, 0,
            static_cast<WPEModifiers>(m_currentModifiers), 0, location.x(), location.y()));
        break;
    case TouchInteraction::MoveTo:
        // TODO: Spread over intermediate points based on the duration, like iOS's WKTouchEventGenerator::moveToPoints
        // See https://bugs.webkit.org/show_bug.cgi?id=275031
        event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_MOVE, page.wpeView(), WPE_INPUT_SOURCE_TOUCHSCREEN, 0,
            static_cast<WPEModifiers>(m_currentModifiers), 0, location.x(), location.y()));
        break;
    }

    wpe_view_event(page.wpeView(), event.get());
#endif
    completionHandler(std::nullopt);
}
#endif

} // namespace WebKit
