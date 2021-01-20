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
#include "WebKitDOMHTMLDocument.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLDocumentPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLDocument* kit(WebCore::HTMLDocument* obj)
{
    return WEBKIT_DOM_HTML_DOCUMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLDocument* core(WebKitDOMHTMLDocument* request)
{
    return request ? static_cast<WebCore::HTMLDocument*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLDocument* wrapHTMLDocument(WebCore::HTMLDocument* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_DOCUMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_DOCUMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_document_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLDocument* coreTarget = static_cast<WebCore::HTMLDocument*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_document_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLDocument* coreTarget = static_cast<WebCore::HTMLDocument*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_document_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLDocument* coreTarget = static_cast<WebCore::HTMLDocument*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_document_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_document_dispatch_event;
    iface->add_event_listener = webkit_dom_html_document_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_document_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLDocument, webkit_dom_html_document, WEBKIT_DOM_TYPE_DOCUMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_document_dom_event_target_init))

enum {
    DOM_HTML_DOCUMENT_PROP_0,
    DOM_HTML_DOCUMENT_PROP_WIDTH,
    DOM_HTML_DOCUMENT_PROP_HEIGHT,
    DOM_HTML_DOCUMENT_PROP_DIR,
    DOM_HTML_DOCUMENT_PROP_BG_COLOR,
    DOM_HTML_DOCUMENT_PROP_FG_COLOR,
    DOM_HTML_DOCUMENT_PROP_ALINK_COLOR,
    DOM_HTML_DOCUMENT_PROP_LINK_COLOR,
    DOM_HTML_DOCUMENT_PROP_VLINK_COLOR,
};

static void webkit_dom_html_document_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLDocument* self = WEBKIT_DOM_HTML_DOCUMENT(object);

    switch (propertyId) {
    case DOM_HTML_DOCUMENT_PROP_DIR:
        webkit_dom_html_document_set_dir(self, g_value_get_string(value));
        break;
    case DOM_HTML_DOCUMENT_PROP_BG_COLOR:
        webkit_dom_html_document_set_bg_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_DOCUMENT_PROP_FG_COLOR:
        webkit_dom_html_document_set_fg_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_DOCUMENT_PROP_ALINK_COLOR:
        webkit_dom_html_document_set_alink_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_DOCUMENT_PROP_LINK_COLOR:
        webkit_dom_html_document_set_link_color(self, g_value_get_string(value));
        break;
    case DOM_HTML_DOCUMENT_PROP_VLINK_COLOR:
        webkit_dom_html_document_set_vlink_color(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_document_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLDocument* self = WEBKIT_DOM_HTML_DOCUMENT(object);

    switch (propertyId) {
    case DOM_HTML_DOCUMENT_PROP_WIDTH:
        g_value_set_long(value, webkit_dom_html_document_get_width(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_HEIGHT:
        g_value_set_long(value, webkit_dom_html_document_get_height(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_DIR:
        g_value_take_string(value, webkit_dom_html_document_get_dir(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_BG_COLOR:
        g_value_take_string(value, webkit_dom_html_document_get_bg_color(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_FG_COLOR:
        g_value_take_string(value, webkit_dom_html_document_get_fg_color(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_ALINK_COLOR:
        g_value_take_string(value, webkit_dom_html_document_get_alink_color(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_LINK_COLOR:
        g_value_take_string(value, webkit_dom_html_document_get_link_color(self));
        break;
    case DOM_HTML_DOCUMENT_PROP_VLINK_COLOR:
        g_value_take_string(value, webkit_dom_html_document_get_vlink_color(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_document_class_init(WebKitDOMHTMLDocumentClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_document_set_property;
    gobjectClass->get_property = webkit_dom_html_document_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_WIDTH,
        g_param_spec_long(
            "width",
            "HTMLDocument:width",
            "read-only glong HTMLDocument:width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_HEIGHT,
        g_param_spec_long(
            "height",
            "HTMLDocument:height",
            "read-only glong HTMLDocument:height",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_DIR,
        g_param_spec_string(
            "dir",
            "HTMLDocument:dir",
            "read-write gchar* HTMLDocument:dir",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_BG_COLOR,
        g_param_spec_string(
            "bg-color",
            "HTMLDocument:bg-color",
            "read-write gchar* HTMLDocument:bg-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_FG_COLOR,
        g_param_spec_string(
            "fg-color",
            "HTMLDocument:fg-color",
            "read-write gchar* HTMLDocument:fg-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_ALINK_COLOR,
        g_param_spec_string(
            "alink-color",
            "HTMLDocument:alink-color",
            "read-write gchar* HTMLDocument:alink-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_LINK_COLOR,
        g_param_spec_string(
            "link-color",
            "HTMLDocument:link-color",
            "read-write gchar* HTMLDocument:link-color",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_DOCUMENT_PROP_VLINK_COLOR,
        g_param_spec_string(
            "vlink-color",
            "HTMLDocument:vlink-color",
            "read-write gchar* HTMLDocument:vlink-color",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_document_init(WebKitDOMHTMLDocument* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_html_document_close(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    WebCore::HTMLDocument* item = WebKit::core(self);
    item->close();
}

void webkit_dom_html_document_clear(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    WebCore::HTMLDocument* item = WebKit::core(self);
    item->clear();
}

void webkit_dom_html_document_capture_events(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    WebCore::HTMLDocument* item = WebKit::core(self);
    item->captureEvents();
}

void webkit_dom_html_document_release_events(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    WebCore::HTMLDocument* item = WebKit::core(self);
    item->releaseEvents();
}

glong webkit_dom_html_document_get_width(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    glong result = item->width();
    return result;
}

glong webkit_dom_html_document_get_height(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    glong result = item->height();
    return result;
}

gchar* webkit_dom_html_document_get_dir(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->dir());
    return result;
}

void webkit_dom_html_document_set_dir(WebKitDOMHTMLDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLDocument* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDir(convertedValue);
}

gchar* webkit_dom_html_document_get_bg_color(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->bgColor());
    return result;
}

void webkit_dom_html_document_set_bg_color(WebKitDOMHTMLDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLDocument* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setBgColor(convertedValue);
}

gchar* webkit_dom_html_document_get_fg_color(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->fgColor());
    return result;
}

void webkit_dom_html_document_set_fg_color(WebKitDOMHTMLDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLDocument* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setFgColor(convertedValue);
}

gchar* webkit_dom_html_document_get_alink_color(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->alinkColor());
    return result;
}

void webkit_dom_html_document_set_alink_color(WebKitDOMHTMLDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLDocument* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAlinkColor(convertedValue);
}

gchar* webkit_dom_html_document_get_link_color(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->linkColorForBindings());
    return result;
}

void webkit_dom_html_document_set_link_color(WebKitDOMHTMLDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLDocument* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setLinkColorForBindings(convertedValue);
}

gchar* webkit_dom_html_document_get_vlink_color(WebKitDOMHTMLDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), 0);
    WebCore::HTMLDocument* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->vlinkColor());
    return result;
}

void webkit_dom_html_document_set_vlink_color(WebKitDOMHTMLDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLDocument* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setVlinkColor(convertedValue);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
