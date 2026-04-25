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

#ifndef WebKitDOMHTMLImageElement_h
#define WebKitDOMHTMLImageElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT            (webkit_dom_html_image_element_get_type())
#define WEBKIT_DOM_HTML_IMAGE_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT, WebKitDOMHTMLImageElement))
#define WEBKIT_DOM_HTML_IMAGE_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT, WebKitDOMHTMLImageElementClass)
#define WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT))
#define WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT))
#define WEBKIT_DOM_HTML_IMAGE_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT, WebKitDOMHTMLImageElementClass))

struct _WebKitDOMHTMLImageElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLImageElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_image_element_get_type(void);

/**
 * webkit_dom_html_image_element_get_name:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_name(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_name:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_name(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_align:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_align(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_align:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_align(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_alt:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_alt(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_alt:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_alt(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_border:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_border(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_border:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_border(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_height:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_height(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_height:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_height(WebKitDOMHTMLImageElement* self, glong value);

/**
 * webkit_dom_html_image_element_get_hspace:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_hspace(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_hspace:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_hspace(WebKitDOMHTMLImageElement* self, glong value);

/**
 * webkit_dom_html_image_element_get_is_map:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_image_element_get_is_map(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_is_map:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_is_map(WebKitDOMHTMLImageElement* self, gboolean value);

/**
 * webkit_dom_html_image_element_get_long_desc:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_long_desc(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_long_desc:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_long_desc(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_src:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_src(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_src:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_src(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_use_map:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_use_map(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_use_map:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_use_map(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_vspace:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_vspace(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_vspace:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_vspace(WebKitDOMHTMLImageElement* self, glong value);

/**
 * webkit_dom_html_image_element_get_width:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_width(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_width:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_width(WebKitDOMHTMLImageElement* self, glong value);

/**
 * webkit_dom_html_image_element_get_complete:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_image_element_get_complete(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_get_lowsrc:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_image_element_get_lowsrc(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_set_lowsrc:
 * @self: A #WebKitDOMHTMLImageElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_image_element_set_lowsrc(WebKitDOMHTMLImageElement* self, const gchar* value);

/**
 * webkit_dom_html_image_element_get_natural_height:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_natural_height(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_get_natural_width:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_natural_width(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_get_x:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_x(WebKitDOMHTMLImageElement* self);

/**
 * webkit_dom_html_image_element_get_y:
 * @self: A #WebKitDOMHTMLImageElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_image_element_get_y(WebKitDOMHTMLImageElement* self);

G_END_DECLS

#endif /* WebKitDOMHTMLImageElement_h */
