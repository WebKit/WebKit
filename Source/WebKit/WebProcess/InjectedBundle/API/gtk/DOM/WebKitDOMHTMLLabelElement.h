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

#ifndef WebKitDOMHTMLLabelElement_h
#define WebKitDOMHTMLLabelElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_LABEL_ELEMENT            (webkit_dom_html_label_element_get_type())
#define WEBKIT_DOM_HTML_LABEL_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_LABEL_ELEMENT, WebKitDOMHTMLLabelElement))
#define WEBKIT_DOM_HTML_LABEL_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_LABEL_ELEMENT, WebKitDOMHTMLLabelElementClass)
#define WEBKIT_DOM_IS_HTML_LABEL_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_LABEL_ELEMENT))
#define WEBKIT_DOM_IS_HTML_LABEL_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_LABEL_ELEMENT))
#define WEBKIT_DOM_HTML_LABEL_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_LABEL_ELEMENT, WebKitDOMHTMLLabelElementClass))

struct _WebKitDOMHTMLLabelElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLLabelElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_label_element_get_type(void);

/**
 * webkit_dom_html_label_element_get_form:
 * @self: A #WebKitDOMHTMLLabelElement
 *
 * Returns: (transfer none): A #WebKitDOMHTMLFormElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLFormElement*
webkit_dom_html_label_element_get_form(WebKitDOMHTMLLabelElement* self);

/**
 * webkit_dom_html_label_element_get_html_for:
 * @self: A #WebKitDOMHTMLLabelElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_label_element_get_html_for(WebKitDOMHTMLLabelElement* self);

/**
 * webkit_dom_html_label_element_set_html_for:
 * @self: A #WebKitDOMHTMLLabelElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_label_element_set_html_for(WebKitDOMHTMLLabelElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLLabelElement_h */
