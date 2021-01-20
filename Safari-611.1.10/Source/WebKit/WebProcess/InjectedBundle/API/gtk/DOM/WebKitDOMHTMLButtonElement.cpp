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
#include "WebKitDOMHTMLButtonElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLButtonElementPrivate.h"
#include "WebKitDOMHTMLFormElementPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLButtonElement* kit(WebCore::HTMLButtonElement* obj)
{
    return WEBKIT_DOM_HTML_BUTTON_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLButtonElement* core(WebKitDOMHTMLButtonElement* request)
{
    return request ? static_cast<WebCore::HTMLButtonElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLButtonElement* wrapHTMLButtonElement(WebCore::HTMLButtonElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_BUTTON_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_BUTTON_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_button_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLButtonElement* coreTarget = static_cast<WebCore::HTMLButtonElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_button_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLButtonElement* coreTarget = static_cast<WebCore::HTMLButtonElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_button_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLButtonElement* coreTarget = static_cast<WebCore::HTMLButtonElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_button_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_button_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_button_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_button_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLButtonElement, webkit_dom_html_button_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_button_element_dom_event_target_init))

enum {
    DOM_HTML_BUTTON_ELEMENT_PROP_0,
    DOM_HTML_BUTTON_ELEMENT_PROP_AUTOFOCUS,
    DOM_HTML_BUTTON_ELEMENT_PROP_DISABLED,
    DOM_HTML_BUTTON_ELEMENT_PROP_FORM,
    DOM_HTML_BUTTON_ELEMENT_PROP_TYPE,
    DOM_HTML_BUTTON_ELEMENT_PROP_NAME,
    DOM_HTML_BUTTON_ELEMENT_PROP_VALUE,
    DOM_HTML_BUTTON_ELEMENT_PROP_WILL_VALIDATE,
};

static void webkit_dom_html_button_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLButtonElement* self = WEBKIT_DOM_HTML_BUTTON_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_BUTTON_ELEMENT_PROP_AUTOFOCUS:
        webkit_dom_html_button_element_set_autofocus(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_DISABLED:
        webkit_dom_html_button_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_TYPE:
        webkit_dom_html_button_element_set_button_type(self, g_value_get_string(value));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_NAME:
        webkit_dom_html_button_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_VALUE:
        webkit_dom_html_button_element_set_value(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_button_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLButtonElement* self = WEBKIT_DOM_HTML_BUTTON_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_BUTTON_ELEMENT_PROP_AUTOFOCUS:
        g_value_set_boolean(value, webkit_dom_html_button_element_get_autofocus(self));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_button_element_get_disabled(self));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_FORM:
        g_value_set_object(value, webkit_dom_html_button_element_get_form(self));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_button_element_get_button_type(self));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_button_element_get_name(self));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_VALUE:
        g_value_take_string(value, webkit_dom_html_button_element_get_value(self));
        break;
    case DOM_HTML_BUTTON_ELEMENT_PROP_WILL_VALIDATE:
        g_value_set_boolean(value, webkit_dom_html_button_element_get_will_validate(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_button_element_class_init(WebKitDOMHTMLButtonElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_button_element_set_property;
    gobjectClass->get_property = webkit_dom_html_button_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_AUTOFOCUS,
        g_param_spec_boolean(
            "autofocus",
            "HTMLButtonElement:autofocus",
            "read-write gboolean HTMLButtonElement:autofocus",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLButtonElement:disabled",
            "read-write gboolean HTMLButtonElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_FORM,
        g_param_spec_object(
            "form",
            "HTMLButtonElement:form",
            "read-only WebKitDOMHTMLFormElement* HTMLButtonElement:form",
            WEBKIT_DOM_TYPE_HTML_FORM_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLButtonElement:type",
            "read-write gchar* HTMLButtonElement:type",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLButtonElement:name",
            "read-write gchar* HTMLButtonElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_VALUE,
        g_param_spec_string(
            "value",
            "HTMLButtonElement:value",
            "read-write gchar* HTMLButtonElement:value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_BUTTON_ELEMENT_PROP_WILL_VALIDATE,
        g_param_spec_boolean(
            "will-validate",
            "HTMLButtonElement:will-validate",
            "read-only gboolean HTMLButtonElement:will-validate",
            FALSE,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_html_button_element_init(WebKitDOMHTMLButtonElement* request)
{
    UNUSED_PARAM(request);
}

gboolean webkit_dom_html_button_element_get_autofocus(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), FALSE);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::autofocusAttr);
    return result;
}

void webkit_dom_html_button_element_set_autofocus(WebKitDOMHTMLButtonElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self));
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::autofocusAttr, value);
}

gboolean webkit_dom_html_button_element_get_disabled(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), FALSE);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
    return result;
}

void webkit_dom_html_button_element_set_disabled(WebKitDOMHTMLButtonElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self));
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, value);
}

WebKitDOMHTMLFormElement* webkit_dom_html_button_element_get_form(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), 0);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLFormElement> gobjectResult = WTF::getPtr(item->form());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_html_button_element_get_button_type(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), 0);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

void webkit_dom_html_button_element_set_button_type(WebKitDOMHTMLButtonElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setType(convertedValue);
}

gchar* webkit_dom_html_button_element_get_name(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), 0);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_button_element_set_name(WebKitDOMHTMLButtonElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_button_element_get_value(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), 0);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::valueAttr));
    return result;
}

void webkit_dom_html_button_element_set_value(WebKitDOMHTMLButtonElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::valueAttr, convertedValue);
}

gboolean webkit_dom_html_button_element_get_will_validate(WebKitDOMHTMLButtonElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_BUTTON_ELEMENT(self), FALSE);
    WebCore::HTMLButtonElement* item = WebKit::core(self);
    gboolean result = item->willValidate();
    return result;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
