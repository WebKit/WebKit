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
#include "WebKitDOMHTMLStyleElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLStyleElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMStyleSheetPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLStyleElement* kit(WebCore::HTMLStyleElement* obj)
{
    return WEBKIT_DOM_HTML_STYLE_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLStyleElement* core(WebKitDOMHTMLStyleElement* request)
{
    return request ? static_cast<WebCore::HTMLStyleElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLStyleElement* wrapHTMLStyleElement(WebCore::HTMLStyleElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_STYLE_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_STYLE_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_style_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLStyleElement* coreTarget = static_cast<WebCore::HTMLStyleElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_style_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLStyleElement* coreTarget = static_cast<WebCore::HTMLStyleElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_style_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLStyleElement* coreTarget = static_cast<WebCore::HTMLStyleElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_style_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_style_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_style_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_style_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLStyleElement, webkit_dom_html_style_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_style_element_dom_event_target_init))

enum {
    DOM_HTML_STYLE_ELEMENT_PROP_0,
    DOM_HTML_STYLE_ELEMENT_PROP_DISABLED,
    DOM_HTML_STYLE_ELEMENT_PROP_MEDIA,
    DOM_HTML_STYLE_ELEMENT_PROP_TYPE,
    DOM_HTML_STYLE_ELEMENT_PROP_SHEET,
};

static void webkit_dom_html_style_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLStyleElement* self = WEBKIT_DOM_HTML_STYLE_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_STYLE_ELEMENT_PROP_DISABLED:
        webkit_dom_html_style_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_STYLE_ELEMENT_PROP_MEDIA:
        webkit_dom_html_style_element_set_media(self, g_value_get_string(value));
        break;
    case DOM_HTML_STYLE_ELEMENT_PROP_TYPE:
        webkit_dom_html_style_element_set_type_attr(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_style_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLStyleElement* self = WEBKIT_DOM_HTML_STYLE_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_STYLE_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_style_element_get_disabled(self));
        break;
    case DOM_HTML_STYLE_ELEMENT_PROP_MEDIA:
        g_value_take_string(value, webkit_dom_html_style_element_get_media(self));
        break;
    case DOM_HTML_STYLE_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_style_element_get_type_attr(self));
        break;
    case DOM_HTML_STYLE_ELEMENT_PROP_SHEET:
        g_value_set_object(value, webkit_dom_html_style_element_get_sheet(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_style_element_class_init(WebKitDOMHTMLStyleElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_style_element_set_property;
    gobjectClass->get_property = webkit_dom_html_style_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_STYLE_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLStyleElement:disabled",
            "read-write gboolean HTMLStyleElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_STYLE_ELEMENT_PROP_MEDIA,
        g_param_spec_string(
            "media",
            "HTMLStyleElement:media",
            "read-write gchar* HTMLStyleElement:media",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_STYLE_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLStyleElement:type",
            "read-write gchar* HTMLStyleElement:type",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_STYLE_ELEMENT_PROP_SHEET,
        g_param_spec_object(
            "sheet",
            "HTMLStyleElement:sheet",
            "read-only WebKitDOMStyleSheet* HTMLStyleElement:sheet",
            WEBKIT_DOM_TYPE_STYLE_SHEET,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_html_style_element_init(WebKitDOMHTMLStyleElement* request)
{
    UNUSED_PARAM(request);
}

gboolean webkit_dom_html_style_element_get_disabled(WebKitDOMHTMLStyleElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self), FALSE);
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    gboolean result = item->disabled();
    return result;
}

void webkit_dom_html_style_element_set_disabled(WebKitDOMHTMLStyleElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self));
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    item->setDisabled(value);
}

gchar* webkit_dom_html_style_element_get_media(WebKitDOMHTMLStyleElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self), 0);
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::mediaAttr));
    return result;
}

void webkit_dom_html_style_element_set_media(WebKitDOMHTMLStyleElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::mediaAttr, convertedValue);
}

gchar* webkit_dom_html_style_element_get_type_attr(WebKitDOMHTMLStyleElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self), 0);
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr));
    return result;
}

void webkit_dom_html_style_element_set_type_attr(WebKitDOMHTMLStyleElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, convertedValue);
}

WebKitDOMStyleSheet* webkit_dom_html_style_element_get_sheet(WebKitDOMHTMLStyleElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_STYLE_ELEMENT(self), 0);
    WebCore::HTMLStyleElement* item = WebKit::core(self);
    RefPtr<WebCore::StyleSheet> gobjectResult = WTF::getPtr(item->sheet());
    return WebKit::kit(gobjectResult.get());
}
G_GNUC_END_IGNORE_DEPRECATIONS;
