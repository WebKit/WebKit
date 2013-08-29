/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebKitDOMCustom_h
#define WebKitDOMCustom_h

#include <glib-object.h>
#include <glib.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

/**
 * webkit_dom_html_text_area_element_is_edited:
 * @input: A #WebKitDOMHTMLTextAreaElement
 *
 */
WEBKIT_API gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* input);

/**
 * webkit_dom_html_input_element_is_edited:
 * @input: A #WebKitDOMHTMLInputElement
 *
 */
WEBKIT_API gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input);

/**
 * webkit_dom_blob_webkit_slice:
 * @self: A #WebKitDOMBlob
 * @start: A #gint64
 * @end: A #gint64
 * @content_type: A #gchar
 *
 * Returns: (transfer none):
 */
WEBKIT_API WebKitDOMBlob* webkit_dom_blob_webkit_slice(WebKitDOMBlob* self, gint64 start, gint64 end, const gchar* content_type);

/**
 * webkit_dom_html_element_get_class_name:
 * @element: A #WebKitDOMHTMLElement
 *
 * Returns:
 *
 */
WEBKIT_API gchar* webkit_dom_html_element_get_class_name(WebKitDOMHTMLElement* element);

/**
 * webkit_dom_html_element_set_class_name:
 * @element: A #WebKitDOMHTMLElement
 * @value: A #gchar
 *
 */
WEBKIT_API void webkit_dom_html_element_set_class_name(WebKitDOMHTMLElement* element, const gchar* value);

/**
 * webkit_dom_html_element_get_class_list:
 * @element: A #WebKitDOMHTMLElement
 *
 * Returns: (transfer none):
 *
 */
WEBKIT_API WebKitDOMDOMTokenList* webkit_dom_html_element_get_class_list(WebKitDOMHTMLElement* element);

/**
 * webkit_dom_html_form_element_dispatch_form_change:
 * @self: A #WebKitDOMHTMLFormElement
 *
 */
WEBKIT_API void webkit_dom_html_form_element_dispatch_form_change(WebKitDOMHTMLFormElement* self);

/**
 * webkit_dom_html_form_element_dispatch_form_input:
 * @self: A #WebKitDOMHTMLFormElement
 *
 */
WEBKIT_API void webkit_dom_html_form_element_dispatch_form_input(WebKitDOMHTMLFormElement* self);

/**
 * webkit_dom_html_element_get_id:
 * @self: A #WebKitDOMHTMLElement
 *
 * This method is deprecated. Use webkit_dom_element_set_id() instead.
 *
 * Returns:
 *
**/
WEBKIT_API gchar* webkit_dom_html_element_get_id(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_id:
 * @self: A #WebKitDOMHTMLElement
 * @value: A #gchar
 *
 * This method is deprecated. Use webkit_dom_element_set_id() instead.
 *
 * Returns:
 *
**/
WEBKIT_API void webkit_dom_html_element_set_id(WebKitDOMHTMLElement* self, const gchar* value);

/**
 * webkit_dom_webkit_named_flow_get_overflow:
 * @flow: A #WebKitDOMWebKitNamedFlow
 *
 * Returns:
 *
 */
WEBKIT_API gboolean webkit_dom_webkit_named_flow_get_overflow(WebKitDOMWebKitNamedFlow* flow);

/**
 * webkit_dom_element_get_webkit_region_overflow:
 * @element: A #WebKitDOMElement
 *
 * Returns:
 *
 */
WEBKIT_API gchar* webkit_dom_element_get_webkit_region_overflow(WebKitDOMElement* element);

/**
 * webkit_dom_webkit_named_flow_get_content_nodes:
 * @flow: A #WebKitDOMWebKitNamedFlow
 *
 * Returns: (transfer none):
 *
 */
WEBKIT_API WebKitDOMNodeList* webkit_dom_webkit_named_flow_get_content_nodes(WebKitDOMWebKitNamedFlow* flow);

/**
 * webkit_dom_webkit_named_flow_get_regions_by_content_node:
 * @flow: A #WebKitDOMWebKitNamedFlow
 * @content_node: A #WebKitDOMNode
 *
 * Returns: (transfer none):
 *
 */
WEBKIT_API WebKitDOMNodeList* webkit_dom_webkit_named_flow_get_regions_by_content_node(WebKitDOMWebKitNamedFlow* flow, WebKitDOMNode* content_node);

WEBKIT_API GType webkit_dom_bar_info_get_type(void);

/**
 * webkit_dom_bar_info_get_visible:
 * @self: A #WebKitDOMBarInfo
 *
 * The BarInfo type has been removed from the DOM spec, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gboolean webkit_dom_bar_info_get_visible(void* self);

/**
 * webkit_dom_console_get_memory:
 * @self: A #WebKitDOMConsole
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API void* webkit_dom_console_get_memory(WebKitDOMConsole* self);

/**
 * webkit_dom_css_style_declaration_get_property_css_value:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMCSSValue* webkit_dom_css_style_declaration_get_property_css_value(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName);

/**
 * webkit_dom_document_get_webkit_hidden:
 * @self: A #WebKitDOMDocument
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gboolean webkit_dom_document_get_webkit_hidden(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_webkit_visibility_state:
 * @self: A #WebKitDOMDocument
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gchar* webkit_dom_document_get_webkit_visibility_state(WebKitDOMDocument* self);

/**
 * webkit_dom_html_document_open:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns:
 *
**/
WEBKIT_API void webkit_dom_html_document_open(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_element_set_item_id:
 * @self: A #WebKitDOMHTMLElement
 * @value: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API void webkit_dom_html_element_set_item_id(WebKitDOMHTMLElement* self, const gchar* value);

/**
 * webkit_dom_html_element_get_item_id:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gchar* webkit_dom_html_element_get_item_id(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_get_item_ref:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMDOMSettableTokenList* webkit_dom_html_element_get_item_ref(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_get_item_prop:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMDOMSettableTokenList* webkit_dom_html_element_get_item_prop(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_item_scope:
 * @self: A #WebKitDOMHTMLElement
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API void webkit_dom_html_element_set_item_scope(WebKitDOMHTMLElement* self, gboolean value);

/**
 * webkit_dom_html_element_get_item_scope:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gboolean webkit_dom_html_element_get_item_scope(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_get_item_type:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API void* webkit_dom_html_element_get_item_type(WebKitDOMHTMLElement* self);

WEBKIT_API GType webkit_dom_html_properties_collection_get_type(void);

/**
 * webkit_dom_html_properties_collection_item:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 * @index: A #gulong
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMNode* webkit_dom_html_properties_collection_item(void* self, gulong index);

/**
 * webkit_dom_html_properties_collection_named_item:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 * @name: A #gchar
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API void* webkit_dom_html_properties_collection_named_item(void* self, const gchar* name);

/**
 * webkit_dom_html_properties_collection_get_length:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gulong webkit_dom_html_properties_collection_get_length(void* self);

/**
 * webkit_dom_html_properties_collection_get_names:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMDOMStringList* webkit_dom_html_properties_collection_get_names(void* self);

/**
 * webkit_dom_node_get_attributes:
 * @self: A #WebKitDOMNode
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMNamedNodeMap* webkit_dom_node_get_attributes(WebKitDOMNode* self);

/**
 * webkit_dom_node_has_attributes:
 * @self: A #WebKitDOMNode
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gboolean webkit_dom_node_has_attributes(WebKitDOMNode* self);

WEBKIT_API GType webkit_dom_memory_info_get_type(void);

/**
 * webkit_dom_memory_info_get_total_js_heap_size:
 * @self: A #WebKitDOMMemoryInfo
 *
 * The MemoryInfo object has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gulong webkit_dom_memory_info_get_total_js_heap_size(void* self);

/**
 * webkit_dom_memory_info_get_used_js_heap_size:
 * @self: A #WebKitDOMMemoryInfo
 *
 * The MemoryInfo object has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gulong webkit_dom_memory_info_get_used_js_heap_size(void* self);

/**
 * webkit_dom_memory_info_get_js_heap_size_limit:
 * @self: A #WebKitDOMMemoryInfo
 *
 * The MemoryInfo object has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gulong webkit_dom_memory_info_get_js_heap_size_limit(void* self);

WEBKIT_API GType webkit_dom_micro_data_item_value_get_type(void);

/**
 * webkit_dom_performance_get_memory:
 * @self: A #WebKitDOMPerformance
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API void* webkit_dom_performance_get_memory(WebKitDOMPerformance* self);

WEBKIT_API GType webkit_dom_property_node_list_get_type(void);

/**
 * webkit_dom_property_node_list_item:
 * @self: A #WebKitDOMPropertyNodeList
 * @index: A #gulong
 *
 * The PropertyNodeList object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
**/
WEBKIT_API WebKitDOMNode* webkit_dom_property_node_list_item(void* self, gulong index);

/**
 * webkit_dom_property_node_list_get_length:
 * @self: A #WebKitDOMPropertyNodeList
 *
 * The PropertyNodeList object has been removed from WebKit, this function does nothing.
 *
 * Returns:
 *
**/
WEBKIT_API gulong webkit_dom_property_node_list_get_length(void* self);

G_END_DECLS

#endif
