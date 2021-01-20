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
#include "WebKitDOMCSSStyleSheet.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMCSSRuleListPrivate.h"
#include "WebKitDOMCSSRulePrivate.h"
#include "WebKitDOMCSSStyleSheetPrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMStyleSheetPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMCSSStyleSheet* kit(WebCore::CSSStyleSheet* obj)
{
    return WEBKIT_DOM_CSS_STYLE_SHEET(kit(static_cast<WebCore::StyleSheet*>(obj)));
}

WebCore::CSSStyleSheet* core(WebKitDOMCSSStyleSheet* request)
{
    return request ? static_cast<WebCore::CSSStyleSheet*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMCSSStyleSheet* wrapCSSStyleSheet(WebCore::CSSStyleSheet* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_CSS_STYLE_SHEET(g_object_new(WEBKIT_DOM_TYPE_CSS_STYLE_SHEET, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMCSSStyleSheet, webkit_dom_css_style_sheet, WEBKIT_DOM_TYPE_STYLE_SHEET)

enum {
    DOM_CSS_STYLE_SHEET_PROP_0,
    DOM_CSS_STYLE_SHEET_PROP_OWNER_RULE,
    DOM_CSS_STYLE_SHEET_PROP_CSS_RULES,
    DOM_CSS_STYLE_SHEET_PROP_RULES,
};

static void webkit_dom_css_style_sheet_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMCSSStyleSheet* self = WEBKIT_DOM_CSS_STYLE_SHEET(object);

    switch (propertyId) {
    case DOM_CSS_STYLE_SHEET_PROP_OWNER_RULE:
        g_value_set_object(value, webkit_dom_css_style_sheet_get_owner_rule(self));
        break;
    case DOM_CSS_STYLE_SHEET_PROP_CSS_RULES:
        g_value_set_object(value, webkit_dom_css_style_sheet_get_css_rules(self));
        break;
    case DOM_CSS_STYLE_SHEET_PROP_RULES:
        g_value_set_object(value, webkit_dom_css_style_sheet_get_rules(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_css_style_sheet_class_init(WebKitDOMCSSStyleSheetClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_css_style_sheet_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_STYLE_SHEET_PROP_OWNER_RULE,
        g_param_spec_object(
            "owner-rule",
            "CSSStyleSheet:owner-rule",
            "read-only WebKitDOMCSSRule* CSSStyleSheet:owner-rule",
            WEBKIT_DOM_TYPE_CSS_RULE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_STYLE_SHEET_PROP_CSS_RULES,
        g_param_spec_object(
            "css-rules",
            "CSSStyleSheet:css-rules",
            "read-only WebKitDOMCSSRuleList* CSSStyleSheet:css-rules",
            WEBKIT_DOM_TYPE_CSS_RULE_LIST,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CSS_STYLE_SHEET_PROP_RULES,
        g_param_spec_object(
            "rules",
            "CSSStyleSheet:rules",
            "read-only WebKitDOMCSSRuleList* CSSStyleSheet:rules",
            WEBKIT_DOM_TYPE_CSS_RULE_LIST,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_css_style_sheet_init(WebKitDOMCSSStyleSheet* request)
{
    UNUSED_PARAM(request);
}

gulong webkit_dom_css_style_sheet_insert_rule(WebKitDOMCSSStyleSheet* self, const gchar* rule, gulong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self), 0);
    g_return_val_if_fail(rule, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    WTF::String convertedRule = WTF::String::fromUTF8(rule);
    auto result = item->insertRule(convertedRule, index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return 0;
    }
    return result.releaseReturnValue();
}

void webkit_dom_css_style_sheet_delete_rule(WebKitDOMCSSStyleSheet* self, gulong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self));
    g_return_if_fail(!error || !*error);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    auto result = item->deleteRule(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

glong webkit_dom_css_style_sheet_add_rule(WebKitDOMCSSStyleSheet* self, const gchar* selector, const gchar* style, gulong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self), 0);
    g_return_val_if_fail(selector, 0);
    g_return_val_if_fail(style, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    WTF::String convertedSelector = WTF::String::fromUTF8(selector);
    WTF::String convertedStyle = WTF::String::fromUTF8(style);
    auto result = item->addRule(convertedSelector, convertedStyle, index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return -1;
    }
    return result.releaseReturnValue();
}

void webkit_dom_css_style_sheet_remove_rule(WebKitDOMCSSStyleSheet* self, gulong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self));
    g_return_if_fail(!error || !*error);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    auto result = item->removeRule(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMCSSRule* webkit_dom_css_style_sheet_get_owner_rule(WebKitDOMCSSStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self), 0);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    RefPtr<WebCore::CSSRule> gobjectResult = WTF::getPtr(item->ownerRule());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMCSSRuleList* webkit_dom_css_style_sheet_get_css_rules(WebKitDOMCSSStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self), 0);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    RefPtr<WebCore::CSSRuleList> gobjectResult = WTF::getPtr(item->cssRules());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMCSSRuleList* webkit_dom_css_style_sheet_get_rules(WebKitDOMCSSStyleSheet* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CSS_STYLE_SHEET(self), 0);
    WebCore::CSSStyleSheet* item = WebKit::core(self);
    RefPtr<WebCore::CSSRuleList> gobjectResult = WTF::getPtr(item->rules());
    return WebKit::kit(gobjectResult.get());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
