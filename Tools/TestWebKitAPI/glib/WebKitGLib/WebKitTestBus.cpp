/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitTestBus.h"

#include <wtf/Environment.h>

WebKitTestBus::WebKitTestBus()
    : m_bus(adoptGRef(g_test_dbus_new(G_TEST_DBUS_NONE)))
{
}

WebKitTestBus::~WebKitTestBus()
{
    g_test_dbus_down(m_bus.get());
}

bool WebKitTestBus::run()
{
    auto display = Environment::get("DISPLAY");
    auto runtimeDir = Environment::get("XDG_RUNTIME_DIR");
    g_test_dbus_up(m_bus.get());
    m_address = g_test_dbus_get_bus_address(m_bus.get());
    if (display)
        Environment::setIfNotDefined("DISPLAY", *display);
    if (runtimeDir)
        Environment::setIfNotDefined("XDG_RUNTIME_DIR", *runtimeDir);
    return !m_address.isNull();
}

GDBusConnection* WebKitTestBus::getOrCreateConnection()
{
    if (m_connection)
        return m_connection.get();

    g_assert_false(m_address.isNull());
    m_connection = adoptGRef(g_dbus_connection_new_for_address_sync(m_address.data(),
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
        nullptr, nullptr, nullptr));
    g_assert_nonnull(m_connection.get());
    return m_connection.get();
}

static void onNameAppeared(GDBusConnection*, const char*, const char*, gpointer userData)
{
    g_main_loop_quit(static_cast<GMainLoop*>(userData));
}

GDBusProxy* WebKitTestBus::createProxy(const char* serviceName, const char* objectPath, const char* interfaceName, GMainLoop* mainLoop)
{
    unsigned watcherID = g_bus_watch_name_on_connection(getOrCreateConnection(), serviceName, G_BUS_NAME_WATCHER_FLAGS_NONE, onNameAppeared, nullptr, mainLoop, nullptr);
    g_main_loop_run(mainLoop);
    g_bus_unwatch_name(watcherID);

    GDBusProxy* proxy = g_dbus_proxy_new_sync(
        connection(),
        G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
        nullptr, // GDBusInterfaceInfo
        serviceName,
        objectPath,
        interfaceName,
        nullptr, // GCancellable
        nullptr);
    g_assert_nonnull(proxy);
    return proxy;
}
