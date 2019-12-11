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
#include "WebKitDOMEvent.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTargetPrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_EVENT_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_EVENT, WebKitDOMEventPrivate)

typedef struct _WebKitDOMEventPrivate {
    RefPtr<WebCore::Event> coreObject;
} WebKitDOMEventPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMEvent* kit(WebCore::Event* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_EVENT(ret);

    return wrap(obj);
}

WebCore::Event* core(WebKitDOMEvent* request)
{
    return request ? static_cast<WebCore::Event*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMEvent* wrapEvent(WebCore::Event* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_EVENT(g_object_new(WEBKIT_DOM_TYPE_EVENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMEvent, webkit_dom_event, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_EVENT_PROP_0,
    DOM_EVENT_PROP_TYPE,
    DOM_EVENT_PROP_TARGET,
    DOM_EVENT_PROP_CURRENT_TARGET,
    DOM_EVENT_PROP_EVENT_PHASE,
    DOM_EVENT_PROP_BUBBLES,
    DOM_EVENT_PROP_CANCELABLE,
    DOM_EVENT_PROP_TIME_STAMP,
    DOM_EVENT_PROP_SRC_ELEMENT,
    DOM_EVENT_PROP_RETURN_VALUE,
    DOM_EVENT_PROP_CANCEL_BUBBLE,
};

static void webkit_dom_event_finalize(GObject* object)
{
    WebKitDOMEventPrivate* priv = WEBKIT_DOM_EVENT_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMEventPrivate();
    G_OBJECT_CLASS(webkit_dom_event_parent_class)->finalize(object);
}

static void webkit_dom_event_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMEvent* self = WEBKIT_DOM_EVENT(object);

    switch (propertyId) {
    case DOM_EVENT_PROP_RETURN_VALUE:
        webkit_dom_event_set_return_value(self, g_value_get_boolean(value));
        break;
    case DOM_EVENT_PROP_CANCEL_BUBBLE:
        webkit_dom_event_set_cancel_bubble(self, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_event_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMEvent* self = WEBKIT_DOM_EVENT(object);

    switch (propertyId) {
    case DOM_EVENT_PROP_TYPE:
        g_value_take_string(value, webkit_dom_event_get_event_type(self));
        break;
    case DOM_EVENT_PROP_TARGET:
        g_value_set_object(value, webkit_dom_event_get_target(self));
        break;
    case DOM_EVENT_PROP_CURRENT_TARGET:
        g_value_set_object(value, webkit_dom_event_get_current_target(self));
        break;
    case DOM_EVENT_PROP_EVENT_PHASE:
        g_value_set_uint(value, webkit_dom_event_get_event_phase(self));
        break;
    case DOM_EVENT_PROP_BUBBLES:
        g_value_set_boolean(value, webkit_dom_event_get_bubbles(self));
        break;
    case DOM_EVENT_PROP_CANCELABLE:
        g_value_set_boolean(value, webkit_dom_event_get_cancelable(self));
        break;
    case DOM_EVENT_PROP_TIME_STAMP:
        g_value_set_uint(value, webkit_dom_event_get_time_stamp(self));
        break;
    case DOM_EVENT_PROP_SRC_ELEMENT:
        g_value_set_object(value, webkit_dom_event_get_src_element(self));
        break;
    case DOM_EVENT_PROP_RETURN_VALUE:
        g_value_set_boolean(value, webkit_dom_event_get_return_value(self));
        break;
    case DOM_EVENT_PROP_CANCEL_BUBBLE:
        g_value_set_boolean(value, webkit_dom_event_get_cancel_bubble(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_event_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_event_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMEventPrivate* priv = WEBKIT_DOM_EVENT_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::Event*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_event_class_init(WebKitDOMEventClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMEventPrivate));
    gobjectClass->constructor = webkit_dom_event_constructor;
    gobjectClass->finalize = webkit_dom_event_finalize;
    gobjectClass->set_property = webkit_dom_event_set_property;
    gobjectClass->get_property = webkit_dom_event_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_TYPE,
        g_param_spec_string(
            "type",
            "Event:type",
            "read-only gchar* Event:type",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_TARGET,
        g_param_spec_object(
            "target",
            "Event:target",
            "read-only WebKitDOMEventTarget* Event:target",
            WEBKIT_DOM_TYPE_EVENT_TARGET,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_CURRENT_TARGET,
        g_param_spec_object(
            "current-target",
            "Event:current-target",
            "read-only WebKitDOMEventTarget* Event:current-target",
            WEBKIT_DOM_TYPE_EVENT_TARGET,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_EVENT_PHASE,
        g_param_spec_uint(
            "event-phase",
            "Event:event-phase",
            "read-only gushort Event:event-phase",
            0, G_MAXUINT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_BUBBLES,
        g_param_spec_boolean(
            "bubbles",
            "Event:bubbles",
            "read-only gboolean Event:bubbles",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_CANCELABLE,
        g_param_spec_boolean(
            "cancelable",
            "Event:cancelable",
            "read-only gboolean Event:cancelable",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_TIME_STAMP,
        g_param_spec_uint(
            "time-stamp",
            "Event:time-stamp",
            "read-only guint32 Event:time-stamp",
            0, G_MAXUINT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_SRC_ELEMENT,
        g_param_spec_object(
            "src-element",
            "Event:src-element",
            "read-only WebKitDOMEventTarget* Event:src-element",
            WEBKIT_DOM_TYPE_EVENT_TARGET,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_RETURN_VALUE,
        g_param_spec_boolean(
            "return-value",
            "Event:return-value",
            "read-write gboolean Event:return-value",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_EVENT_PROP_CANCEL_BUBBLE,
        g_param_spec_boolean(
            "cancel-bubble",
            "Event:cancel-bubble",
            "read-write gboolean Event:cancel-bubble",
            FALSE,
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_event_init(WebKitDOMEvent* request)
{
    WebKitDOMEventPrivate* priv = WEBKIT_DOM_EVENT_GET_PRIVATE(request);
    new (priv) WebKitDOMEventPrivate();
}

void webkit_dom_event_stop_propagation(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_EVENT(self));
    WebCore::Event* item = WebKit::core(self);
    item->stopPropagation();
}

void webkit_dom_event_prevent_default(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_EVENT(self));
    WebCore::Event* item = WebKit::core(self);
    item->preventDefault();
}

void webkit_dom_event_init_event(WebKitDOMEvent* self, const gchar* eventTypeArg, gboolean canBubbleArg, gboolean cancelableArg)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_EVENT(self));
    g_return_if_fail(eventTypeArg);
    WebCore::Event* item = WebKit::core(self);
    WTF::String convertedEventTypeArg = WTF::String::fromUTF8(eventTypeArg);
    item->initEvent(convertedEventTypeArg, canBubbleArg, cancelableArg);
}

gchar* webkit_dom_event_get_event_type(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), 0);
    WebCore::Event* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

WebKitDOMEventTarget* webkit_dom_event_get_target(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), 0);
    WebCore::Event* item = WebKit::core(self);
    RefPtr<WebCore::EventTarget> gobjectResult = WTF::getPtr(item->target());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMEventTarget* webkit_dom_event_get_current_target(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), 0);
    WebCore::Event* item = WebKit::core(self);
    RefPtr<WebCore::EventTarget> gobjectResult = WTF::getPtr(item->currentTarget());
    return WebKit::kit(gobjectResult.get());
}

gushort webkit_dom_event_get_event_phase(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), 0);
    WebCore::Event* item = WebKit::core(self);
    gushort result = item->eventPhase();
    return result;
}

gboolean webkit_dom_event_get_bubbles(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), FALSE);
    WebCore::Event* item = WebKit::core(self);
    gboolean result = item->bubbles();
    return result;
}

gboolean webkit_dom_event_get_cancelable(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), FALSE);
    WebCore::Event* item = WebKit::core(self);
    gboolean result = item->cancelable();
    return result;
}

guint32 webkit_dom_event_get_time_stamp(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), 0);
    WebCore::Event* item = WebKit::core(self);
    guint32 result = item->timeStamp().approximateWallTime().secondsSinceEpoch().milliseconds();
    return result;
}

WebKitDOMEventTarget* webkit_dom_event_get_src_element(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), 0);
    WebCore::Event* item = WebKit::core(self);
    RefPtr<WebCore::EventTarget> gobjectResult = WTF::getPtr(item->target());
    return WebKit::kit(gobjectResult.get());
}

gboolean webkit_dom_event_get_return_value(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), FALSE);
    WebCore::Event* item = WebKit::core(self);
    gboolean result = item->legacyReturnValue();
    return result;
}

void webkit_dom_event_set_return_value(WebKitDOMEvent* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_EVENT(self));
    WebCore::Event* item = WebKit::core(self);
    item->setLegacyReturnValue(value);
}

gboolean webkit_dom_event_get_cancel_bubble(WebKitDOMEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(self), FALSE);
    WebCore::Event* item = WebKit::core(self);
    gboolean result = item->cancelBubble();
    return result;
}

void webkit_dom_event_set_cancel_bubble(WebKitDOMEvent* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_EVENT(self));
    WebCore::Event* item = WebKit::core(self);
    item->setCancelBubble(value);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
