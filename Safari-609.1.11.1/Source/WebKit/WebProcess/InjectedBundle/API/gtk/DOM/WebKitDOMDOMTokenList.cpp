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
#include "WebKitDOMDOMTokenList.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMDOMTokenListPrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_DOM_TOKEN_LIST_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_DOM_TOKEN_LIST, WebKitDOMDOMTokenListPrivate)

typedef struct _WebKitDOMDOMTokenListPrivate {
    RefPtr<WebCore::DOMTokenList> coreObject;
} WebKitDOMDOMTokenListPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMDOMTokenList* kit(WebCore::DOMTokenList* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_DOM_TOKEN_LIST(ret);

    return wrapDOMTokenList(obj);
}

WebCore::DOMTokenList* core(WebKitDOMDOMTokenList* request)
{
    return request ? static_cast<WebCore::DOMTokenList*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMDOMTokenList* wrapDOMTokenList(WebCore::DOMTokenList* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_DOM_TOKEN_LIST(g_object_new(WEBKIT_DOM_TYPE_DOM_TOKEN_LIST, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMDOMTokenList, webkit_dom_dom_token_list, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_DOM_TOKEN_LIST_PROP_0,
    DOM_DOM_TOKEN_LIST_PROP_LENGTH,
    DOM_DOM_TOKEN_LIST_PROP_VALUE,
};

static void webkit_dom_dom_token_list_finalize(GObject* object)
{
    WebKitDOMDOMTokenListPrivate* priv = WEBKIT_DOM_DOM_TOKEN_LIST_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMDOMTokenListPrivate();
    G_OBJECT_CLASS(webkit_dom_dom_token_list_parent_class)->finalize(object);
}

static void webkit_dom_dom_token_list_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMDOMTokenList* self = WEBKIT_DOM_DOM_TOKEN_LIST(object);

    switch (propertyId) {
    case DOM_DOM_TOKEN_LIST_PROP_VALUE:
        webkit_dom_dom_token_list_set_value(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_dom_token_list_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMDOMTokenList* self = WEBKIT_DOM_DOM_TOKEN_LIST(object);

    switch (propertyId) {
    case DOM_DOM_TOKEN_LIST_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_dom_token_list_get_length(self));
        break;
    case DOM_DOM_TOKEN_LIST_PROP_VALUE:
        g_value_take_string(value, webkit_dom_dom_token_list_get_value(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_dom_token_list_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_dom_token_list_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMDOMTokenListPrivate* priv = WEBKIT_DOM_DOM_TOKEN_LIST_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::DOMTokenList*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_dom_token_list_class_init(WebKitDOMDOMTokenListClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMDOMTokenListPrivate));
    gobjectClass->constructor = webkit_dom_dom_token_list_constructor;
    gobjectClass->finalize = webkit_dom_dom_token_list_finalize;
    gobjectClass->set_property = webkit_dom_dom_token_list_set_property;
    gobjectClass->get_property = webkit_dom_dom_token_list_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_TOKEN_LIST_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "DOMTokenList:length",
            "read-only gulong DOMTokenList:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_TOKEN_LIST_PROP_VALUE,
        g_param_spec_string(
            "value",
            "DOMTokenList:value",
            "read-write gchar* DOMTokenList:value",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_dom_token_list_init(WebKitDOMDOMTokenList* request)
{
    WebKitDOMDOMTokenListPrivate* priv = WEBKIT_DOM_DOM_TOKEN_LIST_GET_PRIVATE(request);
    new (priv) WebKitDOMDOMTokenListPrivate();
}

gchar* webkit_dom_dom_token_list_item(WebKitDOMDOMTokenList* self, gulong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self), 0);
    WebCore::DOMTokenList* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->item(index));
    return result;
}

gboolean webkit_dom_dom_token_list_contains(WebKitDOMDOMTokenList* self, const gchar* token)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self), FALSE);
    g_return_val_if_fail(token, FALSE);
    WebCore::DOMTokenList* item = WebKit::core(self);
    WTF::String convertedToken = WTF::String::fromUTF8(token);
    gboolean result = item->contains(convertedToken);
    return result;
}

void webkit_dom_dom_token_list_add(WebKitDOMDOMTokenList* self, GError** error, ...)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self));
    g_return_if_fail(!error || !*error);
    WebCore::DOMTokenList* item = WebKit::core(self);
    va_list variadicParameterList;
    Vector<WTF::String> convertedTokens;
    va_start(variadicParameterList, error);
    while (gchar* variadicParameter = va_arg(variadicParameterList, gchar*))
        convertedTokens.append(WTF::String::fromUTF8(variadicParameter));
    va_end(variadicParameterList);
    auto result = item->add(WTFMove(convertedTokens));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_dom_token_list_remove(WebKitDOMDOMTokenList* self, GError** error, ...)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self));
    g_return_if_fail(!error || !*error);
    WebCore::DOMTokenList* item = WebKit::core(self);
    va_list variadicParameterList;
    Vector<WTF::String> convertedTokens;
    va_start(variadicParameterList, error);
    while (gchar* variadicParameter = va_arg(variadicParameterList, gchar*))
        convertedTokens.append(WTF::String::fromUTF8(variadicParameter));
    va_end(variadicParameterList);
    auto result = item->remove(WTFMove(convertedTokens));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gboolean webkit_dom_dom_token_list_toggle(WebKitDOMDOMTokenList* self, const gchar* token, gboolean force, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self), FALSE);
    g_return_val_if_fail(token, FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    WebCore::DOMTokenList* item = WebKit::core(self);
    WTF::String convertedToken = WTF::String::fromUTF8(token);
    auto result = item->toggle(convertedToken, force);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

void webkit_dom_dom_token_list_replace(WebKitDOMDOMTokenList* self, const gchar* token, const gchar* newToken, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self));
    g_return_if_fail(token);
    g_return_if_fail(newToken);
    g_return_if_fail(!error || !*error);
    WebCore::DOMTokenList* item = WebKit::core(self);
    WTF::String convertedToken = WTF::String::fromUTF8(token);
    WTF::String convertedNewToken = WTF::String::fromUTF8(newToken);
    auto result = item->replace(convertedToken, convertedNewToken);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gulong webkit_dom_dom_token_list_get_length(WebKitDOMDOMTokenList* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self), 0);
    WebCore::DOMTokenList* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

gchar* webkit_dom_dom_token_list_get_value(WebKitDOMDOMTokenList* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self), 0);
    WebCore::DOMTokenList* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->value());
    return result;
}

void webkit_dom_dom_token_list_set_value(WebKitDOMDOMTokenList* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_TOKEN_LIST(self));
    g_return_if_fail(value);
    WebCore::DOMTokenList* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setValue(convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
