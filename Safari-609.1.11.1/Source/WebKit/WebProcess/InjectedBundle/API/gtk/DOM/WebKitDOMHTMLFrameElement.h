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

#ifndef WebKitDOMHTMLFrameElement_h
#define WebKitDOMHTMLFrameElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_FRAME_ELEMENT            (webkit_dom_html_frame_element_get_type())
#define WEBKIT_DOM_HTML_FRAME_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_FRAME_ELEMENT, WebKitDOMHTMLFrameElement))
#define WEBKIT_DOM_HTML_FRAME_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_FRAME_ELEMENT, WebKitDOMHTMLFrameElementClass)
#define WEBKIT_DOM_IS_HTML_FRAME_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_FRAME_ELEMENT))
#define WEBKIT_DOM_IS_HTML_FRAME_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_FRAME_ELEMENT))
#define WEBKIT_DOM_HTML_FRAME_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_FRAME_ELEMENT, WebKitDOMHTMLFrameElementClass))

struct _WebKitDOMHTMLFrameElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLFrameElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_frame_element_get_type(void);

/**
 * webkit_dom_html_frame_element_get_frame_border:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_frame_border(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_frame_border:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_frame_border(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_long_desc:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_long_desc(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_long_desc:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_long_desc(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_margin_height:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_margin_height(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_margin_height:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_margin_height(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_margin_width:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_margin_width(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_margin_width:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_margin_width(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_name:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_name(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_name:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_name(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_no_resize:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_frame_element_get_no_resize(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_no_resize:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_no_resize(WebKitDOMHTMLFrameElement* self, gboolean value);

/**
 * webkit_dom_html_frame_element_get_scrolling:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_scrolling(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_scrolling:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_scrolling(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_src:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_frame_element_get_src(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_set_src:
 * @self: A #WebKitDOMHTMLFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_frame_element_set_src(WebKitDOMHTMLFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_frame_element_get_content_document:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: (transfer none): A #WebKitDOMDocument
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocument*
webkit_dom_html_frame_element_get_content_document(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_get_content_window:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_html_frame_element_get_content_window(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_get_width:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_frame_element_get_width(WebKitDOMHTMLFrameElement* self);

/**
 * webkit_dom_html_frame_element_get_height:
 * @self: A #WebKitDOMHTMLFrameElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_frame_element_get_height(WebKitDOMHTMLFrameElement* self);

G_END_DECLS

#endif /* WebKitDOMHTMLFrameElement_h */
