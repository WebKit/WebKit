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
#include "WebKitDOMCSSRule.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMCSSRulePrivate.h"
#include "WebKitDOMCSSStyleSheetPrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_CSS_RULE_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_CSS_RULE, WebKitDOMCSSRulePrivate)

typedef struct _WebKitDOMCSSRulePrivate {
    RefPtr<WebCore::CSSRule> coreObject;
} WebKitDOMCSSRulePrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMCSSRule* kit(WebCore::CSSRule* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_CSS_RULE(ret);

    return wrapCSSRule(obj);
}

WebCore::CSSRule* core(WebKitDOMCSSRule* request)
{
    return request ? static_cast<WebCore::CSSRule*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMCSSRule* wrapCSSRule(WebCore::CSSRule* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_CSS_RULE(g_object_new(WEBKIT_DOM_TYPE_CSS_RULE, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMCSSRule, webkit_dom_css_rule, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_CSS_RULE_PROP_0,
    DOM_CSS_RULE_PROP_TYPE,
    DOM_CSS_RULE_PROP_CSS_TEXT,
    DOM_CSS_RULE_PROP_PARENT_STYLE_SHEET,
    DOM_CSS_RULE_PROP_PARENT_RULE,
};

static void webkit_dom_css_rule_finalize(GObject* object)
{
    WebKitDOMCSSRulePrivate* priv = WEBKIT_DOM_CSS_RULE_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMCSSRulePrivate();
    G_OBJECT_CLASS(webkit_dom_css_rule_parent_class)->finalize(object);
}

static void webkit_dom_css_rule_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMCSSRule* self = WEBKIT_DOM_CSS_RULE(object);

    switch (propertyId) {
    case DOM_CSS_RULE_PROP_CSS_TEXT:
        webkit_dom_css_rule_set_css_text(self, g_value_get_string(value), nullptr);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_css_rule_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMCSSRule* self = WEBKIT_DOM_CSS_RULE(object);

    switch (propertyId) {
    case DOM_CSS_RULE_PROP_TYPE:
        g_value_set_uint(value, webkit_dom_css_rule_get_rule_type(self));
        break;
    case DOM_CSS_RULE_PROP_CSS_TEXT:
        g_value_take_string(value, webkit_dom_css_rule_get_css_text(self));
        break;
    case DOM_CSS_RULE_PROP_PARENT_STYLE_SHEET:
        g_value_set_object(value, webkit_dom_css_rule_get_parent_style_sheet(self));
        break;
    case DOM_CSS_RULE_PROP_PARENT_RULE:
        g_value_set_object(value, webkit_dom_css_rule_get_parent_rule(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_css_rule_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_css_rule_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMCSSRulePrivate* priv = WEBKIT_DOM_CSS_RULE_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::CSSRule*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_css_rule_class_init(WebKitDOMCSSRuleClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMCSSRulePrivate));
    gobjectClass->constructor = webkit_dom_css_rule_constructor;
    gobjectClass->finalize = webkit_dom_css_rule_finalize;
    gobjectClass->set_property = webkit_dom_css_rule_set_property;
    gobjectClass->get_property = webkit_dom_css_rule_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_RULE_PROP_TYPE,
        g_param_spec_uint(
            "type",
            "CSSRule:type",
            "read-only gushort CSSRule:type",
            0, G_MAXUINT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_RULE_PROP_CSS_TEXT,
        g_param_spec_string(
            "css-text",
            "CSSRule:css-text",
            "read-write gchar* CSSRule:css-text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_RULE_PROP_PARENT_STYLE_SHEET,
        g_param_spec_object(
            "parent-style-sheet",
            "CSSRule:parent-style-sheet",
            "read-only WebKitDOMCSSStyleSheet* CSSRule:parent-style-sheet",
            WEBKIT_DOM_TYPE_CSS_STYLE_SHEET,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_RULE_PROP_PARENT_RULE,
        g_param_spec_object(
            "parent-rule",
            "CSSRule:parent-rule",
            "read-only WebKitDOMCSSRule* CSSRule:parent-rule",
            WEBKIT_DOM_TYPE_CSS_RULE,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_css_rule_init(WebKitDOMCSSRule* request)
{
    WebKitDOMCSSRulePrivate* priv = WEBKIT_DOM_CSS_RULE_GET_PRIVATE(request);
    new (priv) WebKitDOMCSSRulePrivate();
}

gushort webkit_dom_css_rule_get_rule_type(WebKitDOMCSSRule* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_RULE(self), 0);
    WebCore::CSSRule* item = WebKit::core(self);
    gushort result = item->type();
    return result;
}

gchar* webkit_dom_css_rule_get_css_text(WebKitDOMCSSRule* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_RULE(self), 0);
    WebCore::CSSRule* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->cssText());
    return result;
}

void webkit_dom_css_rule_set_css_text(WebKitDOMCSSRule* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_CSS_RULE(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::CSSRule* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setCssText(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMCSSStyleSheet* webkit_dom_css_rule_get_parent_style_sheet(WebKitDOMCSSRule* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_RULE(self), 0);
    WebCore::CSSRule* item = WebKit::core(self);
    RefPtr<WebCore::CSSStyleSheet> gobjectResult = WTF::getPtr(item->parentStyleSheet());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMCSSRule* webkit_dom_css_rule_get_parent_rule(WebKitDOMCSSRule* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_RULE(self), 0);
    WebCore::CSSRule* item = WebKit::core(self);
    RefPtr<WebCore::CSSRule> gobjectResult = WTF::getPtr(item->parentRule());
    return WebKit::kit(gobjectResult.get());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
