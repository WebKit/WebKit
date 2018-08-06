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
#include "WebKitDOMHTMLAppletElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLAppletElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLAppletElement* kit(WebCore::HTMLAppletElement* obj)
{
    return WEBKIT_DOM_HTML_APPLET_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLAppletElement* core(WebKitDOMHTMLAppletElement* request)
{
    return request ? static_cast<WebCore::HTMLAppletElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLAppletElement* wrapHTMLAppletElement(WebCore::HTMLAppletElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_APPLET_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_APPLET_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_applet_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLAppletElement* coreTarget = static_cast<WebCore::HTMLAppletElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_applet_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLAppletElement* coreTarget = static_cast<WebCore::HTMLAppletElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_applet_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLAppletElement* coreTarget = static_cast<WebCore::HTMLAppletElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_applet_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_applet_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_applet_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_applet_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLAppletElement, webkit_dom_html_applet_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_applet_element_dom_event_target_init))

enum {
    DOM_HTML_APPLET_ELEMENT_PROP_0,
    DOM_HTML_APPLET_ELEMENT_PROP_ALIGN,
    DOM_HTML_APPLET_ELEMENT_PROP_ALT,
    DOM_HTML_APPLET_ELEMENT_PROP_ARCHIVE,
    DOM_HTML_APPLET_ELEMENT_PROP_CODE,
    DOM_HTML_APPLET_ELEMENT_PROP_CODE_BASE,
    DOM_HTML_APPLET_ELEMENT_PROP_HEIGHT,
    DOM_HTML_APPLET_ELEMENT_PROP_HSPACE,
    DOM_HTML_APPLET_ELEMENT_PROP_NAME,
    DOM_HTML_APPLET_ELEMENT_PROP_OBJECT,
    DOM_HTML_APPLET_ELEMENT_PROP_VSPACE,
    DOM_HTML_APPLET_ELEMENT_PROP_WIDTH,
};

static void webkit_dom_html_applet_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLAppletElement* self = WEBKIT_DOM_HTML_APPLET_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_APPLET_ELEMENT_PROP_ALIGN:
        webkit_dom_html_applet_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_ALT:
        webkit_dom_html_applet_element_set_alt(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_ARCHIVE:
        webkit_dom_html_applet_element_set_archive(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_CODE:
        webkit_dom_html_applet_element_set_code(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_CODE_BASE:
        webkit_dom_html_applet_element_set_code_base(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_HEIGHT:
        webkit_dom_html_applet_element_set_height(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_HSPACE:
        webkit_dom_html_applet_element_set_hspace(self, g_value_get_long(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_NAME:
        webkit_dom_html_applet_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_OBJECT:
        webkit_dom_html_applet_element_set_object(self, g_value_get_string(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_VSPACE:
        webkit_dom_html_applet_element_set_vspace(self, g_value_get_long(value));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_WIDTH:
        webkit_dom_html_applet_element_set_width(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_applet_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLAppletElement* self = WEBKIT_DOM_HTML_APPLET_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_APPLET_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_applet_element_get_align(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_ALT:
        g_value_take_string(value, webkit_dom_html_applet_element_get_alt(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_ARCHIVE:
        g_value_take_string(value, webkit_dom_html_applet_element_get_archive(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_CODE:
        g_value_take_string(value, webkit_dom_html_applet_element_get_code(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_CODE_BASE:
        g_value_take_string(value, webkit_dom_html_applet_element_get_code_base(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_HEIGHT:
        g_value_take_string(value, webkit_dom_html_applet_element_get_height(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_HSPACE:
        g_value_set_long(value, webkit_dom_html_applet_element_get_hspace(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_applet_element_get_name(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_OBJECT:
        g_value_take_string(value, webkit_dom_html_applet_element_get_object(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_VSPACE:
        g_value_set_long(value, webkit_dom_html_applet_element_get_vspace(self));
        break;
    case DOM_HTML_APPLET_ELEMENT_PROP_WIDTH:
        g_value_take_string(value, webkit_dom_html_applet_element_get_width(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_applet_element_class_init(WebKitDOMHTMLAppletElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_applet_element_set_property;
    gobjectClass->get_property = webkit_dom_html_applet_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLAppletElement:align",
            "read-write gchar* HTMLAppletElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_ALT,
        g_param_spec_string(
            "alt",
            "HTMLAppletElement:alt",
            "read-write gchar* HTMLAppletElement:alt",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_ARCHIVE,
        g_param_spec_string(
            "archive",
            "HTMLAppletElement:archive",
            "read-write gchar* HTMLAppletElement:archive",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_CODE,
        g_param_spec_string(
            "code",
            "HTMLAppletElement:code",
            "read-write gchar* HTMLAppletElement:code",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_CODE_BASE,
        g_param_spec_string(
            "code-base",
            "HTMLAppletElement:code-base",
            "read-write gchar* HTMLAppletElement:code-base",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_HEIGHT,
        g_param_spec_string(
            "height",
            "HTMLAppletElement:height",
            "read-write gchar* HTMLAppletElement:height",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_HSPACE,
        g_param_spec_long(
            "hspace",
            "HTMLAppletElement:hspace",
            "read-write glong HTMLAppletElement:hspace",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLAppletElement:name",
            "read-write gchar* HTMLAppletElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_OBJECT,
        g_param_spec_string(
            "object",
            "HTMLAppletElement:object",
            "read-write gchar* HTMLAppletElement:object",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_VSPACE,
        g_param_spec_long(
            "vspace",
            "HTMLAppletElement:vspace",
            "read-write glong HTMLAppletElement:vspace",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_APPLET_ELEMENT_PROP_WIDTH,
        g_param_spec_string(
            "width",
            "HTMLAppletElement:width",
            "read-write gchar* HTMLAppletElement:width",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_applet_element_init(WebKitDOMHTMLAppletElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_applet_element_get_align(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_applet_element_set_align(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_applet_element_get_alt(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::altAttr));
    return result;
}

void webkit_dom_html_applet_element_set_alt(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::altAttr, convertedValue);
}

gchar* webkit_dom_html_applet_element_get_archive(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::archiveAttr));
    return result;
}

void webkit_dom_html_applet_element_set_archive(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::archiveAttr, convertedValue);
}

gchar* webkit_dom_html_applet_element_get_code(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::codeAttr));
    return result;
}

void webkit_dom_html_applet_element_set_code(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::codeAttr, convertedValue);
}

gchar* webkit_dom_html_applet_element_get_code_base(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::codebaseAttr));
    return result;
}

void webkit_dom_html_applet_element_set_code_base(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::codebaseAttr, convertedValue);
}

gchar* webkit_dom_html_applet_element_get_height(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::heightAttr));
    return result;
}

void webkit_dom_html_applet_element_set_height(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::heightAttr, convertedValue);
}

glong webkit_dom_html_applet_element_get_hspace(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    glong result = item->getIntegralAttribute(WebCore::HTMLNames::hspaceAttr);
    return result;
}

void webkit_dom_html_applet_element_set_hspace(WebKitDOMHTMLAppletElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    item->setIntegralAttribute(WebCore::HTMLNames::hspaceAttr, value);
}

gchar* webkit_dom_html_applet_element_get_name(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_applet_element_set_name(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_applet_element_get_object(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::objectAttr));
    return result;
}

void webkit_dom_html_applet_element_set_object(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::objectAttr, convertedValue);
}

glong webkit_dom_html_applet_element_get_vspace(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    glong result = item->getIntegralAttribute(WebCore::HTMLNames::vspaceAttr);
    return result;
}

void webkit_dom_html_applet_element_set_vspace(WebKitDOMHTMLAppletElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    item->setIntegralAttribute(WebCore::HTMLNames::vspaceAttr, value);
}

gchar* webkit_dom_html_applet_element_get_width(WebKitDOMHTMLAppletElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self), 0);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::widthAttr));
    return result;
}

void webkit_dom_html_applet_element_set_width(WebKitDOMHTMLAppletElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_APPLET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAppletElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
