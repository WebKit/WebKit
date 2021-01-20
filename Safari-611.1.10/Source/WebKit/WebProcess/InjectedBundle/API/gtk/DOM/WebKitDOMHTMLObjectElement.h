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

#ifndef WebKitDOMHTMLObjectElement_h
#define WebKitDOMHTMLObjectElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_OBJECT_ELEMENT            (webkit_dom_html_object_element_get_type())
#define WEBKIT_DOM_HTML_OBJECT_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_OBJECT_ELEMENT, WebKitDOMHTMLObjectElement))
#define WEBKIT_DOM_HTML_OBJECT_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_OBJECT_ELEMENT, WebKitDOMHTMLObjectElementClass)
#define WEBKIT_DOM_IS_HTML_OBJECT_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_OBJECT_ELEMENT))
#define WEBKIT_DOM_IS_HTML_OBJECT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_OBJECT_ELEMENT))
#define WEBKIT_DOM_HTML_OBJECT_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_OBJECT_ELEMENT, WebKitDOMHTMLObjectElementClass))

struct _WebKitDOMHTMLObjectElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLObjectElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_object_element_get_type(void);

/**
 * webkit_dom_html_object_element_get_form:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: (transfer none): A #WebKitDOMHTMLFormElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLFormElement*
webkit_dom_html_object_element_get_form(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_get_code:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_code(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_code:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_code(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_align:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_align(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_align:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_align(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_archive:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_archive(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_archive:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_archive(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_border:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_border(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_border:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_border(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_code_base:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_code_base(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_code_base:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_code_base(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_code_type:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_code_type(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_code_type:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_code_type(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_data:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_data(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_data:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_data(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_declare:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_object_element_get_declare(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_declare:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_declare(WebKitDOMHTMLObjectElement* self, gboolean value);

/**
 * webkit_dom_html_object_element_get_height:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_height(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_height:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_height(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_hspace:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_object_element_get_hspace(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_hspace:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_hspace(WebKitDOMHTMLObjectElement* self, glong value);

/**
 * webkit_dom_html_object_element_get_name:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_name(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_name:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_name(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_standby:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_standby(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_standby:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_standby(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_type_attr:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_type_attr(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_type_attr:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_type_attr(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_use_map:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_use_map(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_use_map:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_use_map(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_vspace:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_object_element_get_vspace(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_vspace:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_vspace(WebKitDOMHTMLObjectElement* self, glong value);

/**
 * webkit_dom_html_object_element_get_width:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_object_element_get_width(WebKitDOMHTMLObjectElement* self);

/**
 * webkit_dom_html_object_element_set_width:
 * @self: A #WebKitDOMHTMLObjectElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_object_element_set_width(WebKitDOMHTMLObjectElement* self, const gchar* value);

/**
 * webkit_dom_html_object_element_get_content_document:
 * @self: A #WebKitDOMHTMLObjectElement
 *
 * Returns: (transfer none): A #WebKitDOMDocument
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocument*
webkit_dom_html_object_element_get_content_document(WebKitDOMHTMLObjectElement* self);

G_END_DECLS

#endif /* WebKitDOMHTMLObjectElement_h */
