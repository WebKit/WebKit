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

#ifndef WebKitDOMRange_h
#define WebKitDOMRange_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_RANGE            (webkit_dom_range_get_type())
#define WEBKIT_DOM_RANGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_RANGE, WebKitDOMRange))
#define WEBKIT_DOM_RANGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_RANGE, WebKitDOMRangeClass)
#define WEBKIT_DOM_IS_RANGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_RANGE))
#define WEBKIT_DOM_IS_RANGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_RANGE))
#define WEBKIT_DOM_RANGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_RANGE, WebKitDOMRangeClass))

#ifndef WEBKIT_DISABLE_DEPRECATED

/**
 * WEBKIT_DOM_RANGE_START_TO_START:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_START_TO_START 0

/**
 * WEBKIT_DOM_RANGE_START_TO_END:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_START_TO_END 1

/**
 * WEBKIT_DOM_RANGE_END_TO_END:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_END_TO_END 2

/**
 * WEBKIT_DOM_RANGE_END_TO_START:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_END_TO_START 3

/**
 * WEBKIT_DOM_RANGE_NODE_BEFORE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_NODE_BEFORE 0

/**
 * WEBKIT_DOM_RANGE_NODE_AFTER:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_NODE_AFTER 1

/**
 * WEBKIT_DOM_RANGE_NODE_BEFORE_AND_AFTER:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_NODE_BEFORE_AND_AFTER 2

/**
 * WEBKIT_DOM_RANGE_NODE_INSIDE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_RANGE_NODE_INSIDE 3

#endif /* WEBKIT_DISABLE_DEPRECATED */

struct _WebKitDOMRange {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMRangeClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_range_get_type(void);

/**
 * webkit_dom_range_set_start:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @offset: A #glong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_set_start(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error);

/**
 * webkit_dom_range_set_end:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @offset: A #glong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_set_end(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error);

/**
 * webkit_dom_range_set_start_before:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_set_start_before(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_set_start_after:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_set_start_after(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_set_end_before:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_set_end_before(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_set_end_after:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_set_end_after(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_collapse:
 * @self: A #WebKitDOMRange
 * @toStart: A #gboolean
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_collapse(WebKitDOMRange* self, gboolean toStart, GError** error);

/**
 * webkit_dom_range_select_node:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_select_node(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_select_node_contents:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_select_node_contents(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_compare_boundary_points:
 * @self: A #WebKitDOMRange
 * @how: A #gushort
 * @sourceRange: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: A #gshort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gshort
webkit_dom_range_compare_boundary_points(WebKitDOMRange* self, gushort how, WebKitDOMRange* sourceRange, GError** error);

/**
 * webkit_dom_range_delete_contents:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_delete_contents(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_extract_contents:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMDocumentFragment
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocumentFragment*
webkit_dom_range_extract_contents(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_clone_contents:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMDocumentFragment
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocumentFragment*
webkit_dom_range_clone_contents(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_insert_node:
 * @self: A #WebKitDOMRange
 * @newNode: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_insert_node(WebKitDOMRange* self, WebKitDOMNode* newNode, GError** error);

/**
 * webkit_dom_range_surround_contents:
 * @self: A #WebKitDOMRange
 * @newParent: A #WebKitDOMNode
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_surround_contents(WebKitDOMRange* self, WebKitDOMNode* newParent, GError** error);

/**
 * webkit_dom_range_clone_range:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMRange
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMRange*
webkit_dom_range_clone_range(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_to_string:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_range_to_string(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_detach:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_detach(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_create_contextual_fragment:
 * @self: A #WebKitDOMRange
 * @html: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMDocumentFragment
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocumentFragment*
webkit_dom_range_create_contextual_fragment(WebKitDOMRange* self, const gchar* html, GError** error);

/**
 * webkit_dom_range_compare_node:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: A #gshort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gshort
webkit_dom_range_compare_node(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_intersects_node:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_range_intersects_node(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error);

/**
 * webkit_dom_range_compare_point:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @offset: A #glong
 * @error: #GError
 *
 * Returns: A #gshort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gshort
webkit_dom_range_compare_point(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error);

/**
 * webkit_dom_range_is_point_in_range:
 * @self: A #WebKitDOMRange
 * @refNode: A #WebKitDOMNode
 * @offset: A #glong
 * @error: #GError
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_range_is_point_in_range(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error);

/**
 * webkit_dom_range_get_start_container:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_range_get_start_container(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_get_start_offset:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_range_get_start_offset(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_get_end_container:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_range_get_end_container(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_get_end_offset:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_range_get_end_offset(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_get_collapsed:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_range_get_collapsed(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_get_common_ancestor_container:
 * @self: A #WebKitDOMRange
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_range_get_common_ancestor_container(WebKitDOMRange* self, GError** error);

/**
 * webkit_dom_range_get_text:
 * @self: A #WebKitDOMRange
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_range_get_text(WebKitDOMRange* self);

/**
 * webkit_dom_range_expand:
 * @self: A #WebKitDOMRange
 * @unit: A #gchar
 * @error: #GError
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_range_expand(WebKitDOMRange* self, const gchar* unit, GError** error);

G_END_DECLS

#endif /* WebKitDOMRange_h */
