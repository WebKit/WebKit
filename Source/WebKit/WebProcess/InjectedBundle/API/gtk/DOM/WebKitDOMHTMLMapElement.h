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

#ifndef WebKitDOMHTMLMapElement_h
#define WebKitDOMHTMLMapElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_MAP_ELEMENT            (webkit_dom_html_map_element_get_type())
#define WEBKIT_DOM_HTML_MAP_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_MAP_ELEMENT, WebKitDOMHTMLMapElement))
#define WEBKIT_DOM_HTML_MAP_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_MAP_ELEMENT, WebKitDOMHTMLMapElementClass)
#define WEBKIT_DOM_IS_HTML_MAP_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_MAP_ELEMENT))
#define WEBKIT_DOM_IS_HTML_MAP_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_MAP_ELEMENT))
#define WEBKIT_DOM_HTML_MAP_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_MAP_ELEMENT, WebKitDOMHTMLMapElementClass))

struct _WebKitDOMHTMLMapElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLMapElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_map_element_get_type(void);

/**
 * webkit_dom_html_map_element_get_areas:
 * @self: A #WebKitDOMHTMLMapElement
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_html_map_element_get_areas(WebKitDOMHTMLMapElement* self);

/**
 * webkit_dom_html_map_element_get_name:
 * @self: A #WebKitDOMHTMLMapElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_map_element_get_name(WebKitDOMHTMLMapElement* self);

/**
 * webkit_dom_html_map_element_set_name:
 * @self: A #WebKitDOMHTMLMapElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_map_element_set_name(WebKitDOMHTMLMapElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLMapElement_h */
