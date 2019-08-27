/*
 *  Copyright (C) 2018 Igalia S.L.
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

#include "config.h"
#include "WebKitDOMNode.h"

#include "DOMObjectCache.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include <WebCore/JSNode.h>
#include <jsc/JSCContextPrivate.h>
#include <jsc/JSCValuePrivate.h>
#include <wtf/RefPtr.h>

#if PLATFORM(GTK)
#include "WebKitDOMEventTarget.h"
#endif

#define WEBKIT_DOM_NODE_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_NODE, WebKitDOMNodePrivate)

typedef struct _WebKitDOMNodePrivate {
    ~_WebKitDOMNodePrivate()
    {
        WebKit::DOMObjectCache::forget(coreObject.get());
    }

    RefPtr<WebCore::Node> coreObject;
} WebKitDOMNodePrivate;

namespace WebKit {

WebKitDOMNode* kit(WebCore::Node* obj)
{
    if (!obj)
        return nullptr;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_NODE(ret);

    return wrap(obj);
}

WebCore::Node* core(WebKitDOMNode* node)
{
    return node ? webkitDOMNodeGetCoreObject(node) : nullptr;
}

WebKitDOMNode* wrapNode(WebCore::Node* coreObject)
{
    ASSERT(coreObject);
#if PLATFORM(GTK)
    return WEBKIT_DOM_NODE(g_object_new(WEBKIT_DOM_TYPE_NODE, "core-object", coreObject, nullptr));
#else
    auto* node = WEBKIT_DOM_NODE(g_object_new(WEBKIT_DOM_TYPE_NODE, nullptr));
    webkitDOMNodeSetCoreObject(node, coreObject);
    return node;
#endif
}

} // namespace WebKit

#if PLATFORM(GTK)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE(WebKitDOMNode, webkit_dom_node, WEBKIT_DOM_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkitDOMNodeDOMEventTargetInit))
G_GNUC_END_IGNORE_DEPRECATIONS;
#else
WEBKIT_DEFINE_TYPE(WebKitDOMNode, webkit_dom_node, WEBKIT_DOM_TYPE_OBJECT)
#endif

#if PLATFORM(GTK)
static GObject* webkitDOMNodeConstructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_node_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    webkitDOMNodeSetCoreObject(WEBKIT_DOM_NODE(object), static_cast<WebCore::Node*>(WEBKIT_DOM_OBJECT(object)->coreObject));

    return object;
}

static void webkitDOMNodeFinalize(GObject* object)
{
    WebKitDOMNode* node = WEBKIT_DOM_NODE(object);

    WebKitDOMNodePrivate* priv = WEBKIT_DOM_NODE_GET_PRIVATE(node);
    priv->~WebKitDOMNodePrivate();

    G_OBJECT_CLASS(webkit_dom_node_parent_class)->finalize(object);
}

static void webkit_dom_node_init(WebKitDOMNode* node)
{
    WebKitDOMNodePrivate* priv = WEBKIT_DOM_NODE_GET_PRIVATE(node);
    new (priv) WebKitDOMNodePrivate();
}
#endif

static void webkit_dom_node_class_init(WebKitDOMNodeClass* nodeClass)
{
#if PLATFORM(GTK)
    GObjectClass* gobjectClass = G_OBJECT_CLASS(nodeClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMNodePrivate));
    gobjectClass->constructor = webkitDOMNodeConstructor;
    gobjectClass->finalize = webkitDOMNodeFinalize;
    webkitDOMNodeInstallProperties(gobjectClass);
#endif
}

void webkitDOMNodeSetCoreObject(WebKitDOMNode* node, WebCore::Node* coreObject)
{
    WebKitDOMNodePrivate* priv = WEBKIT_DOM_NODE_GET_PRIVATE(node);
    priv->coreObject = coreObject;
    WebKit::DOMObjectCache::put(coreObject, node);
}

WebCore::Node* webkitDOMNodeGetCoreObject(WebKitDOMNode* node)
{
    WebKitDOMNodePrivate* priv = WEBKIT_DOM_NODE_GET_PRIVATE(node);
    return priv->coreObject.get();
}

/**
 * webkit_dom_node_for_js_value:
 * @value: a #JSCValue
 *
 * Get the #WebKitDOMNode for the DOM node referenced by @value.
 *
 * Returns: (transfer none): a #WebKitDOMNode, or %NULL if @value doesn't reference a DOM node.
 *
 * Since: 2.22
 */
WebKitDOMNode* webkit_dom_node_for_js_value(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);
    g_return_val_if_fail(jsc_value_is_object(value), nullptr);

    auto* jsObject = JSValueToObject(jscContextGetJSContext(jsc_value_get_context(value)), jscValueGetJSValue(value), nullptr);
    return jsObject ? WebKit::kit(WebCore::JSNode::toWrapped(toJS(jsObject)->vm(), toJS(jsObject))) : nullptr;
}
