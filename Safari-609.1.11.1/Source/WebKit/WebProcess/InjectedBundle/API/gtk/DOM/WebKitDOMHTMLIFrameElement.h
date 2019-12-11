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

#ifndef WebKitDOMHTMLIFrameElement_h
#define WebKitDOMHTMLIFrameElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT            (webkit_dom_html_iframe_element_get_type())
#define WEBKIT_DOM_HTML_IFRAME_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT, WebKitDOMHTMLIFrameElement))
#define WEBKIT_DOM_HTML_IFRAME_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT, WebKitDOMHTMLIFrameElementClass)
#define WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT))
#define WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT))
#define WEBKIT_DOM_HTML_IFRAME_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT, WebKitDOMHTMLIFrameElementClass))

struct _WebKitDOMHTMLIFrameElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLIFrameElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_iframe_element_get_type(void);

/**
 * webkit_dom_html_iframe_element_get_align:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_align(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_align:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_align(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_frame_border:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_frame_border(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_frame_border:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_frame_border(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_height:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_height(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_height:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_height(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_long_desc:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_long_desc(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_long_desc:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_long_desc(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_margin_height:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_margin_height(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_margin_height:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_margin_height(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_margin_width:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_margin_width(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_margin_width:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_margin_width(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_name:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_name(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_name:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_name(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_scrolling:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_scrolling(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_scrolling:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_scrolling(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_src:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_src(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_src:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_src(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_width:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_iframe_element_get_width(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_width:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_width(WebKitDOMHTMLIFrameElement* self, const gchar* value);

/**
 * webkit_dom_html_iframe_element_get_content_document:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: (transfer none): A #WebKitDOMDocument
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocument*
webkit_dom_html_iframe_element_get_content_document(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_get_content_window:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_html_iframe_element_get_content_window(WebKitDOMHTMLIFrameElement* self);

G_END_DECLS

#endif /* WebKitDOMHTMLIFrameElement_h */
