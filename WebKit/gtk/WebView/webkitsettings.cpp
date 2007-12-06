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

#include "config.h"

#include "webkitsettings.h"

extern "C" {
GType webkit_web_settings_get_type(void)
{
    return GType();
}

WebKitSettings* webkit_web_settings_copy(WebKitSettings* setting)
{
    return 0;
}

void webkit_web_settings_free(WebKitSettings* setting)
{
}

WebKitSettings* webkit_web_settings_get_global(void)
{
    return 0;
}

void webkit_web_settings_set_global (WebKitSettings* setting)
{
}

void webkit_web_settings_set_font_family(WebKitSettings*, WebKitFontFamily family, gchar* family_name)
{
}

const gchar* webkit_web_settings_get_font_family(WebKitSettings*, WebKitFontFamily family)
{
    return 0;
}

void webkit_web_settings_set_user_style_sheet_location(WebKitSettings*, gchar*)
{
}

void webkit_set_ftp_directory_template_path(WebKitSettings*, gchar*)
{
}
}
