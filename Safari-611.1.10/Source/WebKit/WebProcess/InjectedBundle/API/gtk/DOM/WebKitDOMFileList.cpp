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
#include "WebKitDOMFileList.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMFileListPrivate.h"
#include "WebKitDOMFilePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_FILE_LIST_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_FILE_LIST, WebKitDOMFileListPrivate)

typedef struct _WebKitDOMFileListPrivate {
    RefPtr<WebCore::FileList> coreObject;
} WebKitDOMFileListPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMFileList* kit(WebCore::FileList* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_FILE_LIST(ret);

    return wrapFileList(obj);
}

WebCore::FileList* core(WebKitDOMFileList* request)
{
    return request ? static_cast<WebCore::FileList*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMFileList* wrapFileList(WebCore::FileList* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_FILE_LIST(g_object_new(WEBKIT_DOM_TYPE_FILE_LIST, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMFileList, webkit_dom_file_list, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_FILE_LIST_PROP_0,
    DOM_FILE_LIST_PROP_LENGTH,
};

static void webkit_dom_file_list_finalize(GObject* object)
{
    WebKitDOMFileListPrivate* priv = WEBKIT_DOM_FILE_LIST_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMFileListPrivate();
    G_OBJECT_CLASS(webkit_dom_file_list_parent_class)->finalize(object);
}

static void webkit_dom_file_list_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMFileList* self = WEBKIT_DOM_FILE_LIST(object);

    switch (propertyId) {
    case DOM_FILE_LIST_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_file_list_get_length(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_file_list_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_file_list_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMFileListPrivate* priv = WEBKIT_DOM_FILE_LIST_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::FileList*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_file_list_class_init(WebKitDOMFileListClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMFileListPrivate));
    gobjectClass->constructor = webkit_dom_file_list_constructor;
    gobjectClass->finalize = webkit_dom_file_list_finalize;
    gobjectClass->get_property = webkit_dom_file_list_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_FILE_LIST_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "FileList:length",
            "read-only gulong FileList:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_file_list_init(WebKitDOMFileList* request)
{
    WebKitDOMFileListPrivate* priv = WEBKIT_DOM_FILE_LIST_GET_PRIVATE(request);
    new (priv) WebKitDOMFileListPrivate();
}

WebKitDOMFile* webkit_dom_file_list_item(WebKitDOMFileList* self, gulong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_FILE_LIST(self), 0);
    WebCore::FileList* item = WebKit::core(self);
    RefPtr<WebCore::File> gobjectResult = WTF::getPtr(item->item(index));
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_file_list_get_length(WebKitDOMFileList* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_FILE_LIST(self), 0);
    WebCore::FileList* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
