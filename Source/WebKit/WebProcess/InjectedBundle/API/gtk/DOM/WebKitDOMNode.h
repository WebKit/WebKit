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

#ifndef WebKitDOMNode_h
#define WebKitDOMNode_h

#include <glib-object.h>
#include <jsc/jsc.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_NODE            (webkit_dom_node_get_type())
#define WEBKIT_DOM_NODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_NODE, WebKitDOMNode))
#define WEBKIT_DOM_NODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_NODE, WebKitDOMNodeClass)
#define WEBKIT_DOM_IS_NODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_NODE))
#define WEBKIT_DOM_IS_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_NODE))
#define WEBKIT_DOM_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_NODE, WebKitDOMNodeClass))

struct _WebKitDOMNode {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMNodeClass {
    WebKitDOMObjectClass parent_class;
};

#ifndef WEBKIT_DISABLE_DEPRECATED

/**
 * WEBKIT_DOM_NODE_ELEMENT_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_ELEMENT_NODE 1

/**
 * WEBKIT_DOM_NODE_ATTRIBUTE_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_ATTRIBUTE_NODE 2

/**
 * WEBKIT_DOM_NODE_TEXT_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_TEXT_NODE 3

/**
 * WEBKIT_DOM_NODE_CDATA_SECTION_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_CDATA_SECTION_NODE 4

/**
 * WEBKIT_DOM_NODE_ENTITY_REFERENCE_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_ENTITY_REFERENCE_NODE 5

/**
 * WEBKIT_DOM_NODE_ENTITY_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_ENTITY_NODE 6

/**
 * WEBKIT_DOM_NODE_PROCESSING_INSTRUCTION_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_PROCESSING_INSTRUCTION_NODE 7

/**
 * WEBKIT_DOM_NODE_COMMENT_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_COMMENT_NODE 8

/**
 * WEBKIT_DOM_NODE_DOCUMENT_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_NODE 9

/**
 * WEBKIT_DOM_NODE_DOCUMENT_TYPE_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_TYPE_NODE 10

/**
 * WEBKIT_DOM_NODE_DOCUMENT_FRAGMENT_NODE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_FRAGMENT_NODE 11

/**
 * WEBKIT_DOM_NODE_DOCUMENT_POSITION_DISCONNECTED:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_POSITION_DISCONNECTED 0x01

/**
 * WEBKIT_DOM_NODE_DOCUMENT_POSITION_PRECEDING:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_POSITION_PRECEDING 0x02

/**
 * WEBKIT_DOM_NODE_DOCUMENT_POSITION_FOLLOWING:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_POSITION_FOLLOWING 0x04

/**
 * WEBKIT_DOM_NODE_DOCUMENT_POSITION_CONTAINS:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_POSITION_CONTAINS 0x08

/**
 * WEBKIT_DOM_NODE_DOCUMENT_POSITION_CONTAINED_BY:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_POSITION_CONTAINED_BY 0x10

/**
 * WEBKIT_DOM_NODE_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_NODE_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC 0x20

#endif /* WEBKIT_DISABLE_DEPRECATED */

WEBKIT_DEPRECATED GType
webkit_dom_node_get_type(void);

/**
 * webkit_dom_node_insert_before:
 * @self: A #WebKitDOMNode
 * @newChild: A #WebKitDOMNode
 * @refChild: (allow-none): A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_insert_before(WebKitDOMNode* self, WebKitDOMNode* newChild, WebKitDOMNode* refChild, GError** error);

/**
 * webkit_dom_node_replace_child:
 * @self: A #WebKitDOMNode
 * @newChild: A #WebKitDOMNode
 * @oldChild: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_replace_child(WebKitDOMNode* self, WebKitDOMNode* newChild, WebKitDOMNode* oldChild, GError** error);

/**
 * webkit_dom_node_remove_child:
 * @self: A #WebKitDOMNode
 * @oldChild: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_remove_child(WebKitDOMNode* self, WebKitDOMNode* oldChild, GError** error);

/**
 * webkit_dom_node_append_child:
 * @self: A #WebKitDOMNode
 * @newChild: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_append_child(WebKitDOMNode* self, WebKitDOMNode* newChild, GError** error);

/**
 * webkit_dom_node_has_child_nodes:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_node_has_child_nodes(WebKitDOMNode* self);

/**
 * webkit_dom_node_clone_node_with_error:
 * @self: A #WebKitDOMNode
 * @deep: A #gboolean
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_clone_node_with_error(WebKitDOMNode* self, gboolean deep, GError** error);

/**
 * webkit_dom_node_normalize:
 * @self: A #WebKitDOMNode
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_node_normalize(WebKitDOMNode* self);

/**
 * webkit_dom_node_is_supported:
 * @self: A #WebKitDOMNode
 * @feature: A #gchar
 * @version: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_node_is_supported(WebKitDOMNode* self, const gchar* feature, const gchar* version);

/**
 * webkit_dom_node_is_same_node:
 * @self: A #WebKitDOMNode
 * @other: A #WebKitDOMNode
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_node_is_same_node(WebKitDOMNode* self, WebKitDOMNode* other);

/**
 * webkit_dom_node_is_equal_node:
 * @self: A #WebKitDOMNode
 * @other: A #WebKitDOMNode
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_node_is_equal_node(WebKitDOMNode* self, WebKitDOMNode* other);

/**
 * webkit_dom_node_lookup_prefix:
 * @self: A #WebKitDOMNode
 * @namespaceURI: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_node_lookup_prefix(WebKitDOMNode* self, const gchar* namespaceURI);

/**
 * webkit_dom_node_lookup_namespace_uri:
 * @self: A #WebKitDOMNode
 * @prefix: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_node_lookup_namespace_uri(WebKitDOMNode* self, const gchar* prefix);

/**
 * webkit_dom_node_is_default_namespace:
 * @self: A #WebKitDOMNode
 * @namespaceURI: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_node_is_default_namespace(WebKitDOMNode* self, const gchar* namespaceURI);

/**
 * webkit_dom_node_compare_document_position:
 * @self: A #WebKitDOMNode
 * @other: A #WebKitDOMNode
 *
 * Returns: A #gushort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gushort
webkit_dom_node_compare_document_position(WebKitDOMNode* self, WebKitDOMNode* other);

/**
 * webkit_dom_node_contains:
 * @self: A #WebKitDOMNode
 * @other: A #WebKitDOMNode
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_node_contains(WebKitDOMNode* self, WebKitDOMNode* other);

/**
 * webkit_dom_node_get_node_name:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_node_name(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_node_value:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_node_value(WebKitDOMNode* self);

/**
 * webkit_dom_node_set_node_value:
 * @self: A #WebKitDOMNode
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_node_set_node_value(WebKitDOMNode* self, const gchar* value, GError** error);

/**
 * webkit_dom_node_get_node_type:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gushort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gushort
webkit_dom_node_get_node_type(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_parent_node:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_get_parent_node(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_child_nodes:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer full): A #WebKitDOMNodeList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_node_get_child_nodes(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_first_child:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_get_first_child(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_last_child:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_get_last_child(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_previous_sibling:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_get_previous_sibling(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_next_sibling:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_node_get_next_sibling(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_owner_document:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMDocument
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocument*
webkit_dom_node_get_owner_document(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_base_uri:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_base_uri(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_text_content:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_text_content(WebKitDOMNode* self);

/**
 * webkit_dom_node_set_text_content:
 * @self: A #WebKitDOMNode
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_node_set_text_content(WebKitDOMNode* self, const gchar* value, GError** error);

/**
 * webkit_dom_node_get_parent_element:
 * @self: A #WebKitDOMNode
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_node_get_parent_element(WebKitDOMNode* self);

WEBKIT_DEPRECATED WebKitDOMNode *
webkit_dom_node_for_js_value(JSCValue* value);

G_END_DECLS

#endif /* WebKitDOMNode_h */
