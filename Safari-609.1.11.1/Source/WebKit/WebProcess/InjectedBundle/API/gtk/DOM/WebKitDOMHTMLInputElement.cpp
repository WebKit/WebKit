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
#include "WebKitDOMHTMLInputElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMFileListPrivate.h"
#include "WebKitDOMHTMLElementPrivate.h"
#include "WebKitDOMHTMLFormElementPrivate.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLInputElement* kit(WebCore::HTMLInputElement* obj)
{
    return WEBKIT_DOM_HTML_INPUT_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLInputElement* core(WebKitDOMHTMLInputElement* request)
{
    return request ? static_cast<WebCore::HTMLInputElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLInputElement* wrapHTMLInputElement(WebCore::HTMLInputElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_INPUT_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_input_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLInputElement* coreTarget = static_cast<WebCore::HTMLInputElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_input_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLInputElement* coreTarget = static_cast<WebCore::HTMLInputElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_input_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLInputElement* coreTarget = static_cast<WebCore::HTMLInputElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_input_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_input_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_input_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_input_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLInputElement, webkit_dom_html_input_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_input_element_dom_event_target_init))

enum {
    DOM_HTML_INPUT_ELEMENT_PROP_0,
    DOM_HTML_INPUT_ELEMENT_PROP_ACCEPT,
    DOM_HTML_INPUT_ELEMENT_PROP_ALT,
    DOM_HTML_INPUT_ELEMENT_PROP_AUTOFOCUS,
    DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_CHECKED,
    DOM_HTML_INPUT_ELEMENT_PROP_CHECKED,
    DOM_HTML_INPUT_ELEMENT_PROP_DISABLED,
    DOM_HTML_INPUT_ELEMENT_PROP_FORM,
    DOM_HTML_INPUT_ELEMENT_PROP_FILES,
    DOM_HTML_INPUT_ELEMENT_PROP_HEIGHT,
    DOM_HTML_INPUT_ELEMENT_PROP_INDETERMINATE,
    DOM_HTML_INPUT_ELEMENT_PROP_MAX_LENGTH,
    DOM_HTML_INPUT_ELEMENT_PROP_MULTIPLE,
    DOM_HTML_INPUT_ELEMENT_PROP_NAME,
    DOM_HTML_INPUT_ELEMENT_PROP_READ_ONLY,
    DOM_HTML_INPUT_ELEMENT_PROP_SIZE,
    DOM_HTML_INPUT_ELEMENT_PROP_SRC,
    DOM_HTML_INPUT_ELEMENT_PROP_TYPE,
    DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_VALUE,
    DOM_HTML_INPUT_ELEMENT_PROP_VALUE,
    DOM_HTML_INPUT_ELEMENT_PROP_WIDTH,
    DOM_HTML_INPUT_ELEMENT_PROP_WILL_VALIDATE,
    DOM_HTML_INPUT_ELEMENT_PROP_ALIGN,
    DOM_HTML_INPUT_ELEMENT_PROP_USE_MAP,
    DOM_HTML_INPUT_ELEMENT_PROP_CAPTURE,
};

static void webkit_dom_html_input_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLInputElement* self = WEBKIT_DOM_HTML_INPUT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_INPUT_ELEMENT_PROP_ACCEPT:
        webkit_dom_html_input_element_set_accept(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_ALT:
        webkit_dom_html_input_element_set_alt(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_AUTOFOCUS:
        webkit_dom_html_input_element_set_autofocus(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_CHECKED:
        webkit_dom_html_input_element_set_default_checked(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_CHECKED:
        webkit_dom_html_input_element_set_checked(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_DISABLED:
        webkit_dom_html_input_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_HEIGHT:
        webkit_dom_html_input_element_set_height(self, g_value_get_ulong(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_INDETERMINATE:
        webkit_dom_html_input_element_set_indeterminate(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_MAX_LENGTH:
        webkit_dom_html_input_element_set_max_length(self, g_value_get_long(value), nullptr);
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_MULTIPLE:
        webkit_dom_html_input_element_set_multiple(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_NAME:
        webkit_dom_html_input_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_READ_ONLY:
        webkit_dom_html_input_element_set_read_only(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_SIZE:
        webkit_dom_html_input_element_set_size(self, g_value_get_ulong(value), nullptr);
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_SRC:
        webkit_dom_html_input_element_set_src(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_TYPE:
        webkit_dom_html_input_element_set_input_type(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_VALUE:
        webkit_dom_html_input_element_set_default_value(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_VALUE:
        webkit_dom_html_input_element_set_value(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_WIDTH:
        webkit_dom_html_input_element_set_width(self, g_value_get_ulong(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_ALIGN:
        webkit_dom_html_input_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_USE_MAP:
        webkit_dom_html_input_element_set_use_map(self, g_value_get_string(value));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_CAPTURE:
        webkit_dom_html_input_element_set_capture_type(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_input_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLInputElement* self = WEBKIT_DOM_HTML_INPUT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_INPUT_ELEMENT_PROP_ACCEPT:
        g_value_take_string(value, webkit_dom_html_input_element_get_accept(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_ALT:
        g_value_take_string(value, webkit_dom_html_input_element_get_alt(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_AUTOFOCUS:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_autofocus(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_CHECKED:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_default_checked(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_CHECKED:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_checked(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_disabled(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_FORM:
        g_value_set_object(value, webkit_dom_html_input_element_get_form(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_FILES:
        g_value_set_object(value, webkit_dom_html_input_element_get_files(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_HEIGHT:
        g_value_set_ulong(value, webkit_dom_html_input_element_get_height(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_INDETERMINATE:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_indeterminate(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_MAX_LENGTH:
        g_value_set_long(value, webkit_dom_html_input_element_get_max_length(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_MULTIPLE:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_multiple(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_input_element_get_name(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_READ_ONLY:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_read_only(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_SIZE:
        g_value_set_ulong(value, webkit_dom_html_input_element_get_size(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_SRC:
        g_value_take_string(value, webkit_dom_html_input_element_get_src(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_input_element_get_input_type(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_VALUE:
        g_value_take_string(value, webkit_dom_html_input_element_get_default_value(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_VALUE:
        g_value_take_string(value, webkit_dom_html_input_element_get_value(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_WIDTH:
        g_value_set_ulong(value, webkit_dom_html_input_element_get_width(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_WILL_VALIDATE:
        g_value_set_boolean(value, webkit_dom_html_input_element_get_will_validate(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_input_element_get_align(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_USE_MAP:
        g_value_take_string(value, webkit_dom_html_input_element_get_use_map(self));
        break;
    case DOM_HTML_INPUT_ELEMENT_PROP_CAPTURE:
        g_value_take_string(value, webkit_dom_html_input_element_get_capture_type(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_input_element_class_init(WebKitDOMHTMLInputElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_input_element_set_property;
    gobjectClass->get_property = webkit_dom_html_input_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_ACCEPT,
        g_param_spec_string(
            "accept",
            "HTMLInputElement:accept",
            "read-write gchar* HTMLInputElement:accept",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_ALT,
        g_param_spec_string(
            "alt",
            "HTMLInputElement:alt",
            "read-write gchar* HTMLInputElement:alt",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_AUTOFOCUS,
        g_param_spec_boolean(
            "autofocus",
            "HTMLInputElement:autofocus",
            "read-write gboolean HTMLInputElement:autofocus",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_CHECKED,
        g_param_spec_boolean(
            "default-checked",
            "HTMLInputElement:default-checked",
            "read-write gboolean HTMLInputElement:default-checked",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_CHECKED,
        g_param_spec_boolean(
            "checked",
            "HTMLInputElement:checked",
            "read-write gboolean HTMLInputElement:checked",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLInputElement:disabled",
            "read-write gboolean HTMLInputElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_FORM,
        g_param_spec_object(
            "form",
            "HTMLInputElement:form",
            "read-only WebKitDOMHTMLFormElement* HTMLInputElement:form",
            WEBKIT_DOM_TYPE_HTML_FORM_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_FILES,
        g_param_spec_object(
            "files",
            "HTMLInputElement:files",
            "read-only WebKitDOMFileList* HTMLInputElement:files",
            WEBKIT_DOM_TYPE_FILE_LIST,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_HEIGHT,
        g_param_spec_ulong(
            "height",
            "HTMLInputElement:height",
            "read-write gulong HTMLInputElement:height",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_INDETERMINATE,
        g_param_spec_boolean(
            "indeterminate",
            "HTMLInputElement:indeterminate",
            "read-write gboolean HTMLInputElement:indeterminate",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_MAX_LENGTH,
        g_param_spec_long(
            "max-length",
            "HTMLInputElement:max-length",
            "read-write glong HTMLInputElement:max-length",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_MULTIPLE,
        g_param_spec_boolean(
            "multiple",
            "HTMLInputElement:multiple",
            "read-write gboolean HTMLInputElement:multiple",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLInputElement:name",
            "read-write gchar* HTMLInputElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_READ_ONLY,
        g_param_spec_boolean(
            "read-only",
            "HTMLInputElement:read-only",
            "read-write gboolean HTMLInputElement:read-only",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_SIZE,
        g_param_spec_ulong(
            "size",
            "HTMLInputElement:size",
            "read-write gulong HTMLInputElement:size",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_SRC,
        g_param_spec_string(
            "src",
            "HTMLInputElement:src",
            "read-write gchar* HTMLInputElement:src",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLInputElement:type",
            "read-write gchar* HTMLInputElement:type",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_DEFAULT_VALUE,
        g_param_spec_string(
            "default-value",
            "HTMLInputElement:default-value",
            "read-write gchar* HTMLInputElement:default-value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_VALUE,
        g_param_spec_string(
            "value",
            "HTMLInputElement:value",
            "read-write gchar* HTMLInputElement:value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_WIDTH,
        g_param_spec_ulong(
            "width",
            "HTMLInputElement:width",
            "read-write gulong HTMLInputElement:width",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_WILL_VALIDATE,
        g_param_spec_boolean(
            "will-validate",
            "HTMLInputElement:will-validate",
            "read-only gboolean HTMLInputElement:will-validate",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLInputElement:align",
            "read-write gchar* HTMLInputElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_USE_MAP,
        g_param_spec_string(
            "use-map",
            "HTMLInputElement:use-map",
            "read-write gchar* HTMLInputElement:use-map",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_INPUT_ELEMENT_PROP_CAPTURE,
        g_param_spec_string(
            "capture",
            "HTMLInputElement:capture",
            "read-write gchar* HTMLInputElement:capture",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_input_element_init(WebKitDOMHTMLInputElement* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_html_input_element_select(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->select();
}

gchar* webkit_dom_html_input_element_get_accept(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::acceptAttr));
    return result;
}

void webkit_dom_html_input_element_set_accept(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::acceptAttr, convertedValue);
}

gchar* webkit_dom_html_input_element_get_alt(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::altAttr));
    return result;
}

void webkit_dom_html_input_element_set_alt(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::altAttr, convertedValue);
}

gboolean webkit_dom_html_input_element_get_autofocus(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::autofocusAttr);
    return result;
}

void webkit_dom_html_input_element_set_autofocus(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::autofocusAttr, value);
}

gboolean webkit_dom_html_input_element_get_default_checked(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::checkedAttr);
    return result;
}

void webkit_dom_html_input_element_set_default_checked(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::checkedAttr, value);
}

gboolean webkit_dom_html_input_element_get_checked(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->checked();
    return result;
}

void webkit_dom_html_input_element_set_checked(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setChecked(value);
}

gboolean webkit_dom_html_input_element_get_disabled(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
    return result;
}

void webkit_dom_html_input_element_set_disabled(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, value);
}

WebKitDOMHTMLFormElement* webkit_dom_html_input_element_get_form(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLFormElement> gobjectResult = WTF::getPtr(item->form());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMFileList* webkit_dom_html_input_element_get_files(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    RefPtr<WebCore::FileList> gobjectResult = WTF::getPtr(item->files());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_input_element_set_files(WebKitDOMHTMLInputElement* self, WebKitDOMFileList* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(WEBKIT_DOM_IS_FILE_LIST(value));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WebCore::FileList* convertedValue = WebKit::core(value);
    item->setFiles(convertedValue);
}

gulong webkit_dom_html_input_element_get_height(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gulong result = item->height();
    return result;
}

void webkit_dom_html_input_element_set_height(WebKitDOMHTMLInputElement* self, gulong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setHeight(value);
}

gboolean webkit_dom_html_input_element_get_indeterminate(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->indeterminate();
    return result;
}

void webkit_dom_html_input_element_set_indeterminate(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setIndeterminate(value);
}

glong webkit_dom_html_input_element_get_max_length(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    glong result = item->maxLength();
    return result;
}

void webkit_dom_html_input_element_set_max_length(WebKitDOMHTMLInputElement* self, glong value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    auto result = item->setMaxLength(value);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gboolean webkit_dom_html_input_element_get_multiple(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::multipleAttr);
    return result;
}

void webkit_dom_html_input_element_set_multiple(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::multipleAttr, value);
}

gchar* webkit_dom_html_input_element_get_name(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_input_element_set_name(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gboolean webkit_dom_html_input_element_get_read_only(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::readonlyAttr);
    return result;
}

void webkit_dom_html_input_element_set_read_only(WebKitDOMHTMLInputElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::readonlyAttr, value);
}

gulong webkit_dom_html_input_element_get_size(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gulong result = item->size();
    return result;
}

void webkit_dom_html_input_element_set_size(WebKitDOMHTMLInputElement* self, gulong value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(!error || !*error);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    auto result = item->setSize(value);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gchar* webkit_dom_html_input_element_get_src(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::srcAttr));
    return result;
}

void webkit_dom_html_input_element_set_src(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, convertedValue);
}

gchar* webkit_dom_html_input_element_get_input_type(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

void webkit_dom_html_input_element_set_input_type(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setType(convertedValue);
}

gchar* webkit_dom_html_input_element_get_default_value(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->defaultValue());
    return result;
}

void webkit_dom_html_input_element_set_default_value(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDefaultValue(convertedValue);
}

gchar* webkit_dom_html_input_element_get_value(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->value());
    return result;
}

void webkit_dom_html_input_element_set_value(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setValue(convertedValue);
}

gulong webkit_dom_html_input_element_get_width(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gulong result = item->width();
    return result;
}

void webkit_dom_html_input_element_set_width(WebKitDOMHTMLInputElement* self, gulong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    WebCore::HTMLInputElement* item = WebKit::core(self);
    item->setWidth(value);
}

gboolean webkit_dom_html_input_element_get_will_validate(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gboolean result = item->willValidate();
    return result;
}

gchar* webkit_dom_html_input_element_get_align(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_input_element_set_align(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_input_element_get_use_map(WebKitDOMHTMLInputElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr));
    return result;
}

void webkit_dom_html_input_element_set_use_map(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr, convertedValue);
}

gchar* webkit_dom_html_input_element_get_capture_type(WebKitDOMHTMLInputElement* self)
{
#if ENABLE(MEDIA_CAPTURE)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), 0);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::captureAttr));
    return result;
#else
    UNUSED_PARAM(self);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Media Capture")
    return 0;
#endif /* ENABLE(MEDIA_CAPTURE) */
}

void webkit_dom_html_input_element_set_capture_type(WebKitDOMHTMLInputElement* self, const gchar* value)
{
#if ENABLE(MEDIA_CAPTURE)
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLInputElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::captureAttr, convertedValue);
#else
    UNUSED_PARAM(self);
    UNUSED_PARAM(value);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Media Capture")
#endif /* ENABLE(MEDIA_CAPTURE) */
}

gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(input), FALSE);

    return WebKit::core(input)->lastChangeWasUserEdit();
}

gboolean webkit_dom_html_input_element_get_auto_filled(WebKitDOMHTMLInputElement* self)
{
  g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);

  return WebKit::core(self)->isAutoFilled();
}

void webkit_dom_html_input_element_set_auto_filled(WebKitDOMHTMLInputElement* self, gboolean value)
{
  g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));

  WebKit::core(self)->setAutoFilled(value);
}

void webkit_dom_html_input_element_set_editing_value(WebKitDOMHTMLInputElement* self, const gchar* value)
{
  g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
  g_return_if_fail(value);

  WebKit::core(self)->setValueForUser(WTF::String::fromUTF8(value));
}
G_GNUC_END_IGNORE_DEPRECATIONS;
