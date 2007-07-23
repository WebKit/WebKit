/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_GTK_SETTINGS_H
#define WEBKIT_GTK_SETTINGS_H

#include <glib.h>
#include <glib-object.h>

#include "webkitgtkdefines.h"

G_BEGIN_DECLS

typedef enum {
    WEBKIT_GTK_FONT_FAMILY_STANDARD,
    WEBKIT_GTK_FONT_FAMILY_FIXED,
    WEBKIT_GTK_FONT_FAMILY_SERIF,
    WEBKIT_GTK_FONT_FAMILY_SANS_SERIF,
    WEBKIT_GTK_FONT_FAMILY_CURSIVE,
    WEBKIT_GTK_FONT_FAMILY_FANTASY,
    WEBKIT_GTK_FONT_FAMILY_LAST_ENTRY
} WebKitGtkFontFamily;

typedef enum {
    WEBKIT_GTK_EDITABLE_LINK_DEFAULT_BEHAVIOUR,
    WEBKIT_GTK_EDITABLE_LINK_ALWAYS_LIVE,
    WEBKIT_GTK_EDITABLE_LINK_ONLY_WITH_SHIFT_KEY,
    WEBKIT_GTK_EDITABLE_LINK_LIVE_WHEN_NOT_FOCUSED,
    WEBKIT_GTK_EDITABLE_LINK_NEVER_LIVE
} WebKitGtkEditableLinkBehaviour;

typedef struct _WebKitGtkSettings WebKitGtkSettings;
typedef struct _WebKitGtkSettingsPrivate WebKitGtkSettingsPrivate;

struct _WebKitGtkSettings {
    gchar* font_name[WEBKIT_GTK_FONT_FAMILY_LAST_ENTRY];
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
    WebKitGtkEditableLinkBehaviour editable_link_behaviour;
    gboolean uses_page_cache;
    gboolean shrink_standalone_images_to_fit;
    gboolean show_urls_in_tool_tips;
    gchar* ftp_directory_template_path;
    gboolean force_ftp_directory_listings;
    gboolean developer_extras_enabled;


    WebKitGtkSettingsPrivate *private_data;
};

GType
webkit_gtk_web_settings_get_type (void);

WebKitGtkSettings*
webkit_gtk_web_settings_copy (WebKitGtkSettings* setting);

void
webkit_gtk_web_settings_free (WebKitGtkSettings* setting);

WebKitGtkSettings*
webkit_gtk_web_settings_get_global (void);

void
webkit_gtk_web_settings_set_global (WebKitGtkSettings* setting);

void
webkit_gtk_web_settings_set_font_family(WebKitGtkSettings*, WebKitGtkFontFamily family, gchar *family_name);

gchar*
webkit_gtk_web_settings_get_font_family(WebKitGtkSettings*, WebKitGtkFontFamily family);

void
webkit_gtk_web_settings_set_user_style_sheet_location(WebKitGtkSettings*, gchar*);

void
webkit_gtk_set_ftp_directory_template_path(WebKitGtkSettings*, gchar*);


G_END_DECLS


#endif
