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

#ifndef WebKitDOMHTMLCollection_h
#define WebKitDOMHTMLCollection_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_COLLECTION            (webkit_dom_html_collection_get_type())
#define WEBKIT_DOM_HTML_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_COLLECTION, WebKitDOMHTMLCollection))
#define WEBKIT_DOM_HTML_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_COLLECTION, WebKitDOMHTMLCollectionClass)
#define WEBKIT_DOM_IS_HTML_COLLECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_COLLECTION))
#define WEBKIT_DOM_IS_HTML_COLLECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_COLLECTION))
#define WEBKIT_DOM_HTML_COLLECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_COLLECTION, WebKitDOMHTMLCollectionClass))

struct _WebKitDOMHTMLCollection {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMHTMLCollectionClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_collection_get_type(void);

/**
 * webkit_dom_html_collection_item:
 * @self: A #WebKitDOMHTMLCollection
 * @index: A #gulong
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_html_collection_item(WebKitDOMHTMLCollection* self, gulong index);

/**
 * webkit_dom_html_collection_named_item:
 * @self: A #WebKitDOMHTMLCollection
 * @name: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_html_collection_named_item(WebKitDOMHTMLCollection* self, const gchar* name);

/**
 * webkit_dom_html_collection_get_length:
 * @self: A #WebKitDOMHTMLCollection
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_html_collection_get_length(WebKitDOMHTMLCollection* self);

G_END_DECLS

#endif /* WebKitDOMHTMLCollection_h */
