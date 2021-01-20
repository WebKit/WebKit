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
#include "WebKitDOMHTMLTableRowElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMHTMLElementPrivate.h"
#include "WebKitDOMHTMLTableRowElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLTableRowElement* kit(WebCore::HTMLTableRowElement* obj)
{
    return WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLTableRowElement* core(WebKitDOMHTMLTableRowElement* request)
{
    return request ? static_cast<WebCore::HTMLTableRowElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLTableRowElement* wrapHTMLTableRowElement(WebCore::HTMLTableRowElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_TABLE_ROW_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_table_row_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLTableRowElement* coreTarget = static_cast<WebCore::HTMLTableRowElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_table_row_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableRowElement* coreTarget = static_cast<WebCore::HTMLTableRowElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_table_row_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableRowElement* coreTarget = static_cast<WebCore::HTMLTableRowElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_table_row_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_table_row_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_table_row_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_table_row_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLTableRowElement, webkit_dom_html_table_row_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_table_row_element_dom_event_target_init))

enum {
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_0,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_ROW_INDEX,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_SECTION_ROW_INDEX,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_CELLS,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_ALIGN,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_BG_COLOR,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH_OFF,
    DOM_HTML_TABLE_ROW_ELEMENT_PROP_V_ALIGN,
};

static void webkit_dom_html_table_row_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableRowElement* self = WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_ALIGN:
        webkit_dom_html_table_row_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_BG_COLOR:
        webkit_dom_html_table_row_element_set_bg_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH:
        webkit_dom_html_table_row_element_set_ch(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH_OFF:
        webkit_dom_html_table_row_element_set_ch_off(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_V_ALIGN:
        webkit_dom_html_table_row_element_set_v_align(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_row_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableRowElement* self = WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_ROW_INDEX:
        g_value_set_long(value, webkit_dom_html_table_row_element_get_row_index(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_SECTION_ROW_INDEX:
        g_value_set_long(value, webkit_dom_html_table_row_element_get_section_row_index(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_CELLS:
        g_value_set_object(value, webkit_dom_html_table_row_element_get_cells(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_row_element_get_align(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_BG_COLOR:
        g_value_take_string(value, webkit_dom_html_table_row_element_get_bg_color(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH:
        g_value_take_string(value, webkit_dom_html_table_row_element_get_ch(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH_OFF:
        g_value_take_string(value, webkit_dom_html_table_row_element_get_ch_off(self));
        break;
    case DOM_HTML_TABLE_ROW_ELEMENT_PROP_V_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_row_element_get_v_align(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_row_element_class_init(WebKitDOMHTMLTableRowElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_table_row_element_set_property;
    gobjectClass->get_property = webkit_dom_html_table_row_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_ROW_INDEX,
        g_param_spec_long(
            "row-index",
            "HTMLTableRowElement:row-index",
            "read-only glong HTMLTableRowElement:row-index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_SECTION_ROW_INDEX,
        g_param_spec_long(
            "section-row-index",
            "HTMLTableRowElement:section-row-index",
            "read-only glong HTMLTableRowElement:section-row-index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_CELLS,
        g_param_spec_object(
            "cells",
            "HTMLTableRowElement:cells",
            "read-only WebKitDOMHTMLCollection* HTMLTableRowElement:cells",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLTableRowElement:align",
            "read-write gchar* HTMLTableRowElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_BG_COLOR,
        g_param_spec_string(
            "bg-color",
            "HTMLTableRowElement:bg-color",
            "read-write gchar* HTMLTableRowElement:bg-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH,
        g_param_spec_string(
            "ch",
            "HTMLTableRowElement:ch",
            "read-write gchar* HTMLTableRowElement:ch",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_CH_OFF,
        g_param_spec_string(
            "ch-off",
            "HTMLTableRowElement:ch-off",
            "read-write gchar* HTMLTableRowElement:ch-off",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ROW_ELEMENT_PROP_V_ALIGN,
        g_param_spec_string(
            "v-align",
            "HTMLTableRowElement:v-align",
            "read-write gchar* HTMLTableRowElement:v-align",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_table_row_element_init(WebKitDOMHTMLTableRowElement* request)
{
    UNUSED_PARAM(request);
}

WebKitDOMHTMLElement* webkit_dom_html_table_row_element_insert_cell(WebKitDOMHTMLTableRowElement* self, glong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    auto result = item->insertCell(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WEBKIT_DOM_HTML_ELEMENT(WebKit::kit(result.releaseReturnValue().ptr()));
}

void webkit_dom_html_table_row_element_delete_cell(WebKitDOMHTMLTableRowElement* self, glong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    auto result = item->deleteCell(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

glong webkit_dom_html_table_row_element_get_row_index(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    glong result = item->rowIndex();
    return result;
}

glong webkit_dom_html_table_row_element_get_section_row_index(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    glong result = item->sectionRowIndex();
    return result;
}

WebKitDOMHTMLCollection* webkit_dom_html_table_row_element_get_cells(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->cells());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_html_table_row_element_get_align(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_table_row_element_set_align(WebKitDOMHTMLTableRowElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_table_row_element_get_bg_color(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr));
    return result;
}

void webkit_dom_html_table_row_element_set_bg_color(WebKitDOMHTMLTableRowElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr, convertedValue);
}

gchar* webkit_dom_html_table_row_element_get_ch(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charAttr));
    return result;
}

void webkit_dom_html_table_row_element_set_ch(WebKitDOMHTMLTableRowElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charAttr, convertedValue);
}

gchar* webkit_dom_html_table_row_element_get_ch_off(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr));
    return result;
}

void webkit_dom_html_table_row_element_set_ch_off(WebKitDOMHTMLTableRowElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr, convertedValue);
}

gchar* webkit_dom_html_table_row_element_get_v_align(WebKitDOMHTMLTableRowElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self), 0);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::valignAttr));
    return result;
}

void webkit_dom_html_table_row_element_set_v_align(WebKitDOMHTMLTableRowElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableRowElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::valignAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
