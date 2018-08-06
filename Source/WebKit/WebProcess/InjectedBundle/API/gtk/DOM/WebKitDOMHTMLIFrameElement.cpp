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
#include "WebKitDOMHTMLIFrameElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLIFrameElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLIFrameElement* kit(WebCore::HTMLIFrameElement* obj)
{
    return WEBKIT_DOM_HTML_IFRAME_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLIFrameElement* core(WebKitDOMHTMLIFrameElement* request)
{
    return request ? static_cast<WebCore::HTMLIFrameElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLIFrameElement* wrapHTMLIFrameElement(WebCore::HTMLIFrameElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_IFRAME_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_IFRAME_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_iframe_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLIFrameElement* coreTarget = static_cast<WebCore::HTMLIFrameElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_iframe_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLIFrameElement* coreTarget = static_cast<WebCore::HTMLIFrameElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_iframe_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLIFrameElement* coreTarget = static_cast<WebCore::HTMLIFrameElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_iframe_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_iframe_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_iframe_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_iframe_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLIFrameElement, webkit_dom_html_iframe_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_iframe_element_dom_event_target_init))

enum {
    DOM_HTML_IFRAME_ELEMENT_PROP_0,
    DOM_HTML_IFRAME_ELEMENT_PROP_ALIGN,
    DOM_HTML_IFRAME_ELEMENT_PROP_FRAME_BORDER,
    DOM_HTML_IFRAME_ELEMENT_PROP_HEIGHT,
    DOM_HTML_IFRAME_ELEMENT_PROP_LONG_DESC,
    DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_HEIGHT,
    DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_WIDTH,
    DOM_HTML_IFRAME_ELEMENT_PROP_NAME,
    DOM_HTML_IFRAME_ELEMENT_PROP_SCROLLING,
    DOM_HTML_IFRAME_ELEMENT_PROP_SRC,
    DOM_HTML_IFRAME_ELEMENT_PROP_WIDTH,
    DOM_HTML_IFRAME_ELEMENT_PROP_CONTENT_DOCUMENT,
    DOM_HTML_IFRAME_ELEMENT_PROP_CONTENT_WINDOW,
};

static void webkit_dom_html_iframe_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLIFrameElement* self = WEBKIT_DOM_HTML_IFRAME_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_IFRAME_ELEMENT_PROP_ALIGN:
        webkit_dom_html_iframe_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_FRAME_BORDER:
        webkit_dom_html_iframe_element_set_frame_border(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_HEIGHT:
        webkit_dom_html_iframe_element_set_height(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_LONG_DESC:
        webkit_dom_html_iframe_element_set_long_desc(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_HEIGHT:
        webkit_dom_html_iframe_element_set_margin_height(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_WIDTH:
        webkit_dom_html_iframe_element_set_margin_width(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_NAME:
        webkit_dom_html_iframe_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_SCROLLING:
        webkit_dom_html_iframe_element_set_scrolling(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_SRC:
        webkit_dom_html_iframe_element_set_src(self, g_value_get_string(value));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_WIDTH:
        webkit_dom_html_iframe_element_set_width(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_iframe_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLIFrameElement* self = WEBKIT_DOM_HTML_IFRAME_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_IFRAME_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_align(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_FRAME_BORDER:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_frame_border(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_HEIGHT:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_height(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_LONG_DESC:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_long_desc(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_HEIGHT:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_margin_height(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_WIDTH:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_margin_width(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_name(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_SCROLLING:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_scrolling(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_SRC:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_src(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_WIDTH:
        g_value_take_string(value, webkit_dom_html_iframe_element_get_width(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_CONTENT_DOCUMENT:
        g_value_set_object(value, webkit_dom_html_iframe_element_get_content_document(self));
        break;
    case DOM_HTML_IFRAME_ELEMENT_PROP_CONTENT_WINDOW:
        g_value_set_object(value, webkit_dom_html_iframe_element_get_content_window(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_iframe_element_class_init(WebKitDOMHTMLIFrameElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_iframe_element_set_property;
    gobjectClass->get_property = webkit_dom_html_iframe_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLIFrameElement:align",
            "read-write gchar* HTMLIFrameElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_FRAME_BORDER,
        g_param_spec_string(
            "frame-border",
            "HTMLIFrameElement:frame-border",
            "read-write gchar* HTMLIFrameElement:frame-border",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_HEIGHT,
        g_param_spec_string(
            "height",
            "HTMLIFrameElement:height",
            "read-write gchar* HTMLIFrameElement:height",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_LONG_DESC,
        g_param_spec_string(
            "long-desc",
            "HTMLIFrameElement:long-desc",
            "read-write gchar* HTMLIFrameElement:long-desc",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_HEIGHT,
        g_param_spec_string(
            "margin-height",
            "HTMLIFrameElement:margin-height",
            "read-write gchar* HTMLIFrameElement:margin-height",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_MARGIN_WIDTH,
        g_param_spec_string(
            "margin-width",
            "HTMLIFrameElement:margin-width",
            "read-write gchar* HTMLIFrameElement:margin-width",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLIFrameElement:name",
            "read-write gchar* HTMLIFrameElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_SCROLLING,
        g_param_spec_string(
            "scrolling",
            "HTMLIFrameElement:scrolling",
            "read-write gchar* HTMLIFrameElement:scrolling",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_SRC,
        g_param_spec_string(
            "src",
            "HTMLIFrameElement:src",
            "read-write gchar* HTMLIFrameElement:src",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_WIDTH,
        g_param_spec_string(
            "width",
            "HTMLIFrameElement:width",
            "read-write gchar* HTMLIFrameElement:width",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_CONTENT_DOCUMENT,
        g_param_spec_object(
            "content-document",
            "HTMLIFrameElement:content-document",
            "read-only WebKitDOMDocument* HTMLIFrameElement:content-document",
            WEBKIT_DOM_TYPE_DOCUMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IFRAME_ELEMENT_PROP_CONTENT_WINDOW,
        g_param_spec_object(
            "content-window",
            "HTMLIFrameElement:content-window",
            "read-only WebKitDOMDOMWindow* HTMLIFrameElement:content-window",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_html_iframe_element_init(WebKitDOMHTMLIFrameElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_iframe_element_get_align(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_align(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_frame_border(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::frameborderAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_frame_border(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::frameborderAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_height(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::heightAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_height(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::heightAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_long_desc(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::longdescAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_long_desc(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::longdescAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_margin_height(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::marginheightAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_margin_height(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::marginheightAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_margin_width(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::marginwidthAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_margin_width(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::marginwidthAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_name(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_iframe_element_set_name(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_scrolling(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::scrollingAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_scrolling(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::scrollingAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_src(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::srcAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_src(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, convertedValue);
}

gchar* webkit_dom_html_iframe_element_get_width(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::widthAttr));
    return result;
}

void webkit_dom_html_iframe_element_set_width(WebKitDOMHTMLIFrameElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, convertedValue);
}

WebKitDOMDocument* webkit_dom_html_iframe_element_get_content_document(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    RefPtr<WebCore::Document> gobjectResult = WTF::getPtr(item->contentDocument());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMDOMWindow* webkit_dom_html_iframe_element_get_content_window(WebKitDOMHTMLIFrameElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT(self), 0);
    WebCore::HTMLIFrameElement* item = WebKit::core(self);
    return WebKit::kit(item->contentWindow());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
