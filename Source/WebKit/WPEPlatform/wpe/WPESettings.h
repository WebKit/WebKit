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
 * WPE_SETTING_FONT_NAME:
 *
 * String representing the font in the format of
 * "name size", e.g. "Deja Vu Sans, 16"
 *
 * VariantType: string
 *
 * Default: Sans 10
 */
#define WPE_SETTING_FONT_NAME "/wpe-platform/font-name"
/**
 * WPE_SETTING_DARK_MODE:
 *
 * Enables dark mode for websites.
 *
 * VariantType: boolean
 *
 * Default: false
 */
#define WPE_SETTING_DARK_MODE "/wpe-platform/dark-mode"
/**
 * WPE_SETTING_DISABLE_ANIMATIONS:
 *
 * Disables animations on websites.
 *
 * VariantType: boolean
 *
 * Default: false
 */
#define WPE_SETTING_DISABLE_ANIMATIONS "/wpe-platform/disable-animations"
/**
 * WPE_SETTING_FONT_ANTIALIAS:
 *
 * If antialiasing is applied to fonts.
 *
 * VariantType: boolean
 *
 * Default: true
 */
#define WPE_SETTING_FONT_ANTIALIAS "/wpe-platform/font-antialias"
/**
 * WPESettingsHintingStyle:
 * @WPE_SETTINGS_HINTING_STYLE_NONE
 * @WPE_SETTINGS_HINTING_STYLE_SLIGHT
 * @WPE_SETTINGS_HINTING_STYLE_MEDIUM
 * @WPE_SETTINGS_HINTING_STYLE_FULL
 */
typedef enum {
    WPE_SETTINGS_HINTING_STYLE_NONE,
    WPE_SETTINGS_HINTING_STYLE_SLIGHT,
    WPE_SETTINGS_HINTING_STYLE_MEDIUM,
    WPE_SETTINGS_HINTING_STYLE_FULL,
} WPESettingsHintingStyle;
/**
 * WPE_SETTING_FONT_HINTING_STYLE:
 *
 * Style of hinting to apply to font.
 *
 * VariantType: byte (WPESettingsHintingStyle)
 *
 * Default: WPE_SETTINGS_HINTING_STYLE_SLIGHT
 */
#define WPE_SETTING_FONT_HINTING_STYLE "/wpe-platform/font-hinting-style"
/**
 * WPESettingsSubpixelLayout:
 * @WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB
 * @WPE_SETTINGS_SUBPIXEL_LAYOUT_BGR
 * @WPE_SETTINGS_SUBPIXEL_LAYOUT_VRGB
 * @WPE_SETTINGS_SUBPIXEL_LAYOUT_VBGR
 */
typedef enum {
    WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB,
    WPE_SETTINGS_SUBPIXEL_LAYOUT_BGR,
    WPE_SETTINGS_SUBPIXEL_LAYOUT_VRGB,
    WPE_SETTINGS_SUBPIXEL_LAYOUT_VBGR,
} WPESettingsSubpixelLayout;
/**
 * WPE_SETTING_FONT_SUBPIXEL_LAYOUT:
 *
 * Layout of subpixels used for fonts.
 *
 * VariantType: byte (WPESettingsSubpixelLayout)
 *
 * Default: WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB
 */
#define WPE_SETTING_FONT_SUBPIXEL_LAYOUT "/wpe-platform/font-subpixel-layout"
/**
 * WPE_SETTING_FONT_DPI:
 *
 * DPI used for fonts.
 *
 * VariantType: double
 *
 * Default: 96.0
 */
#define WPE_SETTING_FONT_DPI "/wpe-platform/font-dpi"
/**
 * WPE_SETTING_CURSOR_BLINK_TIME:
 *
 * Rate of cursor blinking in ms.
 *
 * Setting to 0 will disable blinking.
 *
 * VariantType: uint32
 *
 * Default: 1200
 */
#define WPE_SETTING_CURSOR_BLINK_TIME "/wpe-platform/cursor-blink-time"


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

WPE_API guint8       wpe_settings_get_byte                        (WPESettings        *settings,
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

WPE_API gboolean     wpe_settings_set_byte                        (WPESettings        *settings,
                                                                   const char         *key,
                                                                   guint8              value,
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

