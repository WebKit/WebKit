/*
 * Copyright (C) 2013 Igalia S.L.
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
#include "WebProcessTestRunner.h"

#include <wtf/gobject/GUniquePtr.h>

WebProcessTestRunner::WebProcessTestRunner()
    : m_mainLoop(g_main_loop_new(0, TRUE))
    , m_bus(adoptGRef(g_test_dbus_new(G_TEST_DBUS_NONE)))
{
    // Save the DISPLAY env var to restore it after calling g_test_dbus_up() that unsets it.
    // See https://bugs.webkit.org/show_bug.cgi?id=125621.
    const char* display = g_getenv("DISPLAY");
    g_test_dbus_up(m_bus.get());
    g_setenv("DISPLAY", display, FALSE);
    m_connection = adoptGRef(g_bus_get_sync(G_BUS_TYPE_SESSION, 0, 0));
}

WebProcessTestRunner::~WebProcessTestRunner()
{
    g_main_loop_unref(m_mainLoop);

    // g_test_dbus_down waits until the connection is freed, so release our refs explicitly before calling it.
    m_connection = 0;
    m_proxy = 0;
    g_test_dbus_down(m_bus.get());
}

void WebProcessTestRunner::proxyCreatedCallback(GObject*, GAsyncResult* result, WebProcessTestRunner* testRunner)
{
    testRunner->m_proxy = adoptGRef(g_dbus_proxy_new_finish(result, 0));
    g_main_loop_quit(testRunner->m_mainLoop);
}

GDBusProxy* WebProcessTestRunner::proxy()
{
    if (m_proxy)
        return m_proxy.get();

    g_dbus_proxy_new(m_connection.get(), G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, 0,
        "org.webkit.gtk.WebProcessTest", "/org/webkit/gtk/WebProcessTest", "org.webkit.gtk.WebProcessTest",
        0, reinterpret_cast<GAsyncReadyCallback>(WebProcessTestRunner::proxyCreatedCallback), this);
    g_main_loop_run(m_mainLoop);
    g_assert(m_proxy.get());

    return m_proxy.get();
}

void WebProcessTestRunner::onNameAppeared(GDBusConnection*, const char*, const char*, gpointer userData)
{
    WebProcessTestRunner* testRunner = static_cast<WebProcessTestRunner*>(userData);
    g_main_loop_quit(testRunner->m_mainLoop);
}

void WebProcessTestRunner::onNameVanished(GDBusConnection*, const char* name, gpointer userData)
{
    _exit(1);
}

void WebProcessTestRunner::testFinishedCallback(GDBusProxy* proxy, GAsyncResult* result, WebProcessTestRunner* testRunner)
{
    GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(proxy, result, 0));
    g_assert(returnValue.get());
    gboolean testResult;
    g_variant_get(returnValue.get(), "(b)", &testResult);
    testRunner->finishTest(testResult);
}

bool WebProcessTestRunner::runTest(const char* suiteName, const char* testName, GVariant* args)
{
    g_assert(g_variant_is_of_type(args, G_VARIANT_TYPE_VARDICT));

    unsigned watcherID = g_bus_watch_name_on_connection(m_connection.get(), "org.webkit.gtk.WebProcessTest", G_BUS_NAME_WATCHER_FLAGS_NONE,
        WebProcessTestRunner::onNameAppeared, WebProcessTestRunner::onNameVanished, this, 0);
    g_main_loop_run(m_mainLoop);

    m_testResult = false;
    GUniquePtr<char> testPath(g_strdup_printf("%s/%s", suiteName, testName));
    g_dbus_proxy_call(
        proxy(),
        "RunTest",
        g_variant_new("(s@a{sv})", testPath.get(), args),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0,
        reinterpret_cast<GAsyncReadyCallback>(WebProcessTestRunner::testFinishedCallback),
        this);
    g_main_loop_run(m_mainLoop);
    g_bus_unwatch_name(watcherID);

    return m_testResult;
}

void WebProcessTestRunner::finishTest(bool result)
{
    m_testResult = result;
    g_main_loop_quit(m_mainLoop);
}
