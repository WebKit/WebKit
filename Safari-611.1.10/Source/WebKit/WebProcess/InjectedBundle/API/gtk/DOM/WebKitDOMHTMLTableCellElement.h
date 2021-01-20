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

#ifndef WebKitDOMHTMLTableCellElement_h
#define WebKitDOMHTMLTableCellElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT            (webkit_dom_html_table_cell_element_get_type())
#define WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT, WebKitDOMHTMLTableCellElement))
#define WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT, WebKitDOMHTMLTableCellElementClass)
#define WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT))
#define WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT))
#define WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT, WebKitDOMHTMLTableCellElementClass))

struct _WebKitDOMHTMLTableCellElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLTableCellElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_table_cell_element_get_type(void);

/**
 * webkit_dom_html_table_cell_element_get_cell_index:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_table_cell_element_get_cell_index(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_get_align:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_align(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_align:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_align(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_axis:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_axis(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_axis:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_axis(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_bg_color:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_bg_color(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_bg_color:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_bg_color(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_ch:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_ch(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_ch:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_ch(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_ch_off:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_ch_off(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_ch_off:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_ch_off(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_col_span:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_table_cell_element_get_col_span(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_col_span:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_col_span(WebKitDOMHTMLTableCellElement* self, glong value);

/**
 * webkit_dom_html_table_cell_element_get_row_span:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_table_cell_element_get_row_span(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_row_span:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_row_span(WebKitDOMHTMLTableCellElement* self, glong value);

/**
 * webkit_dom_html_table_cell_element_get_headers:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_headers(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_headers:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_headers(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_height:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_height(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_height:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_height(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_no_wrap:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_table_cell_element_get_no_wrap(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_no_wrap:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_no_wrap(WebKitDOMHTMLTableCellElement* self, gboolean value);

/**
 * webkit_dom_html_table_cell_element_get_v_align:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_v_align(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_v_align:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_v_align(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_width:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_width(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_width:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_width(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_abbr:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_abbr(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_abbr:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_abbr(WebKitDOMHTMLTableCellElement* self, const gchar* value);

/**
 * webkit_dom_html_table_cell_element_get_scope:
 * @self: A #WebKitDOMHTMLTableCellElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_table_cell_element_get_scope(WebKitDOMHTMLTableCellElement* self);

/**
 * webkit_dom_html_table_cell_element_set_scope:
 * @self: A #WebKitDOMHTMLTableCellElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_table_cell_element_set_scope(WebKitDOMHTMLTableCellElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLTableCellElement_h */
