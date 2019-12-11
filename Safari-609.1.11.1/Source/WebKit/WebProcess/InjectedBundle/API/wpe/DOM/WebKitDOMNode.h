/*
 * Copyright (C) 2018 Igalia S.L.
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

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMNode_h
#define WebKitDOMNode_h

#include <glib-object.h>
#include <jsc/jsc.h>
#include <wpe/WebKitDOMObject.h>
#include <wpe/WebKitDOMDefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_NODE            (webkit_dom_node_get_type())
#define WEBKIT_DOM_NODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_NODE, WebKitDOMNode))
#define WEBKIT_DOM_NODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_NODE, WebKitDOMNodeClass)
#define WEBKIT_DOM_IS_NODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_NODE))
#define WEBKIT_DOM_IS_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_NODE))
#define WEBKIT_DOM_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_NODE, WebKitDOMNodeClass))

typedef struct _WebKitDOMNode WebKitDOMNode;
typedef struct _WebKitDOMNodeClass WebKitDOMNodeClass;
typedef struct _WebKitDOMNodePrivate WebKitDOMNodePrivate;

struct _WebKitDOMNode {
    WebKitDOMObject parent_instance;

    WebKitDOMNodePrivate *priv;
};

struct _WebKitDOMNodeClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_API GType
webkit_dom_node_get_type     (void);

WEBKIT_API WebKitDOMNode *
webkit_dom_node_for_js_value (JSCValue *value);

G_END_DECLS

#endif /* WebKitDOMNode_h */
