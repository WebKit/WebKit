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
#include "WebKitDOMNamedNodeMap.h"

#include "ConvertToUTF8String.h"
#include "DOMObjectCache.h"
#include "WebKitDOMNamedNodeMapPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include <WebCore/Attr.h>
#include <WebCore/CSSImportRule.h>
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_NAMED_NODE_MAP_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_NAMED_NODE_MAP, WebKitDOMNamedNodeMapPrivate)

typedef struct _WebKitDOMNamedNodeMapPrivate {
    RefPtr<WebCore::NamedNodeMap> coreObject;
} WebKitDOMNamedNodeMapPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMNamedNodeMap* kit(WebCore::NamedNodeMap* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_NAMED_NODE_MAP(ret);

    return wrapNamedNodeMap(obj);
}

WebCore::NamedNodeMap* core(WebKitDOMNamedNodeMap* request)
{
    return request ? static_cast<WebCore::NamedNodeMap*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMNamedNodeMap* wrapNamedNodeMap(WebCore::NamedNodeMap* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_NAMED_NODE_MAP(g_object_new(WEBKIT_DOM_TYPE_NAMED_NODE_MAP, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMNamedNodeMap, webkit_dom_named_node_map, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_NAMED_NODE_MAP_PROP_0,
    DOM_NAMED_NODE_MAP_PROP_LENGTH,
};

static void webkit_dom_named_node_map_finalize(GObject* object)
{
    WebKitDOMNamedNodeMapPrivate* priv = WEBKIT_DOM_NAMED_NODE_MAP_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMNamedNodeMapPrivate();
    G_OBJECT_CLASS(webkit_dom_named_node_map_parent_class)->finalize(object);
}

static void webkit_dom_named_node_map_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMNamedNodeMap* self = WEBKIT_DOM_NAMED_NODE_MAP(object);

    switch (propertyId) {
    case DOM_NAMED_NODE_MAP_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_named_node_map_get_length(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_named_node_map_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_named_node_map_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMNamedNodeMapPrivate* priv = WEBKIT_DOM_NAMED_NODE_MAP_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::NamedNodeMap*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_named_node_map_class_init(WebKitDOMNamedNodeMapClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMNamedNodeMapPrivate));
    gobjectClass->constructor = webkit_dom_named_node_map_constructor;
    gobjectClass->finalize = webkit_dom_named_node_map_finalize;
    gobjectClass->get_property = webkit_dom_named_node_map_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_NAMED_NODE_MAP_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "NamedNodeMap:length",
            "read-only gulong NamedNodeMap:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_named_node_map_init(WebKitDOMNamedNodeMap* request)
{
    WebKitDOMNamedNodeMapPrivate* priv = WEBKIT_DOM_NAMED_NODE_MAP_GET_PRIVATE(request);
    new (priv) WebKitDOMNamedNodeMapPrivate();
}

WebKitDOMNode* webkit_dom_named_node_map_get_named_item(WebKitDOMNamedNodeMap* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->getNamedItem(convertedName));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_named_node_map_set_named_item(WebKitDOMNamedNodeMap* self, WebKitDOMNode* node, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(node), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    WebCore::Node* convertedNode = WebKit::core(node);
    if (!is<WebCore::Attr>(*convertedNode)) {
        auto description = WebCore::DOMException::description(WebCore::TypeError);
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    auto result = item->setNamedItem(downcast<WebCore::Attr>(*convertedNode));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().get());
}

WebKitDOMNode* webkit_dom_named_node_map_remove_named_item(WebKitDOMNamedNodeMap* self, const gchar* name, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    g_return_val_if_fail(name, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    auto result = item->removeNamedItem(convertedName);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMNode* webkit_dom_named_node_map_item(WebKitDOMNamedNodeMap* self, gulong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->item(index));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_named_node_map_get_named_item_ns(WebKitDOMNamedNodeMap* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    g_return_val_if_fail(namespaceURI, 0);
    g_return_val_if_fail(localName, 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->getNamedItemNS(convertedNamespaceURI, convertedLocalName));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_named_node_map_set_named_item_ns(WebKitDOMNamedNodeMap* self, WebKitDOMNode* node, GError** error)
{
    return webkit_dom_named_node_map_set_named_item(self, node, error);
}

WebKitDOMNode* webkit_dom_named_node_map_remove_named_item_ns(WebKitDOMNamedNodeMap* self, const gchar* namespaceURI, const gchar* localName, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    g_return_val_if_fail(namespaceURI, 0);
    g_return_val_if_fail(localName, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    auto result = item->removeNamedItemNS(convertedNamespaceURI, convertedLocalName);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

gulong webkit_dom_named_node_map_get_length(WebKitDOMNamedNodeMap* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NAMED_NODE_MAP(self), 0);
    WebCore::NamedNodeMap* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
