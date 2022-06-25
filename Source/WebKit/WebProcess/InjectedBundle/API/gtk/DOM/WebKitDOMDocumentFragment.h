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

#ifndef WebKitDOMDocumentFragment_h
#define WebKitDOMDocumentFragment_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMNode.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT            (webkit_dom_document_fragment_get_type())
#define WEBKIT_DOM_DOCUMENT_FRAGMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT, WebKitDOMDocumentFragment))
#define WEBKIT_DOM_DOCUMENT_FRAGMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT, WebKitDOMDocumentFragmentClass)
#define WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT))
#define WEBKIT_DOM_IS_DOCUMENT_FRAGMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT))
#define WEBKIT_DOM_DOCUMENT_FRAGMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT, WebKitDOMDocumentFragmentClass))

struct _WebKitDOMDocumentFragment {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMDocumentFragmentClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_document_fragment_get_type(void);

/**
 * webkit_dom_document_fragment_get_element_by_id:
 * @self: A #WebKitDOMDocumentFragment
 * @elementId: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_fragment_get_element_by_id(WebKitDOMDocumentFragment* self, const gchar* elementId);

/**
 * webkit_dom_document_fragment_query_selector:
 * @self: A #WebKitDOMDocumentFragment
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_fragment_query_selector(WebKitDOMDocumentFragment* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_document_fragment_query_selector_all:
 * @self: A #WebKitDOMDocumentFragment
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMNodeList
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_document_fragment_query_selector_all(WebKitDOMDocumentFragment* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_document_fragment_get_children:
 * @self: A #WebKitDOMDocumentFragment
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_fragment_get_children(WebKitDOMDocumentFragment* self);

/**
 * webkit_dom_document_fragment_get_first_element_child:
 * @self: A #WebKitDOMDocumentFragment
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_fragment_get_first_element_child(WebKitDOMDocumentFragment* self);

/**
 * webkit_dom_document_fragment_get_last_element_child:
 * @self: A #WebKitDOMDocumentFragment
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_fragment_get_last_element_child(WebKitDOMDocumentFragment* self);

/**
 * webkit_dom_document_fragment_get_child_element_count:
 * @self: A #WebKitDOMDocumentFragment
 *
 * Returns: A #gulong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gulong
webkit_dom_document_fragment_get_child_element_count(WebKitDOMDocumentFragment* self);

G_END_DECLS

#endif /* WebKitDOMDocumentFragment_h */
