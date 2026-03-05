/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebViewTest.h"

#include <wtf/glib/GUniquePtr.h>

void WebViewTest::platformDestroy()
{
}

void WebViewTest::quitMainLoopAfterProcessingPendingEvents()
{
    // FIXME: implement if needed.
    quitMainLoop();
}

void WebViewTest::resizeView(int width, int height)
{
    // FIXME: implement.
}

void WebViewTest::showInWindow(int width, int height)
{
#if ENABLE(WPE_PLATFORM)
    if (m_display) {
        auto* view = webkit_web_view_get_wpe_view(m_webView.get());
        g_assert_true(WPE_IS_VIEW(view));
        if (width && height)
            wpe_toplevel_resize(wpe_view_get_toplevel(view), width, height);
        wpe_view_set_visible(view, TRUE);
        wpe_view_focus_in(view);
        return;
    }
#endif
#if USE(LIBWPE)
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView.get()));
    wpe_view_backend_add_activity_state(backend, wpe_view_activity_state_visible | wpe_view_activity_state_in_window | wpe_view_activity_state_focused);
#endif
}

void WebViewTest::hideView()
{
#if ENABLE(WPE_PLATFORM)
    if (m_display) {
        auto* view = webkit_web_view_get_wpe_view(m_webView.get());
        wpe_view_set_visible(view, FALSE);
        wpe_view_focus_out(view);
        return;
    }
#endif
#if USE(LIBWPE)
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView.get()));
    wpe_view_backend_remove_activity_state(backend, wpe_view_activity_state_visible | wpe_view_activity_state_focused);
#endif
}

#if USE(LIBWPE)
static unsigned testModifiersToWPELegacy(const OptionSet<WebViewTest::Modifiers>& modifiers)
{
    unsigned wpeModifiers = 0;
    if (modifiers.contains(WebViewTest::Modifiers::Control))
        wpeModifiers |= wpe_input_keyboard_modifier_control;
    if (modifiers.contains(WebViewTest::Modifiers::Shift))
        wpeModifiers |= wpe_input_keyboard_modifier_shift;
    if (modifiers.contains(WebViewTest::Modifiers::Alt))
        wpeModifiers |= wpe_input_keyboard_modifier_alt;
    if (modifiers.contains(WebViewTest::Modifiers::Meta))
        wpeModifiers |= wpe_input_keyboard_modifier_meta;
    return wpeModifiers;
}

static unsigned testMouseButtonToWPELegacy(WebViewTest::MouseButton button)
{
    switch (button) {
    case WebViewTest::MouseButton::Primary:
        return 1;
    case WebViewTest::MouseButton::Middle:
        return 3;
    case WebViewTest::MouseButton::Secondary:
        return 2;
    }
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

#if ENABLE(WPE_PLATFORM)
static WPEModifiers testModifiersToWPE(const OptionSet<WebViewTest::Modifiers>& modifiers)
{
    unsigned wpeModifiers = 0;
    if (modifiers.contains(WebViewTest::Modifiers::Control))
        wpeModifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (modifiers.contains(WebViewTest::Modifiers::Shift))
        wpeModifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (modifiers.contains(WebViewTest::Modifiers::Alt))
        wpeModifiers |= WPE_MODIFIER_KEYBOARD_ALT;
    if (modifiers.contains(WebViewTest::Modifiers::Meta))
        wpeModifiers |= WPE_MODIFIER_KEYBOARD_META;
    return static_cast<WPEModifiers>(wpeModifiers);
}

static unsigned testMouseButtonToWPE(WebViewTest::MouseButton button)
{
    switch (button) {
    case WebViewTest::MouseButton::Primary:
        return WPE_BUTTON_PRIMARY;
    case WebViewTest::MouseButton::Middle:
        return WPE_BUTTON_MIDDLE;
    case WebViewTest::MouseButton::Secondary:
        return WPE_BUTTON_SECONDARY;
    }
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

void WebViewTest::mouseMoveTo(int x, int y, OptionSet<Modifiers> mouseModifiers)
{
    // FIXME: implement.
}

void WebViewTest::clickMouseButton(int x, int y, MouseButton button, OptionSet<Modifiers> mouseModifiers)
{
#if ENABLE(WPE_PLATFORM)
    if (m_display) {
        auto wpeModifiers = testModifiersToWPE(mouseModifiers);
        auto wpeButton = testMouseButtonToWPE(button);
        auto* view = webkit_web_view_get_wpe_view(m_webView.get());
        auto* event = wpe_event_pointer_button_new(WPE_EVENT_POINTER_DOWN, view, WPE_INPUT_SOURCE_MOUSE, 0, wpeModifiers, wpeButton, x, y, 1);
        wpe_view_event(view, event);
        wpe_event_unref(event);
        event = wpe_event_pointer_button_new(WPE_EVENT_POINTER_UP, view, WPE_INPUT_SOURCE_MOUSE, 0, wpeModifiers, wpeButton, x, y, 0);
        wpe_view_event(view, event);
        wpe_event_unref(event);
        return;
    }
#endif
#if USE(LIBWPE)
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView.get()));
    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, 0, x, y, testMouseButtonToWPELegacy(button), 1, testModifiersToWPELegacy(mouseModifiers) };
    wpe_view_backend_dispatch_pointer_event(backend, &event);
    event.state = 0;
    wpe_view_backend_dispatch_pointer_event(backend, &event);
#endif
}

void WebViewTest::keyStroke(unsigned keyVal, OptionSet<Modifiers> keyModifiers)
{
#if ENABLE(WPE_PLATFORM)
    if (m_display) {
        auto* view = webkit_web_view_get_wpe_view(m_webView.get());
        unsigned keycode = 0;
        auto* keymap = wpe_display_get_keymap(wpe_view_get_display(view));
        GUniqueOutPtr<WPEKeymapEntry> entries;
        guint entriesCount;
        if (wpe_keymap_get_entries_for_keyval(keymap, keyVal, &entries.outPtr(), &entriesCount))
            keycode = entries.get()[0].keycode;

        auto wpeModifiers = testModifiersToWPE(keyModifiers);
        auto* event = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_DOWN, view, WPE_INPUT_SOURCE_KEYBOARD, 0, wpeModifiers, keycode, keyVal);
        wpe_view_event(view, event);
        wpe_event_unref(event);
        event = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP, view, WPE_INPUT_SOURCE_KEYBOARD, 0, wpeModifiers, keycode, keyVal);
        wpe_view_event(view, event);
        wpe_event_unref(event);
        return;
    }
#endif
#if USE(LIBWPE)
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView.get()));
    struct wpe_input_xkb_keymap_entry* entries;
    uint32_t entriesCount;
    wpe_input_xkb_context_get_entries_for_key_code(wpe_input_xkb_context_get_default(), keyVal, &entries, &entriesCount);
    struct wpe_input_keyboard_event event { 0, keyVal, entriesCount ? entries[0].hardware_key_code : 0, true, testModifiersToWPELegacy(keyModifiers) };
    wpe_view_backend_dispatch_keyboard_event(backend, &event);
    event.pressed = false;
    wpe_view_backend_dispatch_keyboard_event(backend, &event);
    free(entries);
#endif
}
