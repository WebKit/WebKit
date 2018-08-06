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
#include "WebKitDOMHTMLOptionElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLFormElementPrivate.h"
#include "WebKitDOMHTMLOptionElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLOptionElement* kit(WebCore::HTMLOptionElement* obj)
{
    return WEBKIT_DOM_HTML_OPTION_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLOptionElement* core(WebKitDOMHTMLOptionElement* request)
{
    return request ? static_cast<WebCore::HTMLOptionElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLOptionElement* wrapHTMLOptionElement(WebCore::HTMLOptionElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_OPTION_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_OPTION_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_option_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLOptionElement* coreTarget = static_cast<WebCore::HTMLOptionElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_option_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLOptionElement* coreTarget = static_cast<WebCore::HTMLOptionElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_option_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLOptionElement* coreTarget = static_cast<WebCore::HTMLOptionElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_option_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_option_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_option_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_option_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLOptionElement, webkit_dom_html_option_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_option_element_dom_event_target_init))

enum {
    DOM_HTML_OPTION_ELEMENT_PROP_0,
    DOM_HTML_OPTION_ELEMENT_PROP_DISABLED,
    DOM_HTML_OPTION_ELEMENT_PROP_FORM,
    DOM_HTML_OPTION_ELEMENT_PROP_LABEL,
    DOM_HTML_OPTION_ELEMENT_PROP_DEFAULT_SELECTED,
    DOM_HTML_OPTION_ELEMENT_PROP_SELECTED,
    DOM_HTML_OPTION_ELEMENT_PROP_VALUE,
    DOM_HTML_OPTION_ELEMENT_PROP_TEXT,
    DOM_HTML_OPTION_ELEMENT_PROP_INDEX,
};

static void webkit_dom_html_option_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLOptionElement* self = WEBKIT_DOM_HTML_OPTION_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_OPTION_ELEMENT_PROP_DISABLED:
        webkit_dom_html_option_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_LABEL:
        webkit_dom_html_option_element_set_label(self, g_value_get_string(value));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_DEFAULT_SELECTED:
        webkit_dom_html_option_element_set_default_selected(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_SELECTED:
        webkit_dom_html_option_element_set_selected(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_VALUE:
        webkit_dom_html_option_element_set_value(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_option_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLOptionElement* self = WEBKIT_DOM_HTML_OPTION_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_OPTION_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_option_element_get_disabled(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_FORM:
        g_value_set_object(value, webkit_dom_html_option_element_get_form(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_LABEL:
        g_value_take_string(value, webkit_dom_html_option_element_get_label(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_DEFAULT_SELECTED:
        g_value_set_boolean(value, webkit_dom_html_option_element_get_default_selected(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_SELECTED:
        g_value_set_boolean(value, webkit_dom_html_option_element_get_selected(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_VALUE:
        g_value_take_string(value, webkit_dom_html_option_element_get_value(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_TEXT:
        g_value_take_string(value, webkit_dom_html_option_element_get_text(self));
        break;
    case DOM_HTML_OPTION_ELEMENT_PROP_INDEX:
        g_value_set_long(value, webkit_dom_html_option_element_get_index(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_option_element_class_init(WebKitDOMHTMLOptionElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_option_element_set_property;
    gobjectClass->get_property = webkit_dom_html_option_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLOptionElement:disabled",
            "read-write gboolean HTMLOptionElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_FORM,
        g_param_spec_object(
            "form",
            "HTMLOptionElement:form",
            "read-only WebKitDOMHTMLFormElement* HTMLOptionElement:form",
            WEBKIT_DOM_TYPE_HTML_FORM_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_LABEL,
        g_param_spec_string(
            "label",
            "HTMLOptionElement:label",
            "read-write gchar* HTMLOptionElement:label",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_DEFAULT_SELECTED,
        g_param_spec_boolean(
            "default-selected",
            "HTMLOptionElement:default-selected",
            "read-write gboolean HTMLOptionElement:default-selected",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_SELECTED,
        g_param_spec_boolean(
            "selected",
            "HTMLOptionElement:selected",
            "read-write gboolean HTMLOptionElement:selected",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_VALUE,
        g_param_spec_string(
            "value",
            "HTMLOptionElement:value",
            "read-write gchar* HTMLOptionElement:value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_TEXT,
        g_param_spec_string(
            "text",
            "HTMLOptionElement:text",
            "read-only gchar* HTMLOptionElement:text",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTION_ELEMENT_PROP_INDEX,
        g_param_spec_long(
            "index",
            "HTMLOptionElement:index",
            "read-only glong HTMLOptionElement:index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_html_option_element_init(WebKitDOMHTMLOptionElement* request)
{
    UNUSED_PARAM(request);
}

gboolean webkit_dom_html_option_element_get_disabled(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), FALSE);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
    return result;
}

void webkit_dom_html_option_element_set_disabled(WebKitDOMHTMLOptionElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self));
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, value);
}

WebKitDOMHTMLFormElement* webkit_dom_html_option_element_get_form(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), 0);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLFormElement> gobjectResult = WTF::getPtr(item->form());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_html_option_element_get_label(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), 0);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->label());
    return result;
}

void webkit_dom_html_option_element_set_label(WebKitDOMHTMLOptionElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setLabel(convertedValue);
}

gboolean webkit_dom_html_option_element_get_default_selected(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), FALSE);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::selectedAttr);
    return result;
}

void webkit_dom_html_option_element_set_default_selected(WebKitDOMHTMLOptionElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self));
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::selectedAttr, value);
}

gboolean webkit_dom_html_option_element_get_selected(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), FALSE);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    gboolean result = item->selected();
    return result;
}

void webkit_dom_html_option_element_set_selected(WebKitDOMHTMLOptionElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self));
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    item->setSelected(value);
}

gchar* webkit_dom_html_option_element_get_value(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), 0);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->value());
    return result;
}

void webkit_dom_html_option_element_set_value(WebKitDOMHTMLOptionElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setValue(convertedValue);
}

gchar* webkit_dom_html_option_element_get_text(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), 0);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->text());
    return result;
}

glong webkit_dom_html_option_element_get_index(WebKitDOMHTMLOptionElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTION_ELEMENT(self), 0);
    WebCore::HTMLOptionElement* item = WebKit::core(self);
    glong result = item->index();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
