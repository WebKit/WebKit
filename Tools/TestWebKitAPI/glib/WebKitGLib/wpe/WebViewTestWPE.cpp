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

#include <wpe/wpe.h>

void WebViewTest::platformDestroy()
{
}

void WebViewTest::platformInitializeWebView()
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

void WebViewTest::showInWindow(int, int)
{
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView));
    wpe_view_backend_add_activity_state(backend, wpe_view_activity_state_visible | wpe_view_activity_state_in_window | wpe_view_activity_state_focused);
}

void WebViewTest::hideView()
{
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView));
    wpe_view_backend_remove_activity_state(backend, wpe_view_activity_state_visible | wpe_view_activity_state_focused);
}

void WebViewTest::mouseMoveTo(int x, int y, unsigned mouseModifiers)
{
    // FIXME: implement.
}

void WebViewTest::clickMouseButton(int x, int y, unsigned button, unsigned mouseModifiers)
{
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView));
    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, 0, x, y, button, 1, mouseModifiers };
    wpe_view_backend_dispatch_pointer_event(backend, &event);
    event.state = 0;
    wpe_view_backend_dispatch_pointer_event(backend, &event);
}

void WebViewTest::keyStroke(unsigned keyVal, unsigned keyModifiers)
{
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView));
    struct wpe_input_xkb_keymap_entry* entries;
    uint32_t entriesCount;
    wpe_input_xkb_context_get_entries_for_key_code(wpe_input_xkb_context_get_default(), keyVal, &entries, &entriesCount);
    struct wpe_input_keyboard_event event { 0, keyVal, entriesCount ? entries[0].hardware_key_code : 0, true, keyModifiers };
    wpe_view_backend_dispatch_keyboard_event(backend, &event);
    event.pressed = false;
    wpe_view_backend_dispatch_keyboard_event(backend, &event);
    free(entries);
}
