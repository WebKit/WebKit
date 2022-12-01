/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMDocument_h
#define WebKitDOMDocument_h

#include <glib-object.h>
#include <wpe/WebKitDOMNode.h>
#include <wpe/WebKitDOMDefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_DOCUMENT            (webkit_dom_document_get_type())
#define WEBKIT_DOM_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_DOCUMENT, WebKitDOMDocument))
#define WEBKIT_DOM_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_DOCUMENT, WebKitDOMDocumentClass)
#define WEBKIT_DOM_IS_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_DOCUMENT))
#define WEBKIT_DOM_IS_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_DOCUMENT))
#define WEBKIT_DOM_DOCUMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_DOCUMENT, WebKitDOMDocumentClass))

typedef struct _WebKitDOMDocument WebKitDOMDocument;
typedef struct _WebKitDOMDocumentClass WebKitDOMDocumentClass;

struct _WebKitDOMDocument {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMDocumentClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_document_get_type(void);

G_END_DECLS

#endif /* WebKitDOMDocument_h */
