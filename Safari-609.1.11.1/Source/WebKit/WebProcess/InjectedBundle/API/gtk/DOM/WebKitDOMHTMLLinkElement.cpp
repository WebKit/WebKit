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
#include "WebKitDOMHTMLLinkElement.h"

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
#include "WebKitDOMHTMLLinkElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMStyleSheetPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLLinkElement* kit(WebCore::HTMLLinkElement* obj)
{
    return WEBKIT_DOM_HTML_LINK_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLLinkElement* core(WebKitDOMHTMLLinkElement* request)
{
    return request ? static_cast<WebCore::HTMLLinkElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLLinkElement* wrapHTMLLinkElement(WebCore::HTMLLinkElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_LINK_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_LINK_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_link_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLLinkElement* coreTarget = static_cast<WebCore::HTMLLinkElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_link_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLLinkElement* coreTarget = static_cast<WebCore::HTMLLinkElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_link_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLLinkElement* coreTarget = static_cast<WebCore::HTMLLinkElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_link_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_link_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_link_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_link_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLLinkElement, webkit_dom_html_link_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_link_element_dom_event_target_init))

enum {
    DOM_HTML_LINK_ELEMENT_PROP_0,
    DOM_HTML_LINK_ELEMENT_PROP_DISABLED,
    DOM_HTML_LINK_ELEMENT_PROP_CHARSET,
    DOM_HTML_LINK_ELEMENT_PROP_HREF,
    DOM_HTML_LINK_ELEMENT_PROP_HREFLANG,
    DOM_HTML_LINK_ELEMENT_PROP_MEDIA,
    DOM_HTML_LINK_ELEMENT_PROP_REL,
    DOM_HTML_LINK_ELEMENT_PROP_REV,
    DOM_HTML_LINK_ELEMENT_PROP_SIZES,
    DOM_HTML_LINK_ELEMENT_PROP_TARGET,
    DOM_HTML_LINK_ELEMENT_PROP_TYPE,
    DOM_HTML_LINK_ELEMENT_PROP_SHEET,
};

static void webkit_dom_html_link_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLLinkElement* self = WEBKIT_DOM_HTML_LINK_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_LINK_ELEMENT_PROP_DISABLED:
        webkit_dom_html_link_element_set_disabled(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_CHARSET:
        webkit_dom_html_link_element_set_charset(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_HREF:
        webkit_dom_html_link_element_set_href(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_HREFLANG:
        webkit_dom_html_link_element_set_hreflang(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_MEDIA:
        webkit_dom_html_link_element_set_media(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_REL:
        webkit_dom_html_link_element_set_rel(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_REV:
        webkit_dom_html_link_element_set_rev(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_TARGET:
        webkit_dom_html_link_element_set_target(self, g_value_get_string(value));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_TYPE:
        webkit_dom_html_link_element_set_type_attr(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_link_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLLinkElement* self = WEBKIT_DOM_HTML_LINK_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_LINK_ELEMENT_PROP_DISABLED:
        g_value_set_boolean(value, webkit_dom_html_link_element_get_disabled(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_CHARSET:
        g_value_take_string(value, webkit_dom_html_link_element_get_charset(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_HREF:
        g_value_take_string(value, webkit_dom_html_link_element_get_href(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_HREFLANG:
        g_value_take_string(value, webkit_dom_html_link_element_get_hreflang(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_MEDIA:
        g_value_take_string(value, webkit_dom_html_link_element_get_media(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_REL:
        g_value_take_string(value, webkit_dom_html_link_element_get_rel(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_REV:
        g_value_take_string(value, webkit_dom_html_link_element_get_rev(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_SIZES:
        g_value_set_object(value, webkit_dom_html_link_element_get_sizes(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_TARGET:
        g_value_take_string(value, webkit_dom_html_link_element_get_target(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_html_link_element_get_type_attr(self));
        break;
    case DOM_HTML_LINK_ELEMENT_PROP_SHEET:
        g_value_set_object(value, webkit_dom_html_link_element_get_sheet(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_link_element_class_init(WebKitDOMHTMLLinkElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_link_element_set_property;
    gobjectClass->get_property = webkit_dom_html_link_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_DISABLED,
        g_param_spec_boolean(
            "disabled",
            "HTMLLinkElement:disabled",
            "read-write gboolean HTMLLinkElement:disabled",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_CHARSET,
        g_param_spec_string(
            "charset",
            "HTMLLinkElement:charset",
            "read-write gchar* HTMLLinkElement:charset",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_HREF,
        g_param_spec_string(
            "href",
            "HTMLLinkElement:href",
            "read-write gchar* HTMLLinkElement:href",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_HREFLANG,
        g_param_spec_string(
            "hreflang",
            "HTMLLinkElement:hreflang",
            "read-write gchar* HTMLLinkElement:hreflang",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_MEDIA,
        g_param_spec_string(
            "media",
            "HTMLLinkElement:media",
            "read-write gchar* HTMLLinkElement:media",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_REL,
        g_param_spec_string(
            "rel",
            "HTMLLinkElement:rel",
            "read-write gchar* HTMLLinkElement:rel",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_REV,
        g_param_spec_string(
            "rev",
            "HTMLLinkElement:rev",
            "read-write gchar* HTMLLinkElement:rev",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_SIZES,
        g_param_spec_object(
            "sizes",
            "HTMLLinkElement:sizes",
            "read-only WebKitDOMDOMTokenList* HTMLLinkElement:sizes",
            WEBKIT_DOM_TYPE_DOM_TOKEN_LIST,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_TARGET,
        g_param_spec_string(
            "target",
            "HTMLLinkElement:target",
            "read-write gchar* HTMLLinkElement:target",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "HTMLLinkElement:type",
            "read-write gchar* HTMLLinkElement:type",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_LINK_ELEMENT_PROP_SHEET,
        g_param_spec_object(
            "sheet",
            "HTMLLinkElement:sheet",
            "read-only WebKitDOMStyleSheet* HTMLLinkElement:sheet",
            WEBKIT_DOM_TYPE_STYLE_SHEET,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_html_link_element_init(WebKitDOMHTMLLinkElement* request)
{
    UNUSED_PARAM(request);
}

gboolean webkit_dom_html_link_element_get_disabled(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), FALSE);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
    return result;
}

void webkit_dom_html_link_element_set_disabled(WebKitDOMHTMLLinkElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, value);
}

gchar* webkit_dom_html_link_element_get_charset(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr));
    return result;
}

void webkit_dom_html_link_element_set_charset(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr, convertedValue);
}

gchar* webkit_dom_html_link_element_get_href(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::hrefAttr));
    return result;
}

void webkit_dom_html_link_element_set_href(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::hrefAttr, convertedValue);
}

gchar* webkit_dom_html_link_element_get_hreflang(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::hreflangAttr));
    return result;
}

void webkit_dom_html_link_element_set_hreflang(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::hreflangAttr, convertedValue);
}

gchar* webkit_dom_html_link_element_get_media(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::mediaAttr));
    return result;
}

void webkit_dom_html_link_element_set_media(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::mediaAttr, convertedValue);
}

gchar* webkit_dom_html_link_element_get_rel(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::relAttr));
    return result;
}

void webkit_dom_html_link_element_set_rel(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::relAttr, convertedValue);
}

gchar* webkit_dom_html_link_element_get_rev(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::revAttr));
    return result;
}

void webkit_dom_html_link_element_set_rev(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::revAttr, convertedValue);
}

WebKitDOMDOMTokenList* webkit_dom_html_link_element_get_sizes(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    RefPtr<WebCore::DOMTokenList> gobjectResult = WTF::getPtr(item->sizes());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_html_link_element_get_target(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::targetAttr));
    return result;
}

void webkit_dom_html_link_element_set_target(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::targetAttr, convertedValue);
}

gchar* webkit_dom_html_link_element_get_type_attr(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr));
    return result;
}

void webkit_dom_html_link_element_set_type_attr(WebKitDOMHTMLLinkElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, convertedValue);
}

WebKitDOMStyleSheet* webkit_dom_html_link_element_get_sheet(WebKitDOMHTMLLinkElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(self), 0);
    WebCore::HTMLLinkElement* item = WebKit::core(self);
    RefPtr<WebCore::StyleSheet> gobjectResult = WTF::getPtr(item->sheet());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_html_link_element_set_sizes(WebKitDOMHTMLLinkElement* linkElement, const gchar* value)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(linkElement));
    g_return_if_fail(value);

    WebKit::core(linkElement)->sizes().setValue(String::fromUTF8(value));
}
G_GNUC_END_IGNORE_DEPRECATIONS;
