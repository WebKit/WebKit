/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebKitDOMCustom_h
#define WebKitDOMCustom_h

#include <glib-object.h>
#include <glib.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

/**
 * webkit_dom_html_text_area_element_is_edited:
 * @input: A #WebKitDOMHTMLTextAreaElement
 *
 * Returns: A #gboolean
 */
WEBKIT_API gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* input);

/**
 * webkit_dom_html_media_element_set_current_time:
 * @self: A #WebKitDOMHTMLMediaElement
 * @value: A #gdouble
 * @error: #GError
 *
 */
WEBKIT_API void webkit_dom_html_media_element_set_current_time(WebKitDOMHTMLMediaElement* self, gdouble value, GError** error);

/**
 * webkit_dom_html_input_element_is_edited:
 * @input: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 */
WEBKIT_API gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input);

G_END_DECLS

#endif
