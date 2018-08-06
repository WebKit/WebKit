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
#include "WebKitDOMHTMLTextAreaElement.h"

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
#include "WebKitDOMHTMLTextAreaElementPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLTextAreaElement* kit(WebCore::HTMLTextAreaElement* obj)
{
    return WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLTextAreaElement* core(WebKitDOMHTMLTextAreaElement* request)
{
    return request ? static_cast<WebCore::HTMLTextAreaElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLTextAreaElement* wrapHTMLTextAreaElement(WebCore::HTMLTextAreaElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_TEXT_AREA_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_text_area_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLTextAreaElement* coreTarget = static_cast<WebCore::HTMLTextAreaElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_text_area_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTextAreaElement* coreTarget = static_cast<WebCore::HTMLTextAreaElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_text_area_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTextAreaElement* coreTarget = static_cast<WebCore::HTMLTextAreaElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_text_area_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_text_area_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_text_area_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_text_area_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLTextAreaElement, webkit_dom_html_text_area_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_text_area_element_dom_event_target_init))

enum {
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_0,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_AUTOFOCUS,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_DISABLED,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_FORM,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_NAME,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_READ_ONLY,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_ROWS,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_COLS,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_TYPE,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_DEFAULT_VALUE,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_VALUE,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_WILL_VALIDATE,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_START,
    DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_END,
};

static void webkit_dom_html_text_area_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTextAreaElement* self = WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_AUTOFOCUS:
        webkit_dom_html_text_area_element_set_autofocus(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_DISABLED:
        webkit_dom_html_text_area_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_NAME:
        webkit_dom_html_text_area_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_READ_ONLY:
        webkit_dom_html_text_area_element_set_read_only(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_ROWS:
        webkit_dom_html_text_area_element_set_rows(self, g_value_get_long(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_COLS:
        webkit_dom_html_text_area_element_set_cols(self, g_value_get_long(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_DEFAULT_VALUE:
        webkit_dom_html_text_area_element_set_default_value(self, g_value_get_string(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_VALUE:
        webkit_dom_html_text_area_element_set_value(self, g_value_get_string(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_START:
        webkit_dom_html_text_area_element_set_selection_start(self, g_value_get_long(value));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_END:
        webkit_dom_html_text_area_element_set_selection_end(self, g_value_get_long(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_text_area_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTextAreaElement* self = WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_AUTOFOCUS:
        g_value_set_boolean(value, webkit_dom_html_text_area_element_get_autofocus(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_text_area_element_get_disabled(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_FORM:
        g_value_set_object(value, webkit_dom_html_text_area_element_get_form(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_text_area_element_get_name(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_READ_ONLY:
        g_value_set_boolean(value, webkit_dom_html_text_area_element_get_read_only(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_ROWS:
        g_value_set_long(value, webkit_dom_html_text_area_element_get_rows(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_COLS:
        g_value_set_long(value, webkit_dom_html_text_area_element_get_cols(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_text_area_element_get_area_type(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_DEFAULT_VALUE:
        g_value_take_string(value, webkit_dom_html_text_area_element_get_default_value(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_VALUE:
        g_value_take_string(value, webkit_dom_html_text_area_element_get_value(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_WILL_VALIDATE:
        g_value_set_boolean(value, webkit_dom_html_text_area_element_get_will_validate(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_START:
        g_value_set_long(value, webkit_dom_html_text_area_element_get_selection_start(self));
        break;
    case DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_END:
        g_value_set_long(value, webkit_dom_html_text_area_element_get_selection_end(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_text_area_element_class_init(WebKitDOMHTMLTextAreaElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_text_area_element_set_property;
    gobjectClass->get_property = webkit_dom_html_text_area_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_AUTOFOCUS,
        g_param_spec_boolean(
            "autofocus",
            "HTMLTextAreaElement:autofocus",
            "read-write gboolean HTMLTextAreaElement:autofocus",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLTextAreaElement:disabled",
            "read-write gboolean HTMLTextAreaElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_FORM,
        g_param_spec_object(
            "form",
            "HTMLTextAreaElement:form",
            "read-only WebKitDOMHTMLFormElement* HTMLTextAreaElement:form",
            WEBKIT_DOM_TYPE_HTML_FORM_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLTextAreaElement:name",
            "read-write gchar* HTMLTextAreaElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_READ_ONLY,
        g_param_spec_boolean(
            "read-only",
            "HTMLTextAreaElement:read-only",
            "read-write gboolean HTMLTextAreaElement:read-only",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_ROWS,
        g_param_spec_long(
            "rows",
            "HTMLTextAreaElement:rows",
            "read-write glong HTMLTextAreaElement:rows",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_COLS,
        g_param_spec_long(
            "cols",
            "HTMLTextAreaElement:cols",
            "read-write glong HTMLTextAreaElement:cols",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLTextAreaElement:type",
            "read-only gchar* HTMLTextAreaElement:type",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_DEFAULT_VALUE,
        g_param_spec_string(
            "default-value",
            "HTMLTextAreaElement:default-value",
            "read-write gchar* HTMLTextAreaElement:default-value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_VALUE,
        g_param_spec_string(
            "value",
            "HTMLTextAreaElement:value",
            "read-write gchar* HTMLTextAreaElement:value",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_WILL_VALIDATE,
        g_param_spec_boolean(
            "will-validate",
            "HTMLTextAreaElement:will-validate",
            "read-only gboolean HTMLTextAreaElement:will-validate",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_START,
        g_param_spec_long(
            "selection-start",
            "HTMLTextAreaElement:selection-start",
            "read-write glong HTMLTextAreaElement:selection-start",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_TEXT_AREA_ELEMENT_PROP_SELECTION_END,
        g_param_spec_long(
            "selection-end",
            "HTMLTextAreaElement:selection-end",
            "read-write glong HTMLTextAreaElement:selection-end",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));
}

static void webkit_dom_html_text_area_element_init(WebKitDOMHTMLTextAreaElement* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_html_text_area_element_select(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->select();
}

void webkit_dom_html_text_area_element_set_selection_range(WebKitDOMHTMLTextAreaElement* self, glong start, glong end, const gchar* direction)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    g_return_if_fail(direction);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    WTF::String convertedDirection = WTF::String::fromUTF8(direction);
    item->setSelectionRange(start, end, convertedDirection);
}

gboolean webkit_dom_html_text_area_element_get_autofocus(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), FALSE);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::autofocusAttr);
    return result;
}

void webkit_dom_html_text_area_element_set_autofocus(WebKitDOMHTMLTextAreaElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::autofocusAttr, value);
}

gboolean webkit_dom_html_text_area_element_get_disabled(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), FALSE);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
    return result;
}

void webkit_dom_html_text_area_element_set_disabled(WebKitDOMHTMLTextAreaElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, value);
}

WebKitDOMHTMLFormElement* webkit_dom_html_text_area_element_get_form(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    RefPtr<WebCore::HTMLFormElement> gobjectResult = WTF::getPtr(item->form());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_html_text_area_element_get_name(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_text_area_element_set_name(WebKitDOMHTMLTextAreaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gboolean webkit_dom_html_text_area_element_get_read_only(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), FALSE);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::readonlyAttr);
    return result;
}

void webkit_dom_html_text_area_element_set_read_only(WebKitDOMHTMLTextAreaElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::readonlyAttr, value);
}

glong webkit_dom_html_text_area_element_get_rows(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    glong result = item->rows();
    return result;
}

void webkit_dom_html_text_area_element_set_rows(WebKitDOMHTMLTextAreaElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setRows(value);
}

glong webkit_dom_html_text_area_element_get_cols(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    glong result = item->cols();
    return result;
}

void webkit_dom_html_text_area_element_set_cols(WebKitDOMHTMLTextAreaElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setCols(value);
}

gchar* webkit_dom_html_text_area_element_get_area_type(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

gchar* webkit_dom_html_text_area_element_get_default_value(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->defaultValue());
    return result;
}

void webkit_dom_html_text_area_element_set_default_value(WebKitDOMHTMLTextAreaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDefaultValue(convertedValue);
}

gchar* webkit_dom_html_text_area_element_get_value(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->value());
    return result;
}

void webkit_dom_html_text_area_element_set_value(WebKitDOMHTMLTextAreaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setValue(convertedValue);
}

gboolean webkit_dom_html_text_area_element_get_will_validate(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), FALSE);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    gboolean result = item->willValidate();
    return result;
}

glong webkit_dom_html_text_area_element_get_selection_start(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    glong result = item->selectionStart();
    return result;
}

void webkit_dom_html_text_area_element_set_selection_start(WebKitDOMHTMLTextAreaElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setSelectionStart(value);
}

glong webkit_dom_html_text_area_element_get_selection_end(WebKitDOMHTMLTextAreaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self), 0);
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    glong result = item->selectionEnd();
    return result;
}

void webkit_dom_html_text_area_element_set_selection_end(WebKitDOMHTMLTextAreaElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(self));
    WebCore::HTMLTextAreaElement* item = WebKit::core(self);
    item->setSelectionEnd(value);
}

gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* area)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(area), FALSE);

    return WebKit::core(area)->lastChangeWasUserEdit();
}
G_GNUC_END_IGNORE_DEPRECATIONS;
