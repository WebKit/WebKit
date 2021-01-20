/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "TestMain.h"

#include <glib/gstdio.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

GRefPtr<GDBusServer> Test::s_dbusServer;
Vector<GRefPtr<GDBusConnection>> Test::s_dbusConnections;
HashMap<uint64_t, GDBusConnection*> Test::s_dbusConnectionPageMap;

void beforeAll();
void afterAll();

static GUniquePtr<char> testDataDirectory(g_dir_make_tmp("WebKitGLibTests-XXXXXX", nullptr));

const char* Test::dataDirectory()
{
    return testDataDirectory.get();
}

static void registerGResource(void)
{
    GUniquePtr<char> resourcesPath(g_build_filename(WEBKIT_TEST_RESOURCES_DIR, "webkitglib-tests-resources.gresource", nullptr));
    GResource* resource = g_resource_load(resourcesPath.get(), nullptr);
    g_assert_nonnull(resource);

    g_resources_register(resource);
    g_resource_unref(resource);
}

static void removeNonEmptyDirectory(const char* directoryPath)
{
    GDir* directory = g_dir_open(directoryPath, 0, 0);
    g_assert_nonnull(directory);
    const char* fileName;
    while ((fileName = g_dir_read_name(directory))) {
        GUniquePtr<char> filePath(g_build_filename(directoryPath, fileName, nullptr));
        if (g_file_test(filePath.get(), G_FILE_TEST_IS_DIR))
            removeNonEmptyDirectory(filePath.get());
        else
            g_unlink(filePath.get());
    }
    g_dir_close(directory);
    g_rmdir(directoryPath);
}

static void dbusConnectionClosed(GDBusConnection* connection)
{
    auto it = Test::s_dbusConnections.find(connection);
    g_assert(it != notFound);

    for (auto it : Test::s_dbusConnectionPageMap) {
        if (it.value == connection)
            it.value = nullptr;
    }
    Test::s_dbusConnections.remove(it);
}

static gboolean dbusServerConnection(GDBusServer* server, GDBusConnection* connection)
{
    g_signal_connect(connection, "closed", G_CALLBACK(dbusConnectionClosed), nullptr);
    g_assert(!Test::s_dbusConnections.contains(connection));
    Test::s_dbusConnections.append(connection);

    g_dbus_connection_signal_subscribe(connection, nullptr, "org.webkit.gtk.WebExtensionTest", "PageCreated", "/org/webkit/gtk/WebExtensionTest",
        nullptr, G_DBUS_SIGNAL_FLAGS_NONE, [](GDBusConnection* connection, const char*, const char*, const char*, const char*, GVariant* parameters, gpointer) {
            guint64 pageID;
            g_variant_get(parameters, "(t)", &pageID);
            g_assert(Test::s_dbusConnections.contains(connection));
            Test::s_dbusConnectionPageMap.set(pageID, connection);
        }, nullptr, nullptr);

    return TRUE;
}

static void startDBusServer()
{
    GUniqueOutPtr<GError> error;
    GUniquePtr<char> address(g_strdup_printf("unix:tmpdir=%s", testDataDirectory.get()));
    GUniquePtr<char> guid(g_dbus_generate_guid());
    Test::s_dbusServer = adoptGRef(g_dbus_server_new_sync(address.get(), G_DBUS_SERVER_FLAGS_NONE, guid.get(), nullptr, nullptr, &error.outPtr()));
    if (!Test::s_dbusServer)
        g_error("Failed to start DBus server: %s", error->message);

    g_signal_connect(Test::s_dbusServer.get(), "new-connection", G_CALLBACK(dbusServerConnection), nullptr);
    g_dbus_server_start(Test::s_dbusServer.get());
}

static void stopDBusServer()
{
    g_dbus_server_stop(Test::s_dbusServer.get());
    Test::s_dbusServer = nullptr;
}

int main(int argc, char** argv)
{
    g_unsetenv("DBUS_SESSION_BUS_ADDRESS");
#if PLATFORM(GTK)
    gtk_test_init(&argc, &argv, nullptr);
#else
    g_test_init(&argc, &argv, nullptr);
#endif
    g_setenv("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH, FALSE);
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);
    g_setenv("LC_ALL", "C", TRUE);
    g_setenv("GIO_USE_VFS", "local", TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    // Get rid of runtime warnings about deprecated properties and signals, since they break the tests.
    g_setenv("G_ENABLE_DIAGNOSTIC", "0", TRUE);
    g_test_bug_base("https://bugs.webkit.org/");

    registerGResource();
    startDBusServer();

    beforeAll();
    int returnValue = g_test_run();
    afterAll();

    stopDBusServer();
    removeNonEmptyDirectory(testDataDirectory.get());

    return returnValue;
}

