/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#include "WebKitTestBus.h"
#include "WebViewTest.h"
#include <wtf/gobject/GRefPtr.h>

static WebKitTestBus* bus;

static void testWebExtension(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><head><title>WebKitGTK+ Web Extensions Test</title></head><body></body></html>", 0);
    test->waitUntilLoadFinished();

    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy("org.webkit.gtk.WebExtensionTest",
        "/org/webkit/gtk/WebExtensionTest" , "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "GetTitle",
        g_variant_new("(t)", webkit_web_view_get_page_id(test->m_webView)),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert(result);

    const char* title;
    g_variant_get(result.get(), "(&s)", &title);
    g_assert_cmpstr(title, ==, "WebKitGTK+ Web Extensions Test");
}

void beforeAll()
{
    webkit_web_context_set_web_extensions_directory(webkit_web_context_get_default(), WEBKIT_TEST_WEB_EXTENSIONS_DIR);
    bus = new WebKitTestBus();
    if (!bus->run())
        return;

    WebViewTest::add("WebKitWebExtension", "dom-document-title", testWebExtension);
}

void afterAll()
{
    delete bus;
}
