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
#include "WebKitDOMDOMWindow.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/JSExecState.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/UserMessageHandlersNamespace.h>
#include <WebCore/WebKitNamespace.h>
#include <WebCore/WindowProxy.h>
#include "WebKitDOMCSSStyleDeclarationPrivate.h"
#include "WebKitDOMDOMSelectionPrivate.h"
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_DOM_WINDOW_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_DOM_WINDOW, WebKitDOMDOMWindowPrivate)

typedef struct _WebKitDOMDOMWindowPrivate {
    RefPtr<WebCore::DOMWindow> coreObject;
} WebKitDOMDOMWindowPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMDOMWindow* kit(WebCore::DOMWindow* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_DOM_WINDOW(ret);

    return wrapDOMWindow(obj);
}

WebKitDOMDOMWindow* kit(WebCore::WindowProxy* windowProxy)
{
    if (!windowProxy || !is<WebCore::DOMWindow>(windowProxy->window()))
        return nullptr;

    return kit(downcast<WebCore::DOMWindow>(windowProxy->window()));
}

WebCore::WindowProxy* toWindowProxy(WebKitDOMDOMWindow* view)
{
    auto* window = core(view);
    if (!window || !window->frame())
        return nullptr;
    return &window->frame()->windowProxy();
}

WebCore::DOMWindow* core(WebKitDOMDOMWindow* request)
{
    return request ? static_cast<WebCore::DOMWindow*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMDOMWindow* wrapDOMWindow(WebCore::DOMWindow* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_DOM_WINDOW(g_object_new(WEBKIT_DOM_TYPE_DOM_WINDOW, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_dom_window_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::DOMWindow* coreTarget = static_cast<WebCore::DOMWindow*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_dom_window_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::DOMWindow* coreTarget = static_cast<WebCore::DOMWindow*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_dom_window_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::DOMWindow* coreTarget = static_cast<WebCore::DOMWindow*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_dom_window_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_dom_window_dispatch_event;
    iface->add_event_listener = webkit_dom_dom_window_add_event_listener;
    iface->remove_event_listener = webkit_dom_dom_window_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMDOMWindow, webkit_dom_dom_window, WEBKIT_DOM_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_dom_window_dom_event_target_init))

enum {
    DOM_WINDOW_PROP_0,
    DOM_WINDOW_PROP_FRAME_ELEMENT,
    DOM_WINDOW_PROP_OFFSCREEN_BUFFERING,
    DOM_WINDOW_PROP_OUTER_HEIGHT,
    DOM_WINDOW_PROP_OUTER_WIDTH,
    DOM_WINDOW_PROP_INNER_HEIGHT,
    DOM_WINDOW_PROP_INNER_WIDTH,
    DOM_WINDOW_PROP_SCREEN_X,
    DOM_WINDOW_PROP_SCREEN_Y,
    DOM_WINDOW_PROP_SCREEN_LEFT,
    DOM_WINDOW_PROP_SCREEN_TOP,
    DOM_WINDOW_PROP_SCROLL_X,
    DOM_WINDOW_PROP_SCROLL_Y,
    DOM_WINDOW_PROP_PAGE_X_OFFSET,
    DOM_WINDOW_PROP_PAGE_Y_OFFSET,
    DOM_WINDOW_PROP_CLOSED,
    DOM_WINDOW_PROP_LENGTH,
    DOM_WINDOW_PROP_NAME,
    DOM_WINDOW_PROP_STATUS,
    DOM_WINDOW_PROP_DEFAULT_STATUS,
    DOM_WINDOW_PROP_SELF,
    DOM_WINDOW_PROP_WINDOW,
    DOM_WINDOW_PROP_FRAMES,
    DOM_WINDOW_PROP_OPENER,
    DOM_WINDOW_PROP_PARENT,
    DOM_WINDOW_PROP_TOP,
    DOM_WINDOW_PROP_DOCUMENT,
    DOM_WINDOW_PROP_DEVICE_PIXEL_RATIO,
    DOM_WINDOW_PROP_ORIENTATION,
};

static void webkit_dom_dom_window_finalize(GObject* object)
{
    WebKitDOMDOMWindowPrivate* priv = WEBKIT_DOM_DOM_WINDOW_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMDOMWindowPrivate();
    G_OBJECT_CLASS(webkit_dom_dom_window_parent_class)->finalize(object);
}

static void webkit_dom_dom_window_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMDOMWindow* self = WEBKIT_DOM_DOM_WINDOW(object);

    switch (propertyId) {
    case DOM_WINDOW_PROP_NAME:
        webkit_dom_dom_window_set_name(self, g_value_get_string(value));
        break;
    case DOM_WINDOW_PROP_STATUS:
        webkit_dom_dom_window_set_status(self, g_value_get_string(value));
        break;
    case DOM_WINDOW_PROP_DEFAULT_STATUS:
        webkit_dom_dom_window_set_default_status(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_dom_window_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMDOMWindow* self = WEBKIT_DOM_DOM_WINDOW(object);

    switch (propertyId) {
    case DOM_WINDOW_PROP_FRAME_ELEMENT:
        g_value_set_object(value, webkit_dom_dom_window_get_frame_element(self));
        break;
    case DOM_WINDOW_PROP_OFFSCREEN_BUFFERING:
        g_value_set_boolean(value, webkit_dom_dom_window_get_offscreen_buffering(self));
        break;
    case DOM_WINDOW_PROP_OUTER_HEIGHT:
        g_value_set_long(value, webkit_dom_dom_window_get_outer_height(self));
        break;
    case DOM_WINDOW_PROP_OUTER_WIDTH:
        g_value_set_long(value, webkit_dom_dom_window_get_outer_width(self));
        break;
    case DOM_WINDOW_PROP_INNER_HEIGHT:
        g_value_set_long(value, webkit_dom_dom_window_get_inner_height(self));
        break;
    case DOM_WINDOW_PROP_INNER_WIDTH:
        g_value_set_long(value, webkit_dom_dom_window_get_inner_width(self));
        break;
    case DOM_WINDOW_PROP_SCREEN_X:
        g_value_set_long(value, webkit_dom_dom_window_get_screen_x(self));
        break;
    case DOM_WINDOW_PROP_SCREEN_Y:
        g_value_set_long(value, webkit_dom_dom_window_get_screen_y(self));
        break;
    case DOM_WINDOW_PROP_SCREEN_LEFT:
        g_value_set_long(value, webkit_dom_dom_window_get_screen_left(self));
        break;
    case DOM_WINDOW_PROP_SCREEN_TOP:
        g_value_set_long(value, webkit_dom_dom_window_get_screen_top(self));
        break;
    case DOM_WINDOW_PROP_SCROLL_X:
        g_value_set_long(value, webkit_dom_dom_window_get_scroll_x(self));
        break;
    case DOM_WINDOW_PROP_SCROLL_Y:
        g_value_set_long(value, webkit_dom_dom_window_get_scroll_y(self));
        break;
    case DOM_WINDOW_PROP_PAGE_X_OFFSET:
        g_value_set_long(value, webkit_dom_dom_window_get_page_x_offset(self));
        break;
    case DOM_WINDOW_PROP_PAGE_Y_OFFSET:
        g_value_set_long(value, webkit_dom_dom_window_get_page_y_offset(self));
        break;
    case DOM_WINDOW_PROP_CLOSED:
        g_value_set_boolean(value, webkit_dom_dom_window_get_closed(self));
        break;
    case DOM_WINDOW_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_dom_window_get_length(self));
        break;
    case DOM_WINDOW_PROP_NAME:
        g_value_take_string(value, webkit_dom_dom_window_get_name(self));
        break;
    case DOM_WINDOW_PROP_STATUS:
        g_value_take_string(value, webkit_dom_dom_window_get_status(self));
        break;
    case DOM_WINDOW_PROP_DEFAULT_STATUS:
        g_value_take_string(value, webkit_dom_dom_window_get_default_status(self));
        break;
    case DOM_WINDOW_PROP_SELF:
        g_value_set_object(value, webkit_dom_dom_window_get_self(self));
        break;
    case DOM_WINDOW_PROP_WINDOW:
        g_value_set_object(value, webkit_dom_dom_window_get_window(self));
        break;
    case DOM_WINDOW_PROP_FRAMES:
        g_value_set_object(value, webkit_dom_dom_window_get_frames(self));
        break;
    case DOM_WINDOW_PROP_OPENER:
        g_value_set_object(value, webkit_dom_dom_window_get_opener(self));
        break;
    case DOM_WINDOW_PROP_PARENT:
        g_value_set_object(value, webkit_dom_dom_window_get_parent(self));
        break;
    case DOM_WINDOW_PROP_TOP:
        g_value_set_object(value, webkit_dom_dom_window_get_top(self));
        break;
    case DOM_WINDOW_PROP_DOCUMENT:
        g_value_set_object(value, webkit_dom_dom_window_get_document(self));
        break;
    case DOM_WINDOW_PROP_DEVICE_PIXEL_RATIO:
        g_value_set_double(value, webkit_dom_dom_window_get_device_pixel_ratio(self));
        break;
    case DOM_WINDOW_PROP_ORIENTATION:
        g_value_set_long(value, webkit_dom_dom_window_get_orientation(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_dom_window_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_dom_window_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMDOMWindowPrivate* priv = WEBKIT_DOM_DOM_WINDOW_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::DOMWindow*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_dom_window_class_init(WebKitDOMDOMWindowClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMDOMWindowPrivate));
    gobjectClass->constructor = webkit_dom_dom_window_constructor;
    gobjectClass->finalize = webkit_dom_dom_window_finalize;
    gobjectClass->set_property = webkit_dom_dom_window_set_property;
    gobjectClass->get_property = webkit_dom_dom_window_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_FRAME_ELEMENT,
        g_param_spec_object(
            "frame-element",
            "DOMWindow:frame-element",
            "read-only WebKitDOMElement* DOMWindow:frame-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_OFFSCREEN_BUFFERING,
        g_param_spec_boolean(
            "offscreen-buffering",
            "DOMWindow:offscreen-buffering",
            "read-only gboolean DOMWindow:offscreen-buffering",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_OUTER_HEIGHT,
        g_param_spec_long(
            "outer-height",
            "DOMWindow:outer-height",
            "read-only glong DOMWindow:outer-height",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_OUTER_WIDTH,
        g_param_spec_long(
            "outer-width",
            "DOMWindow:outer-width",
            "read-only glong DOMWindow:outer-width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_INNER_HEIGHT,
        g_param_spec_long(
            "inner-height",
            "DOMWindow:inner-height",
            "read-only glong DOMWindow:inner-height",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_INNER_WIDTH,
        g_param_spec_long(
            "inner-width",
            "DOMWindow:inner-width",
            "read-only glong DOMWindow:inner-width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SCREEN_X,
        g_param_spec_long(
            "screen-x",
            "DOMWindow:screen-x",
            "read-only glong DOMWindow:screen-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SCREEN_Y,
        g_param_spec_long(
            "screen-y",
            "DOMWindow:screen-y",
            "read-only glong DOMWindow:screen-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SCREEN_LEFT,
        g_param_spec_long(
            "screen-left",
            "DOMWindow:screen-left",
            "read-only glong DOMWindow:screen-left",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SCREEN_TOP,
        g_param_spec_long(
            "screen-top",
            "DOMWindow:screen-top",
            "read-only glong DOMWindow:screen-top",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SCROLL_X,
        g_param_spec_long(
            "scroll-x",
            "DOMWindow:scroll-x",
            "read-only glong DOMWindow:scroll-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SCROLL_Y,
        g_param_spec_long(
            "scroll-y",
            "DOMWindow:scroll-y",
            "read-only glong DOMWindow:scroll-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_PAGE_X_OFFSET,
        g_param_spec_long(
            "page-x-offset",
            "DOMWindow:page-x-offset",
            "read-only glong DOMWindow:page-x-offset",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_PAGE_Y_OFFSET,
        g_param_spec_long(
            "page-y-offset",
            "DOMWindow:page-y-offset",
            "read-only glong DOMWindow:page-y-offset",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_CLOSED,
        g_param_spec_boolean(
            "closed",
            "DOMWindow:closed",
            "read-only gboolean DOMWindow:closed",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "DOMWindow:length",
            "read-only gulong DOMWindow:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_NAME,
        g_param_spec_string(
            "name",
            "DOMWindow:name",
            "read-write gchar* DOMWindow:name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_STATUS,
        g_param_spec_string(
            "status",
            "DOMWindow:status",
            "read-write gchar* DOMWindow:status",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_DEFAULT_STATUS,
        g_param_spec_string(
            "default-status",
            "DOMWindow:default-status",
            "read-write gchar* DOMWindow:default-status",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_SELF,
        g_param_spec_object(
            "self",
            "DOMWindow:self",
            "read-only WebKitDOMDOMWindow* DOMWindow:self",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_WINDOW,
        g_param_spec_object(
            "window",
            "DOMWindow:window",
            "read-only WebKitDOMDOMWindow* DOMWindow:window",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_FRAMES,
        g_param_spec_object(
            "frames",
            "DOMWindow:frames",
            "read-only WebKitDOMDOMWindow* DOMWindow:frames",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_OPENER,
        g_param_spec_object(
            "opener",
            "DOMWindow:opener",
            "read-only WebKitDOMDOMWindow* DOMWindow:opener",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_PARENT,
        g_param_spec_object(
            "parent",
            "DOMWindow:parent",
            "read-only WebKitDOMDOMWindow* DOMWindow:parent",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_TOP,
        g_param_spec_object(
            "top",
            "DOMWindow:top",
            "read-only WebKitDOMDOMWindow* DOMWindow:top",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_DOCUMENT,
        g_param_spec_object(
            "document",
            "DOMWindow:document",
            "read-only WebKitDOMDocument* DOMWindow:document",
            WEBKIT_DOM_TYPE_DOCUMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_DEVICE_PIXEL_RATIO,
        g_param_spec_double(
            "device-pixel-ratio",
            "DOMWindow:device-pixel-ratio",
            "read-only gdouble DOMWindow:device-pixel-ratio",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_WINDOW_PROP_ORIENTATION,
        g_param_spec_long(
            "orientation",
            "DOMWindow:orientation",
            "read-only glong DOMWindow:orientation",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_dom_window_init(WebKitDOMDOMWindow* request)
{
    WebKitDOMDOMWindowPrivate* priv = WEBKIT_DOM_DOM_WINDOW_GET_PRIVATE(request);
    new (priv) WebKitDOMDOMWindowPrivate();
}

WebKitDOMDOMSelection* webkit_dom_dom_window_get_selection(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    RefPtr<WebCore::DOMSelection> gobjectResult = WTF::getPtr(item->getSelection());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_dom_window_focus(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->focus();
}

void webkit_dom_dom_window_blur(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->blur();
}

void webkit_dom_dom_window_close(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->close();
}

void webkit_dom_dom_window_print(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->print();
}

void webkit_dom_dom_window_stop(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->stop();
}

void webkit_dom_dom_window_alert(WebKitDOMDOMWindow* self, const gchar* message)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    g_return_if_fail(message);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedMessage = WTF::String::fromUTF8(message);
    item->alert(convertedMessage);
}

gboolean webkit_dom_dom_window_confirm(WebKitDOMDOMWindow* self, const gchar* message)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), FALSE);
    g_return_val_if_fail(message, FALSE);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedMessage = WTF::String::fromUTF8(message);
    gboolean result = item->confirmForBindings(convertedMessage);
    return result;
}

gchar* webkit_dom_dom_window_prompt(WebKitDOMDOMWindow* self, const gchar* message, const gchar* defaultValue)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    g_return_val_if_fail(message, 0);
    g_return_val_if_fail(defaultValue, 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedMessage = WTF::String::fromUTF8(message);
    WTF::String convertedDefaultValue = WTF::String::fromUTF8(defaultValue);
    gchar* result = convertToUTF8String(item->prompt(convertedMessage, convertedDefaultValue));
    return result;
}

gboolean webkit_dom_dom_window_find(WebKitDOMDOMWindow* self, const gchar* string, gboolean caseSensitive, gboolean backwards, gboolean wrap, gboolean wholeWord, gboolean searchInFrames, gboolean showDialog)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), FALSE);
    g_return_val_if_fail(string, FALSE);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedString = WTF::String::fromUTF8(string);
    gboolean result = item->find(convertedString, caseSensitive, backwards, wrap, wholeWord, searchInFrames, showDialog);
    return result;
}

void webkit_dom_dom_window_scroll_by(WebKitDOMDOMWindow* self, gdouble x, gdouble y)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->scrollBy(x, y);
}

void webkit_dom_dom_window_scroll_to(WebKitDOMDOMWindow* self, gdouble x, gdouble y)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->scrollTo(x, y);
}

void webkit_dom_dom_window_move_by(WebKitDOMDOMWindow* self, gfloat x, gfloat y)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->moveBy(x, y);
}

void webkit_dom_dom_window_move_to(WebKitDOMDOMWindow* self, gfloat x, gfloat y)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->moveTo(x, y);
}

void webkit_dom_dom_window_resize_by(WebKitDOMDOMWindow* self, gfloat x, gfloat y)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->resizeBy(x, y);
}

void webkit_dom_dom_window_resize_to(WebKitDOMDOMWindow* self, gfloat width, gfloat height)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->resizeTo(width, height);
}

WebKitDOMCSSStyleDeclaration* webkit_dom_dom_window_get_computed_style(WebKitDOMDOMWindow* self, WebKitDOMElement* element, const gchar* pseudoElement)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(element), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    WebCore::Element* convertedElement = WebKit::core(element);
    WTF::String convertedPseudoElement = WTF::String::fromUTF8(pseudoElement);
    RefPtr<WebCore::CSSStyleDeclaration> gobjectResult = WTF::getPtr(item->getComputedStyle(*convertedElement, convertedPseudoElement));
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_dom_window_capture_events(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->captureEvents();
}

void webkit_dom_dom_window_release_events(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    WebCore::DOMWindow* item = WebKit::core(self);
    item->releaseEvents();
}

WebKitDOMElement* webkit_dom_dom_window_get_frame_element(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->frameElement());
    return WebKit::kit(gobjectResult.get());
}

gboolean webkit_dom_dom_window_get_offscreen_buffering(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), FALSE);
    WebCore::DOMWindow* item = WebKit::core(self);
    gboolean result = item->offscreenBuffering();
    return result;
}

glong webkit_dom_dom_window_get_outer_height(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->outerHeight();
    return result;
}

glong webkit_dom_dom_window_get_outer_width(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->outerWidth();
    return result;
}

glong webkit_dom_dom_window_get_inner_height(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->innerHeight();
    return result;
}

glong webkit_dom_dom_window_get_inner_width(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->innerWidth();
    return result;
}

glong webkit_dom_dom_window_get_screen_x(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->screenX();
    return result;
}

glong webkit_dom_dom_window_get_screen_y(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->screenY();
    return result;
}

glong webkit_dom_dom_window_get_screen_left(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->screenLeft();
    return result;
}

glong webkit_dom_dom_window_get_screen_top(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->screenTop();
    return result;
}

glong webkit_dom_dom_window_get_scroll_x(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->scrollX();
    return result;
}

glong webkit_dom_dom_window_get_scroll_y(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->scrollY();
    return result;
}

glong webkit_dom_dom_window_get_page_x_offset(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->scrollX();
    return result;
}

glong webkit_dom_dom_window_get_page_y_offset(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->scrollY();
    return result;
}

gboolean webkit_dom_dom_window_get_closed(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), FALSE);
    WebCore::DOMWindow* item = WebKit::core(self);
    gboolean result = item->closed();
    return result;
}

gulong webkit_dom_dom_window_get_length(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

gchar* webkit_dom_dom_window_get_name(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->name());
    return result;
}

void webkit_dom_dom_window_set_name(WebKitDOMDOMWindow* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    g_return_if_fail(value);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setName(convertedValue);
}

gchar* webkit_dom_dom_window_get_status(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->status());
    return result;
}

void webkit_dom_dom_window_set_status(WebKitDOMDOMWindow* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    g_return_if_fail(value);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setStatus(convertedValue);
}

gchar* webkit_dom_dom_window_get_default_status(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->defaultStatus());
    return result;
}

void webkit_dom_dom_window_set_default_status(WebKitDOMDOMWindow* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self));
    g_return_if_fail(value);
    WebCore::DOMWindow* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDefaultStatus(convertedValue);
}

WebKitDOMDOMWindow* webkit_dom_dom_window_get_self(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    return WebKit::kit(item);
}

WebKitDOMDOMWindow* webkit_dom_dom_window_get_window(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    return WebKit::kit(item);
}

WebKitDOMDOMWindow* webkit_dom_dom_window_get_frames(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    return WebKit::kit(item);
}

WebKitDOMDOMWindow* webkit_dom_dom_window_get_opener(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    auto* openerWindowProxy = item->opener();
    RefPtr<WebCore::DOMWindow> gobjectResult = downcast<WebCore::DOMWindow>(openerWindowProxy->window());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMDOMWindow* webkit_dom_dom_window_get_parent(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    auto* parentWindowProxy = item->parent();
    RefPtr<WebCore::DOMWindow> gobjectResult = downcast<WebCore::DOMWindow>(parentWindowProxy->window());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMDOMWindow* webkit_dom_dom_window_get_top(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    auto* topWindowProxy = item->top();
    RefPtr<WebCore::DOMWindow> gobjectResult = downcast<WebCore::DOMWindow>(topWindowProxy->window());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMDocument* webkit_dom_dom_window_get_document(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    RefPtr<WebCore::Document> gobjectResult = WTF::getPtr(item->document());
    return WebKit::kit(gobjectResult.get());
}

gdouble webkit_dom_dom_window_get_device_pixel_ratio(WebKitDOMDOMWindow* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    gdouble result = item->devicePixelRatio();
    return result;
}

glong webkit_dom_dom_window_get_orientation(WebKitDOMDOMWindow* self)
{
#if ENABLE(ORIENTATION_EVENTS)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(self), 0);
    WebCore::DOMWindow* item = WebKit::core(self);
    glong result = item->orientation();
    return result;
#else
    UNUSED_PARAM(self);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Orientation Events")
    return static_cast<glong>(0);
#endif /* ENABLE(ORIENTATION_EVENTS) */
}

gboolean webkit_dom_dom_window_webkit_message_handlers_post_message(WebKitDOMDOMWindow* window, const gchar* handlerName, const gchar* message)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(window), FALSE);
    g_return_val_if_fail(handlerName, FALSE);
    g_return_val_if_fail(message, FALSE);

    WebCore::DOMWindow* domWindow = WebKit::core(window);
    if (!domWindow->shouldHaveWebKitNamespaceForWorld(WebCore::mainThreadNormalWorld()))
        return FALSE;

    auto webkitNamespace = domWindow->webkitNamespace();
    if (!webkitNamespace)
        return FALSE;

    auto handler = webkitNamespace->messageHandlers()->namedItem(WebCore::mainThreadNormalWorld(), String::fromUTF8(handlerName));
    if (!handler)
        return FALSE;

    auto result = handler->postMessage(WebCore::SerializedScriptValue::create(String::fromUTF8(message)));
    if (result.hasException())
        return FALSE;

    return TRUE;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
