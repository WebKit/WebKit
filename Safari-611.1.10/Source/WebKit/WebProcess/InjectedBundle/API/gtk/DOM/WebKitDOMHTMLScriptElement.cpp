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
#include "WebKitDOMHTMLScriptElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLScriptElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLScriptElement* kit(WebCore::HTMLScriptElement* obj)
{
    return WEBKIT_DOM_HTML_SCRIPT_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLScriptElement* core(WebKitDOMHTMLScriptElement* request)
{
    return request ? static_cast<WebCore::HTMLScriptElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLScriptElement* wrapHTMLScriptElement(WebCore::HTMLScriptElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_SCRIPT_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_SCRIPT_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_script_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLScriptElement* coreTarget = static_cast<WebCore::HTMLScriptElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_script_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLScriptElement* coreTarget = static_cast<WebCore::HTMLScriptElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_script_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLScriptElement* coreTarget = static_cast<WebCore::HTMLScriptElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_script_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_script_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_script_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_script_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLScriptElement, webkit_dom_html_script_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_script_element_dom_event_target_init))

enum {
    DOM_HTML_SCRIPT_ELEMENT_PROP_0,
    DOM_HTML_SCRIPT_ELEMENT_PROP_TEXT,
    DOM_HTML_SCRIPT_ELEMENT_PROP_HTML_FOR,
    DOM_HTML_SCRIPT_ELEMENT_PROP_EVENT,
    DOM_HTML_SCRIPT_ELEMENT_PROP_CHARSET,
    DOM_HTML_SCRIPT_ELEMENT_PROP_DEFER,
    DOM_HTML_SCRIPT_ELEMENT_PROP_SRC,
    DOM_HTML_SCRIPT_ELEMENT_PROP_TYPE,
};

static void webkit_dom_html_script_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLScriptElement* self = WEBKIT_DOM_HTML_SCRIPT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_SCRIPT_ELEMENT_PROP_TEXT:
        webkit_dom_html_script_element_set_text(self, g_value_get_string(value));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_HTML_FOR:
        webkit_dom_html_script_element_set_html_for(self, g_value_get_string(value));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_EVENT:
        webkit_dom_html_script_element_set_event(self, g_value_get_string(value));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_CHARSET:
        webkit_dom_html_script_element_set_charset(self, g_value_get_string(value));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_DEFER:
        webkit_dom_html_script_element_set_defer(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_SRC:
        webkit_dom_html_script_element_set_src(self, g_value_get_string(value));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_TYPE:
        webkit_dom_html_script_element_set_type_attr(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_script_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLScriptElement* self = WEBKIT_DOM_HTML_SCRIPT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_SCRIPT_ELEMENT_PROP_TEXT:
        g_value_take_string(value, webkit_dom_html_script_element_get_text(self));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_HTML_FOR:
        g_value_take_string(value, webkit_dom_html_script_element_get_html_for(self));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_EVENT:
        g_value_take_string(value, webkit_dom_html_script_element_get_event(self));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_CHARSET:
        g_value_take_string(value, webkit_dom_html_script_element_get_charset(self));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_DEFER:
        g_value_set_boolean(value, webkit_dom_html_script_element_get_defer(self));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_SRC:
        g_value_take_string(value, webkit_dom_html_script_element_get_src(self));
        break;
    case DOM_HTML_SCRIPT_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_script_element_get_type_attr(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_script_element_class_init(WebKitDOMHTMLScriptElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_script_element_set_property;
    gobjectClass->get_property = webkit_dom_html_script_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_TEXT,
        g_param_spec_string(
            "text",
            "HTMLScriptElement:text",
            "read-write gchar* HTMLScriptElement:text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_HTML_FOR,
        g_param_spec_string(
            "html-for",
            "HTMLScriptElement:html-for",
            "read-write gchar* HTMLScriptElement:html-for",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_EVENT,
        g_param_spec_string(
            "event",
            "HTMLScriptElement:event",
            "read-write gchar* HTMLScriptElement:event",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_CHARSET,
        g_param_spec_string(
            "charset",
            "HTMLScriptElement:charset",
            "read-write gchar* HTMLScriptElement:charset",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_DEFER,
        g_param_spec_boolean(
            "defer",
            "HTMLScriptElement:defer",
            "read-write gboolean HTMLScriptElement:defer",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_SRC,
        g_param_spec_string(
            "src",
            "HTMLScriptElement:src",
            "read-write gchar* HTMLScriptElement:src",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SCRIPT_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLScriptElement:type",
            "read-write gchar* HTMLScriptElement:type",
            "",
            WEBKIT_PARAM_READWRITE));
}

static void webkit_dom_html_script_element_init(WebKitDOMHTMLScriptElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_script_element_get_text(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), 0);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->text());
    return result;
}

void webkit_dom_html_script_element_set_text(WebKitDOMHTMLScriptElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setText(convertedValue);
}

gchar* webkit_dom_html_script_element_get_html_for(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), 0);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::forAttr));
    return result;
}

void webkit_dom_html_script_element_set_html_for(WebKitDOMHTMLScriptElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::forAttr, convertedValue);
}

gchar* webkit_dom_html_script_element_get_event(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), 0);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::eventAttr));
    return result;
}

void webkit_dom_html_script_element_set_event(WebKitDOMHTMLScriptElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::eventAttr, convertedValue);
}

gchar* webkit_dom_html_script_element_get_charset(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), 0);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr));
    return result;
}

void webkit_dom_html_script_element_set_charset(WebKitDOMHTMLScriptElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr, convertedValue);
}

gboolean webkit_dom_html_script_element_get_defer(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), FALSE);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::deferAttr);
    return result;
}

void webkit_dom_html_script_element_set_defer(WebKitDOMHTMLScriptElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::deferAttr, value);
}

gchar* webkit_dom_html_script_element_get_src(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), 0);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::srcAttr));
    return result;
}

void webkit_dom_html_script_element_set_src(WebKitDOMHTMLScriptElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, convertedValue);
}

gchar* webkit_dom_html_script_element_get_type_attr(WebKitDOMHTMLScriptElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self), 0);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr));
    return result;
}

void webkit_dom_html_script_element_set_type_attr(WebKitDOMHTMLScriptElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SCRIPT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLScriptElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, convertedValue);
}
G_GNUC_END_IGNORE_DEPRECATIONS;
