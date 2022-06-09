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

#include "config.h"
#include "WebKitRectangle.h"

/**
 * SECTION: WebKitRectangle
 * @Short_description: A boxed type representing a rectangle with integer coordinates
 * @Title: WebKitRectangle
 * @See_also: #WebKitWebView.
 *
 * A WebKitRectangle is a boxed type representing a rectangle with integer coordiantes.
 *
 * Since: 2.28
 */

static WebKitRectangle* webkit_rectangle_copy(WebKitRectangle* rectangle)
{
    g_return_val_if_fail(rectangle, nullptr);

    WebKitRectangle* copy = static_cast<WebKitRectangle*>(fastZeroedMalloc(sizeof(WebKitRectangle)));
    *copy = *rectangle;
    return copy;
}

static void webkit_rectangle_free(WebKitRectangle* rectangle)
{
    g_return_if_fail(rectangle);

    fastFree(rectangle);
}

G_DEFINE_BOXED_TYPE(WebKitRectangle, webkit_rectangle, webkit_rectangle_copy, webkit_rectangle_free)
