/*
 * Copyright (C) 2020 Igalia S.L.
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

#ifndef WebKitRectangle_h
#define WebKitRectangle_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

/**
 * WebKitRectangle:
 * @x: The X coordinate of the top-left corner of the rectangle.
 * @y: The Y coordinate of the top-left corner of the rectangle.
 * @width: The width of the rectangle.
 * @height: The height of the rectangle.
 *
 * A WebKitRectangle is a boxed type representing a rectangle with integer coordinates.
 *
 * Since: 2.28
 */
struct _WebKitRectangle {
    gint x;
    gint y;
    gint width;
    gint height;
};

typedef struct _WebKitRectangle WebKitRectangle;

#define WEBKIT_TYPE_RECTANGLE (webkit_rectangle_get_type())

WEBKIT_API GType
webkit_rectangle_get_type     (void);

G_END_DECLS

#endif /* WebKitRectangle_h */
