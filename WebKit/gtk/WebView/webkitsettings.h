/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
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

#ifndef WEBKIT_SETTINGS_H
#define WEBKIT_SETTINGS_H

#include <glib.h>
#include <glib-object.h>

#include "webkitdefines.h"

G_BEGIN_DECLS

typedef enum {
    WEBKIT_FONT_FAMILY_STANDARD,
    WEBKIT_FONT_FAMILY_FIXED,
    WEBKIT_FONT_FAMILY_SERIF,
    WEBKIT_FONT_FAMILY_SANS_SERIF,
    WEBKIT_FONT_FAMILY_CURSIVE,
    WEBKIT_FONT_FAMILY_FANTASY,
    WEBKIT_FONT_FAMILY_LAST_ENTRY
} WebKitFontFamily;

typedef enum {
    WEBKIT_EDITABLE_LINK_DEFAULT_BEHAVIOUR,
    WEBKIT_EDITABLE_LINK_ALWAYS_LIVE,
    WEBKIT_EDITABLE_LINK_ONLY_WITH_SHIFT_KEY,
    WEBKIT_EDITABLE_LINK_LIVE_WHEN_NOT_FOCUSED,
    WEBKIT_EDITABLE_LINK_NEVER_LIVE
} WebKitEditableLinkBehaviour;

typedef struct _WebKitSettings WebKitSettings;
typedef struct _WebKitSettingsPrivate WebKitSettingsPrivate;

struct _WebKitSettings {
    gchar* font_name[WEBKIT_FONT_FAMILY_LAST_ENTRY];
    gint   minimum_font_size;
    gint   minimum_logical_font_size;
    gint   default_font_size;
    gint   default_fixed_font_size;
    gboolean load_images_automatically;
    gboolean is_java_script_enabled;
    gboolean java_script_can_open_windows_automatically;
    gboolean plugins_enabled;
    gboolean private_browsing;
    gchar* user_style_sheet_location;
    gboolean should_print_backgrounds;
    gboolean text_areas_are_resizable;
    WebKitEditableLinkBehaviour editable_link_behaviour;
    gboolean uses_web_view_cache;
    gboolean shrink_standalone_images_to_fit;
    gboolean show_uris_in_tool_tips;
    gchar* ftp_directory_template_path;
    gboolean force_ftp_directory_listings;
    gboolean developer_extras_enabled;


    WebKitSettingsPrivate *private_data;
};

GType
webkit_web_settings_get_type (void);

WebKitSettings*
webkit_web_settings_copy (WebKitSettings* setting);

void
webkit_web_settings_free (WebKitSettings* setting);

WebKitSettings*
webkit_web_settings_get_global (void);

void
webkit_web_settings_set_global (WebKitSettings* setting);

void
webkit_web_settings_set_font_family(WebKitSettings*, WebKitFontFamily family, gchar *family_name);

const gchar*
webkit_web_settings_get_font_family(WebKitSettings*, WebKitFontFamily family);

void
webkit_web_settings_set_user_style_sheet_location(WebKitSettings*, gchar*);

void
webkit_set_ftp_directory_template_path(WebKitSettings*, gchar*);


G_END_DECLS


#endif
