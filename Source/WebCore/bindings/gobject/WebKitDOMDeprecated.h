/*
 *  Copyright (C) 2014 Igalia S.L.
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

#ifndef WebKitDOMDeprecated_h
#define WebKitDOMDeprecated_h

#if !defined(WEBKIT_DISABLE_DEPRECATED)

#include <glib.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

/**
 * webkit_dom_html_element_get_inner_html:
 * @self: a #WebKitDOMHTMLElement
 *
 * Returns: a #gchar
 *
 * Deprecated: 2.8: Use webkit_dom_element_get_inner_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_inner_html) gchar*
webkit_dom_html_element_get_inner_html(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_inner_html:
 * @self: a #WebKitDOMHTMLElement
 * @contents: a #gchar with contents to set
 * @error: a #GError or %NULL
 *
 * Deprecated: 2.8: Use webkit_dom_element_set_inner_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_set_inner_html) void
webkit_dom_html_element_set_inner_html(WebKitDOMHTMLElement* self, const gchar* contents, GError** error);

/**
 * webkit_dom_html_element_get_outer_html:
 * @self: a #WebKitDOMHTMLElement
 *
 * Returns: a #gchar
 *
 * Deprecated: 2.8: Use webkit_dom_element_get_outer_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_outer_html) gchar*
webkit_dom_html_element_get_outer_html(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_outer_html:
 * @self: a #WebKitDOMHTMLElement
 * @contents: a #gchar with contents to set
 * @error: a #GError or %NULL
 *
 * Deprecated: 2.8: Use webkit_dom_element_set_outer_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_set_outer_html) void
webkit_dom_html_element_set_outer_html(WebKitDOMHTMLElement* self, const gchar* contents, GError** error);

/**
 * webkit_dom_html_element_get_children:
 * @self: A #WebKitDOMHTMLElement
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.10: Use webkit_dom_element_get_children() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_children) WebKitDOMHTMLCollection*
webkit_dom_html_element_get_children(WebKitDOMHTMLElement* self);

G_END_DECLS

#endif /* WEBKIT_DISABLE_DEPRECATED */

#endif
