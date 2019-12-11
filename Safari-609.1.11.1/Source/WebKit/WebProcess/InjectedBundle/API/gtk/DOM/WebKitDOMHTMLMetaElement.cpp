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
#include "WebKitDOMHTMLMetaElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLMetaElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLMetaElement* kit(WebCore::HTMLMetaElement* obj)
{
    return WEBKIT_DOM_HTML_META_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLMetaElement* core(WebKitDOMHTMLMetaElement* request)
{
    return request ? static_cast<WebCore::HTMLMetaElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLMetaElement* wrapHTMLMetaElement(WebCore::HTMLMetaElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_META_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_META_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_meta_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLMetaElement* coreTarget = static_cast<WebCore::HTMLMetaElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_meta_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLMetaElement* coreTarget = static_cast<WebCore::HTMLMetaElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_meta_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLMetaElement* coreTarget = static_cast<WebCore::HTMLMetaElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_meta_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_meta_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_meta_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_meta_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLMetaElement, webkit_dom_html_meta_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_meta_element_dom_event_target_init))

enum {
    DOM_HTML_META_ELEMENT_PROP_0,
    DOM_HTML_META_ELEMENT_PROP_CONTENT,
    DOM_HTML_META_ELEMENT_PROP_HTTP_EQUIV,
    DOM_HTML_META_ELEMENT_PROP_NAME,
    DOM_HTML_META_ELEMENT_PROP_SCHEME,
};

static void webkit_dom_html_meta_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLMetaElement* self = WEBKIT_DOM_HTML_META_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_META_ELEMENT_PROP_CONTENT:
        webkit_dom_html_meta_element_set_content(self, g_value_get_string(value));
        break;
    case DOM_HTML_META_ELEMENT_PROP_HTTP_EQUIV:
        webkit_dom_html_meta_element_set_http_equiv(self, g_value_get_string(value));
        break;
    case DOM_HTML_META_ELEMENT_PROP_NAME:
        webkit_dom_html_meta_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_META_ELEMENT_PROP_SCHEME:
        webkit_dom_html_meta_element_set_scheme(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_meta_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLMetaElement* self = WEBKIT_DOM_HTML_META_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_META_ELEMENT_PROP_CONTENT:
        g_value_take_string(value, webkit_dom_html_meta_element_get_content(self));
        break;
    case DOM_HTML_META_ELEMENT_PROP_HTTP_EQUIV:
        g_value_take_string(value, webkit_dom_html_meta_element_get_http_equiv(self));
        break;
    case DOM_HTML_META_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_meta_element_get_name(self));
        break;
    case DOM_HTML_META_ELEMENT_PROP_SCHEME:
        g_value_take_string(value, webkit_dom_html_meta_element_get_scheme(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_meta_element_class_init(WebKitDOMHTMLMetaElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_meta_element_set_property;
    gobjectClass->get_property = webkit_dom_html_meta_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_META_ELEMENT_PROP_CONTENT,
        g_param_spec_string(
            "content",
            "HTMLMetaElement:content",
            "read-write gchar* HTMLMetaElement:content",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_META_ELEMENT_PROP_HTTP_EQUIV,
        g_param_spec_string(
            "http-equiv",
            "HTMLMetaElement:http-equiv",
            "read-write gchar* HTMLMetaElement:http-equiv",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_META_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLMetaElement:name",
            "read-write gchar* HTMLMetaElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_META_ELEMENT_PROP_SCHEME,
        g_param_spec_string(
            "scheme",
            "HTMLMetaElement:scheme",
            "read-write gchar* HTMLMetaElement:scheme",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_meta_element_init(WebKitDOMHTMLMetaElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_meta_element_get_content(WebKitDOMHTMLMetaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self), 0);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::contentAttr));
    return result;
}

void webkit_dom_html_meta_element_set_content(WebKitDOMHTMLMetaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::contentAttr, convertedValue);
}

gchar* webkit_dom_html_meta_element_get_http_equiv(WebKitDOMHTMLMetaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self), 0);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::http_equivAttr));
    return result;
}

void webkit_dom_html_meta_element_set_http_equiv(WebKitDOMHTMLMetaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::http_equivAttr, convertedValue);
}

gchar* webkit_dom_html_meta_element_get_name(WebKitDOMHTMLMetaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self), 0);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_meta_element_set_name(WebKitDOMHTMLMetaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_meta_element_get_scheme(WebKitDOMHTMLMetaElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self), 0);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::schemeAttr));
    return result;
}

void webkit_dom_html_meta_element_set_scheme(WebKitDOMHTMLMetaElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_META_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLMetaElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::schemeAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
