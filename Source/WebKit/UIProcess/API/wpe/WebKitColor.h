/*
 * Copyright (C) 2019 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitColor_h
#define WebKitColor_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

/**
 * WebKitColor:
 * @red: Red channel, between 0.0 and 1.0 inclusive
 * @green: Green channel, between 0.0 and 1.0 inclusive
 * @blue: Blue channel, between 0.0 and 1.0 inclusive
 * @alpha: Alpha channel, between 0.0 and 1.0 inclusive
 *
 * A WebKitColor is a boxed type representing a RGBA color.
 *
 * Since: 2.24
 */
struct _WebKitColor {
    gdouble red;
    gdouble green;
    gdouble blue;
    gdouble alpha;
};

typedef struct _WebKitColor WebKitColor;

#define WEBKIT_TYPE_COLOR (webkit_color_get_type())

WEBKIT_API GType
webkit_color_get_type     (void);

WEBKIT_API gboolean
webkit_color_parse        (WebKitColor *color, const gchar *color_string);

G_END_DECLS

#endif /* WebKitColor_h */
