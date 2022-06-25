/*
 *  Copyright (C) 2017 Aidan Holm <aidanholm@gmail.com>
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
#include "WebKitDOMClientRectList.h"

#include "ConvertToUTF8String.h"
#include "DOMObjectCache.h"
#include "WebKitDOMClientRectListPrivate.h"
#include "WebKitDOMClientRectPrivate.h"
#include "WebKitDOMPrivate.h"
#include <WebCore/CSSImportRule.h>
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_CLIENT_RECT_LIST_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_CLIENT_RECT_LIST, WebKitDOMClientRectListPrivate)

typedef struct _WebKitDOMClientRectListPrivate {
    RefPtr<WebCore::DOMRectList> coreObject;
} WebKitDOMClientRectListPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMClientRectList* kit(WebCore::DOMRectList* obj)
{
    if (!obj)
        return nullptr;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_CLIENT_RECT_LIST(ret);

    return wrapDOMRectList(obj);
}

WebCore::DOMRectList* core(WebKitDOMClientRectList* request)
{
    return request ? static_cast<WebCore::DOMRectList*>(WEBKIT_DOM_OBJECT(request)->coreObject) : nullptr;
}

WebKitDOMClientRectList* wrapDOMRectList(WebCore::DOMRectList* coreObject)
{
    return WEBKIT_DOM_CLIENT_RECT_LIST(g_object_new(WEBKIT_DOM_TYPE_CLIENT_RECT_LIST, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMClientRectList, webkit_dom_client_rect_list, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_CLIENT_RECT_LIST_PROP_0,
    DOM_CLIENT_RECT_LIST_PROP_LENGTH,
};

static void webkit_dom_client_rect_list_finalize(GObject* object)
{
    WebKitDOMClientRectListPrivate* priv = WEBKIT_DOM_CLIENT_RECT_LIST_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMClientRectListPrivate();
    G_OBJECT_CLASS(webkit_dom_client_rect_list_parent_class)->finalize(object);
}

static void webkit_dom_client_rect_list_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMClientRectList* self = WEBKIT_DOM_CLIENT_RECT_LIST(object);

    switch (propertyId) {
    case DOM_CLIENT_RECT_LIST_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_client_rect_list_get_length(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_client_rect_list_constructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_dom_client_rect_list_parent_class)->constructed(object);

    WebKitDOMClientRectListPrivate* priv = WEBKIT_DOM_CLIENT_RECT_LIST_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::DOMRectList*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);
}

static void webkit_dom_client_rect_list_class_init(WebKitDOMClientRectListClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMClientRectListPrivate));
    gobjectClass->constructed = webkit_dom_client_rect_list_constructed;
    gobjectClass->finalize = webkit_dom_client_rect_list_finalize;
    gobjectClass->get_property = webkit_dom_client_rect_list_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_LIST_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "ClientRectList:length",
            "read-only gulong ClientRectList:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_client_rect_list_init(WebKitDOMClientRectList* request)
{
    WebKitDOMClientRectListPrivate* priv = WEBKIT_DOM_CLIENT_RECT_LIST_GET_PRIVATE(request);
    new (priv) WebKitDOMClientRectListPrivate();
}

WebKitDOMClientRect* webkit_dom_client_rect_list_item(WebKitDOMClientRectList* self, gulong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT_LIST(self), nullptr);
    auto* list = WebKit::core(self);
    RefPtr<WebCore::DOMRect> gobjectResult = WTF::getPtr(list->item(index));
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_client_rect_list_get_length(WebKitDOMClientRectList* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT_LIST(self), 0);
    return WebKit::core(self)->length();
}
G_GNUC_END_IGNORE_DEPRECATIONS;
