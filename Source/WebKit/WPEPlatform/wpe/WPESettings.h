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

#pragma once

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

/**
 * WPESettingsError:
 * @WPE_SETTINGS_ERROR_INCORRECT_TYPE: Incorrect GVariantType for key
 * @WPE_SETTINGS_ERROR_NOT_REGISTERED: Key has not been registered
 * @WPE_SETTINGS_ERROR_ALREADY_REGISTERED: Key has already been registered
 * @WPE_SETTINGS_ERROR_INVALID_VALUE: Failed to parse a value from a keyfile
 * 
 * #WPESettings errors
 */
typedef enum {
    WPE_SETTINGS_ERROR_INCORRECT_TYPE,
    WPE_SETTINGS_ERROR_NOT_REGISTERED,
    WPE_SETTINGS_ERROR_ALREADY_REGISTERED,
    WPE_SETTINGS_ERROR_INVALID_VALUE,
} WPESettingsError;

#define WPE_SETTINGS_ERROR (wpe_settings_error_quark())
WPE_API GQuark wpe_settings_error_quark(void);

/**
 * WPESettingsSource:
 * @WPE_SETTINGS_SOURCE_PLATFORM: Set by the platform
 * @WPE_SETTINGS_SOURCE_APPLICATION: Set by the application
 *
 * Indicates the source of a settings change.
 */
typedef enum {
    WPE_SETTINGS_SOURCE_PLATFORM,
    WPE_SETTINGS_SOURCE_APPLICATION,
} WPESettingsSource;

#define WPE_TYPE_SETTINGS (wpe_settings_get_type())
WPE_API G_DECLARE_FINAL_TYPE(WPESettings, wpe_settings, WPE, SETTINGS, GObject)


WPE_API gboolean      wpe_settings_register                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   const GVariantType *type,
                                                                   GVariant           *default_value,
                                                                   GError            **error);

WPE_API gboolean      wpe_settings_load_from_keyfile              (WPESettings        *settings,
                                                                   GKeyFile           *keyfile,
                                                                   GError            **error);

WPE_API void         wpe_settings_save_to_keyfile                 (WPESettings        *settings,
                                                                   GKeyFile           *keyfile);

WPE_API gboolean     wpe_settings_set_value                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GVariant           *value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API GVariant    *wpe_settings_get_value                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API gint32       wpe_settings_get_int32                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API gint64       wpe_settings_get_int64                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API guint32      wpe_settings_get_uint32                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API guint64      wpe_settings_get_uint64                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API const char *wpe_settings_get_string                      (WPESettings        *settings,
                                                                  const char         *key,
                                                                  GError            **error);

WPE_API gdouble      wpe_settings_get_double                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_get_boolean                     (WPESettings        *settings,
                                                                   const char         *key,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_int32                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   gint32              value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_int64                       (WPESettings        *settings,
                                                                   const char         *key,
                                                                   gint64              value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_uint32                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   guint32             value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_uint64                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   guint64             value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_string                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   const char         *value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_double                      (WPESettings        *settings,
                                                                   const char         *key,
                                                                   gdouble             value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

WPE_API gboolean     wpe_settings_set_boolean                     (WPESettings        *settings,
                                                                   const char         *key,
                                                                   gboolean            value,
                                                                   WPESettingsSource   source,
                                                                   GError            **error);

G_END_DECLS

