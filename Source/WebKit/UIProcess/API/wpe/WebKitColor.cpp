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

#include "config.h"
#include "WebKitColor.h"

#include "WebKitColorPrivate.h"

/**
 * SECTION: WebKitColor
 * @Short_description: A boxed type representing a RGBA color
 * @Title: WebKitColor
 * @See_also: #WebKitWebView.
 *
 * A WebKitColor is a boxed type representing a RGBA color.
 *
 * Since: 2.24
 */

/**
 * webkit_color_copy:
 * @color: a #WebKitColor
 *
 * Make a copy of @color.
 *
 * Returns: (transfer full): A copy of passed in #WebKitColor.
 *
 * Since: 2.24
 */
WebKitColor* webkit_color_copy(WebKitColor* color)
{
    g_return_val_if_fail(color, nullptr);

    WebKitColor* copy = static_cast<WebKitColor*>(fastZeroedMalloc(sizeof(WebKitColor)));
    copy->red = color->red;
    copy->green = color->green;
    copy->blue = color->blue;
    copy->alpha = color->alpha;
    return copy;
}

/**
 * webkit_color_free:
 * @color: a #WebKitColor
 *
 * Free the #WebKitColor.
 *
 * Since: 2.24
 */
void webkit_color_free(WebKitColor* color)
{
    g_return_if_fail(color);

    fastFree(color);
}

G_DEFINE_BOXED_TYPE(WebKitColor, webkit_color, webkit_color_copy, webkit_color_free);

const WebCore::Color webkitColorToWebCoreColor(WebKitColor* color)
{
    return WebCore::Color(static_cast<float>(color->red), static_cast<float>(color->green),
        static_cast<float>(color->blue), static_cast<float>(color->alpha));
}

void webkitColorFillFromWebCoreColor(const WebCore::Color& webCoreColor, WebKitColor* color)
{
    RELEASE_ASSERT(webCoreColor.isValid());

    double r, g, b, a;
    webCoreColor.getRGBA(r, g, b, a);
    color->red = r;
    color->green = g;
    color->blue = b;
    color->alpha = a;
}

/**
 * webkit_color_parse:
 * @color: a #WebKitColor to fill in
 * @color_string: color representation as color nickname or HEX string
 *
 * Create a new #WebKitColor for the given @color_string
 * representation. There are two valid representation types: standard color
 * names (see https://htmlcolorcodes.com/color-names/ for instance) or HEX
 * values.
 *
 * Returns: a #gboolean indicating if the @color was correctly filled in or not.
 *
 * Since: 2.24
 */
gboolean webkit_color_parse(WebKitColor* color, const gchar* colorString)
{
    g_return_val_if_fail(color, FALSE);
    g_return_val_if_fail(colorString, FALSE);

    auto webCoreColor = WebCore::Color(colorString);
    if (!webCoreColor.isValid())
        return FALSE;

    webkitColorFillFromWebCoreColor(webCoreColor, color);
    return TRUE;
}
