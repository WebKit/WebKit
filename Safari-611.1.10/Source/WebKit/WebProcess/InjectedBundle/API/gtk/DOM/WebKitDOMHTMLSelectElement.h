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

#ifndef WebKitDOMHTMLSelectElement_h
#define WebKitDOMHTMLSelectElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT            (webkit_dom_html_select_element_get_type())
#define WEBKIT_DOM_HTML_SELECT_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT, WebKitDOMHTMLSelectElement))
#define WEBKIT_DOM_HTML_SELECT_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT, WebKitDOMHTMLSelectElementClass)
#define WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT))
#define WEBKIT_DOM_IS_HTML_SELECT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT))
#define WEBKIT_DOM_HTML_SELECT_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT, WebKitDOMHTMLSelectElementClass))

struct _WebKitDOMHTMLSelectElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLSelectElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_select_element_get_type(void);

/**
 * webkit_dom_html_select_element_item:
 * @self: A #WebKitDOMHTMLSelectElement
 * @index: A #gulong
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_html_select_element_item(WebKitDOMHTMLSelectElement* self, gulong index);

/**
 * webkit_dom_html_select_element_named_item:
 * @self: A #WebKitDOMHTMLSelectElement
 * @name: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_html_select_element_named_item(WebKitDOMHTMLSelectElement* self, const gchar* name);

/**
 * webkit_dom_html_select_element_add:
 * @self: A #WebKitDOMHTMLSelectElement
 * @element: A #WebKitDOMHTMLElement
 * @before: A #WebKitDOMHTMLElement
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_add(WebKitDOMHTMLSelectElement* self, WebKitDOMHTMLElement* element, WebKitDOMHTMLElement* before, GError** error);

/**
 * webkit_dom_html_select_element_remove:
 * @self: A #WebKitDOMHTMLSelectElement
 * @index: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_remove(WebKitDOMHTMLSelectElement* self, glong index);

/**
 * webkit_dom_html_select_element_get_autofocus:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_select_element_get_autofocus(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_autofocus:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_autofocus(WebKitDOMHTMLSelectElement* self, gboolean value);

/**
 * webkit_dom_html_select_element_get_disabled:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_select_element_get_disabled(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_disabled:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_disabled(WebKitDOMHTMLSelectElement* self, gboolean value);

/**
 * webkit_dom_html_select_element_get_form:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: (transfer none): A #WebKitDOMHTMLFormElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLFormElement*
webkit_dom_html_select_element_get_form(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_get_multiple:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_select_element_get_multiple(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_multiple:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_multiple(WebKitDOMHTMLSelectElement* self, gboolean value);

/**
 * webkit_dom_html_select_element_get_name:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_select_element_get_name(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_name:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_name(WebKitDOMHTMLSelectElement* self, const gchar* value);

/**
 * webkit_dom_html_select_element_get_size:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_select_element_get_size(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_size:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_size(WebKitDOMHTMLSelectElement* self, glong value);

/**
 * webkit_dom_html_select_element_get_select_type:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_select_element_get_select_type(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_get_options:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: (transfer full): A #WebKitDOMHTMLOptionsCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLOptionsCollection*
webkit_dom_html_select_element_get_options(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_get_length:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_html_select_element_get_length(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_length:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #gulong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_length(WebKitDOMHTMLSelectElement* self, gulong value, GError** error);

/**
 * webkit_dom_html_select_element_get_selected_index:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_select_element_get_selected_index(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_selected_index:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_selected_index(WebKitDOMHTMLSelectElement* self, glong value);

/**
 * webkit_dom_html_select_element_get_value:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_select_element_get_value(WebKitDOMHTMLSelectElement* self);

/**
 * webkit_dom_html_select_element_set_value:
 * @self: A #WebKitDOMHTMLSelectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_select_element_set_value(WebKitDOMHTMLSelectElement* self, const gchar* value);

/**
 * webkit_dom_html_select_element_get_will_validate:
 * @self: A #WebKitDOMHTMLSelectElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_select_element_get_will_validate(WebKitDOMHTMLSelectElement* self);

G_END_DECLS

#endif /* WebKitDOMHTMLSelectElement_h */
