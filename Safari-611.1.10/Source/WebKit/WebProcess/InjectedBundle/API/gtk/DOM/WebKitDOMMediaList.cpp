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
#include "WebKitDOMMediaList.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMMediaListPrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_MEDIA_LIST_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_MEDIA_LIST, WebKitDOMMediaListPrivate)

typedef struct _WebKitDOMMediaListPrivate {
    RefPtr<WebCore::MediaList> coreObject;
} WebKitDOMMediaListPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMMediaList* kit(WebCore::MediaList* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_MEDIA_LIST(ret);

    return wrapMediaList(obj);
}

WebCore::MediaList* core(WebKitDOMMediaList* request)
{
    return request ? static_cast<WebCore::MediaList*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMMediaList* wrapMediaList(WebCore::MediaList* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_MEDIA_LIST(g_object_new(WEBKIT_DOM_TYPE_MEDIA_LIST, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMMediaList, webkit_dom_media_list, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_MEDIA_LIST_PROP_0,
    DOM_MEDIA_LIST_PROP_MEDIA_TEXT,
    DOM_MEDIA_LIST_PROP_LENGTH,
};

static void webkit_dom_media_list_finalize(GObject* object)
{
    WebKitDOMMediaListPrivate* priv = WEBKIT_DOM_MEDIA_LIST_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMMediaListPrivate();
    G_OBJECT_CLASS(webkit_dom_media_list_parent_class)->finalize(object);
}

static void webkit_dom_media_list_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMMediaList* self = WEBKIT_DOM_MEDIA_LIST(object);

    switch (propertyId) {
    case DOM_MEDIA_LIST_PROP_MEDIA_TEXT:
        webkit_dom_media_list_set_media_text(self, g_value_get_string(value), nullptr);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_media_list_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMMediaList* self = WEBKIT_DOM_MEDIA_LIST(object);

    switch (propertyId) {
    case DOM_MEDIA_LIST_PROP_MEDIA_TEXT:
        g_value_take_string(value, webkit_dom_media_list_get_media_text(self));
        break;
    case DOM_MEDIA_LIST_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_media_list_get_length(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_media_list_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_media_list_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMMediaListPrivate* priv = WEBKIT_DOM_MEDIA_LIST_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::MediaList*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_media_list_class_init(WebKitDOMMediaListClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMMediaListPrivate));
    gobjectClass->constructor = webkit_dom_media_list_constructor;
    gobjectClass->finalize = webkit_dom_media_list_finalize;
    gobjectClass->set_property = webkit_dom_media_list_set_property;
    gobjectClass->get_property = webkit_dom_media_list_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_MEDIA_LIST_PROP_MEDIA_TEXT,
        g_param_spec_string(
            "media-text",
            "MediaList:media-text",
            "read-write gchar* MediaList:media-text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MEDIA_LIST_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "MediaList:length",
            "read-only gulong MediaList:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_media_list_init(WebKitDOMMediaList* request)
{
    WebKitDOMMediaListPrivate* priv = WEBKIT_DOM_MEDIA_LIST_GET_PRIVATE(request);
    new (priv) WebKitDOMMediaListPrivate();
}

gchar* webkit_dom_media_list_item(WebKitDOMMediaList* self, gulong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MEDIA_LIST(self), 0);
    WebCore::MediaList* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->item(index));
    return result;
}

void webkit_dom_media_list_delete_medium(WebKitDOMMediaList* self, const gchar* oldMedium, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_MEDIA_LIST(self));
    g_return_if_fail(oldMedium);
    g_return_if_fail(!error || !*error);
    WebCore::MediaList* item = WebKit::core(self);
    WTF::String convertedOldMedium = WTF::String::fromUTF8(oldMedium);
    auto result = item->deleteMedium(convertedOldMedium);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_media_list_append_medium(WebKitDOMMediaList* self, const gchar* newMedium, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_MEDIA_LIST(self));
    g_return_if_fail(newMedium);
    g_return_if_fail(!error || !*error);
    WebCore::MediaList* item = WebKit::core(self);
    WTF::String convertedNewMedium = WTF::String::fromUTF8(newMedium);
    item->appendMedium(convertedNewMedium);
}

gchar* webkit_dom_media_list_get_media_text(WebKitDOMMediaList* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MEDIA_LIST(self), 0);
    WebCore::MediaList* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->mediaText());
    return result;
}

void webkit_dom_media_list_set_media_text(WebKitDOMMediaList* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_MEDIA_LIST(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::MediaList* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setMediaText(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gulong webkit_dom_media_list_get_length(WebKitDOMMediaList* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MEDIA_LIST(self), 0);
    WebCore::MediaList* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
