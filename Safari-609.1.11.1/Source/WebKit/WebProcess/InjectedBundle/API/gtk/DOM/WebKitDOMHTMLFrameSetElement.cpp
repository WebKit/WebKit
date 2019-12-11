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
#include "WebKitDOMHTMLFrameSetElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLFrameSetElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLFrameSetElement* kit(WebCore::HTMLFrameSetElement* obj)
{
    return WEBKIT_DOM_HTML_FRAME_SET_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLFrameSetElement* core(WebKitDOMHTMLFrameSetElement* request)
{
    return request ? static_cast<WebCore::HTMLFrameSetElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLFrameSetElement* wrapHTMLFrameSetElement(WebCore::HTMLFrameSetElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_FRAME_SET_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_FRAME_SET_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_frame_set_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLFrameSetElement* coreTarget = static_cast<WebCore::HTMLFrameSetElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_frame_set_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLFrameSetElement* coreTarget = static_cast<WebCore::HTMLFrameSetElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_frame_set_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLFrameSetElement* coreTarget = static_cast<WebCore::HTMLFrameSetElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_frame_set_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_frame_set_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_frame_set_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_frame_set_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLFrameSetElement, webkit_dom_html_frame_set_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_frame_set_element_dom_event_target_init))

enum {
    DOM_HTML_FRAME_SET_ELEMENT_PROP_0,
    DOM_HTML_FRAME_SET_ELEMENT_PROP_COLS,
    DOM_HTML_FRAME_SET_ELEMENT_PROP_ROWS,
};

static void webkit_dom_html_frame_set_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLFrameSetElement* self = WEBKIT_DOM_HTML_FRAME_SET_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_FRAME_SET_ELEMENT_PROP_COLS:
        webkit_dom_html_frame_set_element_set_cols(self, g_value_get_string(value));
        break;
    case DOM_HTML_FRAME_SET_ELEMENT_PROP_ROWS:
        webkit_dom_html_frame_set_element_set_rows(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_frame_set_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLFrameSetElement* self = WEBKIT_DOM_HTML_FRAME_SET_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_FRAME_SET_ELEMENT_PROP_COLS:
        g_value_take_string(value, webkit_dom_html_frame_set_element_get_cols(self));
        break;
    case DOM_HTML_FRAME_SET_ELEMENT_PROP_ROWS:
        g_value_take_string(value, webkit_dom_html_frame_set_element_get_rows(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_frame_set_element_class_init(WebKitDOMHTMLFrameSetElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_frame_set_element_set_property;
    gobjectClass->get_property = webkit_dom_html_frame_set_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FRAME_SET_ELEMENT_PROP_COLS,
        g_param_spec_string(
            "cols",
            "HTMLFrameSetElement:cols",
            "read-write gchar* HTMLFrameSetElement:cols",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_FRAME_SET_ELEMENT_PROP_ROWS,
        g_param_spec_string(
            "rows",
            "HTMLFrameSetElement:rows",
            "read-write gchar* HTMLFrameSetElement:rows",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_frame_set_element_init(WebKitDOMHTMLFrameSetElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_frame_set_element_get_cols(WebKitDOMHTMLFrameSetElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FRAME_SET_ELEMENT(self), 0);
    WebCore::HTMLFrameSetElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::colsAttr));
    return result;
}

void webkit_dom_html_frame_set_element_set_cols(WebKitDOMHTMLFrameSetElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FRAME_SET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFrameSetElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::colsAttr, convertedValue);
}

gchar* webkit_dom_html_frame_set_element_get_rows(WebKitDOMHTMLFrameSetElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_FRAME_SET_ELEMENT(self), 0);
    WebCore::HTMLFrameSetElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::rowsAttr));
    return result;
}

void webkit_dom_html_frame_set_element_set_rows(WebKitDOMHTMLFrameSetElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_FRAME_SET_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLFrameSetElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::rowsAttr, convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
