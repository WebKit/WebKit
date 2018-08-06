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
#include "WebKitDOMHTMLBodyElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLBodyElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLBodyElement* kit(WebCore::HTMLBodyElement* obj)
{
    return WEBKIT_DOM_HTML_BODY_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLBodyElement* core(WebKitDOMHTMLBodyElement* request)
{
    return request ? static_cast<WebCore::HTMLBodyElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLBodyElement* wrapHTMLBodyElement(WebCore::HTMLBodyElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_BODY_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_BODY_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_body_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLBodyElement* coreTarget = static_cast<WebCore::HTMLBodyElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_body_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLBodyElement* coreTarget = static_cast<WebCore::HTMLBodyElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_body_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLBodyElement* coreTarget = static_cast<WebCore::HTMLBodyElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_body_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_body_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_body_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_body_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLBodyElement, webkit_dom_html_body_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_body_element_dom_event_target_init))

enum {
    DOM_HTML_BODY_ELEMENT_PROP_0,
    DOM_HTML_BODY_ELEMENT_PROP_A_LINK,
    DOM_HTML_BODY_ELEMENT_PROP_BACKGROUND,
    DOM_HTML_BODY_ELEMENT_PROP_BG_COLOR,
    DOM_HTML_BODY_ELEMENT_PROP_LINK,
    DOM_HTML_BODY_ELEMENT_PROP_TEXT,
    DOM_HTML_BODY_ELEMENT_PROP_V_LINK,
};

static void webkit_dom_html_body_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLBodyElement* self = WEBKIT_DOM_HTML_BODY_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_BODY_ELEMENT_PROP_A_LINK:
        webkit_dom_html_body_element_set_a_link(self, g_value_get_string(value));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_BACKGROUND:
        webkit_dom_html_body_element_set_background(self, g_value_get_string(value));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_BG_COLOR:
        webkit_dom_html_body_element_set_bg_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_LINK:
        webkit_dom_html_body_element_set_link(self, g_value_get_string(value));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_TEXT:
        webkit_dom_html_body_element_set_text(self, g_value_get_string(value));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_V_LINK:
        webkit_dom_html_body_element_set_v_link(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_body_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLBodyElement* self = WEBKIT_DOM_HTML_BODY_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_BODY_ELEMENT_PROP_A_LINK:
        g_value_take_string(value, webkit_dom_html_body_element_get_a_link(self));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_BACKGROUND:
        g_value_take_string(value, webkit_dom_html_body_element_get_background(self));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_BG_COLOR:
        g_value_take_string(value, webkit_dom_html_body_element_get_bg_color(self));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_LINK:
        g_value_take_string(value, webkit_dom_html_body_element_get_link(self));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_TEXT:
        g_value_take_string(value, webkit_dom_html_body_element_get_text(self));
        break;
    case DOM_HTML_BODY_ELEMENT_PROP_V_LINK:
        g_value_take_string(value, webkit_dom_html_body_element_get_v_link(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_body_element_class_init(WebKitDOMHTMLBodyElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_body_element_set_property;
    gobjectClass->get_property = webkit_dom_html_body_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BODY_ELEMENT_PROP_A_LINK,
        g_param_spec_string(
            "a-link",
            "HTMLBodyElement:a-link",
            "read-write gchar* HTMLBodyElement:a-link",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BODY_ELEMENT_PROP_BACKGROUND,
        g_param_spec_string(
            "background",
            "HTMLBodyElement:background",
            "read-write gchar* HTMLBodyElement:background",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BODY_ELEMENT_PROP_BG_COLOR,
        g_param_spec_string(
            "bg-color",
            "HTMLBodyElement:bg-color",
            "read-write gchar* HTMLBodyElement:bg-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BODY_ELEMENT_PROP_LINK,
        g_param_spec_string(
            "link",
            "HTMLBodyElement:link",
            "read-write gchar* HTMLBodyElement:link",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BODY_ELEMENT_PROP_TEXT,
        g_param_spec_string(
            "text",
            "HTMLBodyElement:text",
            "read-write gchar* HTMLBodyElement:text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BODY_ELEMENT_PROP_V_LINK,
        g_param_spec_string(
            "v-link",
            "HTMLBodyElement:v-link",
            "read-write gchar* HTMLBodyElement:v-link",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_body_element_init(WebKitDOMHTMLBodyElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_body_element_get_a_link(WebKitDOMHTMLBodyElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self), 0);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alinkAttr));
    return result;
}

void webkit_dom_html_body_element_set_a_link(WebKitDOMHTMLBodyElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alinkAttr, convertedValue);
}

gchar* webkit_dom_html_body_element_get_background(WebKitDOMHTMLBodyElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self), 0);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::backgroundAttr));
    return result;
}

void webkit_dom_html_body_element_set_background(WebKitDOMHTMLBodyElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::backgroundAttr, convertedValue);
}

gchar* webkit_dom_html_body_element_get_bg_color(WebKitDOMHTMLBodyElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self), 0);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr));
    return result;
}

void webkit_dom_html_body_element_set_bg_color(WebKitDOMHTMLBodyElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr, convertedValue);
}

gchar* webkit_dom_html_body_element_get_link(WebKitDOMHTMLBodyElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self), 0);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::linkAttr));
    return result;
}

void webkit_dom_html_body_element_set_link(WebKitDOMHTMLBodyElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::linkAttr, convertedValue);
}

gchar* webkit_dom_html_body_element_get_text(WebKitDOMHTMLBodyElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self), 0);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::textAttr));
    return result;
}

void webkit_dom_html_body_element_set_text(WebKitDOMHTMLBodyElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::textAttr, convertedValue);
}

gchar* webkit_dom_html_body_element_get_v_link(WebKitDOMHTMLBodyElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self), 0);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::vlinkAttr));
    return result;
}

void webkit_dom_html_body_element_set_v_link(WebKitDOMHTMLBodyElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLBodyElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::vlinkAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
