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

#ifndef WebProcessTestRunner_h
#define WebProcessTestRunner_h

#include <gio/gio.h>
#include <webkit2/webkit2.h>
#include <wtf/gobject/GRefPtr.h>

class WebProcessTestRunner {
public:
    WebProcessTestRunner();
    ~WebProcessTestRunner();

    bool runTest(const char* suiteName, const char* testName, GVariant* args);

private:
    static void proxyCreatedCallback(GObject*, GAsyncResult*, WebProcessTestRunner*);
    static void onNameAppeared(GDBusConnection*, const char*, const char*, gpointer);
    static void onNameVanished(GDBusConnection*, const char*, gpointer);
    static void testFinishedCallback(GDBusProxy*, GAsyncResult*, WebProcessTestRunner*);

    GDBusProxy* proxy();
    void finishTest(bool result);

    GMainLoop* m_mainLoop;
    GRefPtr<GTestDBus> m_bus;
    GRefPtr<GDBusConnection> m_connection;
    GRefPtr<GDBusProxy> m_proxy;
    bool m_testResult;
};

#endif // WebProcessTestRunner_h
