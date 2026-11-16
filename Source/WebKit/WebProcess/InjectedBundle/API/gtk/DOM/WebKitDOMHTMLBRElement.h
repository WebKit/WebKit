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

#ifndef WebKitDOMHTMLBRElement_h
#define WebKitDOMHTMLBRElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_BR_ELEMENT            (webkit_dom_html_br_element_get_type())
#define WEBKIT_DOM_HTML_BR_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_BR_ELEMENT, WebKitDOMHTMLBRElement))
#define WEBKIT_DOM_HTML_BR_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_BR_ELEMENT, WebKitDOMHTMLBRElementClass)
#define WEBKIT_DOM_IS_HTML_BR_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_BR_ELEMENT))
#define WEBKIT_DOM_IS_HTML_BR_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_BR_ELEMENT))
#define WEBKIT_DOM_HTML_BR_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_BR_ELEMENT, WebKitDOMHTMLBRElementClass))

struct _WebKitDOMHTMLBRElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLBRElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_br_element_get_type(void);

/**
 * webkit_dom_html_br_element_get_clear:
 * @self: A #WebKitDOMHTMLBRElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_br_element_get_clear(WebKitDOMHTMLBRElement* self);

/**
 * webkit_dom_html_br_element_set_clear:
 * @self: A #WebKitDOMHTMLBRElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_br_element_set_clear(WebKitDOMHTMLBRElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLBRElement_h */
