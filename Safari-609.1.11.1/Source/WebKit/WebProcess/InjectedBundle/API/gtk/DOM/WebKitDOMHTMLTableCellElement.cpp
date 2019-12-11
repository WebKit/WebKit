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
#include "WebKitDOMHTMLTableCellElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLTableCellElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLTableCellElement* kit(WebCore::HTMLTableCellElement* obj)
{
    return WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLTableCellElement* core(WebKitDOMHTMLTableCellElement* request)
{
    return request ? static_cast<WebCore::HTMLTableCellElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLTableCellElement* wrapHTMLTableCellElement(WebCore::HTMLTableCellElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_TABLE_CELL_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_table_cell_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLTableCellElement* coreTarget = static_cast<WebCore::HTMLTableCellElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_table_cell_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableCellElement* coreTarget = static_cast<WebCore::HTMLTableCellElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_table_cell_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableCellElement* coreTarget = static_cast<WebCore::HTMLTableCellElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_table_cell_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_table_cell_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_table_cell_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_table_cell_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLTableCellElement, webkit_dom_html_table_cell_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_table_cell_element_dom_event_target_init))

enum {
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_0,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_CELL_INDEX,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_ALIGN,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_AXIS,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_BG_COLOR,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH_OFF,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_COL_SPAN,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_ROW_SPAN,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEADERS,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEIGHT,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_NO_WRAP,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_V_ALIGN,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_WIDTH,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_ABBR,
    DOM_HTML_TABLE_CELL_ELEMENT_PROP_SCOPE,
};

static void webkit_dom_html_table_cell_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableCellElement* self = WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_ALIGN:
        webkit_dom_html_table_cell_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_AXIS:
        webkit_dom_html_table_cell_element_set_axis(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_BG_COLOR:
        webkit_dom_html_table_cell_element_set_bg_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH:
        webkit_dom_html_table_cell_element_set_ch(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH_OFF:
        webkit_dom_html_table_cell_element_set_ch_off(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_COL_SPAN:
        webkit_dom_html_table_cell_element_set_col_span(self, g_value_get_long(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_ROW_SPAN:
        webkit_dom_html_table_cell_element_set_row_span(self, g_value_get_long(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEADERS:
        webkit_dom_html_table_cell_element_set_headers(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEIGHT:
        webkit_dom_html_table_cell_element_set_height(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_NO_WRAP:
        webkit_dom_html_table_cell_element_set_no_wrap(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_V_ALIGN:
        webkit_dom_html_table_cell_element_set_v_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_WIDTH:
        webkit_dom_html_table_cell_element_set_width(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_ABBR:
        webkit_dom_html_table_cell_element_set_abbr(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_SCOPE:
        webkit_dom_html_table_cell_element_set_scope(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_cell_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableCellElement* self = WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_CELL_INDEX:
        g_value_set_long(value, webkit_dom_html_table_cell_element_get_cell_index(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_align(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_AXIS:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_axis(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_BG_COLOR:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_bg_color(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_ch(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH_OFF:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_ch_off(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_COL_SPAN:
        g_value_set_long(value, webkit_dom_html_table_cell_element_get_col_span(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_ROW_SPAN:
        g_value_set_long(value, webkit_dom_html_table_cell_element_get_row_span(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEADERS:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_headers(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEIGHT:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_height(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_NO_WRAP:
        g_value_set_boolean(value, webkit_dom_html_table_cell_element_get_no_wrap(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_V_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_v_align(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_WIDTH:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_width(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_ABBR:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_abbr(self));
        break;
    case DOM_HTML_TABLE_CELL_ELEMENT_PROP_SCOPE:
        g_value_take_string(value, webkit_dom_html_table_cell_element_get_scope(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_cell_element_class_init(WebKitDOMHTMLTableCellElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_table_cell_element_set_property;
    gobjectClass->get_property = webkit_dom_html_table_cell_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_CELL_INDEX,
        g_param_spec_long(
            "cell-index",
            "HTMLTableCellElement:cell-index",
            "read-only glong HTMLTableCellElement:cell-index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLTableCellElement:align",
            "read-write gchar* HTMLTableCellElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_AXIS,
        g_param_spec_string(
            "axis",
            "HTMLTableCellElement:axis",
            "read-write gchar* HTMLTableCellElement:axis",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_BG_COLOR,
        g_param_spec_string(
            "bg-color",
            "HTMLTableCellElement:bg-color",
            "read-write gchar* HTMLTableCellElement:bg-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH,
        g_param_spec_string(
            "ch",
            "HTMLTableCellElement:ch",
            "read-write gchar* HTMLTableCellElement:ch",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_CH_OFF,
        g_param_spec_string(
            "ch-off",
            "HTMLTableCellElement:ch-off",
            "read-write gchar* HTMLTableCellElement:ch-off",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_COL_SPAN,
        g_param_spec_long(
            "col-span",
            "HTMLTableCellElement:col-span",
            "read-write glong HTMLTableCellElement:col-span",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_ROW_SPAN,
        g_param_spec_long(
            "row-span",
            "HTMLTableCellElement:row-span",
            "read-write glong HTMLTableCellElement:row-span",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEADERS,
        g_param_spec_string(
            "headers",
            "HTMLTableCellElement:headers",
            "read-write gchar* HTMLTableCellElement:headers",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_HEIGHT,
        g_param_spec_string(
            "height",
            "HTMLTableCellElement:height",
            "read-write gchar* HTMLTableCellElement:height",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_NO_WRAP,
        g_param_spec_boolean(
            "no-wrap",
            "HTMLTableCellElement:no-wrap",
            "read-write gboolean HTMLTableCellElement:no-wrap",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_V_ALIGN,
        g_param_spec_string(
            "v-align",
            "HTMLTableCellElement:v-align",
            "read-write gchar* HTMLTableCellElement:v-align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_WIDTH,
        g_param_spec_string(
            "width",
            "HTMLTableCellElement:width",
            "read-write gchar* HTMLTableCellElement:width",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_ABBR,
        g_param_spec_string(
            "abbr",
            "HTMLTableCellElement:abbr",
            "read-write gchar* HTMLTableCellElement:abbr",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_CELL_ELEMENT_PROP_SCOPE,
        g_param_spec_string(
            "scope",
            "HTMLTableCellElement:scope",
            "read-write gchar* HTMLTableCellElement:scope",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_table_cell_element_init(WebKitDOMHTMLTableCellElement* request)
{
    UNUSED_PARAM(request);
}

glong webkit_dom_html_table_cell_element_get_cell_index(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    glong result = item->cellIndex();
    return result;
}

gchar* webkit_dom_html_table_cell_element_get_align(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_align(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_axis(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::axisAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_axis(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::axisAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_bg_color(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_bg_color(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_ch(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_ch(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_ch_off(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_ch_off(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr, convertedValue);
}

glong webkit_dom_html_table_cell_element_get_col_span(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    glong result = item->colSpan();
    return result;
}

void webkit_dom_html_table_cell_element_set_col_span(WebKitDOMHTMLTableCellElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    item->setColSpan(value);
}

glong webkit_dom_html_table_cell_element_get_row_span(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    glong result = item->rowSpanForBindings();
    return result;
}

void webkit_dom_html_table_cell_element_set_row_span(WebKitDOMHTMLTableCellElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    item->setRowSpanForBindings(value);
}

gchar* webkit_dom_html_table_cell_element_get_headers(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::headersAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_headers(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::headersAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_height(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::heightAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_height(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::heightAttr, convertedValue);
}

gboolean webkit_dom_html_table_cell_element_get_no_wrap(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), FALSE);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::nowrapAttr);
    return result;
}

void webkit_dom_html_table_cell_element_set_no_wrap(WebKitDOMHTMLTableCellElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::nowrapAttr, value);
}

gchar* webkit_dom_html_table_cell_element_get_v_align(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::valignAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_v_align(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::valignAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_width(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::widthAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_width(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_abbr(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::abbrAttr));
    return result;
}

void webkit_dom_html_table_cell_element_set_abbr(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::abbrAttr, convertedValue);
}

gchar* webkit_dom_html_table_cell_element_get_scope(WebKitDOMHTMLTableCellElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self), 0);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->scope());
    return result;
}

void webkit_dom_html_table_cell_element_set_scope(WebKitDOMHTMLTableCellElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableCellElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setScope(convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
