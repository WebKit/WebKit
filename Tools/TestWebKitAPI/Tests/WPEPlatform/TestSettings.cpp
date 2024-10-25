/*
 * Copyright (C) 2024 Igalia, S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TestMain.h"
#include <glib.h>
#include <wpe/WPESettings.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

static void testSettingsRegistration(Test*, gconstpointer)
{
    GRefPtr<WPESettings> settings = adoptGRef(WPE_SETTINGS(g_object_new(WPE_TYPE_SETTINGS, nullptr)));
    GUniqueOutPtr<GError> error;

    // Cannot set an unregistered key.
    GRefPtr<GVariant> someValue = g_variant_new_int32(42);
    gboolean ret = wpe_settings_set_value(settings.get(), "/wpe-platform/non-existing-key", someValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, &error.outPtr());
    g_assert_false(ret);
    g_assert_error(error.get(), WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_NOT_REGISTERED);

    // Can register new key.
    g_assert_true(wpe_settings_register(settings.get(), "/wpe-platform/a-new-key", G_VARIANT_TYPE_INT32, someValue.get(), nullptr));

    // Cannot re-register a key.
    g_assert_false(wpe_settings_register(settings.get(), "/wpe-platform/a-new-key", G_VARIANT_TYPE_INT32, someValue.get(), nullptr));

    // Default values are returned.
    GVariant* value = wpe_settings_get_value(settings.get(), "/wpe-platform/a-new-key", nullptr);
    g_assert_cmpvariant(value, someValue.get());

    // Setting a registered key.
    GRefPtr<GVariant> anotherValue = g_variant_new_int32(10000);
    ret = wpe_settings_set_value(settings.get(), "/wpe-platform/a-new-key", anotherValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_true(ret);

    // New value is returned.
    value = wpe_settings_get_value(settings.get(), "/wpe-platform/a-new-key", nullptr);
    g_assert_cmpvariant(value, anotherValue.get());

    // Setting a value back to default.
    ret = wpe_settings_set_value(settings.get(), "/wpe-platform/a-new-key", nullptr, WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_assert_true(ret);
    value = wpe_settings_get_value(settings.get(), "/wpe-platform/a-new-key", nullptr);
    g_assert_cmpvariant(value, someValue.get());

    // Getting an unreigstered key.
    value = wpe_settings_get_value(settings.get(), "/wpe-platform/non-existing-key", nullptr);
    g_assert_null(value);

    // Cannot set a value with a different type.
    GRefPtr<GVariant> invalidValue = g_variant_new_string("invalid");
    ret = wpe_settings_set_value(settings.get(), "/wpe-platform/a-new-key", invalidValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, &error.outPtr());
    g_assert_false(ret);
    g_assert_error(error.get(), WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_INCORRECT_TYPE);

    // Setting an application value overrules the platform value.
    GVariant* applicationValue = g_variant_new_int32(71902379);
    ret = wpe_settings_set_value(settings.get(), "/wpe-platform/a-new-key", applicationValue, WPE_SETTINGS_SOURCE_APPLICATION, nullptr);
    g_assert_true(ret);
    g_assert_cmpvariant(wpe_settings_get_value(settings.get(), "/wpe-platform/a-new-key", nullptr), applicationValue);
    ret = wpe_settings_set_value(settings.get(), "/wpe-platform/a-new-key", someValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_assert_true(ret);
    g_assert_cmpvariant(wpe_settings_get_value(settings.get(), "/wpe-platform/a-new-key", nullptr), applicationValue);
}

static void testSettingsSaveKeyFiles(Test*, gconstpointer)
{
    GRefPtr<WPESettings> settings = adoptGRef(WPE_SETTINGS(g_object_new(WPE_TYPE_SETTINGS, nullptr)));

    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    GRefPtr<GVariant> someValue = g_variant_new_int32(42);
    GRefPtr<GVariant> anotherValue = g_variant_new_int32(10000);

    wpe_settings_register(settings.get(), "/wpe-platform/a-new-key", G_VARIANT_TYPE_INT32, someValue.get(), nullptr);

    // Default setings are not written.
    wpe_settings_save_to_keyfile(settings.get(), keyFile.get());
    g_assert_false(g_key_file_has_key(keyFile.get(), "wpe-platform", "a-new-key", nullptr));

    // Non-default is written.
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-new-key", anotherValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    wpe_settings_save_to_keyfile(settings.get(), keyFile.get());
    g_assert_true(g_key_file_has_key(keyFile.get(), "wpe-platform", "a-new-key", nullptr));
}

static void testSettingsLoadKeyFiles(Test*, gconstpointer)
{
    GRefPtr<WPESettings> settings = adoptGRef(WPE_SETTINGS(g_object_new(WPE_TYPE_SETTINGS, nullptr)));

    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    GRefPtr<GVariant> someValue = g_variant_new_int32(42);
    GRefPtr<GVariant> anotherValue = g_variant_new_int32(10000);
    GUniqueOutPtr<GError> error;
    gboolean ret;

    g_key_file_set_integer(keyFile.get(), "wpe-platform", "a-key", 42);

    // Loading a key that has not been registered.
    ret = wpe_settings_load_from_keyfile(settings.get(), keyFile.get(), &error.outPtr());
    g_assert_error(error.get(), WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_NOT_REGISTERED);
    g_assert_false(ret);

    // Loading a registered key.
    ret = wpe_settings_register(settings.get(), "/wpe-platform/a-key", G_VARIANT_TYPE_INT32, someValue.get(), &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_true(ret);
    ret = wpe_settings_load_from_keyfile(settings.get(), keyFile.get(), &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_true(ret);
    g_assert_cmpvariant(wpe_settings_get_value(settings.get(), "/wpe-platform/a-key", nullptr), someValue.get());

    // Loading again overwrites values.
    g_key_file_set_integer(keyFile.get(), "wpe-platform", "a-key", 10000);
    ret = wpe_settings_load_from_keyfile(settings.get(), keyFile.get(), nullptr);
    g_assert_true(ret);
    g_assert_cmpvariant(wpe_settings_get_value(settings.get(), "/wpe-platform/a-key", nullptr), anotherValue.get());

    // Loading a key with a different type.
    g_key_file_set_string(keyFile.get(), "wpe-platform", "a-key", "'not-a-number'");
    ret = wpe_settings_load_from_keyfile(settings.get(), keyFile.get(), &error.outPtr());
    g_assert_error(error.get(), WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_INVALID_VALUE);
    g_assert_false(ret);
}

static void testSettingsChangedSignal(Test*, gconstpointer)
{
    GRefPtr<WPESettings> settings = adoptGRef(WPE_SETTINGS(g_object_new(WPE_TYPE_SETTINGS, nullptr)));
    GRefPtr<GVariant> someValue = g_variant_new_int32(42);
    GRefPtr<GVariant> anotherValue = g_variant_new_int32(10000);

    wpe_settings_register(settings.get(), "/wpe-platform/a-key", G_VARIANT_TYPE_INT32, someValue.get(), nullptr);
    wpe_settings_register(settings.get(), "/wpe-platform/another-key", G_VARIANT_TYPE_INT32, anotherValue.get(), nullptr);

    // Changing a specific key.
    int count = 0;
    int handler = g_signal_connect(settings.get(), "changed::/wpe-platform/a-key", G_CALLBACK(+[](WPESettings*, const char* key, GVariant* value, int* count) {
        g_assert_cmpstr(key, ==, "/wpe-platform/a-key");
        g_assert_true(g_variant_type_equal(g_variant_get_type(value), G_VARIANT_TYPE_INT32));
        g_assert_cmpint(g_variant_get_int32(value), ==, 12345);
        (*count)++;
    }), &count);
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", g_variant_new_int32(12345), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_signal_handler_disconnect(settings.get(), handler);
    g_assert_cmpint(count, ==, 1);

    // Changing any key.
    count = 0;
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", nullptr, WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    wpe_settings_set_value(settings.get(), "/wpe-platform/another--key", nullptr, WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    handler = g_signal_connect(settings.get(), "changed", G_CALLBACK(+[](WPESettings*, const char* key, GVariant* value, int* count) {
        g_assert_true(g_str_has_prefix(key, "/wpe-platform/"));
        g_assert_true(g_variant_type_equal(g_variant_get_type(value), G_VARIANT_TYPE_INT32));
        (*count)++;
    }), &count);
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", g_variant_new_int32(12345), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    wpe_settings_set_value(settings.get(), "/wpe-platform/another-key", g_variant_new_int32(12345), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_signal_handler_disconnect(settings.get(), handler);
    g_assert_cmpint(count, ==, 2);

    // Resetting to default.
    count = 0;
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", anotherValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    handler = g_signal_connect(settings.get(), "changed", G_CALLBACK(+[](WPESettings*, const char* key, GVariant* value, int* count) {
        (*count)++;
    }), &count);
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", nullptr, WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_signal_handler_disconnect(settings.get(), handler);
    g_assert_cmpint(count, ==, 1);

    // Setting to the same value it already was.
    count = 0;
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", anotherValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    handler = g_signal_connect(settings.get(), "changed", G_CALLBACK(+[](WPESettings*, const char* key, GVariant* value, int* count) {
        (*count)++;
    }), &count);
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", anotherValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_signal_handler_disconnect(settings.get(), handler);
    g_assert_cmpint(count, ==, 0);

    // Setting it to a value equal to default.
    count = 0;
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", nullptr, WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    handler = g_signal_connect(settings.get(), "changed", G_CALLBACK(+[](WPESettings*, const char* key, GVariant* value, int* count) {
        (*count)++;
    }), &count);
    wpe_settings_set_value(settings.get(), "/wpe-platform/a-key", someValue.get(), WPE_SETTINGS_SOURCE_PLATFORM, nullptr);
    g_signal_handler_disconnect(settings.get(), handler);
    g_assert_cmpint(count, ==, 0);

    // Change on load.
    count = 0;
    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    g_key_file_set_integer(keyFile.get(), "wpe-platform", "a-key", 12345);
    handler = g_signal_connect(settings.get(), "changed::/wpe-platform/a-key", G_CALLBACK(+[](WPESettings*, const char* key, GVariant* value, int* count) {
        g_assert_cmpstr(key, ==, "/wpe-platform/a-key");
        g_assert_true(g_variant_type_equal(g_variant_get_type(value), G_VARIANT_TYPE_INT32));
        g_assert_cmpint(g_variant_get_int32(value), ==, 12345);
        (*count)++;
    }), &count);
    wpe_settings_load_from_keyfile(settings.get(), keyFile.get(), nullptr);
    g_assert_cmpint(count, ==, 1);

    // Unchanged on load
    wpe_settings_load_from_keyfile(settings.get(), keyFile.get(), nullptr);
    g_assert_cmpint(count, ==, 1);
    g_signal_handler_disconnect(settings.get(), handler);
}

static void testSettingsHelperAPI(Test*, gconstpointer)
{
    GRefPtr<WPESettings> settings = adoptGRef(WPE_SETTINGS(g_object_new(WPE_TYPE_SETTINGS, nullptr)));

    wpe_settings_register(settings.get(), "/wpe-platform/a-key", G_VARIANT_TYPE_INT32, g_variant_new_int32(42), nullptr);
    GUniqueOutPtr<GError> error;

    g_assert_true(wpe_settings_set_int32(settings.get(), "/wpe-platform/a-key", 12345, WPE_SETTINGS_SOURCE_PLATFORM, &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert(wpe_settings_get_int32(settings.get(), "/wpe-platform/a-key", &error.outPtr()) == 12345);
    g_assert_no_error(error.get());

    g_assert_false(wpe_settings_set_string(settings.get(), "/wpe-platform/a-key", "invalid", WPE_SETTINGS_SOURCE_PLATFORM, &error.outPtr()));
    g_assert_error(error.get(), WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_INCORRECT_TYPE);
}

void beforeAll()
{
    Test::add("Settings", "registration", testSettingsRegistration);
    Test::add("Settings", "save-key-files", testSettingsSaveKeyFiles);
    Test::add("Settings", "load-key-files", testSettingsLoadKeyFiles);
    Test::add("Settings", "changed-signal", testSettingsChangedSignal);
    Test::add("Settings", "helper-api", testSettingsHelperAPI);
}

void afterAll()
{
}
