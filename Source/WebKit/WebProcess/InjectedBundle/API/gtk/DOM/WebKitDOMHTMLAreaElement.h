/*
 *  This file is part of the WebKit open source project.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <webkitdom/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMHTMLAreaElement_h
#define WebKitDOMHTMLAreaElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_AREA_ELEMENT            (webkit_dom_html_area_element_get_type())
#define WEBKIT_DOM_HTML_AREA_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_AREA_ELEMENT, WebKitDOMHTMLAreaElement))
#define WEBKIT_DOM_HTML_AREA_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_AREA_ELEMENT, WebKitDOMHTMLAreaElementClass)
#define WEBKIT_DOM_IS_HTML_AREA_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_AREA_ELEMENT))
#define WEBKIT_DOM_IS_HTML_AREA_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_AREA_ELEMENT))
#define WEBKIT_DOM_HTML_AREA_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_AREA_ELEMENT, WebKitDOMHTMLAreaElementClass))

struct _WebKitDOMHTMLAreaElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLAreaElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_area_element_get_type(void);

/**
 * webkit_dom_html_area_element_get_alt:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_alt(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_alt:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_alt(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_coords:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_coords(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_coords:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_coords(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_no_href:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_area_element_get_no_href(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_no_href:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_no_href(WebKitDOMHTMLAreaElement* self, gboolean value);

/**
 * webkit_dom_html_area_element_get_shape:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_shape(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_shape:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_shape(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_target:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_target(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_target:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_target(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_href:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_href(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_href:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_href(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_protocol:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_protocol(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_protocol:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_protocol(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_host:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_host(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_host:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_host(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_hostname:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_hostname(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_hostname:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_hostname(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_port:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_port(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_port:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_port(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_pathname:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_pathname(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_pathname:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_pathname(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_search:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_search(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_search:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_search(WebKitDOMHTMLAreaElement* self, const gchar* value);

/**
 * webkit_dom_html_area_element_get_hash:
 * @self: A #WebKitDOMHTMLAreaElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_area_element_get_hash(WebKitDOMHTMLAreaElement* self);

/**
 * webkit_dom_html_area_element_set_hash:
 * @self: A #WebKitDOMHTMLAreaElement
 * @value: A #gchar
 *
 * Stability: Unstable
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_area_element_set_hash(WebKitDOMHTMLAreaElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLAreaElement_h */
