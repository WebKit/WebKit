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

#ifndef WebKitDOMHTMLOptionsCollection_h
#define WebKitDOMHTMLOptionsCollection_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLCollection.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION            (webkit_dom_html_options_collection_get_type())
#define WEBKIT_DOM_HTML_OPTIONS_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION, WebKitDOMHTMLOptionsCollection))
#define WEBKIT_DOM_HTML_OPTIONS_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION, WebKitDOMHTMLOptionsCollectionClass)
#define WEBKIT_DOM_IS_HTML_OPTIONS_COLLECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION))
#define WEBKIT_DOM_IS_HTML_OPTIONS_COLLECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION))
#define WEBKIT_DOM_HTML_OPTIONS_COLLECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION, WebKitDOMHTMLOptionsCollectionClass))

struct _WebKitDOMHTMLOptionsCollection {
    WebKitDOMHTMLCollection parent_instance;
};

struct _WebKitDOMHTMLOptionsCollectionClass {
    WebKitDOMHTMLCollectionClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_options_collection_get_type(void);

/**
 * webkit_dom_html_options_collection_named_item:
 * @self: A #WebKitDOMHTMLOptionsCollection
 * @name: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_html_options_collection_named_item(WebKitDOMHTMLOptionsCollection* self, const gchar* name);

/**
 * webkit_dom_html_options_collection_get_selected_index:
 * @self: A #WebKitDOMHTMLOptionsCollection
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_options_collection_get_selected_index(WebKitDOMHTMLOptionsCollection* self);

/**
 * webkit_dom_html_options_collection_set_selected_index:
 * @self: A #WebKitDOMHTMLOptionsCollection
 * @value: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_options_collection_set_selected_index(WebKitDOMHTMLOptionsCollection* self, glong value);

/**
 * webkit_dom_html_options_collection_get_length:
 * @self: A #WebKitDOMHTMLOptionsCollection
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_html_options_collection_get_length(WebKitDOMHTMLOptionsCollection* self);

G_END_DECLS

#endif /* WebKitDOMHTMLOptionsCollection_h */
