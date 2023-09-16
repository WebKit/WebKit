/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <WebCore/SoupVersioning.h>

static WebKitTestServer* kServer;

#if USE(SOUP2)
static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

class MemoryPressureTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(MemoryPressureTest, setup, teardown);

    static void setup()
    {
        WebKitMemoryPressureSettings* settings = webkit_memory_pressure_settings_new();
        webkit_memory_pressure_settings_set_memory_limit(settings, 1);
        webkit_memory_pressure_settings_set_poll_interval(settings, 0.001);
        webkit_memory_pressure_settings_set_kill_threshold(settings, 1);
        webkit_memory_pressure_settings_set_strict_threshold(settings, 0.75);
        webkit_memory_pressure_settings_set_conservative_threshold(settings, 0.5);
#if ENABLE(2022_GLIB_API)
        webkit_network_session_set_memory_pressure_settings(settings);
#else
        webkit_website_data_manager_set_memory_pressure_settings(settings);
#endif
        webkit_memory_pressure_settings_free(settings);
    }

    static void teardown()
    {
#if ENABLE(2022_GLIB_API)
        webkit_network_session_set_memory_pressure_settings(nullptr);
#else
        webkit_website_data_manager_set_memory_pressure_settings(nullptr);
#endif
    }

    static gboolean loadFailedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, const char* failingURI, GError* error, MemoryPressureTest* test)
    {
        g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(loadFailedCallback), test);
        g_main_loop_quit(test->m_mainLoop);

        return TRUE;
    }

    void waitUntilLoadFailed()
    {
        g_signal_connect(m_webView, "load-failed", G_CALLBACK(MemoryPressureTest::loadFailedCallback), this);
        g_main_loop_run(m_mainLoop);
    }
};

static void testMemoryPressureSettings(MemoryPressureTest* test, gconstpointer)
{
    // We have set a memory limit of 1MB with a kill threshold of 1 and a poll interval of 0.001.
    // The network process will use around 2MB to load the test. The MemoryPressureHandler will
    // kill the process as soon as it detects that it's using more than 1MB, so the network process
    // won't be able to complete the resource load. This causes an internal error and the load-failed
    // signal is emitted.
    GUniquePtr<char> fileURL(g_strdup_printf("file://%s/simple.html", Test::getResourcesDir(Test::WebKit2Resources).data()));
    test->loadURI(fileURL.get());
    test->waitUntilLoadFailed();
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    MemoryPressureTest::add("WebKitWebsiteData", "memory-pressure", testMemoryPressureSettings);
}

void afterAll()
{
    delete kServer;
}
