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

#ifndef WebKitDOMStyleSheet_h
#define WebKitDOMStyleSheet_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_STYLE_SHEET            (webkit_dom_style_sheet_get_type())
#define WEBKIT_DOM_STYLE_SHEET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_STYLE_SHEET, WebKitDOMStyleSheet))
#define WEBKIT_DOM_STYLE_SHEET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_STYLE_SHEET, WebKitDOMStyleSheetClass)
#define WEBKIT_DOM_IS_STYLE_SHEET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_STYLE_SHEET))
#define WEBKIT_DOM_IS_STYLE_SHEET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_STYLE_SHEET))
#define WEBKIT_DOM_STYLE_SHEET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_STYLE_SHEET, WebKitDOMStyleSheetClass))

struct _WebKitDOMStyleSheet {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMStyleSheetClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_style_sheet_get_type(void);

/**
 * webkit_dom_style_sheet_get_content_type:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_style_sheet_get_content_type(WebKitDOMStyleSheet* self);

/**
 * webkit_dom_style_sheet_get_disabled:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_style_sheet_get_disabled(WebKitDOMStyleSheet* self);

/**
 * webkit_dom_style_sheet_set_disabled:
 * @self: A #WebKitDOMStyleSheet
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_style_sheet_set_disabled(WebKitDOMStyleSheet* self, gboolean value);

/**
 * webkit_dom_style_sheet_get_owner_node:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_style_sheet_get_owner_node(WebKitDOMStyleSheet* self);

/**
 * webkit_dom_style_sheet_get_parent_style_sheet:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: (transfer full): A #WebKitDOMStyleSheet
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMStyleSheet*
webkit_dom_style_sheet_get_parent_style_sheet(WebKitDOMStyleSheet* self);

/**
 * webkit_dom_style_sheet_get_href:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_style_sheet_get_href(WebKitDOMStyleSheet* self);

/**
 * webkit_dom_style_sheet_get_title:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_style_sheet_get_title(WebKitDOMStyleSheet* self);

/**
 * webkit_dom_style_sheet_get_media:
 * @self: A #WebKitDOMStyleSheet
 *
 * Returns: (transfer full): A #WebKitDOMMediaList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMMediaList*
webkit_dom_style_sheet_get_media(WebKitDOMStyleSheet* self);

G_END_DECLS

#endif /* WebKitDOMStyleSheet_h */
