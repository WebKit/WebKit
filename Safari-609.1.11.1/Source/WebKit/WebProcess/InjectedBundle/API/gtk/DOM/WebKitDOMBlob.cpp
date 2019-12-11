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
#include "WebKitDOMBlob.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMBlobPrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_BLOB_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_BLOB, WebKitDOMBlobPrivate)

typedef struct _WebKitDOMBlobPrivate {
    RefPtr<WebCore::Blob> coreObject;
} WebKitDOMBlobPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMBlob* kit(WebCore::Blob* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_BLOB(ret);

    return wrap(obj);
}

WebCore::Blob* core(WebKitDOMBlob* request)
{
    return request ? static_cast<WebCore::Blob*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMBlob* wrapBlob(WebCore::Blob* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_BLOB(g_object_new(WEBKIT_DOM_TYPE_BLOB, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMBlob, webkit_dom_blob, WEBKIT_DOM_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_SIZE,
};

static void webkit_dom_blob_finalize(GObject* object)
{
    WebKitDOMBlobPrivate* priv = WEBKIT_DOM_BLOB_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMBlobPrivate();
    G_OBJECT_CLASS(webkit_dom_blob_parent_class)->finalize(object);
}

static void webkit_dom_blob_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMBlob* self = WEBKIT_DOM_BLOB(object);

    switch (propertyId) {
    case PROP_SIZE:
        g_value_set_uint64(value, webkit_dom_blob_get_size(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_blob_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_blob_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMBlobPrivate* priv = WEBKIT_DOM_BLOB_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::Blob*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_blob_class_init(WebKitDOMBlobClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMBlobPrivate));
    gobjectClass->constructor = webkit_dom_blob_constructor;
    gobjectClass->finalize = webkit_dom_blob_finalize;
    gobjectClass->get_property = webkit_dom_blob_get_property;

    g_object_class_install_property(
        gobjectClass,
        PROP_SIZE,
        g_param_spec_uint64(
            "size",
            "Blob:size",
            "read-only guint64 Blob:size",
            0, G_MAXUINT64, 0,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_blob_init(WebKitDOMBlob* request)
{
    WebKitDOMBlobPrivate* priv = WEBKIT_DOM_BLOB_GET_PRIVATE(request);
    new (priv) WebKitDOMBlobPrivate();
}

guint64 webkit_dom_blob_get_size(WebKitDOMBlob* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_BLOB(self), 0);
    WebCore::Blob* item = WebKit::core(self);
    guint64 result = item->size();
    return result;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
