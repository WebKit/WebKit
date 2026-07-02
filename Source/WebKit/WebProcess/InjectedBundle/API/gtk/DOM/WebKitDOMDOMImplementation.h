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

#ifndef WebKitDOMDOMImplementation_h
#define WebKitDOMDOMImplementation_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION            (webkit_dom_dom_implementation_get_type())
#define WEBKIT_DOM_DOM_IMPLEMENTATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION, WebKitDOMDOMImplementation))
#define WEBKIT_DOM_DOM_IMPLEMENTATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION, WebKitDOMDOMImplementationClass)
#define WEBKIT_DOM_IS_DOM_IMPLEMENTATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION))
#define WEBKIT_DOM_IS_DOM_IMPLEMENTATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION))
#define WEBKIT_DOM_DOM_IMPLEMENTATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION, WebKitDOMDOMImplementationClass))

struct _WebKitDOMDOMImplementation {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMDOMImplementationClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_dom_implementation_get_type(void);

/**
 * webkit_dom_dom_implementation_has_feature:
 * @self: A #WebKitDOMDOMImplementation
 * @feature: A #gchar
 * @version: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_implementation_has_feature(WebKitDOMDOMImplementation* self, const gchar* feature, const gchar* version);

/**
 * webkit_dom_dom_implementation_create_document_type:
 * @self: A #WebKitDOMDOMImplementation
 * @qualifiedName: A #gchar
 * @publicId: A #gchar
 * @systemId: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMDocumentType
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocumentType*
webkit_dom_dom_implementation_create_document_type(WebKitDOMDOMImplementation* self, const gchar* qualifiedName, const gchar* publicId, const gchar* systemId, GError** error);

/**
 * webkit_dom_dom_implementation_create_document:
 * @self: A #WebKitDOMDOMImplementation
 * @namespaceURI: (allow-none): A #gchar
 * @qualifiedName: A #gchar
 * @doctype: (allow-none): A #WebKitDOMDocumentType
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMDocument
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocument*
webkit_dom_dom_implementation_create_document(WebKitDOMDOMImplementation* self, const gchar* namespaceURI, const gchar* qualifiedName, WebKitDOMDocumentType* doctype, GError** error);

/**
 * webkit_dom_dom_implementation_create_css_style_sheet:
 * @self: A #WebKitDOMDOMImplementation
 * @title: A #gchar
 * @media: A #gchar
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMCSSStyleSheet
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSStyleSheet*
webkit_dom_dom_implementation_create_css_style_sheet(WebKitDOMDOMImplementation* self, const gchar* title, const gchar* media, GError** error);

/**
 * webkit_dom_dom_implementation_create_html_document:
 * @self: A #WebKitDOMDOMImplementation
 * @title: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMHTMLDocument
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLDocument*
webkit_dom_dom_implementation_create_html_document(WebKitDOMDOMImplementation* self, const gchar* title);

G_END_DECLS

#endif /* WebKitDOMDOMImplementation_h */
