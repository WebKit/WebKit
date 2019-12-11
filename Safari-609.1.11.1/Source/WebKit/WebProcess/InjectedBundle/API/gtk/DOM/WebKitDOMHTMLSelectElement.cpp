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
#include "WebKitDOMHTMLSelectElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLOptGroupElement.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMHTMLElementPrivate.h"
#include "WebKitDOMHTMLFormElementPrivate.h"
#include "WebKitDOMHTMLOptionsCollectionPrivate.h"
#include "WebKitDOMHTMLSelectElementPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLSelectElement* kit(WebCore::HTMLSelectElement* obj)
{
    return WEBKIT_DOM_HTML_SELECT_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLSelectElement* core(WebKitDOMHTMLSelectElement* request)
{
    return request ? static_cast<WebCore::HTMLSelectElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLSelectElement* wrapHTMLSelectElement(WebCore::HTMLSelectElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_SELECT_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_SELECT_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_select_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLSelectElement* coreTarget = static_cast<WebCore::HTMLSelectElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_select_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLSelectElement* coreTarget = static_cast<WebCore::HTMLSelectElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_select_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLSelectElement* coreTarget = static_cast<WebCore::HTMLSelectElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_select_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_select_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_select_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_select_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLSelectElement, webkit_dom_html_select_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_select_element_dom_event_target_init))

enum {
    DOM_HTML_SELECT_ELEMENT_PROP_0,
    DOM_HTML_SELECT_ELEMENT_PROP_AUTOFOCUS,
    DOM_HTML_SELECT_ELEMENT_PROP_DISABLED,
    DOM_HTML_SELECT_ELEMENT_PROP_FORM,
    DOM_HTML_SELECT_ELEMENT_PROP_MULTIPLE,
    DOM_HTML_SELECT_ELEMENT_PROP_NAME,
    DOM_HTML_SELECT_ELEMENT_PROP_SIZE,
    DOM_HTML_SELECT_ELEMENT_PROP_TYPE,
    DOM_HTML_SELECT_ELEMENT_PROP_OPTIONS,
    DOM_HTML_SELECT_ELEMENT_PROP_LENGTH,
    DOM_HTML_SELECT_ELEMENT_PROP_SELECTED_INDEX,
    DOM_HTML_SELECT_ELEMENT_PROP_VALUE,
    DOM_HTML_SELECT_ELEMENT_PROP_WILL_VALIDATE,
};

static void webkit_dom_html_select_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLSelectElement* self = WEBKIT_DOM_HTML_SELECT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_SELECT_ELEMENT_PROP_AUTOFOCUS:
        webkit_dom_html_select_element_set_autofocus(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_DISABLED:
        webkit_dom_html_select_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_MULTIPLE:
        webkit_dom_html_select_element_set_multiple(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_NAME:
        webkit_dom_html_select_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_SIZE:
        webkit_dom_html_select_element_set_size(self, g_value_get_long(value));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_LENGTH:
        webkit_dom_html_select_element_set_length(self, g_value_get_ulong(value), nullptr);
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_SELECTED_INDEX:
        webkit_dom_html_select_element_set_selected_index(self, g_value_get_long(value));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_VALUE:
        webkit_dom_html_select_element_set_value(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_select_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLSelectElement* self = WEBKIT_DOM_HTML_SELECT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_SELECT_ELEMENT_PROP_AUTOFOCUS:
        g_value_set_boolean(value, webkit_dom_html_select_element_get_autofocus(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_select_element_get_disabled(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_FORM:
        g_value_set_object(value, webkit_dom_html_select_element_get_form(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_MULTIPLE:
        g_value_set_boolean(value, webkit_dom_html_select_element_get_multiple(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_select_element_get_name(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_SIZE:
        g_value_set_long(value, webkit_dom_html_select_element_get_size(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_select_element_get_select_type(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_OPTIONS:
        g_value_set_object(value, webkit_dom_html_select_element_get_options(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_html_select_element_get_length(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_SELECTED_INDEX:
        g_value_set_long(value, webkit_dom_html_select_element_get_selected_index(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_VALUE:
        g_value_take_string(value, webkit_dom_html_select_element_get_value(self));
        break;
    case DOM_HTML_SELECT_ELEMENT_PROP_WILL_VALIDATE:
        g_value_set_boolean(value, webkit_dom_html_select_element_get_will_validate(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_select_element_class_init(WebKitDOMHTMLSelectElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_select_element_set_property;
    gobjectClass->get_property = webkit_dom_html_select_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_AUTOFOCUS,
        g_param_spec_boolean(
            "autofocus",
            "HTMLSelectElement:autofocus",
            "read-write gboolean HTMLSelectElement:autofocus",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLSelectElement:disabled",
            "read-write gboolean HTMLSelectElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_FORM,
        g_param_spec_object(
            "form",
            "HTMLSelectElement:form",
            "read-only WebKitDOMHTMLFormElement* HTMLSelectElement:form",
            WEBKIT_DOM_TYPE_HTML_FORM_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_MULTIPLE,
        g_param_spec_boolean(
            "multiple",
            "HTMLSelectElement:multiple",
            "read-write gboolean HTMLSelectElement:multiple",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLSelectElement:name",
            "read-write gchar* HTMLSelectElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_SIZE,
        g_param_spec_long(
            "size",
            "HTMLSelectElement:size",
            "read-write glong HTMLSelectElement:size",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLSelectElement:type",
            "read-only gchar* HTMLSelectElement:type",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_OPTIONS,
        g_param_spec_object(
            "options",
            "HTMLSelectElement:options",
            "read-only WebKitDOMHTMLOptionsCollection* HTMLSelectElement:options",
            WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "HTMLSelectElement:length",
            "read-write gulong HTMLSelectElement:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_SELECTED_INDEX,
        g_param_spec_long(
            "selected-index",
            "HTMLSelectElement:selected-index",
            "read-write glong HTMLSelectElement:selected-index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_VALUE,
        g_param_spec_string(
            "value",
            "HTMLSelectElement:value",
            "read-write gchar* HTMLSelectElement:value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_SELECT_ELEMENT_PROP_WILL_VALIDATE,
        g_param_spec_boolean(
            "will-validate",
            "HTMLSelectElement:will-validate",
            "read-only gboolean HTMLSelectElement:will-validate",
            FALSE,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_html_select_element_init(WebKitDOMHTMLSelectElement* request)
{
    UNUSED_PARAM(request);
}

WebKitDOMNode* webkit_dom_html_select_element_item(WebKitDOMHTMLSelectElement* self, gulong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->item(index));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_html_select_element_named_item(WebKitDOMHTMLSelectElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->namedItem(convertedName));
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_select_element_add(WebKitDOMHTMLSelectElement* self, WebKitDOMHTMLElement* element, WebKitDOMHTMLElement* before, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(element));
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(before));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    WebCore::HTMLElement* convertedElement = WebKit::core(element);
    WebCore::HTMLElement* convertedBefore = WebKit::core(before);
    Variant<RefPtr<WebCore::HTMLOptionElement>, RefPtr<WebCore::HTMLOptGroupElement>> variantElement;
    if (is<WebCore::HTMLOptionElement>(convertedElement))
        variantElement = &downcast<WebCore::HTMLOptionElement>(*convertedElement);
    else if (is<WebCore::HTMLOptGroupElement>(convertedElement))
        variantElement = &downcast<WebCore::HTMLOptGroupElement>(*convertedElement);
    else {
        auto description = WebCore::DOMException::description(WebCore::TypeError);
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return;
    }

    auto exception = item->add(WTFMove(variantElement), WebCore::HTMLSelectElement::HTMLElementOrInt(convertedBefore));
    if (exception.hasException()) {
        auto description = WebCore::DOMException::description(exception.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_html_select_element_remove(WebKitDOMHTMLSelectElement* self, glong index)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    item->remove(index);
}

gboolean webkit_dom_html_select_element_get_autofocus(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), FALSE);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::autofocusAttr);
    return result;
}

void webkit_dom_html_select_element_set_autofocus(WebKitDOMHTMLSelectElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::autofocusAttr, value);
}

gboolean webkit_dom_html_select_element_get_disabled(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), FALSE);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
    return result;
}

void webkit_dom_html_select_element_set_disabled(WebKitDOMHTMLSelectElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, value);
}

WebKitDOMHTMLFormElement* webkit_dom_html_select_element_get_form(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLFormElement> gobjectResult = WTF::getPtr(item->form());
    return WebKit::kit(gobjectResult.get());
}

gboolean webkit_dom_html_select_element_get_multiple(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), FALSE);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gboolean result = item->multiple();
    return result;
}

void webkit_dom_html_select_element_set_multiple(WebKitDOMHTMLSelectElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    item->setMultiple(value);
}

gchar* webkit_dom_html_select_element_get_name(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_select_element_set_name(WebKitDOMHTMLSelectElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

glong webkit_dom_html_select_element_get_size(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    glong result = item->size();
    return result;
}

void webkit_dom_html_select_element_set_size(WebKitDOMHTMLSelectElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    item->setSize(value);
}

gchar* webkit_dom_html_select_element_get_select_type(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

WebKitDOMHTMLOptionsCollection* webkit_dom_html_select_element_get_options(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLOptionsCollection> gobjectResult = WTF::getPtr(item->options());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_html_select_element_get_length(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

void webkit_dom_html_select_element_set_length(WebKitDOMHTMLSelectElement* self, gulong value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    auto result = item->setLength(value);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

glong webkit_dom_html_select_element_get_selected_index(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    glong result = item->selectedIndex();
    return result;
}

void webkit_dom_html_select_element_set_selected_index(WebKitDOMHTMLSelectElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    item->setSelectedIndex(value);
}

gchar* webkit_dom_html_select_element_get_value(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), 0);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->value());
    return result;
}

void webkit_dom_html_select_element_set_value(WebKitDOMHTMLSelectElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setValue(convertedValue);
}

gboolean webkit_dom_html_select_element_get_will_validate(WebKitDOMHTMLSelectElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(self), FALSE);
    WebCore::HTMLSelectElement* item = WebKit::core(self);
    gboolean result = item->willValidate();
    return result;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
