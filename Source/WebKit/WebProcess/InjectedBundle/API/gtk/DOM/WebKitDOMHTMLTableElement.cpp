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
#include "WebKitDOMHTMLTableElement.h"

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
#include "WebKitDOMHTMLTableCaptionElementPrivate.h"
#include "WebKitDOMHTMLTableElementPrivate.h"
#include "WebKitDOMHTMLTableSectionElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLTableElement* kit(WebCore::HTMLTableElement* obj)
{
    return WEBKIT_DOM_HTML_TABLE_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLTableElement* core(WebKitDOMHTMLTableElement* request)
{
    return request ? static_cast<WebCore::HTMLTableElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLTableElement* wrapHTMLTableElement(WebCore::HTMLTableElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_TABLE_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_TABLE_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_table_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLTableElement* coreTarget = static_cast<WebCore::HTMLTableElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_table_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableElement* coreTarget = static_cast<WebCore::HTMLTableElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_table_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTableElement* coreTarget = static_cast<WebCore::HTMLTableElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_table_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_table_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_table_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_table_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLTableElement, webkit_dom_html_table_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_table_element_dom_event_target_init))

enum {
    DOM_HTML_TABLE_ELEMENT_PROP_0,
    DOM_HTML_TABLE_ELEMENT_PROP_CAPTION,
    DOM_HTML_TABLE_ELEMENT_PROP_T_HEAD,
    DOM_HTML_TABLE_ELEMENT_PROP_T_FOOT,
    DOM_HTML_TABLE_ELEMENT_PROP_ROWS,
    DOM_HTML_TABLE_ELEMENT_PROP_T_BODIES,
    DOM_HTML_TABLE_ELEMENT_PROP_ALIGN,
    DOM_HTML_TABLE_ELEMENT_PROP_BG_COLOR,
    DOM_HTML_TABLE_ELEMENT_PROP_BORDER,
    DOM_HTML_TABLE_ELEMENT_PROP_CELL_PADDING,
    DOM_HTML_TABLE_ELEMENT_PROP_CELL_SPACING,
    DOM_HTML_TABLE_ELEMENT_PROP_RULES,
    DOM_HTML_TABLE_ELEMENT_PROP_SUMMARY,
    DOM_HTML_TABLE_ELEMENT_PROP_WIDTH,
};

static void webkit_dom_html_table_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableElement* self = WEBKIT_DOM_HTML_TABLE_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_ELEMENT_PROP_ALIGN:
        webkit_dom_html_table_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_BG_COLOR:
        webkit_dom_html_table_element_set_bg_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_BORDER:
        webkit_dom_html_table_element_set_border(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_CELL_PADDING:
        webkit_dom_html_table_element_set_cell_padding(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_CELL_SPACING:
        webkit_dom_html_table_element_set_cell_spacing(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_RULES:
        webkit_dom_html_table_element_set_rules(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_SUMMARY:
        webkit_dom_html_table_element_set_summary(self, g_value_get_string(value));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_WIDTH:
        webkit_dom_html_table_element_set_width(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTableElement* self = WEBKIT_DOM_HTML_TABLE_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TABLE_ELEMENT_PROP_CAPTION:
        g_value_set_object(value, webkit_dom_html_table_element_get_caption(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_T_HEAD:
        g_value_set_object(value, webkit_dom_html_table_element_get_t_head(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_T_FOOT:
        g_value_set_object(value, webkit_dom_html_table_element_get_t_foot(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_ROWS:
        g_value_set_object(value, webkit_dom_html_table_element_get_rows(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_T_BODIES:
        g_value_set_object(value, webkit_dom_html_table_element_get_t_bodies(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_table_element_get_align(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_BG_COLOR:
        g_value_take_string(value, webkit_dom_html_table_element_get_bg_color(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_BORDER:
        g_value_take_string(value, webkit_dom_html_table_element_get_border(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_CELL_PADDING:
        g_value_take_string(value, webkit_dom_html_table_element_get_cell_padding(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_CELL_SPACING:
        g_value_take_string(value, webkit_dom_html_table_element_get_cell_spacing(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_RULES:
        g_value_take_string(value, webkit_dom_html_table_element_get_rules(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_SUMMARY:
        g_value_take_string(value, webkit_dom_html_table_element_get_summary(self));
        break;
    case DOM_HTML_TABLE_ELEMENT_PROP_WIDTH:
        g_value_take_string(value, webkit_dom_html_table_element_get_width(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_table_element_class_init(WebKitDOMHTMLTableElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_table_element_set_property;
    gobjectClass->get_property = webkit_dom_html_table_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_CAPTION,
        g_param_spec_object(
            "caption",
            "HTMLTableElement:caption",
            "read-only WebKitDOMHTMLTableCaptionElement* HTMLTableElement:caption",
            WEBKIT_DOM_TYPE_HTML_TABLE_CAPTION_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_T_HEAD,
        g_param_spec_object(
            "t-head",
            "HTMLTableElement:t-head",
            "read-only WebKitDOMHTMLTableSectionElement* HTMLTableElement:t-head",
            WEBKIT_DOM_TYPE_HTML_TABLE_SECTION_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_T_FOOT,
        g_param_spec_object(
            "t-foot",
            "HTMLTableElement:t-foot",
            "read-only WebKitDOMHTMLTableSectionElement* HTMLTableElement:t-foot",
            WEBKIT_DOM_TYPE_HTML_TABLE_SECTION_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_ROWS,
        g_param_spec_object(
            "rows",
            "HTMLTableElement:rows",
            "read-only WebKitDOMHTMLCollection* HTMLTableElement:rows",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_T_BODIES,
        g_param_spec_object(
            "t-bodies",
            "HTMLTableElement:t-bodies",
            "read-only WebKitDOMHTMLCollection* HTMLTableElement:t-bodies",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLTableElement:align",
            "read-write gchar* HTMLTableElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_BG_COLOR,
        g_param_spec_string(
            "bg-color",
            "HTMLTableElement:bg-color",
            "read-write gchar* HTMLTableElement:bg-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_BORDER,
        g_param_spec_string(
            "border",
            "HTMLTableElement:border",
            "read-write gchar* HTMLTableElement:border",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_CELL_PADDING,
        g_param_spec_string(
            "cell-padding",
            "HTMLTableElement:cell-padding",
            "read-write gchar* HTMLTableElement:cell-padding",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_CELL_SPACING,
        g_param_spec_string(
            "cell-spacing",
            "HTMLTableElement:cell-spacing",
            "read-write gchar* HTMLTableElement:cell-spacing",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_RULES,
        g_param_spec_string(
            "rules",
            "HTMLTableElement:rules",
            "read-write gchar* HTMLTableElement:rules",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_SUMMARY,
        g_param_spec_string(
            "summary",
            "HTMLTableElement:summary",
            "read-write gchar* HTMLTableElement:summary",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TABLE_ELEMENT_PROP_WIDTH,
        g_param_spec_string(
            "width",
            "HTMLTableElement:width",
            "read-write gchar* HTMLTableElement:width",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_table_element_init(WebKitDOMHTMLTableElement* request)
{
    UNUSED_PARAM(request);
}

WebKitDOMHTMLElement* webkit_dom_html_table_element_create_t_head(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLElement> gobjectResult = WTF::getPtr(item->createTHead());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_table_element_delete_t_head(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    WebCore::HTMLTableElement* item = WebKit::core(self);
    item->deleteTHead();
}

WebKitDOMHTMLElement* webkit_dom_html_table_element_create_t_foot(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLElement> gobjectResult = WTF::getPtr(item->createTFoot());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_table_element_delete_t_foot(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    WebCore::HTMLTableElement* item = WebKit::core(self);
    item->deleteTFoot();
}

WebKitDOMHTMLElement* webkit_dom_html_table_element_create_caption(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLElement> gobjectResult = WTF::getPtr(item->createCaption());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_table_element_delete_caption(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    WebCore::HTMLTableElement* item = WebKit::core(self);
    item->deleteCaption();
}

WebKitDOMHTMLElement* webkit_dom_html_table_element_insert_row(WebKitDOMHTMLTableElement* self, glong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    auto result = item->insertRow(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

void webkit_dom_html_table_element_delete_row(WebKitDOMHTMLTableElement* self, glong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    auto result = item->deleteRow(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMHTMLTableCaptionElement* webkit_dom_html_table_element_get_caption(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLTableCaptionElement> gobjectResult = WTF::getPtr(item->caption());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_table_element_set_caption(WebKitDOMHTMLTableElement* self, WebKitDOMHTMLTableCaptionElement* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_CAPTION_ELEMENT(value));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WebCore::HTMLTableCaptionElement* convertedValue = WebKit::core(value);
    auto result = item->setCaption(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMHTMLTableSectionElement* webkit_dom_html_table_element_get_t_head(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLTableSectionElement> gobjectResult = WTF::getPtr(item->tHead());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_table_element_set_t_head(WebKitDOMHTMLTableElement* self, WebKitDOMHTMLTableSectionElement* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_SECTION_ELEMENT(value));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WebCore::HTMLTableSectionElement* convertedValue = WebKit::core(value);
    auto result = item->setTHead(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMHTMLTableSectionElement* webkit_dom_html_table_element_get_t_foot(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLTableSectionElement> gobjectResult = WTF::getPtr(item->tFoot());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_table_element_set_t_foot(WebKitDOMHTMLTableElement* self, WebKitDOMHTMLTableSectionElement* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_SECTION_ELEMENT(value));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WebCore::HTMLTableSectionElement* convertedValue = WebKit::core(value);
    auto result = item->setTFoot(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMHTMLCollection* webkit_dom_html_table_element_get_rows(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->rows());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_html_table_element_get_t_bodies(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->tBodies());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_html_table_element_get_align(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_table_element_set_align(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_bg_color(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr));
    return result;
}

void webkit_dom_html_table_element_set_bg_color(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_border(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::borderAttr));
    return result;
}

void webkit_dom_html_table_element_set_border(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::borderAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_cell_padding(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::cellpaddingAttr));
    return result;
}

void webkit_dom_html_table_element_set_cell_padding(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::cellpaddingAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_cell_spacing(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::cellspacingAttr));
    return result;
}

void webkit_dom_html_table_element_set_cell_spacing(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::cellspacingAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_rules(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::rulesAttr));
    return result;
}

void webkit_dom_html_table_element_set_rules(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::rulesAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_summary(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::summaryAttr));
    return result;
}

void webkit_dom_html_table_element_set_summary(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::summaryAttr, convertedValue);
}

gchar* webkit_dom_html_table_element_get_width(WebKitDOMHTMLTableElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self), 0);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::widthAttr));
    return result;
}

void webkit_dom_html_table_element_set_width(WebKitDOMHTMLTableElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TABLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTableElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
