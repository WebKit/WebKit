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
#include "WebKitDOMHTMLAnchorElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMDOMTokenListPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLAnchorElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLAnchorElement* kit(WebCore::HTMLAnchorElement* obj)
{
    return WEBKIT_DOM_HTML_ANCHOR_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLAnchorElement* core(WebKitDOMHTMLAnchorElement* request)
{
    return request ? static_cast<WebCore::HTMLAnchorElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLAnchorElement* wrapHTMLAnchorElement(WebCore::HTMLAnchorElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_ANCHOR_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_ANCHOR_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_anchor_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLAnchorElement* coreTarget = static_cast<WebCore::HTMLAnchorElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_anchor_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLAnchorElement* coreTarget = static_cast<WebCore::HTMLAnchorElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_anchor_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLAnchorElement* coreTarget = static_cast<WebCore::HTMLAnchorElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_anchor_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_anchor_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_anchor_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_anchor_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLAnchorElement, webkit_dom_html_anchor_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_anchor_element_dom_event_target_init))

enum {
    DOM_HTML_ANCHOR_ELEMENT_PROP_0,
    DOM_HTML_ANCHOR_ELEMENT_PROP_CHARSET,
    DOM_HTML_ANCHOR_ELEMENT_PROP_COORDS,
    DOM_HTML_ANCHOR_ELEMENT_PROP_HREFLANG,
    DOM_HTML_ANCHOR_ELEMENT_PROP_NAME,
    DOM_HTML_ANCHOR_ELEMENT_PROP_REL,
    DOM_HTML_ANCHOR_ELEMENT_PROP_REV,
    DOM_HTML_ANCHOR_ELEMENT_PROP_SHAPE,
    DOM_HTML_ANCHOR_ELEMENT_PROP_TARGET,
    DOM_HTML_ANCHOR_ELEMENT_PROP_TYPE,
    DOM_HTML_ANCHOR_ELEMENT_PROP_TEXT,
    DOM_HTML_ANCHOR_ELEMENT_PROP_HREF,
    DOM_HTML_ANCHOR_ELEMENT_PROP_PROTOCOL,
    DOM_HTML_ANCHOR_ELEMENT_PROP_HOST,
    DOM_HTML_ANCHOR_ELEMENT_PROP_HOSTNAME,
    DOM_HTML_ANCHOR_ELEMENT_PROP_PORT,
    DOM_HTML_ANCHOR_ELEMENT_PROP_PATHNAME,
    DOM_HTML_ANCHOR_ELEMENT_PROP_SEARCH,
    DOM_HTML_ANCHOR_ELEMENT_PROP_HASH,
};

static void webkit_dom_html_anchor_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLAnchorElement* self = WEBKIT_DOM_HTML_ANCHOR_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_ANCHOR_ELEMENT_PROP_CHARSET:
        webkit_dom_html_anchor_element_set_charset(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_COORDS:
        webkit_dom_html_anchor_element_set_coords(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HREFLANG:
        webkit_dom_html_anchor_element_set_hreflang(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_NAME:
        webkit_dom_html_anchor_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_REL:
        webkit_dom_html_anchor_element_set_rel(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_REV:
        webkit_dom_html_anchor_element_set_rev(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_SHAPE:
        webkit_dom_html_anchor_element_set_shape(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_TARGET:
        webkit_dom_html_anchor_element_set_target(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_TYPE:
        webkit_dom_html_anchor_element_set_type_attr(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_TEXT:
        webkit_dom_html_anchor_element_set_text(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HREF:
        webkit_dom_html_anchor_element_set_href(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_PROTOCOL:
        webkit_dom_html_anchor_element_set_protocol(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HOST:
        webkit_dom_html_anchor_element_set_host(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HOSTNAME:
        webkit_dom_html_anchor_element_set_hostname(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_PORT:
        webkit_dom_html_anchor_element_set_port(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_PATHNAME:
        webkit_dom_html_anchor_element_set_pathname(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_SEARCH:
        webkit_dom_html_anchor_element_set_search(self, g_value_get_string(value));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HASH:
        webkit_dom_html_anchor_element_set_hash(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_anchor_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLAnchorElement* self = WEBKIT_DOM_HTML_ANCHOR_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_ANCHOR_ELEMENT_PROP_CHARSET:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_charset(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_COORDS:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_coords(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HREFLANG:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_hreflang(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_name(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_REL:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_rel(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_REV:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_rev(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_SHAPE:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_shape(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_TARGET:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_target(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_type_attr(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_TEXT:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_text(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HREF:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_href(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_PROTOCOL:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_protocol(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HOST:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_host(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HOSTNAME:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_hostname(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_PORT:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_port(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_PATHNAME:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_pathname(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_SEARCH:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_search(self));
        break;
    case DOM_HTML_ANCHOR_ELEMENT_PROP_HASH:
        g_value_take_string(value, webkit_dom_html_anchor_element_get_hash(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_anchor_element_class_init(WebKitDOMHTMLAnchorElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_anchor_element_set_property;
    gobjectClass->get_property = webkit_dom_html_anchor_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_CHARSET,
        g_param_spec_string(
            "charset",
            "HTMLAnchorElement:charset",
            "read-write gchar* HTMLAnchorElement:charset",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_COORDS,
        g_param_spec_string(
            "coords",
            "HTMLAnchorElement:coords",
            "read-write gchar* HTMLAnchorElement:coords",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_HREFLANG,
        g_param_spec_string(
            "hreflang",
            "HTMLAnchorElement:hreflang",
            "read-write gchar* HTMLAnchorElement:hreflang",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLAnchorElement:name",
            "read-write gchar* HTMLAnchorElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_REL,
        g_param_spec_string(
            "rel",
            "HTMLAnchorElement:rel",
            "read-write gchar* HTMLAnchorElement:rel",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_REV,
        g_param_spec_string(
            "rev",
            "HTMLAnchorElement:rev",
            "read-write gchar* HTMLAnchorElement:rev",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_SHAPE,
        g_param_spec_string(
            "shape",
            "HTMLAnchorElement:shape",
            "read-write gchar* HTMLAnchorElement:shape",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_TARGET,
        g_param_spec_string(
            "target",
            "HTMLAnchorElement:target",
            "read-write gchar* HTMLAnchorElement:target",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLAnchorElement:type",
            "read-write gchar* HTMLAnchorElement:type",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_TEXT,
        g_param_spec_string(
            "text",
            "HTMLAnchorElement:text",
            "read-write gchar* HTMLAnchorElement:text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_HREF,
        g_param_spec_string(
            "href",
            "HTMLAnchorElement:href",
            "read-write gchar* HTMLAnchorElement:href",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_PROTOCOL,
        g_param_spec_string(
            "protocol",
            "HTMLAnchorElement:protocol",
            "read-write gchar* HTMLAnchorElement:protocol",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_HOST,
        g_param_spec_string(
            "host",
            "HTMLAnchorElement:host",
            "read-write gchar* HTMLAnchorElement:host",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_HOSTNAME,
        g_param_spec_string(
            "hostname",
            "HTMLAnchorElement:hostname",
            "read-write gchar* HTMLAnchorElement:hostname",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_PORT,
        g_param_spec_string(
            "port",
            "HTMLAnchorElement:port",
            "read-write gchar* HTMLAnchorElement:port",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_PATHNAME,
        g_param_spec_string(
            "pathname",
            "HTMLAnchorElement:pathname",
            "read-write gchar* HTMLAnchorElement:pathname",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_SEARCH,
        g_param_spec_string(
            "search",
            "HTMLAnchorElement:search",
            "read-write gchar* HTMLAnchorElement:search",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ANCHOR_ELEMENT_PROP_HASH,
        g_param_spec_string(
            "hash",
            "HTMLAnchorElement:hash",
            "read-write gchar* HTMLAnchorElement:hash",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_anchor_element_init(WebKitDOMHTMLAnchorElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_anchor_element_get_charset(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_charset(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_coords(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::coordsAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_coords(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::coordsAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_hreflang(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::hreflangAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_hreflang(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::hreflangAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_name(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_anchor_element_set_name(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_rel(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::relAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_rel(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::relAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_rev(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::revAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_rev(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::revAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_shape(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::shapeAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_shape(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::shapeAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_target(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::targetAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_target(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::targetAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_type_attr(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_type_attr(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_text(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->text());
    return result;
}

void webkit_dom_html_anchor_element_set_text(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setText(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_href(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::hrefAttr));
    return result;
}

void webkit_dom_html_anchor_element_set_href(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::hrefAttr, convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_protocol(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->protocol());
    return result;
}

void webkit_dom_html_anchor_element_set_protocol(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setProtocol(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_host(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->host());
    return result;
}

void webkit_dom_html_anchor_element_set_host(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setHost(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_hostname(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->hostname());
    return result;
}

void webkit_dom_html_anchor_element_set_hostname(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setHostname(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_port(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->port());
    return result;
}

void webkit_dom_html_anchor_element_set_port(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setPort(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_pathname(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->pathname());
    return result;
}

void webkit_dom_html_anchor_element_set_pathname(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setPathname(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_search(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->search());
    return result;
}

void webkit_dom_html_anchor_element_set_search(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setSearch(convertedValue);
}

gchar* webkit_dom_html_anchor_element_get_hash(WebKitDOMHTMLAnchorElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self), 0);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->hash());
    return result;
}

void webkit_dom_html_anchor_element_set_hash(WebKitDOMHTMLAnchorElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLAnchorElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setHash(convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
