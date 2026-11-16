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

#ifndef WebKitDOMHTMLAnchorElement_h
#define WebKitDOMHTMLAnchorElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT            (webkit_dom_html_anchor_element_get_type())
#define WEBKIT_DOM_HTML_ANCHOR_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT, WebKitDOMHTMLAnchorElement))
#define WEBKIT_DOM_HTML_ANCHOR_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT, WebKitDOMHTMLAnchorElementClass)
#define WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT))
#define WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT))
#define WEBKIT_DOM_HTML_ANCHOR_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT, WebKitDOMHTMLAnchorElementClass))

struct _WebKitDOMHTMLAnchorElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLAnchorElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_anchor_element_get_type(void);

/**
 * webkit_dom_html_anchor_element_get_charset:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_charset(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_charset:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_charset(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_coords:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_coords(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_coords:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_coords(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_hreflang:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_hreflang(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_hreflang:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_hreflang(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_name:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_name(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_name:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_name(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_rel:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_rel(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_rel:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_rel(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_rev:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_rev(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_rev:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_rev(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_shape:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_shape(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_shape:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_shape(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_target:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_target(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_target:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_target(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_type_attr:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_type_attr(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_type_attr:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_type_attr(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_text:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_text(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_text:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_text(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_href:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_href(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_href:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_href(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_protocol:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_protocol(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_protocol:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_protocol(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_host:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_host(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_host:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_host(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_hostname:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_hostname(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_hostname:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_hostname(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_port:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_port(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_port:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_port(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_pathname:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_pathname(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_pathname:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_pathname(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_search:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_search(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_search:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_search(WebKitDOMHTMLAnchorElement* self, const gchar* value);

/**
 * webkit_dom_html_anchor_element_get_hash:
 * @self: A #WebKitDOMHTMLAnchorElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_anchor_element_get_hash(WebKitDOMHTMLAnchorElement* self);

/**
 * webkit_dom_html_anchor_element_set_hash:
 * @self: A #WebKitDOMHTMLAnchorElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_anchor_element_set_hash(WebKitDOMHTMLAnchorElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLAnchorElement_h */
