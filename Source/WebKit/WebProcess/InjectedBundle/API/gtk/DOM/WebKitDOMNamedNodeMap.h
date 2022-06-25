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

#ifndef WebKitDOMNamedNodeMap_h
#define WebKitDOMNamedNodeMap_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_NAMED_NODE_MAP            (webkit_dom_named_node_map_get_type())
#define WEBKIT_DOM_NAMED_NODE_MAP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_NAMED_NODE_MAP, WebKitDOMNamedNodeMap))
#define WEBKIT_DOM_NAMED_NODE_MAP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_NAMED_NODE_MAP, WebKitDOMNamedNodeMapClass)
#define WEBKIT_DOM_IS_NAMED_NODE_MAP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_NAMED_NODE_MAP))
#define WEBKIT_DOM_IS_NAMED_NODE_MAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_NAMED_NODE_MAP))
#define WEBKIT_DOM_NAMED_NODE_MAP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_NAMED_NODE_MAP, WebKitDOMNamedNodeMapClass))

struct _WebKitDOMNamedNodeMap {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMNamedNodeMapClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_named_node_map_get_type(void);

/**
 * webkit_dom_named_node_map_get_named_item:
 * @self: A #WebKitDOMNamedNodeMap
 * @name: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_get_named_item(WebKitDOMNamedNodeMap* self, const gchar* name);

/**
 * webkit_dom_named_node_map_set_named_item:
 * @self: A #WebKitDOMNamedNodeMap
 * @node: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_set_named_item(WebKitDOMNamedNodeMap* self, WebKitDOMNode* node, GError** error);

/**
 * webkit_dom_named_node_map_remove_named_item:
 * @self: A #WebKitDOMNamedNodeMap
 * @name: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_remove_named_item(WebKitDOMNamedNodeMap* self, const gchar* name, GError** error);

/**
 * webkit_dom_named_node_map_item:
 * @self: A #WebKitDOMNamedNodeMap
 * @index: A #gulong
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_item(WebKitDOMNamedNodeMap* self, gulong index);

/**
 * webkit_dom_named_node_map_get_named_item_ns:
 * @self: A #WebKitDOMNamedNodeMap
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_get_named_item_ns(WebKitDOMNamedNodeMap* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_named_node_map_set_named_item_ns:
 * @self: A #WebKitDOMNamedNodeMap
 * @node: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_set_named_item_ns(WebKitDOMNamedNodeMap* self, WebKitDOMNode* node, GError** error);

/**
 * webkit_dom_named_node_map_remove_named_item_ns:
 * @self: A #WebKitDOMNamedNodeMap
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_named_node_map_remove_named_item_ns(WebKitDOMNamedNodeMap* self, const gchar* namespaceURI, const gchar* localName, GError** error);

/**
 * webkit_dom_named_node_map_get_length:
 * @self: A #WebKitDOMNamedNodeMap
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_named_node_map_get_length(WebKitDOMNamedNodeMap* self);

G_END_DECLS

#endif /* WebKitDOMNamedNodeMap_h */
