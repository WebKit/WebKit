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

#include "config.h"
#include "WebKitDOMTreeWalker.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMNodeFilterPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMTreeWalkerPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_TREE_WALKER_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_TREE_WALKER, WebKitDOMTreeWalkerPrivate)

typedef struct _WebKitDOMTreeWalkerPrivate {
    RefPtr<WebCore::TreeWalker> coreObject;
} WebKitDOMTreeWalkerPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMTreeWalker* kit(WebCore::TreeWalker* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_TREE_WALKER(ret);

    return wrapTreeWalker(obj);
}

WebCore::TreeWalker* core(WebKitDOMTreeWalker* request)
{
    return request ? static_cast<WebCore::TreeWalker*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMTreeWalker* wrapTreeWalker(WebCore::TreeWalker* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_TREE_WALKER(g_object_new(WEBKIT_DOM_TYPE_TREE_WALKER, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMTreeWalker, webkit_dom_tree_walker, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_TREE_WALKER_PROP_0,
    DOM_TREE_WALKER_PROP_ROOT,
    DOM_TREE_WALKER_PROP_WHAT_TO_SHOW,
    DOM_TREE_WALKER_PROP_FILTER,
    DOM_TREE_WALKER_PROP_CURRENT_NODE,
};

static void webkit_dom_tree_walker_finalize(GObject* object)
{
    WebKitDOMTreeWalkerPrivate* priv = WEBKIT_DOM_TREE_WALKER_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMTreeWalkerPrivate();
    G_OBJECT_CLASS(webkit_dom_tree_walker_parent_class)->finalize(object);
}

static void webkit_dom_tree_walker_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMTreeWalker* self = WEBKIT_DOM_TREE_WALKER(object);

    switch (propertyId) {
    case DOM_TREE_WALKER_PROP_ROOT:
        g_value_set_object(value, webkit_dom_tree_walker_get_root(self));
        break;
    case DOM_TREE_WALKER_PROP_WHAT_TO_SHOW:
        g_value_set_ulong(value, webkit_dom_tree_walker_get_what_to_show(self));
        break;
    case DOM_TREE_WALKER_PROP_FILTER:
        g_value_set_object(value, webkit_dom_tree_walker_get_filter(self));
        break;
    case DOM_TREE_WALKER_PROP_CURRENT_NODE:
        g_value_set_object(value, webkit_dom_tree_walker_get_current_node(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_tree_walker_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_tree_walker_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMTreeWalkerPrivate* priv = WEBKIT_DOM_TREE_WALKER_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::TreeWalker*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_tree_walker_class_init(WebKitDOMTreeWalkerClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMTreeWalkerPrivate));
    gobjectClass->constructor = webkit_dom_tree_walker_constructor;
    gobjectClass->finalize = webkit_dom_tree_walker_finalize;
    gobjectClass->get_property = webkit_dom_tree_walker_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_TREE_WALKER_PROP_ROOT,
        g_param_spec_object(
            "root",
            "TreeWalker:root",
            "read-only WebKitDOMNode* TreeWalker:root",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_TREE_WALKER_PROP_WHAT_TO_SHOW,
        g_param_spec_ulong(
            "what-to-show",
            "TreeWalker:what-to-show",
            "read-only gulong TreeWalker:what-to-show",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_TREE_WALKER_PROP_FILTER,
        g_param_spec_object(
            "filter",
            "TreeWalker:filter",
            "read-only WebKitDOMNodeFilter* TreeWalker:filter",
            WEBKIT_DOM_TYPE_NODE_FILTER,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_TREE_WALKER_PROP_CURRENT_NODE,
        g_param_spec_object(
            "current-node",
            "TreeWalker:current-node",
            "read-only WebKitDOMNode* TreeWalker:current-node",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_tree_walker_init(WebKitDOMTreeWalker* request)
{
    WebKitDOMTreeWalkerPrivate* priv = WEBKIT_DOM_TREE_WALKER_GET_PRIVATE(request);
    new (priv) WebKitDOMTreeWalkerPrivate();
}

WebKitDOMNode* webkit_dom_tree_walker_parent_node(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->parentNode();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_first_child(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->firstChild();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_last_child(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->lastChild();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_previous_sibling(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->previousSibling();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_next_sibling(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->nextSibling();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_previous_node(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->previousNode();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_next_node(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);

    auto result = item->nextNode();
    if (result.hasException())
        return nullptr;

    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(result.releaseReturnValue());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_get_root(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->root());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_tree_walker_get_what_to_show(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);
    gulong result = item->whatToShow();
    return result;
}

WebKitDOMNodeFilter* webkit_dom_tree_walker_get_filter(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);
    RefPtr<WebCore::NodeFilter> gobjectResult = WTF::getPtr(item->filter());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_tree_walker_get_current_node(WebKitDOMTreeWalker* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self), 0);
    WebCore::TreeWalker* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->currentNode());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_tree_walker_set_current_node(WebKitDOMTreeWalker* self, WebKitDOMNode* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_TREE_WALKER(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(value));
    UNUSED_PARAM(error);
    WebCore::TreeWalker* item = WebKit::core(self);
    WebCore::Node* convertedValue = WebKit::core(value);
    item->setCurrentNode(*convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
