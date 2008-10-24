/*
 * Copyright (C) 2008 Collabora, Ltd.
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

#ifndef guriescape_h
#define guriescape_h

#include <glib.h>
#include <wtf/Platform.h>

G_BEGIN_DECLS

#if !PLATFORM(WIN_OS) && !GLIB_CHECK_VERSION(2,16,0)

#define g_uri_escape_string _webcore_g_uri_escape_string
#define g_uri_unescape_string _webcore_g_uri_unescape_string

char    *_webcore_g_uri_escape_string   (const char *unescaped,
                                         const char *reserved_chars_allowed,
                                         gboolean    allow_utf8);

char    *_webcore_g_uri_unescape_string (const char *escaped_string,
                                         const char *illegal_characters);

#endif

G_END_DECLS

#endif /* guriescape_h */
