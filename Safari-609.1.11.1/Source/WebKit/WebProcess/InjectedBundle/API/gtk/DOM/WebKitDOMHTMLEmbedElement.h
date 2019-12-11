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

#ifndef WebKitDOMHTMLEmbedElement_h
#define WebKitDOMHTMLEmbedElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_EMBED_ELEMENT            (webkit_dom_html_embed_element_get_type())
#define WEBKIT_DOM_HTML_EMBED_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_EMBED_ELEMENT, WebKitDOMHTMLEmbedElement))
#define WEBKIT_DOM_HTML_EMBED_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_EMBED_ELEMENT, WebKitDOMHTMLEmbedElementClass)
#define WEBKIT_DOM_IS_HTML_EMBED_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_EMBED_ELEMENT))
#define WEBKIT_DOM_IS_HTML_EMBED_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_EMBED_ELEMENT))
#define WEBKIT_DOM_HTML_EMBED_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_EMBED_ELEMENT, WebKitDOMHTMLEmbedElementClass))

struct _WebKitDOMHTMLEmbedElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLEmbedElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_embed_element_get_type(void);

/**
 * webkit_dom_html_embed_element_get_align:
 * @self: A #WebKitDOMHTMLEmbedElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_embed_element_get_align(WebKitDOMHTMLEmbedElement* self);

/**
 * webkit_dom_html_embed_element_set_align:
 * @self: A #WebKitDOMHTMLEmbedElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_embed_element_set_align(WebKitDOMHTMLEmbedElement* self, const gchar* value);

/**
 * webkit_dom_html_embed_element_get_height:
 * @self: A #WebKitDOMHTMLEmbedElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_embed_element_get_height(WebKitDOMHTMLEmbedElement* self);

/**
 * webkit_dom_html_embed_element_set_height:
 * @self: A #WebKitDOMHTMLEmbedElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_embed_element_set_height(WebKitDOMHTMLEmbedElement* self, glong value);

/**
 * webkit_dom_html_embed_element_get_name:
 * @self: A #WebKitDOMHTMLEmbedElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_embed_element_get_name(WebKitDOMHTMLEmbedElement* self);

/**
 * webkit_dom_html_embed_element_set_name:
 * @self: A #WebKitDOMHTMLEmbedElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_embed_element_set_name(WebKitDOMHTMLEmbedElement* self, const gchar* value);

/**
 * webkit_dom_html_embed_element_get_src:
 * @self: A #WebKitDOMHTMLEmbedElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_embed_element_get_src(WebKitDOMHTMLEmbedElement* self);

/**
 * webkit_dom_html_embed_element_set_src:
 * @self: A #WebKitDOMHTMLEmbedElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_embed_element_set_src(WebKitDOMHTMLEmbedElement* self, const gchar* value);

/**
 * webkit_dom_html_embed_element_get_type_attr:
 * @self: A #WebKitDOMHTMLEmbedElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_embed_element_get_type_attr(WebKitDOMHTMLEmbedElement* self);

/**
 * webkit_dom_html_embed_element_set_type_attr:
 * @self: A #WebKitDOMHTMLEmbedElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_embed_element_set_type_attr(WebKitDOMHTMLEmbedElement* self, const gchar* value);

/**
 * webkit_dom_html_embed_element_get_width:
 * @self: A #WebKitDOMHTMLEmbedElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_embed_element_get_width(WebKitDOMHTMLEmbedElement* self);

/**
 * webkit_dom_html_embed_element_set_width:
 * @self: A #WebKitDOMHTMLEmbedElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_embed_element_set_width(WebKitDOMHTMLEmbedElement* self, glong value);

G_END_DECLS

#endif /* WebKitDOMHTMLEmbedElement_h */
