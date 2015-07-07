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
#include "WebProcessTest.h"

#include <gio/gio.h>
#include <wtf/gobject/GUniquePtr.h>

typedef HashMap<String, std::function<PassOwnPtr<WebProcessTest> ()>> TestsMap;
static TestsMap& testsMap()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(TestsMap, s_testsMap, ());
    return s_testsMap;
}

void WebProcessTest::add(const String& testName, std::function<PassOwnPtr<WebProcessTest> ()> closure)
{
    testsMap().add(testName, std::forward<std::function<PassOwnPtr<WebProcessTest> ()>>(closure));
}

PassOwnPtr<WebProcessTest> WebProcessTest::create(const String& testName)
{
    g_assert(testsMap().contains(testName));
    return testsMap().get(testName)();
}

static const char introspectionXML[] =
    "<node>"
    " <interface name='org.webkit.gtk.WebProcessTest'>"
    "  <method name='RunTest'>"
    "   <arg type='s' name='path' direction='in'/>"
    "   <arg type='a{sv}' name='args' direction='in'/>"
    "   <arg type='b' name='result' direction='out'/>"
    "  </method>"
    " </interface>"
    "</node>";

static void methodCallCallback(GDBusConnection* connection, const char* sender, const char* objectPath, const char* interfaceName, const char* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData)
{
    if (g_strcmp0(interfaceName, "org.webkit.gtk.WebProcessTest"))
        return;

    if (!g_strcmp0(methodName, "RunTest")) {
        const char* testPath;
        GVariant* args;
        g_variant_get(parameters, "(&s@a{sv})", &testPath, &args);
        OwnPtr<WebProcessTest> test = WebProcessTest::create(String::fromUTF8(testPath));
        bool result = test->runTest(g_strrstr(testPath, "/") + 1, WEBKIT_WEB_EXTENSION(userData), args);
        g_variant_unref(args);

        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", result));
    } else
        g_assert_not_reached();
}

static const GDBusInterfaceVTable interfaceVirtualTable = {
    methodCallCallback, 0, 0, { 0, }
};

static void busAcquiredCallback(GDBusConnection* connection, const char* name, gpointer userData)
{
    static GDBusNodeInfo* introspectionData = 0;
    if (!introspectionData)
        introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, 0);

    GUniqueOutPtr<GError> error;
    unsigned registrationID = g_dbus_connection_register_object(
        connection,
        "/org/webkit/gtk/WebProcessTest",
        introspectionData->interfaces[0],
        &interfaceVirtualTable,
        g_object_ref(userData),
        static_cast<GDestroyNotify>(g_object_unref),
        &error.outPtr());
    if (!registrationID)
        g_warning("Failed to register object: %s\n", error->message);
}

extern "C" void webkit_web_extension_initialize(WebKitWebExtension* extension)
{
    g_bus_own_name(
        G_BUS_TYPE_SESSION,
        "org.webkit.gtk.WebProcessTest",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        busAcquiredCallback,
        0, 0,
        g_object_ref(extension),
        static_cast<GDestroyNotify>(g_object_unref));
}
