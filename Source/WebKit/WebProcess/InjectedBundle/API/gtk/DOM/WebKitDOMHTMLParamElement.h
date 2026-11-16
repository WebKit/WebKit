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

#ifndef WebKitDOMHTMLParamElement_h
#define WebKitDOMHTMLParamElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_PARAM_ELEMENT            (webkit_dom_html_param_element_get_type())
#define WEBKIT_DOM_HTML_PARAM_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_PARAM_ELEMENT, WebKitDOMHTMLParamElement))
#define WEBKIT_DOM_HTML_PARAM_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_PARAM_ELEMENT, WebKitDOMHTMLParamElementClass)
#define WEBKIT_DOM_IS_HTML_PARAM_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_PARAM_ELEMENT))
#define WEBKIT_DOM_IS_HTML_PARAM_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_PARAM_ELEMENT))
#define WEBKIT_DOM_HTML_PARAM_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_PARAM_ELEMENT, WebKitDOMHTMLParamElementClass))

struct _WebKitDOMHTMLParamElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLParamElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_param_element_get_type(void);

/**
 * webkit_dom_html_param_element_get_name:
 * @self: A #WebKitDOMHTMLParamElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_param_element_get_name(WebKitDOMHTMLParamElement* self);

/**
 * webkit_dom_html_param_element_set_name:
 * @self: A #WebKitDOMHTMLParamElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_param_element_set_name(WebKitDOMHTMLParamElement* self, const gchar* value);

/**
 * webkit_dom_html_param_element_get_type_attr:
 * @self: A #WebKitDOMHTMLParamElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_param_element_get_type_attr(WebKitDOMHTMLParamElement* self);

/**
 * webkit_dom_html_param_element_set_type_attr:
 * @self: A #WebKitDOMHTMLParamElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_param_element_set_type_attr(WebKitDOMHTMLParamElement* self, const gchar* value);

/**
 * webkit_dom_html_param_element_get_value:
 * @self: A #WebKitDOMHTMLParamElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_param_element_get_value(WebKitDOMHTMLParamElement* self);

/**
 * webkit_dom_html_param_element_set_value:
 * @self: A #WebKitDOMHTMLParamElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_param_element_set_value(WebKitDOMHTMLParamElement* self, const gchar* value);

/**
 * webkit_dom_html_param_element_get_value_type:
 * @self: A #WebKitDOMHTMLParamElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_param_element_get_value_type(WebKitDOMHTMLParamElement* self);

/**
 * webkit_dom_html_param_element_set_value_type:
 * @self: A #WebKitDOMHTMLParamElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_param_element_set_value_type(WebKitDOMHTMLParamElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLParamElement_h */
