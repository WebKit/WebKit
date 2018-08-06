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
#include "WebKitDOMHTMLHtmlElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLHtmlElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLHtmlElement* kit(WebCore::HTMLHtmlElement* obj)
{
    return WEBKIT_DOM_HTML_HTML_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLHtmlElement* core(WebKitDOMHTMLHtmlElement* request)
{
    return request ? static_cast<WebCore::HTMLHtmlElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLHtmlElement* wrapHTMLHtmlElement(WebCore::HTMLHtmlElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_HTML_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_HTML_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_html_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLHtmlElement* coreTarget = static_cast<WebCore::HTMLHtmlElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_html_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLHtmlElement* coreTarget = static_cast<WebCore::HTMLHtmlElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_html_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLHtmlElement* coreTarget = static_cast<WebCore::HTMLHtmlElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_html_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_html_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_html_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_html_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLHtmlElement, webkit_dom_html_html_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_html_element_dom_event_target_init))

enum {
    DOM_HTML_HTML_ELEMENT_PROP_0,
    DOM_HTML_HTML_ELEMENT_PROP_VERSION,
};

static void webkit_dom_html_html_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLHtmlElement* self = WEBKIT_DOM_HTML_HTML_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_HTML_ELEMENT_PROP_VERSION:
        webkit_dom_html_html_element_set_version(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_html_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLHtmlElement* self = WEBKIT_DOM_HTML_HTML_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_HTML_ELEMENT_PROP_VERSION:
        g_value_take_string(value, webkit_dom_html_html_element_get_version(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_html_element_class_init(WebKitDOMHTMLHtmlElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_html_element_set_property;
    gobjectClass->get_property = webkit_dom_html_html_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_HTML_ELEMENT_PROP_VERSION,
        g_param_spec_string(
            "version",
            "HTMLHtmlElement:version",
            "read-write gchar* HTMLHtmlElement:version",
            "",
            WEBKIT_PARAM_READWRITE));
}

static void webkit_dom_html_html_element_init(WebKitDOMHTMLHtmlElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_html_element_get_version(WebKitDOMHTMLHtmlElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_HTML_ELEMENT(self), 0);
    WebCore::HTMLHtmlElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::versionAttr));
    return result;
}

void webkit_dom_html_html_element_set_version(WebKitDOMHTMLHtmlElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_HTML_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLHtmlElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::versionAttr, convertedValue);
}
G_GNUC_END_IGNORE_DEPRECATIONS;
