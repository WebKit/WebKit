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

void WebViewTest::showInWindow()
{
#if defined(WPE_BACKEND_CHECK_VERSION) && WPE_BACKEND_CHECK_VERSION(1, 1, 0)
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView));
    wpe_view_backend_add_activity_state(backend, wpe_view_activity_state_visible | wpe_view_activity_state_in_window | wpe_view_activity_state_focused);
#endif
}

void WebViewTest::hideView()
{
#if defined(WPE_BACKEND_CHECK_VERSION) && WPE_BACKEND_CHECK_VERSION(1, 1, 0)
    auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_webView));
    wpe_view_backend_remove_activity_state(backend, wpe_view_activity_state_visible | wpe_view_activity_state_focused);
#endif
}

void WebViewTest::mouseMoveTo(int x, int y, unsigned mouseModifiers)
{
    // FIXME: implement.
}

void WebViewTest::clickMouseButton(int x, int y, unsigned button, unsigned mouseModifiers)
{
    // FIXME: implement.
}

void WebViewTest::keyStroke(unsigned keyVal, unsigned keyModifiers)
{
    // FIXME: implement.
}
