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
#include "WebKitDOMHTMLFormElement.h"

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
#include "WebKitDOMHTMLFormElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLFormElement* kit(WebCore::HTMLFormElement* obj)
{
    return WEBKIT_DOM_HTML_FORM_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLFormElement* core(WebKitDOMHTMLFormElement* request)
{
    return request ? static_cast<WebCore::HTMLFormElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLFormElement* wrapHTMLFormElement(WebCore::HTMLFormElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_FORM_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_FORM_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_form_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLFormElement* coreTarget = static_cast<WebCore::HTMLFormElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_form_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLFormElement* coreTarget = static_cast<WebCore::HTMLFormElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_form_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLFormElement* coreTarget = static_cast<WebCore::HTMLFormElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_form_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_form_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_form_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_form_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLFormElement, webkit_dom_html_form_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_form_element_dom_event_target_init))

enum {
    DOM_HTML_FORM_ELEMENT_PROP_0,
    DOM_HTML_FORM_ELEMENT_PROP_ACCEPT_CHARSET,
    DOM_HTML_FORM_ELEMENT_PROP_ACTION,
    DOM_HTML_FORM_ELEMENT_PROP_ENCTYPE,
    DOM_HTML_FORM_ELEMENT_PROP_ENCODING,
    DOM_HTML_FORM_ELEMENT_PROP_METHOD,
    DOM_HTML_FORM_ELEMENT_PROP_NAME,
    DOM_HTML_FORM_ELEMENT_PROP_TARGET,
    DOM_HTML_FORM_ELEMENT_PROP_ELEMENTS,
    DOM_HTML_FORM_ELEMENT_PROP_LENGTH,
};

static void webkit_dom_html_form_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLFormElement* self = WEBKIT_DOM_HTML_FORM_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_FORM_ELEMENT_PROP_ACCEPT_CHARSET:
        webkit_dom_html_form_element_set_accept_charset(self, g_value_get_string(value));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ACTION:
        webkit_dom_html_form_element_set_action(self, g_value_get_string(value));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ENCTYPE:
        webkit_dom_html_form_element_set_enctype(self, g_value_get_string(value));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ENCODING:
        webkit_dom_html_form_element_set_encoding(self, g_value_get_string(value));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_METHOD:
        webkit_dom_html_form_element_set_method(self, g_value_get_string(value));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_NAME:
        webkit_dom_html_form_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_TARGET:
        webkit_dom_html_form_element_set_target(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_form_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLFormElement* self = WEBKIT_DOM_HTML_FORM_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_FORM_ELEMENT_PROP_ACCEPT_CHARSET:
        g_value_take_string(value, webkit_dom_html_form_element_get_accept_charset(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ACTION:
        g_value_take_string(value, webkit_dom_html_form_element_get_action(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ENCTYPE:
        g_value_take_string(value, webkit_dom_html_form_element_get_enctype(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ENCODING:
        g_value_take_string(value, webkit_dom_html_form_element_get_encoding(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_METHOD:
        g_value_take_string(value, webkit_dom_html_form_element_get_method(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_form_element_get_name(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_TARGET:
        g_value_take_string(value, webkit_dom_html_form_element_get_target(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_ELEMENTS:
        g_value_set_object(value, webkit_dom_html_form_element_get_elements(self));
        break;
    case DOM_HTML_FORM_ELEMENT_PROP_LENGTH:
        g_value_set_long(value, webkit_dom_html_form_element_get_length(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_form_element_class_init(WebKitDOMHTMLFormElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_form_element_set_property;
    gobjectClass->get_property = webkit_dom_html_form_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_ACCEPT_CHARSET,
        g_param_spec_string(
            "accept-charset",
            "HTMLFormElement:accept-charset",
            "read-write gchar* HTMLFormElement:accept-charset",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_ACTION,
        g_param_spec_string(
            "action",
            "HTMLFormElement:action",
            "read-write gchar* HTMLFormElement:action",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_ENCTYPE,
        g_param_spec_string(
            "enctype",
            "HTMLFormElement:enctype",
            "read-write gchar* HTMLFormElement:enctype",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_ENCODING,
        g_param_spec_string(
            "encoding",
            "HTMLFormElement:encoding",
            "read-write gchar* HTMLFormElement:encoding",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_METHOD,
        g_param_spec_string(
            "method",
            "HTMLFormElement:method",
            "read-write gchar* HTMLFormElement:method",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLFormElement:name",
            "read-write gchar* HTMLFormElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_TARGET,
        g_param_spec_string(
            "target",
            "HTMLFormElement:target",
            "read-write gchar* HTMLFormElement:target",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_ELEMENTS,
        g_param_spec_object(
            "elements",
            "HTMLFormElement:elements",
            "read-only WebKitDOMHTMLCollection* HTMLFormElement:elements",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FORM_ELEMENT_PROP_LENGTH,
        g_param_spec_long(
            "length",
            "HTMLFormElement:length",
            "read-only glong HTMLFormElement:length",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_html_form_element_init(WebKitDOMHTMLFormElement* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_html_form_element_submit(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    WebCore::HTMLFormElement* item = WebKit::core(self);
    item->submit();
}

void webkit_dom_html_form_element_reset(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    WebCore::HTMLFormElement* item = WebKit::core(self);
    item->reset();
}

gchar* webkit_dom_html_form_element_get_accept_charset(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::accept_charsetAttr));
    return result;
}

void webkit_dom_html_form_element_set_accept_charset(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::accept_charsetAttr, convertedValue);
}

gchar* webkit_dom_html_form_element_get_action(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::actionAttr));
    return result;
}

void webkit_dom_html_form_element_set_action(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::actionAttr, convertedValue);
}

gchar* webkit_dom_html_form_element_get_enctype(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->enctype());
    return result;
}

void webkit_dom_html_form_element_set_enctype(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setEnctype(convertedValue);
}

gchar* webkit_dom_html_form_element_get_encoding(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->enctype());
    return result;
}

void webkit_dom_html_form_element_set_encoding(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setEnctype(convertedValue);
}

gchar* webkit_dom_html_form_element_get_method(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->method());
    return result;
}

void webkit_dom_html_form_element_set_method(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setMethod(convertedValue);
}

gchar* webkit_dom_html_form_element_get_name(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_form_element_set_name(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_form_element_get_target(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::targetAttr));
    return result;
}

void webkit_dom_html_form_element_set_target(WebKitDOMHTMLFormElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::targetAttr, convertedValue);
}

WebKitDOMHTMLCollection* webkit_dom_html_form_element_get_elements(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->elementsForNativeBindings());
    return WebKit::kit(gobjectResult.get());
}

glong webkit_dom_html_form_element_get_length(WebKitDOMHTMLFormElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FORM_ELEMENT(self), 0);
    WebCore::HTMLFormElement* item = WebKit::core(self);
    glong result = item->length();
    return result;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
