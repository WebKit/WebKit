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
#include "WebKitDOMHTMLTableColElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLTableColElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLTableColElement* kit(WebCore::HTMLTableColElement* obj)
{
    return WEBKIT_DOM_HTML_TABLE_COL_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLTableColElement* core(WebKitDOMHTMLTableColElement* request)
{
    return request ? static_cast<WebCore::HTMLTableColElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLTableColElement* wrapHTMLTableColElement(WebCore::HTMLTableColElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_TABLE_COL_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_TABLE_COL_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_table_col_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLTableColElement* coreTarget = static_cast<WebCore::HTMLTableColElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_table_col_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableColElement* coreTarget = static_cast<WebCore::HTMLTableColElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_table_col_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableColElement* coreTarget = static_cast<WebCore::HTMLTableColElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_table_col_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_table_col_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_table_col_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_table_col_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLTableColElement, webkit_dom_html_table_col_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_table_col_element_dom_event_target_init))

enum {
    DOM_HTML_TABLE_COL_ELEMENT_PROP_0,
    DOM_HTML_TABLE_COL_ELEMENT_PROP_ALIGN,
    DOM_HTML_TABLE_COL_ELEMENT_PROP_CH,
    DOM_HTML_TABLE_COL_ELEMENT_PROP_CH_OFF,
    DOM_HTML_TABLE_COL_ELEMENT_PROP_SPAN,
    DOM_HTML_TABLE_COL_ELEMENT_PROP_V_ALIGN,
    DOM_HTML_TABLE_COL_ELEMENT_PROP_WIDTH,
};

static void webkit_dom_html_table_col_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableColElement* self = WEBKIT_DOM_HTML_TABLE_COL_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_ALIGN:
        webkit_dom_html_table_col_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_CH:
        webkit_dom_html_table_col_element_set_ch(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_CH_OFF:
        webkit_dom_html_table_col_element_set_ch_off(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_SPAN:
        webkit_dom_html_table_col_element_set_span(self, g_value_get_long(value));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_V_ALIGN:
        webkit_dom_html_table_col_element_set_v_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_WIDTH:
        webkit_dom_html_table_col_element_set_width(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_col_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableColElement* self = WEBKIT_DOM_HTML_TABLE_COL_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_col_element_get_align(self));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_CH:
        g_value_take_string(value, webkit_dom_html_table_col_element_get_ch(self));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_CH_OFF:
        g_value_take_string(value, webkit_dom_html_table_col_element_get_ch_off(self));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_SPAN:
        g_value_set_long(value, webkit_dom_html_table_col_element_get_span(self));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_V_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_col_element_get_v_align(self));
        break;
    case DOM_HTML_TABLE_COL_ELEMENT_PROP_WIDTH:
        g_value_take_string(value, webkit_dom_html_table_col_element_get_width(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_col_element_class_init(WebKitDOMHTMLTableColElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_table_col_element_set_property;
    gobjectClass->get_property = webkit_dom_html_table_col_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_COL_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLTableColElement:align",
            "read-write gchar* HTMLTableColElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_COL_ELEMENT_PROP_CH,
        g_param_spec_string(
            "ch",
            "HTMLTableColElement:ch",
            "read-write gchar* HTMLTableColElement:ch",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_COL_ELEMENT_PROP_CH_OFF,
        g_param_spec_string(
            "ch-off",
            "HTMLTableColElement:ch-off",
            "read-write gchar* HTMLTableColElement:ch-off",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_COL_ELEMENT_PROP_SPAN,
        g_param_spec_long(
            "span",
            "HTMLTableColElement:span",
            "read-write glong HTMLTableColElement:span",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_COL_ELEMENT_PROP_V_ALIGN,
        g_param_spec_string(
            "v-align",
            "HTMLTableColElement:v-align",
            "read-write gchar* HTMLTableColElement:v-align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_COL_ELEMENT_PROP_WIDTH,
        g_param_spec_string(
            "width",
            "HTMLTableColElement:width",
            "read-write gchar* HTMLTableColElement:width",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_table_col_element_init(WebKitDOMHTMLTableColElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_table_col_element_get_align(WebKitDOMHTMLTableColElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self), 0);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_table_col_element_set_align(WebKitDOMHTMLTableColElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_table_col_element_get_ch(WebKitDOMHTMLTableColElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self), 0);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charAttr));
    return result;
}

void webkit_dom_html_table_col_element_set_ch(WebKitDOMHTMLTableColElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charAttr, convertedValue);
}

gchar* webkit_dom_html_table_col_element_get_ch_off(WebKitDOMHTMLTableColElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self), 0);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr));
    return result;
}

void webkit_dom_html_table_col_element_set_ch_off(WebKitDOMHTMLTableColElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr, convertedValue);
}

glong webkit_dom_html_table_col_element_get_span(WebKitDOMHTMLTableColElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self), 0);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    glong result = item->span();
    return result;
}

void webkit_dom_html_table_col_element_set_span(WebKitDOMHTMLTableColElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self));
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    item->setSpan(value);
}

gchar* webkit_dom_html_table_col_element_get_v_align(WebKitDOMHTMLTableColElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self), 0);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::valignAttr));
    return result;
}

void webkit_dom_html_table_col_element_set_v_align(WebKitDOMHTMLTableColElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::valignAttr, convertedValue);
}

gchar* webkit_dom_html_table_col_element_get_width(WebKitDOMHTMLTableColElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self), 0);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::widthAttr));
    return result;
}

void webkit_dom_html_table_col_element_set_width(WebKitDOMHTMLTableColElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_COL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableColElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
