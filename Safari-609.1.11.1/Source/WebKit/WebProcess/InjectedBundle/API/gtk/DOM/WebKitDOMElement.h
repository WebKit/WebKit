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

#ifndef WebKitDOMElement_h
#define WebKitDOMElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMNode.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_ELEMENT            (webkit_dom_element_get_type())
#define WEBKIT_DOM_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_ELEMENT, WebKitDOMElement))
#define WEBKIT_DOM_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_ELEMENT, WebKitDOMElementClass)
#define WEBKIT_DOM_IS_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_ELEMENT))
#define WEBKIT_DOM_IS_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_ELEMENT))
#define WEBKIT_DOM_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_ELEMENT, WebKitDOMElementClass))

struct _WebKitDOMElement {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMElementClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_API GType
webkit_dom_element_get_type                             (void);

WEBKIT_API gboolean
webkit_dom_element_html_input_element_is_user_edited    (WebKitDOMElement *element);

WEBKIT_API gboolean
webkit_dom_element_html_input_element_get_auto_filled   (WebKitDOMElement *element);

WEBKIT_API void
webkit_dom_element_html_input_element_set_auto_filled   (WebKitDOMElement *element,
                                                         gboolean          auto_filled);
WEBKIT_API void
webkit_dom_element_html_input_element_set_editing_value (WebKitDOMElement *element,
                                                         const char       *value);

#ifndef WEBKIT_DISABLE_DEPRECATED

/**
 * WEBKIT_DOM_ELEMENT_ALLOW_KEYBOARD_INPUT:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_ELEMENT_ALLOW_KEYBOARD_INPUT 1

#endif

/**
 * webkit_dom_element_get_attribute:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_attribute(WebKitDOMElement* self, const gchar* name);

/**
 * webkit_dom_element_set_attribute:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_attribute(WebKitDOMElement* self, const gchar* name, const gchar* value, GError** error);

/**
 * webkit_dom_element_remove_attribute:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_remove_attribute(WebKitDOMElement* self, const gchar* name);

/**
 * webkit_dom_element_get_attribute_node:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_element_get_attribute_node(WebKitDOMElement* self, const gchar* name);

/**
 * webkit_dom_element_set_attribute_node:
 * @self: A #WebKitDOMElement
 * @newAttr: A #WebKitDOMAttr
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_element_set_attribute_node(WebKitDOMElement* self, WebKitDOMAttr* newAttr, GError** error);

/**
 * webkit_dom_element_remove_attribute_node:
 * @self: A #WebKitDOMElement
 * @oldAttr: A #WebKitDOMAttr
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_element_remove_attribute_node(WebKitDOMElement* self, WebKitDOMAttr* oldAttr, GError** error);

/**
 * webkit_dom_element_get_elements_by_tag_name_as_html_collection:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.12
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_element_get_elements_by_tag_name_as_html_collection(WebKitDOMElement* self, const gchar* name);

/**
 * webkit_dom_element_has_attributes:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_element_has_attributes(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_attribute_ns:
 * @self: A #WebKitDOMElement
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_element_set_attribute_ns:
 * @self: A #WebKitDOMElement
 * @namespaceURI: (allow-none): A #gchar
 * @qualifiedName: A #gchar
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* qualifiedName, const gchar* value, GError** error);

/**
 * webkit_dom_element_remove_attribute_ns:
 * @self: A #WebKitDOMElement
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_remove_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_element_get_elements_by_tag_name_ns_as_html_collection:
 * @self: A #WebKitDOMElement
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.12
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_element_get_elements_by_tag_name_ns_as_html_collection(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_element_get_attribute_node_ns:
 * @self: A #WebKitDOMElement
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_element_get_attribute_node_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_element_set_attribute_node_ns:
 * @self: A #WebKitDOMElement
 * @newAttr: A #WebKitDOMAttr
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_element_set_attribute_node_ns(WebKitDOMElement* self, WebKitDOMAttr* newAttr, GError** error);

/**
 * webkit_dom_element_has_attribute:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_element_has_attribute(WebKitDOMElement* self, const gchar* name);

/**
 * webkit_dom_element_has_attribute_ns:
 * @self: A #WebKitDOMElement
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_element_has_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_element_focus:
 * @self: A #WebKitDOMElement
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_focus(WebKitDOMElement* self);

/**
 * webkit_dom_element_blur:
 * @self: A #WebKitDOMElement
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_blur(WebKitDOMElement* self);

/**
 * webkit_dom_element_scroll_into_view:
 * @self: A #WebKitDOMElement
 * @alignWithTop: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_scroll_into_view(WebKitDOMElement* self, gboolean alignWithTop);

/**
 * webkit_dom_element_scroll_into_view_if_needed:
 * @self: A #WebKitDOMElement
 * @centerIfNeeded: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_scroll_into_view_if_needed(WebKitDOMElement* self, gboolean centerIfNeeded);

/**
 * webkit_dom_element_scroll_by_lines:
 * @self: A #WebKitDOMElement
 * @lines: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_scroll_by_lines(WebKitDOMElement* self, glong lines);

/**
 * webkit_dom_element_scroll_by_pages:
 * @self: A #WebKitDOMElement
 * @pages: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_scroll_by_pages(WebKitDOMElement* self, glong pages);

/**
 * webkit_dom_element_get_elements_by_class_name_as_html_collection:
 * @self: A #WebKitDOMElement
 * @name: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.12
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_element_get_elements_by_class_name_as_html_collection(WebKitDOMElement* self, const gchar* name);

/**
 * webkit_dom_element_query_selector:
 * @self: A #WebKitDOMElement
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_query_selector(WebKitDOMElement* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_element_query_selector_all:
 * @self: A #WebKitDOMElement
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMNodeList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_element_query_selector_all(WebKitDOMElement* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_element_get_tag_name:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_tag_name(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_attributes:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer full): A #WebKitDOMNamedNodeMap
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNamedNodeMap*
webkit_dom_element_get_attributes(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_style:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer full): A #WebKitDOMCSSStyleDeclaration
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSStyleDeclaration*
webkit_dom_element_get_style(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_id:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_id(WebKitDOMElement* self);

/**
 * webkit_dom_element_set_id:
 * @self: A #WebKitDOMElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_id(WebKitDOMElement* self, const gchar* value);

/**
 * webkit_dom_element_get_namespace_uri:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_namespace_uri(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_prefix:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_prefix(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_local_name:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_local_name(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_offset_left:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_offset_left(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_offset_top:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_offset_top(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_offset_width:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_offset_width(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_offset_height:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_offset_height(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_client_left:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_client_left(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_client_top:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_client_top(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_client_width:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_client_width(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_client_height:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gdouble
webkit_dom_element_get_client_height(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_scroll_left:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_element_get_scroll_left(WebKitDOMElement* self);

/**
 * webkit_dom_element_set_scroll_left:
 * @self: A #WebKitDOMElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_scroll_left(WebKitDOMElement* self, glong value);

/**
 * webkit_dom_element_get_scroll_top:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_element_get_scroll_top(WebKitDOMElement* self);

/**
 * webkit_dom_element_set_scroll_top:
 * @self: A #WebKitDOMElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_scroll_top(WebKitDOMElement* self, glong value);

/**
 * webkit_dom_element_get_scroll_width:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_element_get_scroll_width(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_scroll_height:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_element_get_scroll_height(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_bounding_client_rect:
 * @self: A #WebKitDOMElement
 *
 * Returns a #WebKitDOMClientRect representing the size and position of @self
 * relative to the viewport.
 *
 * Returns: (transfer full): A #WebKitDOMClientRect
 *
 * Since: 2.18
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMClientRect*
webkit_dom_element_get_bounding_client_rect(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_client_rects:
 * @self: A #WebKitDOMElement
 *
 * Returns a collection of #WebKitDOMClientRect objects, each of which describe
 * the size and position of a CSS border box relative to the viewport.
 *
 * Returns: (transfer full): A #WebKitDOMClientRectList
 *
 * Since: 2.18
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMClientRectList*
webkit_dom_element_get_client_rects(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_offset_parent:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_get_offset_parent(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_inner_html:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Since: 2.8
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_inner_html(WebKitDOMElement* self);

/**
 * webkit_dom_element_set_inner_html:
 * @self: A #WebKitDOMElement
 * @value: A #gchar
 * @error: #GError
 *
 * Since: 2.8
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_inner_html(WebKitDOMElement* self, const gchar* value, GError** error);

/**
 * webkit_dom_element_get_outer_html:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Since: 2.8
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_outer_html(WebKitDOMElement* self);

/**
 * webkit_dom_element_set_outer_html:
 * @self: A #WebKitDOMElement
 * @value: A #gchar
 * @error: #GError
 *
 * Since: 2.8
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_outer_html(WebKitDOMElement* self, const gchar* value, GError** error);

/**
 * webkit_dom_element_get_class_name:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_class_name(WebKitDOMElement* self);

/**
 * webkit_dom_element_set_class_name:
 * @self: A #WebKitDOMElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_set_class_name(WebKitDOMElement* self, const gchar* value);

/**
 * webkit_dom_element_get_previous_element_sibling:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_get_previous_element_sibling(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_next_element_sibling:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_get_next_element_sibling(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_children:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.10
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_element_get_children(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_first_element_child:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_get_first_element_child(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_last_element_child:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_get_last_element_child(WebKitDOMElement* self);

/**
 * webkit_dom_element_get_child_element_count:
 * @self: A #WebKitDOMElement
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_element_get_child_element_count(WebKitDOMElement* self);

/**
 * webkit_dom_element_matches:
 * @self: A #WebKitDOMElement
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_element_matches(WebKitDOMElement* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_element_closest:
 * @self: A #WebKitDOMElement
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_closest(WebKitDOMElement* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_element_webkit_matches_selector:
 * @self: A #WebKitDOMElement
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_element_webkit_matches_selector(WebKitDOMElement* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_element_webkit_request_fullscreen:
 * @self: A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_webkit_request_fullscreen(WebKitDOMElement* self);

/**
 * webkit_dom_element_insert_adjacent_element:
 * @self: A #WebKitDOMElement
 * @where: A #gchar
 * @element: A #WebKitDOMElement
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_element_insert_adjacent_element(WebKitDOMElement* self, const gchar* where, WebKitDOMElement* element, GError** error);

/**
 * webkit_dom_element_insert_adjacent_html:
 * @self: A #WebKitDOMElement
 * @where: A #gchar
 * @html: A #gchar
 * @error: #GError
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_insert_adjacent_html(WebKitDOMElement* self, const gchar* where, const gchar* html, GError** error);

/**
 * webkit_dom_element_insert_adjacent_text:
 * @self: A #WebKitDOMElement
 * @where: A #gchar
 * @text: A #gchar
 * @error: #GError
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_insert_adjacent_text(WebKitDOMElement* self, const gchar* where, const gchar* text, GError** error);

/**
 * webkit_dom_element_request_pointer_lock:
 * @self: A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_request_pointer_lock(WebKitDOMElement* self);

/**
 * webkit_dom_element_remove:
 * @self: A #WebKitDOMElement
 * @error: #GError
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_element_remove(WebKitDOMElement* self, GError** error);

/**
 * webkit_dom_element_get_class_list:
 * @self: A #WebKitDOMElement
 *
 * Returns: (transfer full): A #WebKitDOMDOMTokenList
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMTokenList*
webkit_dom_element_get_class_list(WebKitDOMElement* self);

G_END_DECLS

#endif /* WebKitDOMElement_h */
