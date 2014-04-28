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

#ifndef WebKitDOMDeprecated_h
#define WebKitDOMDeprecated_h

#if !defined(WEBKIT_DISABLE_DEPRECATED)

#include <glib-object.h>
#include <glib.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

/**
 * webkit_dom_blob_webkit_slice:
 * @self: A #WebKitDOMBlob
 * @start: A #gint64
 * @end: A #gint64
 * @content_type: A #gchar
 *
 * Returns: (transfer none): a #WebKitDOMBlob
 *
 * Deprecated: 1.10: Use webkit_dom_blob_slice() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_blob_slice) WebKitDOMBlob*
webkit_dom_blob_webkit_slice(WebKitDOMBlob* self, gint64 start, gint64 end, const gchar* content_type);

/**
 * webkit_dom_html_element_get_id:
 * @self: A #WebKitDOMHTMLElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.2: Use webkit_dom_element_get_id() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_id) gchar*
webkit_dom_html_element_get_id(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_id:
 * @self: A #WebKitDOMHTMLElement
 * @value: A #gchar
 *
 * Deprecated: 2.2: Use webkit_dom_element_set_id() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_set_id) void
webkit_dom_html_element_set_id(WebKitDOMHTMLElement* self, const gchar* value);

/**
 * webkit_dom_html_element_get_class_name:
 * @element: A #WebKitDOMHTMLElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 1.10: Use webkit_dom_element_get_class_name() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_class_name) gchar*
webkit_dom_html_element_get_class_name(WebKitDOMHTMLElement* element);

/**
 * webkit_dom_html_element_set_class_name:
 * @element: A #WebKitDOMHTMLElement
 * @value: A #gchar
 *
 *
 * Deprecated: 1.10: Use webkit_dom_element_set_class_name() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_set_class_name) void
webkit_dom_html_element_set_class_name(WebKitDOMHTMLElement* element, const gchar* value);

/**
 * webkit_dom_html_element_get_class_list:
 * @element: A #WebKitDOMHTMLElement
 *
 * Returns: (transfer none): a #WebKitDOMDOMTokenList
 *
 * Deprecated: 1.10: Use webkit_dom_element_get_class_list() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_class_list) WebKitDOMDOMTokenList*
webkit_dom_html_element_get_class_list(WebKitDOMHTMLElement* element);

/**
 * webkit_dom_html_document_get_active_element:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: (transfer none): a #WebKitDOMElement
 *
 * Deprecated: 2.6: Use webkit_dom_document_get_active_element() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_active_element) WebKitDOMElement*
webkit_dom_html_document_get_active_element(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_document_has_focus:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.6: Use webkit_dom_document_has_focus() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_has_focus) gboolean
webkit_dom_html_document_has_focus(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_input_element_get_webkitdirectory
 * @self: A #WebKitDOMHTMLInputElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_webkitdirectory(WebKitDOMHTMLInputElement* self);


/**
 * webkit_dom_html_input_element_set_webkitdirectory
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_webkitdirectory(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_form_element_dispatch_form_change:
 * @self: A #WebKitDOMHTMLFormElement
 *
 * Deprecated: 1.6
 */
WEBKIT_DEPRECATED void
webkit_dom_html_form_element_dispatch_form_change(WebKitDOMHTMLFormElement* self);

/**
 * webkit_dom_html_form_element_dispatch_form_input:
 * @self: A #WebKitDOMHTMLFormElement
 *
 * Deprecated: 1.6
 */
WEBKIT_DEPRECATED void
webkit_dom_html_form_element_dispatch_form_input(WebKitDOMHTMLFormElement* self);

/**
 * webkit_dom_webkit_named_flow_get_overflow:
 * @flow: A #WebKitDOMWebKitNamedFlow
 *
 * Returns: A #gboolean
 *
 * Deprecated: 1.10: Use webkit_dom_webkit_named_flow_get_overset() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_webkit_named_flow_get_overset) gboolean
webkit_dom_webkit_named_flow_get_overflow(WebKitDOMWebKitNamedFlow* flow);

/**
 * webkit_dom_element_get_webkit_region_overflow:
 * @element: A #WebKitDOMElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 1.10: Use webkit_dom_element_get_webkit_region_overset() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_webkit_region_overset) gchar*
webkit_dom_element_get_webkit_region_overflow(WebKitDOMElement* element);

/**
 * webkit_dom_webkit_named_flow_get_content_nodes:
 * @flow: A #WebKitDOMWebKitNamedFlow
 *
 * Returns: (transfer none): a #WebKitDOMNodeList
 *
 * Deprecated: 1.10: Use webkit_dom_webkit_named_flow_get_content() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_webkit_named_flow_get_content) WebKitDOMNodeList*
webkit_dom_webkit_named_flow_get_content_nodes(WebKitDOMWebKitNamedFlow* flow);

/**
 * webkit_dom_webkit_named_flow_get_regions_by_content_node:
 * @flow: A #WebKitDOMWebKitNamedFlow
 * @content_node: A #WebKitDOMNode
 *
 * Returns: (transfer none): a #WebKitDOMNodeList
 *
 * Deprecated: 1.10: Use webkit_dom_webkit_named_flow_get_regions_by_content() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_webkit_named_flow_get_regions_by_content) WebKitDOMNodeList*
webkit_dom_webkit_named_flow_get_regions_by_content_node(WebKitDOMWebKitNamedFlow* flow, WebKitDOMNode* content_node);

WEBKIT_DEPRECATED GType
webkit_dom_bar_info_get_type(void);

/**
 * webkit_dom_bar_info_get_visible:
 * @self: A #WebKitDOMBarInfo
 *
 * The BarInfo type has been removed from the DOM spec, this function does nothing.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_bar_info_get_visible(void* self);

/**
 * webkit_dom_css_style_declaration_get_property_css_value:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMCSSValue
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMCSSValue*
webkit_dom_css_style_declaration_get_property_css_value(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName);

/**
 * webkit_dom_document_get_webkit_hidden:
 * @self: A #WebKitDOMDocument
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_document_get_webkit_hidden(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_webkit_visibility_state:
 * @self: A #WebKitDOMDocument
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_webkit_visibility_state(WebKitDOMDocument* self);

/**
 * webkit_dom_html_document_open:
 * @self: A #WebKitDOMHTMLDocument
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void
webkit_dom_html_document_open(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_element_set_item_id:
 * @self: A #WebKitDOMHTMLElement
 * @value: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void
webkit_dom_html_element_set_item_id(WebKitDOMHTMLElement* self, const gchar* value);

/**
 * webkit_dom_html_element_get_item_id:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_html_element_get_item_id(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_get_item_ref:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMDOMSettableTokenList
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMDOMSettableTokenList*
webkit_dom_html_element_get_item_ref(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_get_item_prop:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMDOMSettableTokenList
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMDOMSettableTokenList*
webkit_dom_html_element_get_item_prop(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_item_scope:
 * @self: A #WebKitDOMHTMLElement
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void
webkit_dom_html_element_set_item_scope(WebKitDOMHTMLElement* self, gboolean value);

/**
 * webkit_dom_html_element_get_item_scope:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_html_element_get_item_scope(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_get_item_type:
 * @self: A #WebKitDOMHTMLElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void*
webkit_dom_html_element_get_item_type(WebKitDOMHTMLElement* self);


/**
 * webkit_dom_html_style_element_set_scoped:
 * @self: A #WebKitDOMHTMLStyleElement
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void
webkit_dom_html_style_element_set_scoped(WebKitDOMHTMLStyleElement* self, gboolean value);

/**
 * webkit_dom_html_style_element_get_scoped:
 * @self: A #WebKitDOMHTMLStyleElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_html_style_element_get_scoped(WebKitDOMHTMLStyleElement* self);


WEBKIT_DEPRECATED GType
webkit_dom_html_properties_collection_get_type(void);

/**
 * webkit_dom_html_properties_collection_item:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 * @index: A #gulong
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMNode
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_html_properties_collection_item(void* self, gulong index);

/**
 * webkit_dom_html_properties_collection_named_item:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 * @name: A #gchar
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void*
webkit_dom_html_properties_collection_named_item(void* self, const gchar* name);

/**
 * webkit_dom_html_properties_collection_get_length:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gulong
webkit_dom_html_properties_collection_get_length(void* self);

/**
 * webkit_dom_html_properties_collection_get_names:
 * @self: A #WebKitDOMHTMLPropertiesCollection
 *
 * The PropertiesCollection object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMDOMStringList
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMDOMStringList*
webkit_dom_html_properties_collection_get_names(void* self);

/**
 * webkit_dom_node_get_attributes:
 * @self: A #WebKitDOMNode
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMNamedNodeMap
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMNamedNodeMap*
webkit_dom_node_get_attributes(WebKitDOMNode* self);

/**
 * webkit_dom_node_has_attributes:
 * @self: A #WebKitDOMNode
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_node_has_attributes(WebKitDOMNode* self);

WEBKIT_DEPRECATED GType
webkit_dom_memory_info_get_type(void);

/**
 * webkit_dom_memory_info_get_total_js_heap_size:
 * @self: A #WebKitDOMMemoryInfo
 *
 * The MemoryInfo object has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gulong
webkit_dom_memory_info_get_total_js_heap_size(void* self);

/**
 * webkit_dom_memory_info_get_used_js_heap_size:
 * @self: A #WebKitDOMMemoryInfo
 *
 * The MemoryInfo object has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gulong
webkit_dom_memory_info_get_used_js_heap_size(void* self);

/**
 * webkit_dom_memory_info_get_js_heap_size_limit:
 * @self: A #WebKitDOMMemoryInfo
 *
 * The MemoryInfo object has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gulong
webkit_dom_memory_info_get_js_heap_size_limit(void* self);

WEBKIT_DEPRECATED GType
webkit_dom_micro_data_item_value_get_type(void);

/**
 * webkit_dom_performance_get_memory:
 * @self: A #WebKitDOMPerformance
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none):
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED void*
webkit_dom_performance_get_memory(WebKitDOMPerformance* self);

WEBKIT_DEPRECATED GType
webkit_dom_property_node_list_get_type(void);

/**
 * webkit_dom_property_node_list_item:
 * @self: A #WebKitDOMPropertyNodeList
 * @index: A #gulong
 *
 * The PropertyNodeList object has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMNode
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_property_node_list_item(void* self, gulong index);

/**
 * webkit_dom_property_node_list_get_length:
 * @self: A #WebKitDOMPropertyNodeList
 *
 * The PropertyNodeList object has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gulong
webkit_dom_property_node_list_get_length(void* self);

/**
 * webkit_dom_html_media_element_get_start_time:
 * @self: A #HTMLMediaElement
 *
 * The HTMLMediaElement:start-time property has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gdouble
webkit_dom_html_media_element_get_start_time(WebKitDOMHTMLMediaElement* self);

/**
 * webkit_dom_html_media_element_get_initial_time:
 * @self: A #HTMLMediaElement
 *
 * The HTMLMediaElement:initial-time property has been removed from WebKit, this function does nothing.
 *
 * Returns: A #gdouble
 *
 * Deprecated: 2.2
 */
WEBKIT_DEPRECATED gdouble
webkit_dom_html_media_element_get_initial_time(WebKitDOMHTMLMediaElement* self);

/**
 * webkit_dom_processing_instruction_get_data:
 * @self: A #WebKitDOMProcessingInstruction
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: a #gchar
 *
 * Deprecated: 2.4
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_processing_instruction_get_data(WebKitDOMProcessingInstruction* self);

/**
 * webkit_dom_processing_instruction_set_data:
 * @self: A #WebKitDOMProcessingInstruction
 * @value: A #gchar
 * @error: #GError
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.4
 */
WEBKIT_DEPRECATED void
webkit_dom_processing_instruction_set_data(WebKitDOMProcessingInstruction* self, const gchar* value, GError** error);

/**
 * webkit_dom_file_get_webkit_relative_path:
 * @self: A #WebKitDOMFile
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: a #gchar
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_file_get_webkit_relative_path(WebKitDOMFile* self);

/**
 * webkit_dom_html_iframe_element_get_seamless:
 * @self: A #WebKitDOMHTMLIFrameElement
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: a #gboolean
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_html_iframe_element_get_seamless(WebKitDOMHTMLIFrameElement* self);

/**
 * webkit_dom_html_iframe_element_set_seamless:
 * @self: A #WebKitDOMHTMLIFrameElement
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED void
webkit_dom_html_iframe_element_set_seamless(WebKitDOMHTMLIFrameElement* self, gboolean value);

WEBKIT_DEPRECATED GType
webkit_dom_shadow_root_get_type(void);

typedef struct _WebKitDOMShadowRoot WebKitDOMShadowRoot;

/**
 * webkit_dom_shadow_root_element_from_point:
 * @self: A #WebKitDOMShadowRoot
 * @x: A #glong
 * @y: A #glong
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMElement
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_shadow_root_element_from_point(WebKitDOMShadowRoot* self, glong x, glong y);

/**
 * webkit_dom_shadow_root_get_active_element:
 * @self: A #WebKitDOMShadowRoot
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMElement
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_shadow_root_get_active_element(WebKitDOMShadowRoot* self);

/**
 * webkit_dom_shadow_root_get_apply_author_styles:
 * @self: A #WebKitDOMShadowRoot
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #gboolean
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_shadow_root_get_apply_author_styles(WebKitDOMShadowRoot* self);

/**
 * webkit_dom_shadow_root_get_element_by_id:
 * @self: A #WebKitDOMShadowRoot
 * @id: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMElement
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_shadow_root_get_element_by_id(WebKitDOMShadowRoot* self, const gchar* id);

/**
 * webkit_dom_shadow_root_get_elements_by_class_name:
 * @self: A #WebKitDOMShadowRoot
 * @name: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMNodeList
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_shadow_root_get_elements_by_class_name(WebKitDOMShadowRoot* self, const gchar* name);

/**
 * webkit_dom_shadow_root_get_elements_by_tag_name:
 * @self: A #WebKitDOMShadowRoot
 * @name: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMNodeList
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_shadow_root_get_elements_by_tag_name(WebKitDOMShadowRoot* self, const gchar* name);

/**
 * webkit_dom_shadow_root_get_elements_by_tag_name_ns:
 * @self: A #WebKitDOMShadowRoot
 * @name: A #gchar
 * @ns: A #gchar
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMNodeList
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_shadow_root_get_elements_by_tag_name_ns(WebKitDOMShadowRoot* self, const gchar* name, const gchar* ns);

/**
 * webkit_dom_shadow_root_get_inner_html:
 * @self: A #WebKitDOMShadowRoot
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #gchar
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_shadow_root_get_inner_html(WebKitDOMShadowRoot* self);

/**
 * webkit_dom_shadow_root_get_reset_style_inheritance:
 * @self: A #WebKitDOMShadowRoot
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #gboolean
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_shadow_root_get_reset_style_inheritance(WebKitDOMShadowRoot* self);

/**
 * webkit_dom_shadow_root_get_selection:
 * @self: A #WebKitDOMShadowRoot
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Returns: (transfer none): a #WebKitDOMDOMSelection
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED WebKitDOMDOMSelection*
webkit_dom_shadow_root_get_selection(WebKitDOMShadowRoot* self);

/**
 * webkit_dom_shadow_root_set_apply_author_styles:
 * @self: A #WebKitDOMShadowRoot
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED void
webkit_dom_shadow_root_set_apply_author_styles(WebKitDOMShadowRoot* self, gboolean value);

/**
 * webkit_dom_shadow_root_set_inner_html:
 * @self: A #WebKitDOMShadowRoot
 * @html: A #gchar
 * @error: A #GError
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED void
webkit_dom_shadow_root_set_inner_html(WebKitDOMShadowRoot* self, const gchar* html, GError** error);

/**
 * webkit_dom_shadow_root_set_reset_style_inheritance:
 * @self: A #WebKitDOMShadowRoot
 * @value: A #gboolean
 *
 * This functionality has been removed from WebKit, this function does nothing.
 *
 * Deprecated: 2.6
 */
WEBKIT_DEPRECATED void
webkit_dom_shadow_root_set_reset_style_inheritance(WebKitDOMShadowRoot* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_capture:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.6: Use webkit_dom_html_input_element_get_capture_enabled() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_html_input_element_get_capture_enabled) gchar*
webkit_dom_html_input_element_get_capture(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_capture:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 * Deprecated: 2.6: Use webkit_dom_html_input_element_set_capture_enabled() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_html_input_element_set_capture_enabled) void
webkit_dom_html_input_element_set_capture(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_text_track_cue_get_cue_as_html:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: (transfer none): A #WebKitDOMDocumentFragment
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_cue_as_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_cue_as_html) WebKitDOMDocumentFragment*
webkit_dom_text_track_cue_get_cue_as_html(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_get_vertical:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_vertical() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_vertical) gchar*
webkit_dom_text_track_cue_get_vertical(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_vertical:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #gchar
 * @error: #GError
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_vertical() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_vertical) void
webkit_dom_text_track_cue_set_vertical(WebKitDOMTextTrackCue* self, const gchar* value, GError** error);

/**
 * webkit_dom_text_track_cue_get_snap_to_lines:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_snap_to_lines() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_snap_to_lines) gboolean
webkit_dom_text_track_cue_get_snap_to_lines(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_snap_to_lines:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #gboolean
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_snap_to_lines() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_snap_to_lines) void
webkit_dom_text_track_cue_set_snap_to_lines(WebKitDOMTextTrackCue* self, gboolean value);

/**
 * webkit_dom_text_track_cue_get_line:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #glong
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_line() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_line) glong
webkit_dom_text_track_cue_get_line(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_line:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #glong
 * @error: #GError
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_line() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_line) void
webkit_dom_text_track_cue_set_line(WebKitDOMTextTrackCue* self, glong value, GError** error);

/**
 * webkit_dom_text_track_cue_get_position:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #glong
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_position() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_position) glong
webkit_dom_text_track_cue_get_position(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_position:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #glong
 * @error: #GError
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_position() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_position) void
webkit_dom_text_track_cue_set_position(WebKitDOMTextTrackCue* self, glong value, GError** error);

/**
 * webkit_dom_text_track_cue_get_size:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #glong
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_size() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_size) glong
webkit_dom_text_track_cue_get_size(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_size:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #glong
 * @error: #GError
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_size() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_size) void
webkit_dom_text_track_cue_set_size(WebKitDOMTextTrackCue* self, glong value, GError** error);

/**
 * webkit_dom_text_track_cue_get_align:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_align() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_align) gchar*
webkit_dom_text_track_cue_get_align(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_align:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #gchar
 * @error: #GError
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_align() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_align) void
webkit_dom_text_track_cue_set_align(WebKitDOMTextTrackCue* self, const gchar* value, GError** error);

/**
 * webkit_dom_text_track_cue_get_text:
 * @self: A #WebKitDOMTextTrackCue
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_get_text() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_get_text) gchar*
webkit_dom_text_track_cue_get_text(WebKitDOMTextTrackCue* self);

/**
 * webkit_dom_text_track_cue_set_text:
 * @self: A #WebKitDOMTextTrackCue
 * @value: A #gchar
 *
 * Deprecated: 2.6: Use webkit_dom_vtt_cue_set_text() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_vtt_cue_set_text) void
webkit_dom_text_track_cue_set_text(WebKitDOMTextTrackCue* self, const gchar* value);

/**
 * webkit_dom_text_track_add_cue:
 * @self: A #WebKitDOMTextTrack
 * @cue: A #WebKitDOMTextTrackCue
 *
 * Deprecated: 2.6: Use webkit_dom_text_track_add_cue_with_error() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_text_track_add_cue_with_error) void
webkit_dom_text_track_add_cue(WebKitDOMTextTrack* self, WebKitDOMTextTrackCue* cue);

G_END_DECLS

#endif /* WEBKIT_DISABLE_DEPRECATED */

#endif
