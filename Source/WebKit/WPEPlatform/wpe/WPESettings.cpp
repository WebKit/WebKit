/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPESettings.h"

#include <glib.h>
#include <wpe/WPEDisplay.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>


struct SettingEntry {
    SettingEntry() = default;

    SettingEntry(GRefPtr<GVariant>&& defaultValue, GUniquePtr<GVariantType>&& type)
        : defaultValue(WTFMove(defaultValue))
        , type(WTFMove(type))
    {
    }

    GRefPtr<GVariant> value() const
    {
        return setValue ? setValue : defaultValue;
    }

    GRefPtr<GVariant> setValue;
    GRefPtr<GVariant> defaultValue;
    GUniquePtr<GVariantType> type;
    bool setByApplication { false };
};

/**
 * wpe_settings_error_quark:
 *
 * Gets the WPESettings Error Quark.
 *
 * Returns: a #GQuark.
 **/
G_DEFINE_QUARK(wpe-settings-error-quark, wpe_settings_error)

/**
 * WPESettings:
 *
 * This class stores settings for the WPE platform.
 *
 * It is a map of keys to arbitrary values. These keys are registered with [method@WPESettings.register].
 *
 * They can be stored and loaded with [method@WPESettings.save_to_keyfile] and [method@WPESettings.load_from_keyfile].
 *
 * The [signal@WPESettings::changed] signal is emitted when a setting is changed. You can connect to the detailed signal
 * to watch a specific key, e.g. `changed::/wpe-platform/fonts/font-hinting`.
 */
struct _WPESettingsPrivate {
    HashMap<CString, SettingEntry> settings;

    _WPESettingsPrivate()
    {
        struct Setting {
            const char* key;
            const GVariantType* type;
            GVariant* defaultValue;
        };

        // We convert all floating refs to normal as multiple instances share the same static objects.
        static const std::vector<Setting> defaultSettings = {
            { WPE_SETTING_FONT_NAME, G_VARIANT_TYPE_STRING, g_variant_ref_sink(g_variant_new_string("Sans 10")) },
            { WPE_SETTING_DARK_MODE, G_VARIANT_TYPE_BOOLEAN, g_variant_ref_sink(g_variant_new_boolean(false)) },
            { WPE_SETTING_DISABLE_ANIMATIONS, G_VARIANT_TYPE_BOOLEAN, g_variant_ref_sink(g_variant_new_boolean(false)) },
            { WPE_SETTING_FONT_ANTIALIAS, G_VARIANT_TYPE_BOOLEAN, g_variant_ref_sink(g_variant_new_boolean(true)) },
            { WPE_SETTING_FONT_HINTING_STYLE, G_VARIANT_TYPE_BYTE, g_variant_ref_sink(g_variant_new_byte(WPE_SETTINGS_HINTING_STYLE_SLIGHT)) },
            { WPE_SETTING_FONT_SUBPIXEL_LAYOUT, G_VARIANT_TYPE_BYTE, g_variant_ref_sink(g_variant_new_byte(WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB)) },
            { WPE_SETTING_FONT_DPI, G_VARIANT_TYPE_DOUBLE, g_variant_ref_sink(g_variant_new_double(96.0)) },
            { WPE_SETTING_CURSOR_BLINK_TIME, G_VARIANT_TYPE_UINT32, g_variant_ref_sink(g_variant_new_uint32(1200)) },
        };

        for (auto& setting : defaultSettings)
            settings.add(setting.key, SettingEntry(setting.defaultValue, GUniquePtr<GVariantType>(g_variant_type_copy(setting.type))));
    }
};

enum {
    CHANGED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_FINAL_TYPE(WPESettings, wpe_settings, G_TYPE_OBJECT, GObject)

static void wpe_settings_class_init(WPESettingsClass* settingsClass)
{
    /**
     * WPESettings::changed:
     * @settings: a #WPESettings
     * @key: the key that changed
     * @value: the new value
     *
     * Emitted when a settings is changed.
     * It will contain a detail of the specific key that changed.
     */
    signals[CHANGED] = g_signal_new(
        "changed",
        G_TYPE_FROM_CLASS(G_OBJECT_CLASS(settingsClass)),
        static_cast<GSignalFlags>(G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        G_TYPE_STRING,
        G_TYPE_VARIANT);
}

static CString makeKeyPath(const char* group, const char* key)
{
    std::span<char> buffer;
    size_t length = strlen(group) + strlen(key) + 3;

    CString path = CString::newUninitialized(length - 1, buffer);
    g_snprintf(buffer.data(), length, "/%s/%s", group, key);

    return path;
}

/**
 * wpe_settings_register:
 * @settings: a #WPESettings
 * @key: the key to register
 * @type: the type of the setting
 * @default_value: (transfer floating): the default value for the setting
 * @error: return location for a #GError, or %NULL
 *
 * Registers a key to be used for settings. This is only intended to be used by platforms and not applications.
 *
 * The key must begin with `/wpe-platform/` and not end with `/`. e.g. `/wpe-platform/fonts/hinting`.
 *
 * Any floating reference of @default_value will be consumed.
 *
 * It is an error to call this function with a key that has already been registered.
 */
gboolean wpe_settings_register(WPESettings* settingsObject, const char* key, const GVariantType* type, GVariant* defaultValue, GError** error)
{
    g_return_val_if_fail(WPE_IS_SETTINGS(settingsObject), FALSE);
    g_return_val_if_fail(key && g_str_has_prefix(key, "/wpe-platform/") && !g_str_has_suffix(key, "/"), FALSE);
    g_return_val_if_fail(defaultValue, FALSE);
    g_return_val_if_fail(type, FALSE);
    g_return_val_if_fail(g_variant_type_equal(type, g_variant_get_type(defaultValue)), FALSE);

    if (settingsObject->priv->settings.contains(key)) {
        g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_ALREADY_REGISTERED, "%s has already been reigstered", key);
        return FALSE;
    }

    settingsObject->priv->settings.add(key, SettingEntry(defaultValue, GUniquePtr<GVariantType>(g_variant_type_copy(type))));

    return TRUE;
}

/**
 * wpe_settings_load_from_keyfile:
 * @settings: a #WPESettings
 * @keyfile: the keyfile to load settings from
 * @error: return location for a #GError, or %NULL
 *
 * Loads settings from a keyfile.
 *
 * Only groups named `wpe-platform` or with the prefix `wpe-platform/` will be considered.
 * All keys under these groups must be registered with [method@WPESettings.register].
 *
 * If a value is already set it will be overwritten.
 *
 * All keys loaded are expected to be formatted as a GVariant.
 *
 * [signal@WPESettings::changed] will be emitted for each key that is loaded with a *new* value.
 *
 * Returns: %TRUE if the settings were loaded successfully, %FALSE otherwise.
 */
gboolean wpe_settings_load_from_keyfile(WPESettings* settingsObject, GKeyFile* keyFile, GError** error)
{
    g_return_val_if_fail(WPE_IS_SETTINGS(settingsObject), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    g_return_val_if_fail(keyFile, FALSE);

    GUniquePtr<char*> groups(g_key_file_get_groups(keyFile, nullptr));
    for (unsigned i = 0; groups.get()[i]; i++) {
        const char* group = groups.get()[i];

        if (!g_str_has_prefix(group, "wpe-platform/") && strcmp(group, "wpe-platform"))
            continue;

        GUniquePtr<char*> keys(g_key_file_get_keys(keyFile, group, nullptr, nullptr));

        for (unsigned k = 0; keys.get()[k]; k++) {
            const char* key = keys.get()[k];
            GUniquePtr<char> value(g_key_file_get_value(keyFile, group, key, nullptr));
            if (!value)
                continue;

            auto path = makeKeyPath(group, key);
            auto iter = settingsObject->priv->settings.find(path);
            if (iter == settingsObject->priv->settings.end()) {
                g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_NOT_REGISTERED, "Key %s not registered", path.data());
                return FALSE;
            }

            GUniqueOutPtr<GError> innerError;
            GRefPtr<GVariant> parsedValue = adoptGRef(g_variant_parse(iter->value.type.get(), value.get(), nullptr, nullptr, &innerError.outPtr()));
            if (!parsedValue) {
                g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_INVALID_VALUE, "Failed to parse value for key %s: %s", path.data(), innerError->message);
                return FALSE;
            }

            if (iter->value.setValue && !g_variant_compare(iter->value.setValue.get(), parsedValue.get()))
                continue;

            iter->value.setValue = WTFMove(parsedValue);
            g_signal_emit(settingsObject, signals[CHANGED], g_quark_from_string(path.data()), path.data(), iter->value.setValue.get());
        }
    }

    return TRUE;
}

/**
 * wpe_settings_save_to_keyfile:
 * @settings: a #WPESettings
 * @keyfile: the keyfile to save settings to
 *
 * Adds settings to a keyfile. Keys are transformed into group names, for example:
 * `/wpe-platform/fonts/hinting` will be saved as `hinting` in the group `wpe-platform/fonts`.
 *
 * Any existing values in the keyfile will be overwritten.
 */
void wpe_settings_save_to_keyfile(WPESettings* settingsObject, GKeyFile* keyFile)
{
    g_return_if_fail(WPE_IS_SETTINGS(settingsObject));
    g_return_if_fail(keyFile);

    for (auto& [key, entry] : settingsObject->priv->settings) {
        if (!entry.setValue)
            continue;

        // Transform "/foo/bar/baz" into "foo/bar" and "baz".
        GUniquePtr<char> keyString(g_strdup(key.data()));
        auto* keyStart = strrchr(keyString.get(), '/');
        ASSERT(keyStart && keyStart != keyString.get());
        *keyStart = '\0';
        keyStart++;

        const char* group = keyString.get() + 1;
        // FIXME: Handle empty?

        GUniquePtr<char> variantString(g_variant_print(entry.setValue.get(), FALSE));
        g_key_file_set_value(keyFile, group, keyStart, variantString.get());
    }
}

/**
 * wpe_settings_set_value:
 * @settings: a #WPESettings
 * @key: the key to set
 * @value: (transfer floating) (nullable): the value to set or %NULL
 * @source: the source of the settings change
 * @error: return location for a #GError, or %NULL
 *
 * If @source is %WPE_SETTINGS_SOURCE_APPLICATION, then the value will not be overwritten by the platform.
 * This value should always be %WPE_SETTINGS_SOURCE_PLATFORM for platforms themselves. This can cause this
 * method to return %TRUE even though no setting changes.
 *
 * To set a value @key must have been registered and @value must be of the correct type.
 *
 * Any floating reference of @value will be consumed.
 *
 * Setting @value to %NULL will reset it to the default.
 *
 * On a value being changed it will emit [signal@WPESettings::changed].
 *
 * Returns: %FALSE if @error was set, %TRUE otherwise
 */
gboolean wpe_settings_set_value(WPESettings* settingsObject, const char* key, GVariant* value, WPESettingsSource source, GError** error)
{
    g_return_val_if_fail(WPE_IS_SETTINGS(settingsObject), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    g_return_val_if_fail(key && *key == '/', FALSE);

    auto iter = settingsObject->priv->settings.find(key);
    if (iter == settingsObject->priv->settings.end()) {
        g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_NOT_REGISTERED, "Key %s not registered", key);
        return FALSE;
    }

    if (iter->value.setByApplication && source != WPE_SETTINGS_SOURCE_APPLICATION)
        return TRUE;

    if (!value) {
        auto previousValue = iter->value.setValue;
        iter->value.setValue = nullptr;
        iter->value.setByApplication = source == WPE_SETTINGS_SOURCE_APPLICATION;
        if (previousValue && g_variant_compare(previousValue.get(), iter->value.defaultValue.get()))
            g_signal_emit(settingsObject, signals[CHANGED], g_quark_from_string(key), key, iter->value.value().get());
        return TRUE;
    }

    if (!g_variant_type_equal(g_variant_get_type(value), iter->value.type.get())) {
        g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_INCORRECT_TYPE,
            "Incorrect type (%s) for key %s", g_variant_get_type_string(value), key);
        return FALSE;
    }

    auto previousValue = iter->value.value();
    iter->value.setValue = value;
    iter->value.setByApplication = source == WPE_SETTINGS_SOURCE_APPLICATION;

    if (g_variant_compare(previousValue.get(), value))
        g_signal_emit(settingsObject, signals[CHANGED], g_quark_from_string(key), key, iter->value.value().get());

    return TRUE;
}

/**
 * wpe_settings_get_value:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError, or %NULL
 *
 * If the key is not registered it will return %NULL and set @error.
 *
 * Returns: (transfer none): the value associated with the given key.
 */
GVariant* wpe_settings_get_value(WPESettings* settingsObject, const char* key, GError** error)
{
    g_return_val_if_fail(WPE_IS_SETTINGS(settingsObject), NULL);
    g_return_val_if_fail(key && *key == '/', NULL);

    const auto iter = settingsObject->priv->settings.find(key);
    if (iter == settingsObject->priv->settings.end()) {
        g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_NOT_REGISTERED, "Key %s not registered", key);
        return nullptr;
    }

    return iter->value.value().get();
}

#define g_variant_get_string(str) g_variant_get_string(str, nullptr)
#define WPE_SETTINGS_HELPER_API(type, name, gvariant_type, retval) \
    type wpe_settings_get_##name(WPESettings* settings, const char* key, GError** error) \
    { \
        GVariant* value = wpe_settings_get_value(settings, key, error); \
        if (!value) \
            return retval; \
        if (!g_variant_type_equal(g_variant_get_type(value), gvariant_type)) { \
            g_set_error(error, WPE_SETTINGS_ERROR, WPE_SETTINGS_ERROR_INCORRECT_TYPE, \
                "Key is type %s, expected %s", g_variant_get_type_string(value), reinterpret_cast<const char*>(gvariant_type)); \
            return retval; \
        } \
        return g_variant_get_##name(value); \
    } \
    gboolean wpe_settings_set_##name(WPESettings* settings, const char* key, type value, WPESettingsSource source, GError** error) \
    { \
        return wpe_settings_set_value(settings, key, g_variant_new_##name(value), source, error); \
    }

// Unfortunately the gobject-introspection parser doesn't like comments in macros.

/**
 * wpe_settings_set_int32:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_int32:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(gint32, int32, G_VARIANT_TYPE_INT32, 0)
/**
 * wpe_settings_set_int64:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_int64:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(gint64, int64, G_VARIANT_TYPE_INT64, 0)
/**
 * wpe_settings_set_byte:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_byte:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(guint8, byte, G_VARIANT_TYPE_BYTE, 0)
/**
 * wpe_settings_set_uint32:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_uint32:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(guint32, uint32, G_VARIANT_TYPE_UINT32, 0)
/**
 * wpe_settings_set_uint64:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_uint64:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(guint64, uint64, G_VARIANT_TYPE_UINT64, 0)
/**
 * wpe_settings_set_boolean:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_boolean:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(gboolean, boolean, G_VARIANT_TYPE_BOOLEAN, FALSE)
/**
 * wpe_settings_set_string:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_string:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(const char*, string, G_VARIANT_TYPE_STRING, nullptr)
/**
 * wpe_settings_set_double:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @value: the value to set
 * @source: the source of the settings change
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.set_value] that also validates
 * the type.
 */
/**
 * wpe_settings_get_double:
 * @settings: a #WPESettings
 * @key: the key to look up
 * @error: return location for a #GError or %NULL
 *
 * This is a simple wrapper around [method@WPESettings.get_value] that also validates
 * the return type.
 */
WPE_SETTINGS_HELPER_API(gdouble, double, G_VARIANT_TYPE_DOUBLE, 0.0)

#undef WPE_SETTINGS_HELPER_API
#undef g_variant_get_string
