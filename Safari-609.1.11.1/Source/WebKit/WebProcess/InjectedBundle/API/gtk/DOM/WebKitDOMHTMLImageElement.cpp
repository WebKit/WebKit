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
#include "WebKitDOMHTMLImageElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLImageElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLImageElement* kit(WebCore::HTMLImageElement* obj)
{
    return WEBKIT_DOM_HTML_IMAGE_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLImageElement* core(WebKitDOMHTMLImageElement* request)
{
    return request ? static_cast<WebCore::HTMLImageElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLImageElement* wrapHTMLImageElement(WebCore::HTMLImageElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_IMAGE_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_IMAGE_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_image_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLImageElement* coreTarget = static_cast<WebCore::HTMLImageElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_image_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLImageElement* coreTarget = static_cast<WebCore::HTMLImageElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_image_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLImageElement* coreTarget = static_cast<WebCore::HTMLImageElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_image_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_image_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_image_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_image_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLImageElement, webkit_dom_html_image_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_image_element_dom_event_target_init))

enum {
    DOM_HTML_IMAGE_ELEMENT_PROP_0,
    DOM_HTML_IMAGE_ELEMENT_PROP_NAME,
    DOM_HTML_IMAGE_ELEMENT_PROP_ALIGN,
    DOM_HTML_IMAGE_ELEMENT_PROP_ALT,
    DOM_HTML_IMAGE_ELEMENT_PROP_BORDER,
    DOM_HTML_IMAGE_ELEMENT_PROP_HEIGHT,
    DOM_HTML_IMAGE_ELEMENT_PROP_HSPACE,
    DOM_HTML_IMAGE_ELEMENT_PROP_IS_MAP,
    DOM_HTML_IMAGE_ELEMENT_PROP_LONG_DESC,
    DOM_HTML_IMAGE_ELEMENT_PROP_SRC,
    DOM_HTML_IMAGE_ELEMENT_PROP_USE_MAP,
    DOM_HTML_IMAGE_ELEMENT_PROP_VSPACE,
    DOM_HTML_IMAGE_ELEMENT_PROP_WIDTH,
    DOM_HTML_IMAGE_ELEMENT_PROP_COMPLETE,
    DOM_HTML_IMAGE_ELEMENT_PROP_LOWSRC,
    DOM_HTML_IMAGE_ELEMENT_PROP_NATURAL_HEIGHT,
    DOM_HTML_IMAGE_ELEMENT_PROP_NATURAL_WIDTH,
    DOM_HTML_IMAGE_ELEMENT_PROP_X,
    DOM_HTML_IMAGE_ELEMENT_PROP_Y,
};

static void webkit_dom_html_image_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLImageElement* self = WEBKIT_DOM_HTML_IMAGE_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_IMAGE_ELEMENT_PROP_NAME:
        webkit_dom_html_image_element_set_name(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_ALIGN:
        webkit_dom_html_image_element_set_align(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_ALT:
        webkit_dom_html_image_element_set_alt(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_BORDER:
        webkit_dom_html_image_element_set_border(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_HEIGHT:
        webkit_dom_html_image_element_set_height(self, g_value_get_long(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_HSPACE:
        webkit_dom_html_image_element_set_hspace(self, g_value_get_long(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_IS_MAP:
        webkit_dom_html_image_element_set_is_map(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_LONG_DESC:
        webkit_dom_html_image_element_set_long_desc(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_SRC:
        webkit_dom_html_image_element_set_src(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_USE_MAP:
        webkit_dom_html_image_element_set_use_map(self, g_value_get_string(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_VSPACE:
        webkit_dom_html_image_element_set_vspace(self, g_value_get_long(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_WIDTH:
        webkit_dom_html_image_element_set_width(self, g_value_get_long(value));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_LOWSRC:
        webkit_dom_html_image_element_set_lowsrc(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_image_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLImageElement* self = WEBKIT_DOM_HTML_IMAGE_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_IMAGE_ELEMENT_PROP_NAME:
        g_value_take_string(value, webkit_dom_html_image_element_get_name(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_ALIGN:
        g_value_take_string(value, webkit_dom_html_image_element_get_align(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_ALT:
        g_value_take_string(value, webkit_dom_html_image_element_get_alt(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_BORDER:
        g_value_take_string(value, webkit_dom_html_image_element_get_border(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_HEIGHT:
        g_value_set_long(value, webkit_dom_html_image_element_get_height(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_HSPACE:
        g_value_set_long(value, webkit_dom_html_image_element_get_hspace(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_IS_MAP:
        g_value_set_boolean(value, webkit_dom_html_image_element_get_is_map(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_LONG_DESC:
        g_value_take_string(value, webkit_dom_html_image_element_get_long_desc(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_SRC:
        g_value_take_string(value, webkit_dom_html_image_element_get_src(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_USE_MAP:
        g_value_take_string(value, webkit_dom_html_image_element_get_use_map(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_VSPACE:
        g_value_set_long(value, webkit_dom_html_image_element_get_vspace(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_WIDTH:
        g_value_set_long(value, webkit_dom_html_image_element_get_width(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_COMPLETE:
        g_value_set_boolean(value, webkit_dom_html_image_element_get_complete(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_LOWSRC:
        g_value_take_string(value, webkit_dom_html_image_element_get_lowsrc(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_NATURAL_HEIGHT:
        g_value_set_long(value, webkit_dom_html_image_element_get_natural_height(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_NATURAL_WIDTH:
        g_value_set_long(value, webkit_dom_html_image_element_get_natural_width(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_X:
        g_value_set_long(value, webkit_dom_html_image_element_get_x(self));
        break;
    case DOM_HTML_IMAGE_ELEMENT_PROP_Y:
        g_value_set_long(value, webkit_dom_html_image_element_get_y(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_image_element_class_init(WebKitDOMHTMLImageElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_image_element_set_property;
    gobjectClass->get_property = webkit_dom_html_image_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_NAME,
        g_param_spec_string(
            "name",
            "HTMLImageElement:name",
            "read-write gchar* HTMLImageElement:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_ALIGN,
        g_param_spec_string(
            "align",
            "HTMLImageElement:align",
            "read-write gchar* HTMLImageElement:align",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_ALT,
        g_param_spec_string(
            "alt",
            "HTMLImageElement:alt",
            "read-write gchar* HTMLImageElement:alt",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_BORDER,
        g_param_spec_string(
            "border",
            "HTMLImageElement:border",
            "read-write gchar* HTMLImageElement:border",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_HEIGHT,
        g_param_spec_long(
            "height",
            "HTMLImageElement:height",
            "read-write glong HTMLImageElement:height",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_HSPACE,
        g_param_spec_long(
            "hspace",
            "HTMLImageElement:hspace",
            "read-write glong HTMLImageElement:hspace",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_IS_MAP,
        g_param_spec_boolean(
            "is-map",
            "HTMLImageElement:is-map",
            "read-write gboolean HTMLImageElement:is-map",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_LONG_DESC,
        g_param_spec_string(
            "long-desc",
            "HTMLImageElement:long-desc",
            "read-write gchar* HTMLImageElement:long-desc",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_SRC,
        g_param_spec_string(
            "src",
            "HTMLImageElement:src",
            "read-write gchar* HTMLImageElement:src",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_USE_MAP,
        g_param_spec_string(
            "use-map",
            "HTMLImageElement:use-map",
            "read-write gchar* HTMLImageElement:use-map",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_VSPACE,
        g_param_spec_long(
            "vspace",
            "HTMLImageElement:vspace",
            "read-write glong HTMLImageElement:vspace",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_WIDTH,
        g_param_spec_long(
            "width",
            "HTMLImageElement:width",
            "read-write glong HTMLImageElement:width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_COMPLETE,
        g_param_spec_boolean(
            "complete",
            "HTMLImageElement:complete",
            "read-only gboolean HTMLImageElement:complete",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_LOWSRC,
        g_param_spec_string(
            "lowsrc",
            "HTMLImageElement:lowsrc",
            "read-write gchar* HTMLImageElement:lowsrc",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_NATURAL_HEIGHT,
        g_param_spec_long(
            "natural-height",
            "HTMLImageElement:natural-height",
            "read-only glong HTMLImageElement:natural-height",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_NATURAL_WIDTH,
        g_param_spec_long(
            "natural-width",
            "HTMLImageElement:natural-width",
            "read-only glong HTMLImageElement:natural-width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_X,
        g_param_spec_long(
            "x",
            "HTMLImageElement:x",
            "read-only glong HTMLImageElement:x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_IMAGE_ELEMENT_PROP_Y,
        g_param_spec_long(
            "y",
            "HTMLImageElement:y",
            "read-only glong HTMLImageElement:y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_html_image_element_init(WebKitDOMHTMLImageElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_image_element_get_name(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getNameAttribute());
    return result;
}

void webkit_dom_html_image_element_set_name(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, convertedValue);
}

gchar* webkit_dom_html_image_element_get_align(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::alignAttr));
    return result;
}

void webkit_dom_html_image_element_set_align(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, convertedValue);
}

gchar* webkit_dom_html_image_element_get_alt(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::altAttr));
    return result;
}

void webkit_dom_html_image_element_set_alt(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::altAttr, convertedValue);
}

gchar* webkit_dom_html_image_element_get_border(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::borderAttr));
    return result;
}

void webkit_dom_html_image_element_set_border(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::borderAttr, convertedValue);
}

glong webkit_dom_html_image_element_get_height(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->height();
    return result;
}

void webkit_dom_html_image_element_set_height(WebKitDOMHTMLImageElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    WebCore::HTMLImageElement* item = WebKit::core(self);
    item->setHeight(value);
}

glong webkit_dom_html_image_element_get_hspace(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->getIntegralAttribute(WebCore::HTMLNames::hspaceAttr);
    return result;
}

void webkit_dom_html_image_element_set_hspace(WebKitDOMHTMLImageElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    WebCore::HTMLImageElement* item = WebKit::core(self);
    item->setIntegralAttribute(WebCore::HTMLNames::hspaceAttr, value);
}

gboolean webkit_dom_html_image_element_get_is_map(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), FALSE);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::ismapAttr);
    return result;
}

void webkit_dom_html_image_element_set_is_map(WebKitDOMHTMLImageElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    WebCore::HTMLImageElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::ismapAttr, value);
}

gchar* webkit_dom_html_image_element_get_long_desc(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::longdescAttr));
    return result;
}

void webkit_dom_html_image_element_set_long_desc(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::longdescAttr, convertedValue);
}

gchar* webkit_dom_html_image_element_get_src(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::srcAttr));
    return result;
}

void webkit_dom_html_image_element_set_src(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, convertedValue);
}

gchar* webkit_dom_html_image_element_get_use_map(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr));
    return result;
}

void webkit_dom_html_image_element_set_use_map(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr, convertedValue);
}

glong webkit_dom_html_image_element_get_vspace(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->getIntegralAttribute(WebCore::HTMLNames::vspaceAttr);
    return result;
}

void webkit_dom_html_image_element_set_vspace(WebKitDOMHTMLImageElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    WebCore::HTMLImageElement* item = WebKit::core(self);
    item->setIntegralAttribute(WebCore::HTMLNames::vspaceAttr, value);
}

glong webkit_dom_html_image_element_get_width(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->width();
    return result;
}

void webkit_dom_html_image_element_set_width(WebKitDOMHTMLImageElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    WebCore::HTMLImageElement* item = WebKit::core(self);
    item->setWidth(value);
}

gboolean webkit_dom_html_image_element_get_complete(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), FALSE);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gboolean result = item->complete();
    return result;
}

gchar* webkit_dom_html_image_element_get_lowsrc(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getURLAttribute(WebCore::HTMLNames::lowsrcAttr));
    return result;
}

void webkit_dom_html_image_element_set_lowsrc(WebKitDOMHTMLImageElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::lowsrcAttr, convertedValue);
}

glong webkit_dom_html_image_element_get_natural_height(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->naturalHeight();
    return result;
}

glong webkit_dom_html_image_element_get_natural_width(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->naturalWidth();
    return result;
}

glong webkit_dom_html_image_element_get_x(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->x();
    return result;
}

glong webkit_dom_html_image_element_get_y(WebKitDOMHTMLImageElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT(self), 0);
    WebCore::HTMLImageElement* item = WebKit::core(self);
    glong result = item->y();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
