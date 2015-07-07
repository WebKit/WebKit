/*
 * Copyright (C) 2014 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDOMXPathNSResolver.h"

#include "DOMObjectCache.h"
#include "GObjectXPathNSResolver.h"
#include "JSMainThreadExecState.h"
#include "WebKitDOMObject.h"
#include "WebKitDOMXPathNSResolverPrivate.h"
#include "gobject/ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

typedef WebKitDOMXPathNSResolverIface WebKitDOMXPathNSResolverInterface;

G_DEFINE_INTERFACE(WebKitDOMXPathNSResolver, webkit_dom_xpath_ns_resolver, G_TYPE_OBJECT)

static void webkit_dom_xpath_ns_resolver_default_init(WebKitDOMXPathNSResolverIface*)
{
}

char* webkit_dom_xpath_ns_resolver_lookup_namespace_uri(WebKitDOMXPathNSResolver* resolver, const char* prefix)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_NS_RESOLVER(resolver), nullptr);
    g_return_val_if_fail(prefix, nullptr);

    return WEBKIT_DOM_XPATH_NS_RESOLVER_GET_IFACE(resolver)->lookup_namespace_uri(resolver, prefix);
}

// WebKitDOMNativeXPathNSResolver.
struct _WebKitDOMNativeXPathNSResolver {
    WebKitDOMObject parent;
};

struct _WebKitDOMNativeXPathNSResolverClass {
    WebKitDOMObjectClass parentClass;
};

typedef struct _WebKitDOMNativeXPathNSResolverPrivate {
    RefPtr<WebCore::XPathNSResolver> coreObject;
} WebKitDOMNativeXPathNSResolverPrivate;

#define WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER, WebKitDOMNativeXPathNSResolverPrivate)

static void webkitDOMXPathNSResolverIfaceInit(WebKitDOMXPathNSResolverIface*);

G_DEFINE_TYPE_WITH_CODE(WebKitDOMNativeXPathNSResolver, webkit_dom_native_xpath_ns_resolver, WEBKIT_DOM_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER, webkitDOMXPathNSResolverIfaceInit))

static void webkitDOMNativeXPathNSResolverFinalize(GObject* object)
{
    WebKitDOMNativeXPathNSResolverPrivate* priv = WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER_GET_PRIVATE(object);
    priv->~WebKitDOMNativeXPathNSResolverPrivate();
    G_OBJECT_CLASS(webkit_dom_native_xpath_ns_resolver_parent_class)->finalize(object);
}

static GObject* webkitDOMNativeXPathNSResolverConstructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_native_xpath_ns_resolver_parent_class)->constructor(type, constructPropertiesCount, constructProperties);
    WebKitDOMNativeXPathNSResolverPrivate* priv = WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::XPathNSResolver*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);
    return object;
}

static void webkit_dom_native_xpath_ns_resolver_init(WebKitDOMNativeXPathNSResolver* resolver)
{
    WebKitDOMNativeXPathNSResolverPrivate* priv = WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER_GET_PRIVATE(resolver);
    new (priv) WebKitDOMNativeXPathNSResolverPrivate();
}

static void webkit_dom_native_xpath_ns_resolver_class_init(WebKitDOMNativeXPathNSResolverClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMNativeXPathNSResolverPrivate));
    gobjectClass->constructor = webkitDOMNativeXPathNSResolverConstructor;
    gobjectClass->finalize = webkitDOMNativeXPathNSResolverFinalize;
}

static char* webkitDOMNativeXPathNSResolverLookupNamespaceURI(WebKitDOMXPathNSResolver* resolver, const char* prefix)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_NATIVE_XPATH_NS_RESOLVER(resolver), nullptr);

    return convertToUTF8String(WebKit::core(resolver)->lookupNamespaceURI(WTF::String::fromUTF8(prefix)));
}

static void webkitDOMXPathNSResolverIfaceInit(WebKitDOMXPathNSResolverIface* iface)
{
    iface->lookup_namespace_uri = webkitDOMNativeXPathNSResolverLookupNamespaceURI;
}

namespace WebKit {

PassRefPtr<WebCore::XPathNSResolver> core(WebKitDOMXPathNSResolver* xPathNSResolver)
{
    if (!xPathNSResolver)
        return nullptr;

    RefPtr<WebCore::XPathNSResolver> coreResolver;
    if (WEBKIT_DOM_IS_NATIVE_XPATH_NS_RESOLVER(xPathNSResolver))
        coreResolver = core(WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER(xPathNSResolver));
    else
        coreResolver = WebCore::GObjectXPathNSResolver::create(xPathNSResolver);
    return coreResolver.release();
}

WebKitDOMXPathNSResolver* kit(WebCore::XPathNSResolver* coreXPathNSResolver)
{
    if (!coreXPathNSResolver)
        return nullptr;

    if (gpointer ret = DOMObjectCache::get(coreXPathNSResolver))
        return WEBKIT_DOM_XPATH_NS_RESOLVER(ret);

    return WEBKIT_DOM_XPATH_NS_RESOLVER(g_object_new(WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER, "core-object", coreXPathNSResolver, nullptr));
}

WebCore::XPathNSResolver* core(WebKitDOMNativeXPathNSResolver* xPathNSResolver)
{
    return xPathNSResolver ? static_cast<WebCore::XPathNSResolver*>(WEBKIT_DOM_OBJECT(xPathNSResolver)->coreObject) : nullptr;
}

} // namespace WebKit
