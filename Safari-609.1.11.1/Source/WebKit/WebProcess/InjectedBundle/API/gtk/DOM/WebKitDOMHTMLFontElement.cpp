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
#include "WebKitDOMHTMLFontElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLFontElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLFontElement* kit(WebCore::HTMLFontElement* obj)
{
    return WEBKIT_DOM_HTML_FONT_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLFontElement* core(WebKitDOMHTMLFontElement* request)
{
    return request ? static_cast<WebCore::HTMLFontElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLFontElement* wrapHTMLFontElement(WebCore::HTMLFontElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_FONT_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_FONT_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_font_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLFontElement* coreTarget = static_cast<WebCore::HTMLFontElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_font_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLFontElement* coreTarget = static_cast<WebCore::HTMLFontElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_font_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLFontElement* coreTarget = static_cast<WebCore::HTMLFontElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_font_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_font_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_font_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_font_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLFontElement, webkit_dom_html_font_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_font_element_dom_event_target_init))

enum {
    DOM_HTML_FONT_ELEMENT_PROP_0,
    DOM_HTML_FONT_ELEMENT_PROP_COLOR,
    DOM_HTML_FONT_ELEMENT_PROP_FACE,
    DOM_HTML_FONT_ELEMENT_PROP_SIZE,
};

static void webkit_dom_html_font_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLFontElement* self = WEBKIT_DOM_HTML_FONT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_FONT_ELEMENT_PROP_COLOR:
        webkit_dom_html_font_element_set_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_FONT_ELEMENT_PROP_FACE:
        webkit_dom_html_font_element_set_face(self, g_value_get_string(value));
        break;
    case DOM_HTML_FONT_ELEMENT_PROP_SIZE:
        webkit_dom_html_font_element_set_size(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_font_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLFontElement* self = WEBKIT_DOM_HTML_FONT_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_FONT_ELEMENT_PROP_COLOR:
        g_value_take_string(value, webkit_dom_html_font_element_get_color(self));
        break;
    case DOM_HTML_FONT_ELEMENT_PROP_FACE:
        g_value_take_string(value, webkit_dom_html_font_element_get_face(self));
        break;
    case DOM_HTML_FONT_ELEMENT_PROP_SIZE:
        g_value_take_string(value, webkit_dom_html_font_element_get_size(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_font_element_class_init(WebKitDOMHTMLFontElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_font_element_set_property;
    gobjectClass->get_property = webkit_dom_html_font_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FONT_ELEMENT_PROP_COLOR,
        g_param_spec_string(
            "color",
            "HTMLFontElement:color",
            "read-write gchar* HTMLFontElement:color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FONT_ELEMENT_PROP_FACE,
        g_param_spec_string(
            "face",
            "HTMLFontElement:face",
            "read-write gchar* HTMLFontElement:face",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FONT_ELEMENT_PROP_SIZE,
        g_param_spec_string(
            "size",
            "HTMLFontElement:size",
            "read-write gchar* HTMLFontElement:size",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_font_element_init(WebKitDOMHTMLFontElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_font_element_get_color(WebKitDOMHTMLFontElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FONT_ELEMENT(self), 0);
    WebCore::HTMLFontElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::colorAttr));
    return result;
}

void webkit_dom_html_font_element_set_color(WebKitDOMHTMLFontElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FONT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFontElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::colorAttr, convertedValue);
}

gchar* webkit_dom_html_font_element_get_face(WebKitDOMHTMLFontElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FONT_ELEMENT(self), 0);
    WebCore::HTMLFontElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::faceAttr));
    return result;
}

void webkit_dom_html_font_element_set_face(WebKitDOMHTMLFontElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FONT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFontElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::faceAttr, convertedValue);
}

gchar* webkit_dom_html_font_element_get_size(WebKitDOMHTMLFontElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FONT_ELEMENT(self), 0);
    WebCore::HTMLFontElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::sizeAttr));
    return result;
}

void webkit_dom_html_font_element_set_size(WebKitDOMHTMLFontElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FONT_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFontElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::sizeAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
