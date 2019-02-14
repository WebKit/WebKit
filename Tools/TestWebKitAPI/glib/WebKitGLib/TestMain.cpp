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
#include <wtf/Environment.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

uint32_t Test::s_webExtensionID = 0;

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

int main(int argc, char** argv)
{
    Environment::remove("DBUS_SESSION_BUS_ADDRESS");
#if PLATFORM(GTK)
    gtk_test_init(&argc, &argv, nullptr);
#else
    g_test_init(&argc, &argv, nullptr);
#endif
    Environment::setIfNotDefined("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH);
    Environment::setIfNotDefined("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH);
    Environment::set("LC_ALL", "C");
    Environment::set("GIO_USE_VFS", "local");
    Environment::set("GSETTINGS_BACKEND", "memory");
    // Get rid of runtime warnings about deprecated properties and signals, since they break the tests.
    Environment::set("G_ENABLE_DIAGNOSTIC", "0");
    g_test_bug_base("https://bugs.webkit.org/");

    registerGResource();

    beforeAll();
    int returnValue = g_test_run();
    afterAll();

    removeNonEmptyDirectory(testDataDirectory.get());

    return returnValue;
}

