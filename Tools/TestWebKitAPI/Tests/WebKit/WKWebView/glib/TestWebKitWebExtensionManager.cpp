/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "TestMain.h"
#include "WebExtensionUtilities.h"
#include "WebKitWebExtensionManagerInternal.h"
#include <WebKitWebExtensionInternal.h>
#include <WebKitWebExtensionManager.h>
#include <wtf/HashMap.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

static GRefPtr<GBytes> createGBytes(const gchar* string)
{
    return adoptGRef(g_bytes_new_static(string, strlen(string)));
}

static void testConfigurationInitialization(Test* test, gconstpointer)
{
    GRefPtr<WebKitWebExtensionManager> manager = adoptGRef(webkit_web_extension_manager_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(manager.get()));

    g_assert_true(webkit_web_extension_manager_get_persistence(manager.get()));
    g_assert_null(webkit_web_extension_manager_get_identifier(manager.get()));
    g_assert_false(webkitWebExtensionManagerGetIsTemporary(manager.get()));
    g_assert_nonnull(webkit_web_extension_manager_get_settings(manager.get()));
    g_assert_cmpstr(webkitWebExtensionManagerGetStorageDirectoryPath(manager.get()), ==, webkitWebExtensionManagerGetStorageDirectoryPath(webkit_web_extension_manager_new()));

    manager = adoptGRef(webkit_web_extension_manager_new_with_non_persistent_settings());

    g_assert_false(webkit_web_extension_manager_get_persistence(manager.get()));
    g_assert_null(webkit_web_extension_manager_get_identifier(manager.get()));
    g_assert_false(webkitWebExtensionManagerGetIsTemporary(manager.get()));
    g_assert_nonnull(webkit_web_extension_manager_get_settings(manager.get()));
    g_assert_null(webkitWebExtensionManagerGetStorageDirectoryPath(manager.get()));

    GUniquePtr<char> uuid(g_uuid_string_random());
    manager = adoptGRef(webkit_web_extension_manager_new_with_identifier(uuid.get()));

    g_assert_true(webkit_web_extension_manager_get_persistence(manager.get()));
    g_assert_cmpstr(webkit_web_extension_manager_get_identifier(manager.get()), ==, uuid.get());
    g_assert_false(webkitWebExtensionManagerGetIsTemporary(manager.get()));
    g_assert_nonnull(webkit_web_extension_manager_get_settings(manager.get()));
    g_assert_cmpstr(webkitWebExtensionManagerGetStorageDirectoryPath(manager.get()), ==, webkitWebExtensionManagerGetStorageDirectoryPath(webkit_web_extension_manager_new_with_identifier(uuid.get())));

    manager = adoptGRef(webkitWebExtensionManagerNewWithTemporaryConfiguration());

    g_assert_true(webkit_web_extension_manager_get_persistence(manager.get()));
    g_assert_null(webkit_web_extension_manager_get_identifier(manager.get()));
    g_assert_true(webkitWebExtensionManagerGetIsTemporary(manager.get()));
    g_assert_nonnull(webkit_web_extension_manager_get_settings(manager.get()));
    g_assert_cmpstr(webkitWebExtensionManagerGetStorageDirectoryPath(manager.get()), !=, webkitWebExtensionManagerGetStorageDirectoryPath(webkit_web_extension_manager_new_with_identifier(uuid.get())));
}

static void testLoadingAndUnloadingContexts(Test* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString) {
        GRefPtr extension = adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(extension.get()));
        return extension;
    };

    auto getArraySize = [&](auto array) {
        guint arrayLength = 0;
        if (!array)
            return arrayLength;
        for (; array[arrayLength] != nullptr; arrayLength++)
            continue;
        g_free(array);
        return arrayLength;
    };

    GRefPtr<WebKitWebExtensionManager> manager = adoptGRef(webkit_web_extension_manager_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(manager.get()));

    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_extensions(manager.get())), ==, 0);
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_contexts(manager.get())), ==, 0);

    GRefPtr<WebKitWebExtension> extensionOne = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test One\", \"description\": \"Test One\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> contextOne = adoptGRef(webkit_web_extension_context_new_for_extension(extensionOne.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(contextOne.get()));
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_loaded(contextOne.get()));
    g_assert_null(webkit_web_extension_manager_extension_context_for_extension(manager.get(), extensionOne.get()));

    GRefPtr<WebKitWebExtension> extensionTwo = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test Two\", \"description\": \"Test Two\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> contextTwo = adoptGRef(webkit_web_extension_context_new_for_extension(extensionTwo.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(contextTwo.get()));
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_loaded(contextTwo.get()));
    g_assert_null(webkit_web_extension_manager_extension_context_for_extension(manager.get(), extensionTwo.get()));

    g_assert_true(webkit_web_extension_manager_load_extension_context(manager.get(), contextOne.get(), &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_context_get_loaded(contextOne.get()));
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_extensions(manager.get())), ==, 1);
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_contexts(manager.get())), ==, 1);

    g_assert_false(webkit_web_extension_manager_load_extension_context(manager.get(), contextOne.get(), &error.outPtr()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_CONTEXT_ERROR, WEBKIT_WEB_EXTENSION_CONTEXT_ERROR_ALREADY_LOADED);
    g_assert_true(webkit_web_extension_context_get_loaded(contextOne.get()));
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_extensions(manager.get())), ==, 1);
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_contexts(manager.get())), ==, 1);
    g_assert_true(webkit_web_extension_manager_extension_context_for_extension(manager.get(), extensionOne.get()) == contextOne.get());

    g_assert_true(webkit_web_extension_manager_load_extension_context(manager.get(), contextTwo.get(), &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_context_get_loaded(contextTwo.get()));
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_extensions(manager.get())), ==, 2);
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_contexts(manager.get())), ==, 2);
    g_assert_true(webkit_web_extension_manager_extension_context_for_extension(manager.get(), extensionTwo.get()) == contextTwo.get());
    g_assert_true(webkit_web_extension_manager_unload_extension_context(manager.get(), contextOne.get(), &error.outPtr()));
    g_assert_no_error(error.get());

    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_extensions(manager.get())), ==, 1);
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_contexts(manager.get())), ==, 1);
    g_assert_true(webkit_web_extension_manager_unload_extension_context(manager.get(), contextTwo.get(), &error.outPtr()));
    g_assert_no_error(error.get());

    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_extensions(manager.get())), ==, 0);
    g_assert_cmpint(getArraySize(webkit_web_extension_manager_get_loaded_contexts(manager.get())), ==, 0);
    g_assert_false(webkit_web_extension_manager_unload_extension_context(manager.get(), contextOne.get(), &error.outPtr()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_CONTEXT_ERROR, WEBKIT_WEB_EXTENSION_CONTEXT_ERROR_NOT_LOADED);
}

void beforeAll()
{
    Test::add("WebKitWebExtensionManager", "configuration-initialization", testConfigurationInitialization);
    Test::add("WebKitWebExtensionManager", "loading-and-unloading-contexts", testLoadingAndUnloadingContexts);

    g_set_prgname("org.webkit.app-TestWebKitGTK");
}

void afterAll()
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
