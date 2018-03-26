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

#ifndef WebKitDOMHTMLBodyElement_h
#define WebKitDOMHTMLBodyElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT            (webkit_dom_html_body_element_get_type())
#define WEBKIT_DOM_HTML_BODY_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT, WebKitDOMHTMLBodyElement))
#define WEBKIT_DOM_HTML_BODY_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT, WebKitDOMHTMLBodyElementClass)
#define WEBKIT_DOM_IS_HTML_BODY_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT))
#define WEBKIT_DOM_IS_HTML_BODY_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT))
#define WEBKIT_DOM_HTML_BODY_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT, WebKitDOMHTMLBodyElementClass))

struct _WebKitDOMHTMLBodyElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLBodyElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_body_element_get_type(void);

/**
 * webkit_dom_html_body_element_get_a_link:
 * @self: A #WebKitDOMHTMLBodyElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_body_element_get_a_link(WebKitDOMHTMLBodyElement* self);

/**
 * webkit_dom_html_body_element_set_a_link:
 * @self: A #WebKitDOMHTMLBodyElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_body_element_set_a_link(WebKitDOMHTMLBodyElement* self, const gchar* value);

/**
 * webkit_dom_html_body_element_get_background:
 * @self: A #WebKitDOMHTMLBodyElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_body_element_get_background(WebKitDOMHTMLBodyElement* self);

/**
 * webkit_dom_html_body_element_set_background:
 * @self: A #WebKitDOMHTMLBodyElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_body_element_set_background(WebKitDOMHTMLBodyElement* self, const gchar* value);

/**
 * webkit_dom_html_body_element_get_bg_color:
 * @self: A #WebKitDOMHTMLBodyElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_body_element_get_bg_color(WebKitDOMHTMLBodyElement* self);

/**
 * webkit_dom_html_body_element_set_bg_color:
 * @self: A #WebKitDOMHTMLBodyElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_body_element_set_bg_color(WebKitDOMHTMLBodyElement* self, const gchar* value);

/**
 * webkit_dom_html_body_element_get_link:
 * @self: A #WebKitDOMHTMLBodyElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_body_element_get_link(WebKitDOMHTMLBodyElement* self);

/**
 * webkit_dom_html_body_element_set_link:
 * @self: A #WebKitDOMHTMLBodyElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_body_element_set_link(WebKitDOMHTMLBodyElement* self, const gchar* value);

/**
 * webkit_dom_html_body_element_get_text:
 * @self: A #WebKitDOMHTMLBodyElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_body_element_get_text(WebKitDOMHTMLBodyElement* self);

/**
 * webkit_dom_html_body_element_set_text:
 * @self: A #WebKitDOMHTMLBodyElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_body_element_set_text(WebKitDOMHTMLBodyElement* self, const gchar* value);

/**
 * webkit_dom_html_body_element_get_v_link:
 * @self: A #WebKitDOMHTMLBodyElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_body_element_get_v_link(WebKitDOMHTMLBodyElement* self);

/**
 * webkit_dom_html_body_element_set_v_link:
 * @self: A #WebKitDOMHTMLBodyElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_body_element_set_v_link(WebKitDOMHTMLBodyElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLBodyElement_h */
