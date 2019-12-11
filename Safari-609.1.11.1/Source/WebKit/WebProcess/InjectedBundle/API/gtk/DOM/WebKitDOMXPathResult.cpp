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
#include "WebKitDOMXPathResult.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMXPathResultPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_XPATH_RESULT_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_XPATH_RESULT, WebKitDOMXPathResultPrivate)

typedef struct _WebKitDOMXPathResultPrivate {
    RefPtr<WebCore::XPathResult> coreObject;
} WebKitDOMXPathResultPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMXPathResult* kit(WebCore::XPathResult* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_XPATH_RESULT(ret);

    return wrapXPathResult(obj);
}

WebCore::XPathResult* core(WebKitDOMXPathResult* request)
{
    return request ? static_cast<WebCore::XPathResult*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMXPathResult* wrapXPathResult(WebCore::XPathResult* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_XPATH_RESULT(g_object_new(WEBKIT_DOM_TYPE_XPATH_RESULT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMXPathResult, webkit_dom_xpath_result, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_XPATH_RESULT_PROP_0,
    DOM_XPATH_RESULT_PROP_RESULT_TYPE,
    DOM_XPATH_RESULT_PROP_NUMBER_VALUE,
    DOM_XPATH_RESULT_PROP_STRING_VALUE,
    DOM_XPATH_RESULT_PROP_BOOLEAN_VALUE,
    DOM_XPATH_RESULT_PROP_SINGLE_NODE_VALUE,
    DOM_XPATH_RESULT_PROP_INVALID_ITERATOR_STATE,
    DOM_XPATH_RESULT_PROP_SNAPSHOT_LENGTH,
};

static void webkit_dom_xpath_result_finalize(GObject* object)
{
    WebKitDOMXPathResultPrivate* priv = WEBKIT_DOM_XPATH_RESULT_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMXPathResultPrivate();
    G_OBJECT_CLASS(webkit_dom_xpath_result_parent_class)->finalize(object);
}

static void webkit_dom_xpath_result_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMXPathResult* self = WEBKIT_DOM_XPATH_RESULT(object);

    switch (propertyId) {
    case DOM_XPATH_RESULT_PROP_RESULT_TYPE:
        g_value_set_uint(value, webkit_dom_xpath_result_get_result_type(self));
        break;
    case DOM_XPATH_RESULT_PROP_NUMBER_VALUE:
        g_value_set_double(value, webkit_dom_xpath_result_get_number_value(self, nullptr));
        break;
    case DOM_XPATH_RESULT_PROP_STRING_VALUE:
        g_value_take_string(value, webkit_dom_xpath_result_get_string_value(self, nullptr));
        break;
    case DOM_XPATH_RESULT_PROP_BOOLEAN_VALUE:
        g_value_set_boolean(value, webkit_dom_xpath_result_get_boolean_value(self, nullptr));
        break;
    case DOM_XPATH_RESULT_PROP_SINGLE_NODE_VALUE:
        g_value_set_object(value, webkit_dom_xpath_result_get_single_node_value(self, nullptr));
        break;
    case DOM_XPATH_RESULT_PROP_INVALID_ITERATOR_STATE:
        g_value_set_boolean(value, webkit_dom_xpath_result_get_invalid_iterator_state(self));
        break;
    case DOM_XPATH_RESULT_PROP_SNAPSHOT_LENGTH:
        g_value_set_ulong(value, webkit_dom_xpath_result_get_snapshot_length(self, nullptr));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_xpath_result_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_xpath_result_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMXPathResultPrivate* priv = WEBKIT_DOM_XPATH_RESULT_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::XPathResult*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_xpath_result_class_init(WebKitDOMXPathResultClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMXPathResultPrivate));
    gobjectClass->constructor = webkit_dom_xpath_result_constructor;
    gobjectClass->finalize = webkit_dom_xpath_result_finalize;
    gobjectClass->get_property = webkit_dom_xpath_result_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_RESULT_TYPE,
        g_param_spec_uint(
            "result-type",
            "XPathResult:result-type",
            "read-only gushort XPathResult:result-type",
            0, G_MAXUINT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_NUMBER_VALUE,
        g_param_spec_double(
            "number-value",
            "XPathResult:number-value",
            "read-only gdouble XPathResult:number-value",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_STRING_VALUE,
        g_param_spec_string(
            "string-value",
            "XPathResult:string-value",
            "read-only gchar* XPathResult:string-value",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_BOOLEAN_VALUE,
        g_param_spec_boolean(
            "boolean-value",
            "XPathResult:boolean-value",
            "read-only gboolean XPathResult:boolean-value",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_SINGLE_NODE_VALUE,
        g_param_spec_object(
            "single-node-value",
            "XPathResult:single-node-value",
            "read-only WebKitDOMNode* XPathResult:single-node-value",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_INVALID_ITERATOR_STATE,
        g_param_spec_boolean(
            "invalid-iterator-state",
            "XPathResult:invalid-iterator-state",
            "read-only gboolean XPathResult:invalid-iterator-state",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_XPATH_RESULT_PROP_SNAPSHOT_LENGTH,
        g_param_spec_ulong(
            "snapshot-length",
            "XPathResult:snapshot-length",
            "read-only gulong XPathResult:snapshot-length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_xpath_result_init(WebKitDOMXPathResult* request)
{
    WebKitDOMXPathResultPrivate* priv = WEBKIT_DOM_XPATH_RESULT_GET_PRIVATE(request);
    new (priv) WebKitDOMXPathResultPrivate();
}

WebKitDOMNode* webkit_dom_xpath_result_iterate_next(WebKitDOMXPathResult* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->iterateNext();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

WebKitDOMNode* webkit_dom_xpath_result_snapshot_item(WebKitDOMXPathResult* self, gulong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->snapshotItem(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

gushort webkit_dom_xpath_result_get_result_type(WebKitDOMXPathResult* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    WebCore::XPathResult* item = WebKit::core(self);
    gushort result = item->resultType();
    return result;
}

gdouble webkit_dom_xpath_result_get_number_value(WebKitDOMXPathResult* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->numberValue();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return 0;
    }
    return result.releaseReturnValue();
}

gchar* webkit_dom_xpath_result_get_string_value(WebKitDOMXPathResult* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->stringValue();
    if (result.hasException())
        return nullptr;
    return convertToUTF8String(result.releaseReturnValue());
}

gboolean webkit_dom_xpath_result_get_boolean_value(WebKitDOMXPathResult* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->booleanValue();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

WebKitDOMNode* webkit_dom_xpath_result_get_single_node_value(WebKitDOMXPathResult* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->singleNodeValue();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

gboolean webkit_dom_xpath_result_get_invalid_iterator_state(WebKitDOMXPathResult* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), FALSE);
    WebCore::XPathResult* item = WebKit::core(self);
    gboolean result = item->invalidIteratorState();
    return result;
}

gulong webkit_dom_xpath_result_get_snapshot_length(WebKitDOMXPathResult* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathResult* item = WebKit::core(self);
    auto result = item->snapshotLength();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
    return result.releaseReturnValue();
}

G_GNUC_END_IGNORE_DEPRECATIONS;
