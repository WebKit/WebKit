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

#ifndef WebKitDOMHTMLLinkElement_h
#define WebKitDOMHTMLLinkElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT            (webkit_dom_html_link_element_get_type())
#define WEBKIT_DOM_HTML_LINK_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT, WebKitDOMHTMLLinkElement))
#define WEBKIT_DOM_HTML_LINK_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT, WebKitDOMHTMLLinkElementClass)
#define WEBKIT_DOM_IS_HTML_LINK_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT))
#define WEBKIT_DOM_IS_HTML_LINK_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT))
#define WEBKIT_DOM_HTML_LINK_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT, WebKitDOMHTMLLinkElementClass))

struct _WebKitDOMHTMLLinkElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLLinkElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_link_element_get_type(void);

/**
 * webkit_dom_html_link_element_get_disabled:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_link_element_get_disabled(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_disabled:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_disabled(WebKitDOMHTMLLinkElement* self, gboolean value);

/**
 * webkit_dom_html_link_element_get_charset:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_charset(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_charset:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_charset(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_href:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_href(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_href:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_href(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_hreflang:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_hreflang(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_hreflang:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_hreflang(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_media:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_media(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_media:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_media(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_rel:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_rel(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_rel:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_rel(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_rev:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_rev(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_rev:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_rev(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_target:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_target(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_target:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_target(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_type_attr:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_link_element_get_type_attr(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_type_attr:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_type_attr(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_link_element_get_sheet:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: (transfer full): A #WebKitDOMStyleSheet
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMStyleSheet*
webkit_dom_html_link_element_get_sheet(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_get_sizes:
 * @self: A #WebKitDOMHTMLLinkElement
 *
 * Returns: (transfer full): A #WebKitDOMDOMTokenList
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMTokenList*
webkit_dom_html_link_element_get_sizes(WebKitDOMHTMLLinkElement* self);

/**
 * webkit_dom_html_link_element_set_sizes:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: a #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_link_element_set_sizes(WebKitDOMHTMLLinkElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLLinkElement_h */
