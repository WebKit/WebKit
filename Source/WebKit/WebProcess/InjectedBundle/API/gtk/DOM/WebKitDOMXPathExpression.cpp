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
#include "WebKitDOMXPathExpression.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMXPathExpressionPrivate.h"
#include "WebKitDOMXPathResultPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_XPATH_EXPRESSION_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_XPATH_EXPRESSION, WebKitDOMXPathExpressionPrivate)

typedef struct _WebKitDOMXPathExpressionPrivate {
    RefPtr<WebCore::XPathExpression> coreObject;
} WebKitDOMXPathExpressionPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMXPathExpression* kit(WebCore::XPathExpression* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_XPATH_EXPRESSION(ret);

    return wrapXPathExpression(obj);
}

WebCore::XPathExpression* core(WebKitDOMXPathExpression* request)
{
    return request ? static_cast<WebCore::XPathExpression*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMXPathExpression* wrapXPathExpression(WebCore::XPathExpression* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_XPATH_EXPRESSION(g_object_new(WEBKIT_DOM_TYPE_XPATH_EXPRESSION, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMXPathExpression, webkit_dom_xpath_expression, WEBKIT_DOM_TYPE_OBJECT)

static void webkit_dom_xpath_expression_finalize(GObject* object)
{
    WebKitDOMXPathExpressionPrivate* priv = WEBKIT_DOM_XPATH_EXPRESSION_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMXPathExpressionPrivate();
    G_OBJECT_CLASS(webkit_dom_xpath_expression_parent_class)->finalize(object);
}

static GObject* webkit_dom_xpath_expression_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_xpath_expression_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMXPathExpressionPrivate* priv = WEBKIT_DOM_XPATH_EXPRESSION_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::XPathExpression*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_xpath_expression_class_init(WebKitDOMXPathExpressionClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMXPathExpressionPrivate));
    gobjectClass->constructor = webkit_dom_xpath_expression_constructor;
    gobjectClass->finalize = webkit_dom_xpath_expression_finalize;
}

static void webkit_dom_xpath_expression_init(WebKitDOMXPathExpression* request)
{
    WebKitDOMXPathExpressionPrivate* priv = WEBKIT_DOM_XPATH_EXPRESSION_GET_PRIVATE(request);
    new (priv) WebKitDOMXPathExpressionPrivate();
}

WebKitDOMXPathResult* webkit_dom_xpath_expression_evaluate(WebKitDOMXPathExpression* self, WebKitDOMNode* contextNode, gushort type, WebKitDOMXPathResult* inResult, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_EXPRESSION(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(contextNode), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_RESULT(inResult), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::XPathExpression* item = WebKit::core(self);
    WebCore::Node* convertedContextNode = WebKit::core(contextNode);
    WebCore::XPathResult* convertedInResult = WebKit::core(inResult);
    auto result = item->evaluate(convertedContextNode, type, convertedInResult);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
