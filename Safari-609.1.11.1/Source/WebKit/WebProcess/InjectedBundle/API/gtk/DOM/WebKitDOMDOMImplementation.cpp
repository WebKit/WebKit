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
#include "WebKitDOMDOMImplementation.h"

#include "ConvertToUTF8String.h"
#include "DOMObjectCache.h"
#include "WebKitDOMCSSStyleSheetPrivate.h"
#include "WebKitDOMDOMImplementationPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMDocumentTypePrivate.h"
#include "WebKitDOMHTMLDocumentPrivate.h"
#include "WebKitDOMPrivate.h"
#include <WebCore/CSSImportRule.h>
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include <WebCore/SVGTests.h>
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_DOM_IMPLEMENTATION_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION, WebKitDOMDOMImplementationPrivate)

typedef struct _WebKitDOMDOMImplementationPrivate {
    RefPtr<WebCore::DOMImplementation> coreObject;
} WebKitDOMDOMImplementationPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMDOMImplementation* kit(WebCore::DOMImplementation* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_DOM_IMPLEMENTATION(ret);

    return wrapDOMImplementation(obj);
}

WebCore::DOMImplementation* core(WebKitDOMDOMImplementation* request)
{
    return request ? static_cast<WebCore::DOMImplementation*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMDOMImplementation* wrapDOMImplementation(WebCore::DOMImplementation* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_DOM_IMPLEMENTATION(g_object_new(WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMDOMImplementation, webkit_dom_dom_implementation, WEBKIT_DOM_TYPE_OBJECT)

static void webkit_dom_dom_implementation_finalize(GObject* object)
{
    WebKitDOMDOMImplementationPrivate* priv = WEBKIT_DOM_DOM_IMPLEMENTATION_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMDOMImplementationPrivate();
    G_OBJECT_CLASS(webkit_dom_dom_implementation_parent_class)->finalize(object);
}

static GObject* webkit_dom_dom_implementation_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_dom_implementation_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMDOMImplementationPrivate* priv = WEBKIT_DOM_DOM_IMPLEMENTATION_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::DOMImplementation*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_dom_implementation_class_init(WebKitDOMDOMImplementationClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMDOMImplementationPrivate));
    gobjectClass->constructor = webkit_dom_dom_implementation_constructor;
    gobjectClass->finalize = webkit_dom_dom_implementation_finalize;
}

static void webkit_dom_dom_implementation_init(WebKitDOMDOMImplementation* request)
{
    WebKitDOMDOMImplementationPrivate* priv = WEBKIT_DOM_DOM_IMPLEMENTATION_GET_PRIVATE(request);
    new (priv) WebKitDOMDOMImplementationPrivate();
}

gboolean webkit_dom_dom_implementation_has_feature(WebKitDOMDOMImplementation* self, const gchar* feature, const gchar* version)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_IMPLEMENTATION(self), FALSE);
    g_return_val_if_fail(feature, FALSE);
    g_return_val_if_fail(version, FALSE);
    WTF::String convertedFeature = WTF::String::fromUTF8(feature);
    WTF::String convertedVersion = WTF::String::fromUTF8(version);
    return WebCore::SVGTests::hasFeatureForLegacyBindings(convertedFeature, convertedVersion);
}

WebKitDOMDocumentType* webkit_dom_dom_implementation_create_document_type(WebKitDOMDOMImplementation* self, const gchar* qualifiedName, const gchar* publicId, const gchar* systemId, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_IMPLEMENTATION(self), 0);
    g_return_val_if_fail(qualifiedName, 0);
    g_return_val_if_fail(publicId, 0);
    g_return_val_if_fail(systemId, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::DOMImplementation* item = WebKit::core(self);
    WTF::String convertedQualifiedName = WTF::String::fromUTF8(qualifiedName);
    WTF::String convertedPublicId = WTF::String::fromUTF8(publicId);
    WTF::String convertedSystemId = WTF::String::fromUTF8(systemId);
    auto result = item->createDocumentType(convertedQualifiedName, convertedPublicId, convertedSystemId);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMDocument* webkit_dom_dom_implementation_create_document(WebKitDOMDOMImplementation* self, const gchar* namespaceURI, const gchar* qualifiedName, WebKitDOMDocumentType* doctype, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_IMPLEMENTATION(self), 0);
    g_return_val_if_fail(qualifiedName, 0);
    g_return_val_if_fail(!doctype || WEBKIT_DOM_IS_DOCUMENT_TYPE(doctype), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::DOMImplementation* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedQualifiedName = WTF::String::fromUTF8(qualifiedName);
    WebCore::DocumentType* convertedDoctype = WebKit::core(doctype);
    auto result = item->createDocument(convertedNamespaceURI, convertedQualifiedName, convertedDoctype);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMCSSStyleSheet* webkit_dom_dom_implementation_create_css_style_sheet(WebKitDOMDOMImplementation* self, const gchar* title, const gchar* media, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_IMPLEMENTATION(self), 0);
    g_return_val_if_fail(title, 0);
    g_return_val_if_fail(media, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::DOMImplementation* item = WebKit::core(self);
    WTF::String convertedTitle = WTF::String::fromUTF8(title);
    WTF::String convertedMedia = WTF::String::fromUTF8(media);
    RefPtr<WebCore::CSSStyleSheet> gobjectResult = WTF::getPtr(item->createCSSStyleSheet(convertedTitle, convertedMedia));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLDocument* webkit_dom_dom_implementation_create_html_document(WebKitDOMDOMImplementation* self, const gchar* title)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_IMPLEMENTATION(self), 0);
    g_return_val_if_fail(title, 0);
    WebCore::DOMImplementation* item = WebKit::core(self);
    WTF::String convertedTitle = WTF::String::fromUTF8(title);
    RefPtr<WebCore::HTMLDocument> gobjectResult = WTF::getPtr(item->createHTMLDocument(convertedTitle));
    return WebKit::kit(gobjectResult.get());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
