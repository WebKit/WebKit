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
#include "WebKitDOMStyleSheet.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMMediaListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMStyleSheetPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_STYLE_SHEET_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_STYLE_SHEET, WebKitDOMStyleSheetPrivate)

typedef struct _WebKitDOMStyleSheetPrivate {
    RefPtr<WebCore::StyleSheet> coreObject;
} WebKitDOMStyleSheetPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMStyleSheet* kit(WebCore::StyleSheet* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_STYLE_SHEET(ret);

    return wrap(obj);
}

WebCore::StyleSheet* core(WebKitDOMStyleSheet* request)
{
    return request ? static_cast<WebCore::StyleSheet*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMStyleSheet* wrapStyleSheet(WebCore::StyleSheet* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_STYLE_SHEET(g_object_new(WEBKIT_DOM_TYPE_STYLE_SHEET, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMStyleSheet, webkit_dom_style_sheet, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_STYLE_SHEET_PROP_0,
    DOM_STYLE_SHEET_PROP_TYPE,
    DOM_STYLE_SHEET_PROP_DISABLED,
    DOM_STYLE_SHEET_PROP_OWNER_NODE,
    DOM_STYLE_SHEET_PROP_PARENT_STYLE_SHEET,
    DOM_STYLE_SHEET_PROP_HREF,
    DOM_STYLE_SHEET_PROP_TITLE,
    DOM_STYLE_SHEET_PROP_MEDIA,
};

static void webkit_dom_style_sheet_finalize(GObject* object)
{
    WebKitDOMStyleSheetPrivate* priv = WEBKIT_DOM_STYLE_SHEET_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMStyleSheetPrivate();
    G_OBJECT_CLASS(webkit_dom_style_sheet_parent_class)->finalize(object);
}

static void webkit_dom_style_sheet_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMStyleSheet* self = WEBKIT_DOM_STYLE_SHEET(object);

    switch (propertyId) {
    case DOM_STYLE_SHEET_PROP_DISABLED:
        webkit_dom_style_sheet_set_disabled(self, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_style_sheet_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMStyleSheet* self = WEBKIT_DOM_STYLE_SHEET(object);

    switch (propertyId) {
    case DOM_STYLE_SHEET_PROP_TYPE:
        g_value_take_string(value, webkit_dom_style_sheet_get_content_type(self));
        break;
    case DOM_STYLE_SHEET_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_style_sheet_get_disabled(self));
        break;
    case DOM_STYLE_SHEET_PROP_OWNER_NODE:
        g_value_set_object(value, webkit_dom_style_sheet_get_owner_node(self));
        break;
    case DOM_STYLE_SHEET_PROP_PARENT_STYLE_SHEET:
        g_value_set_object(value, webkit_dom_style_sheet_get_parent_style_sheet(self));
        break;
    case DOM_STYLE_SHEET_PROP_HREF:
        g_value_take_string(value, webkit_dom_style_sheet_get_href(self));
        break;
    case DOM_STYLE_SHEET_PROP_TITLE:
        g_value_take_string(value, webkit_dom_style_sheet_get_title(self));
        break;
    case DOM_STYLE_SHEET_PROP_MEDIA:
        g_value_set_object(value, webkit_dom_style_sheet_get_media(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_style_sheet_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_style_sheet_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMStyleSheetPrivate* priv = WEBKIT_DOM_STYLE_SHEET_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::StyleSheet*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_style_sheet_class_init(WebKitDOMStyleSheetClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMStyleSheetPrivate));
    gobjectClass->constructor = webkit_dom_style_sheet_constructor;
    gobjectClass->finalize = webkit_dom_style_sheet_finalize;
    gobjectClass->set_property = webkit_dom_style_sheet_set_property;
    gobjectClass->get_property = webkit_dom_style_sheet_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_TYPE,
        g_param_spec_string(
            "type",
            "StyleSheet:type",
            "read-only gchar* StyleSheet:type",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "StyleSheet:disabled",
            "read-write gboolean StyleSheet:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_OWNER_NODE,
        g_param_spec_object(
            "owner-node",
            "StyleSheet:owner-node",
            "read-only WebKitDOMNode* StyleSheet:owner-node",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_PARENT_STYLE_SHEET,
        g_param_spec_object(
            "parent-style-sheet",
            "StyleSheet:parent-style-sheet",
            "read-only WebKitDOMStyleSheet* StyleSheet:parent-style-sheet",
            WEBKIT_DOM_TYPE_STYLE_SHEET,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_HREF,
        g_param_spec_string(
            "href",
            "StyleSheet:href",
            "read-only gchar* StyleSheet:href",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_TITLE,
        g_param_spec_string(
            "title",
            "StyleSheet:title",
            "read-only gchar* StyleSheet:title",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_STYLE_SHEET_PROP_MEDIA,
        g_param_spec_object(
            "media",
            "StyleSheet:media",
            "read-only WebKitDOMMediaList* StyleSheet:media",
            WEBKIT_DOM_TYPE_MEDIA_LIST,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_style_sheet_init(WebKitDOMStyleSheet* request)
{
    WebKitDOMStyleSheetPrivate* priv = WEBKIT_DOM_STYLE_SHEET_GET_PRIVATE(request);
    new (priv) WebKitDOMStyleSheetPrivate();
}

gchar* webkit_dom_style_sheet_get_content_type(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), 0);
    WebCore::StyleSheet* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

gboolean webkit_dom_style_sheet_get_disabled(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), FALSE);
    WebCore::StyleSheet* item = WebKit::core(self);
    gboolean result = item->disabled();
    return result;
}

void webkit_dom_style_sheet_set_disabled(WebKitDOMStyleSheet* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self));
    WebCore::StyleSheet* item = WebKit::core(self);
    item->setDisabled(value);
}

WebKitDOMNode* webkit_dom_style_sheet_get_owner_node(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), 0);
    WebCore::StyleSheet* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->ownerNode());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMStyleSheet* webkit_dom_style_sheet_get_parent_style_sheet(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), 0);
    WebCore::StyleSheet* item = WebKit::core(self);
    RefPtr<WebCore::StyleSheet> gobjectResult = WTF::getPtr(item->parentStyleSheet());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_style_sheet_get_href(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), 0);
    WebCore::StyleSheet* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->href());
    return result;
}

gchar* webkit_dom_style_sheet_get_title(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), 0);
    WebCore::StyleSheet* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->title());
    return result;
}

WebKitDOMMediaList* webkit_dom_style_sheet_get_media(WebKitDOMStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_STYLE_SHEET(self), 0);
    WebCore::StyleSheet* item = WebKit::core(self);
    RefPtr<WebCore::MediaList> gobjectResult = WTF::getPtr(item->media());
    return WebKit::kit(gobjectResult.get());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
