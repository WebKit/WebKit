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
#include "WebKitInputMethodContext.h"

#include "WebKitColorPrivate.h"
#include "WebKitInputMethodContextPrivate.h"

using namespace WebCore;

/**
 * webkit_input_method_underline_set_color:
 * @underline: a #WebKitInputMethodUnderline
 * @color: (nullable): a #WebKitColor or %NULL
 *
 * Set the color of the underline. If @rgba is %NULL the foreground text color will be used
 * for the underline too.
 *
 * Since: 2.28
 */
void webkit_input_method_underline_set_color(WebKitInputMethodUnderline* underline, WebKitColor* color)
{
    g_return_if_fail(underline);

    if (!color) {
        underline->underline.compositionUnderlineColor = CompositionUnderlineColor::TextColor;
        return;
    }

    underline->underline.compositionUnderlineColor = CompositionUnderlineColor::GivenColor;
    underline->underline.color = webkitColorToWebCoreColor(color);
}

/**
 * webkit_input_method_context_filter_key_event:
 * @context: a #WebKitInputMethodContext
 * @key_event: the key event to filter
 *
 * Allow @key_event to be handled by the input method. If %TRUE is returned, then no further processing should be
 * done for the key event.
 *
 * Returns: %TRUE if the key event was handled, or %FALSE otherwise
 *
 * Since: 2.28
 */
gboolean webkit_input_method_context_filter_key_event(WebKitInputMethodContext* context, struct wpe_input_keyboard_event* keyEvent)
{
    g_return_val_if_fail(WEBKIT_IS_INPUT_METHOD_CONTEXT(context), FALSE);
    g_return_val_if_fail(keyEvent, FALSE);

    auto* imClass = WEBKIT_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    return imClass->filter_key_event ? imClass->filter_key_event(context, keyEvent) : FALSE;
}
