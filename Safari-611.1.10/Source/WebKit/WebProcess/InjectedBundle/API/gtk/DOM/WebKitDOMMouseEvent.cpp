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
#include "WebKitDOMMouseEvent.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTargetPrivate.h"
#include "WebKitDOMMouseEventPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMMouseEvent* kit(WebCore::MouseEvent* obj)
{
    return WEBKIT_DOM_MOUSE_EVENT(kit(static_cast<WebCore::Event*>(obj)));
}

WebCore::MouseEvent* core(WebKitDOMMouseEvent* request)
{
    return request ? static_cast<WebCore::MouseEvent*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMMouseEvent* wrapMouseEvent(WebCore::MouseEvent* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_MOUSE_EVENT(g_object_new(WEBKIT_DOM_TYPE_MOUSE_EVENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMMouseEvent, webkit_dom_mouse_event, WEBKIT_DOM_TYPE_UI_EVENT)

enum {
    DOM_MOUSE_EVENT_PROP_0,
    DOM_MOUSE_EVENT_PROP_SCREEN_X,
    DOM_MOUSE_EVENT_PROP_SCREEN_Y,
    DOM_MOUSE_EVENT_PROP_CLIENT_X,
    DOM_MOUSE_EVENT_PROP_CLIENT_Y,
    DOM_MOUSE_EVENT_PROP_CTRL_KEY,
    DOM_MOUSE_EVENT_PROP_SHIFT_KEY,
    DOM_MOUSE_EVENT_PROP_ALT_KEY,
    DOM_MOUSE_EVENT_PROP_META_KEY,
    DOM_MOUSE_EVENT_PROP_BUTTON,
    DOM_MOUSE_EVENT_PROP_RELATED_TARGET,
    DOM_MOUSE_EVENT_PROP_OFFSET_X,
    DOM_MOUSE_EVENT_PROP_OFFSET_Y,
    DOM_MOUSE_EVENT_PROP_X,
    DOM_MOUSE_EVENT_PROP_Y,
    DOM_MOUSE_EVENT_PROP_FROM_ELEMENT,
    DOM_MOUSE_EVENT_PROP_TO_ELEMENT,
};

static void webkit_dom_mouse_event_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMMouseEvent* self = WEBKIT_DOM_MOUSE_EVENT(object);

    switch (propertyId) {
    case DOM_MOUSE_EVENT_PROP_SCREEN_X:
        g_value_set_long(value, webkit_dom_mouse_event_get_screen_x(self));
        break;
    case DOM_MOUSE_EVENT_PROP_SCREEN_Y:
        g_value_set_long(value, webkit_dom_mouse_event_get_screen_y(self));
        break;
    case DOM_MOUSE_EVENT_PROP_CLIENT_X:
        g_value_set_long(value, webkit_dom_mouse_event_get_client_x(self));
        break;
    case DOM_MOUSE_EVENT_PROP_CLIENT_Y:
        g_value_set_long(value, webkit_dom_mouse_event_get_client_y(self));
        break;
    case DOM_MOUSE_EVENT_PROP_CTRL_KEY:
        g_value_set_boolean(value, webkit_dom_mouse_event_get_ctrl_key(self));
        break;
    case DOM_MOUSE_EVENT_PROP_SHIFT_KEY:
        g_value_set_boolean(value, webkit_dom_mouse_event_get_shift_key(self));
        break;
    case DOM_MOUSE_EVENT_PROP_ALT_KEY:
        g_value_set_boolean(value, webkit_dom_mouse_event_get_alt_key(self));
        break;
    case DOM_MOUSE_EVENT_PROP_META_KEY:
        g_value_set_boolean(value, webkit_dom_mouse_event_get_meta_key(self));
        break;
    case DOM_MOUSE_EVENT_PROP_BUTTON:
        g_value_set_uint(value, webkit_dom_mouse_event_get_button(self));
        break;
    case DOM_MOUSE_EVENT_PROP_RELATED_TARGET:
        g_value_set_object(value, webkit_dom_mouse_event_get_related_target(self));
        break;
    case DOM_MOUSE_EVENT_PROP_OFFSET_X:
        g_value_set_long(value, webkit_dom_mouse_event_get_offset_x(self));
        break;
    case DOM_MOUSE_EVENT_PROP_OFFSET_Y:
        g_value_set_long(value, webkit_dom_mouse_event_get_offset_y(self));
        break;
    case DOM_MOUSE_EVENT_PROP_X:
        g_value_set_long(value, webkit_dom_mouse_event_get_x(self));
        break;
    case DOM_MOUSE_EVENT_PROP_Y:
        g_value_set_long(value, webkit_dom_mouse_event_get_y(self));
        break;
    case DOM_MOUSE_EVENT_PROP_FROM_ELEMENT:
        g_value_set_object(value, webkit_dom_mouse_event_get_from_element(self));
        break;
    case DOM_MOUSE_EVENT_PROP_TO_ELEMENT:
        g_value_set_object(value, webkit_dom_mouse_event_get_to_element(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_mouse_event_class_init(WebKitDOMMouseEventClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_mouse_event_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_SCREEN_X,
        g_param_spec_long(
            "screen-x",
            "MouseEvent:screen-x",
            "read-only glong MouseEvent:screen-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_SCREEN_Y,
        g_param_spec_long(
            "screen-y",
            "MouseEvent:screen-y",
            "read-only glong MouseEvent:screen-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_CLIENT_X,
        g_param_spec_long(
            "client-x",
            "MouseEvent:client-x",
            "read-only glong MouseEvent:client-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_CLIENT_Y,
        g_param_spec_long(
            "client-y",
            "MouseEvent:client-y",
            "read-only glong MouseEvent:client-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_CTRL_KEY,
        g_param_spec_boolean(
            "ctrl-key",
            "MouseEvent:ctrl-key",
            "read-only gboolean MouseEvent:ctrl-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_SHIFT_KEY,
        g_param_spec_boolean(
            "shift-key",
            "MouseEvent:shift-key",
            "read-only gboolean MouseEvent:shift-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_ALT_KEY,
        g_param_spec_boolean(
            "alt-key",
            "MouseEvent:alt-key",
            "read-only gboolean MouseEvent:alt-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_META_KEY,
        g_param_spec_boolean(
            "meta-key",
            "MouseEvent:meta-key",
            "read-only gboolean MouseEvent:meta-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_BUTTON,
        g_param_spec_uint(
            "button",
            "MouseEvent:button",
            "read-only gushort MouseEvent:button",
            0, G_MAXUINT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_RELATED_TARGET,
        g_param_spec_object(
            "related-target",
            "MouseEvent:related-target",
            "read-only WebKitDOMEventTarget* MouseEvent:related-target",
            WEBKIT_DOM_TYPE_EVENT_TARGET,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_OFFSET_X,
        g_param_spec_long(
            "offset-x",
            "MouseEvent:offset-x",
            "read-only glong MouseEvent:offset-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_OFFSET_Y,
        g_param_spec_long(
            "offset-y",
            "MouseEvent:offset-y",
            "read-only glong MouseEvent:offset-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_X,
        g_param_spec_long(
            "x",
            "MouseEvent:x",
            "read-only glong MouseEvent:x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_Y,
        g_param_spec_long(
            "y",
            "MouseEvent:y",
            "read-only glong MouseEvent:y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_FROM_ELEMENT,
        g_param_spec_object(
            "from-element",
            "MouseEvent:from-element",
            "read-only WebKitDOMNode* MouseEvent:from-element",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_MOUSE_EVENT_PROP_TO_ELEMENT,
        g_param_spec_object(
            "to-element",
            "MouseEvent:to-element",
            "read-only WebKitDOMNode* MouseEvent:to-element",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_mouse_event_init(WebKitDOMMouseEvent* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_mouse_event_init_mouse_event(WebKitDOMMouseEvent* self, const gchar* type, gboolean canBubble, gboolean cancelable, WebKitDOMDOMWindow* view, glong detail, glong screenX, glong screenY, glong clientX, glong clientY, gboolean ctrlKey, gboolean altKey, gboolean shiftKey, gboolean metaKey, gushort button, WebKitDOMEventTarget* relatedTarget)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self));
    g_return_if_fail(type);
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(view));
    g_return_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(relatedTarget));
    WebCore::MouseEvent* item = WebKit::core(self);
    WTF::String convertedType = WTF::String::fromUTF8(type);
    WebCore::EventTarget* convertedRelatedTarget = WebKit::core(relatedTarget);
    item->initMouseEvent(convertedType, canBubble, cancelable, WebKit::toWindowProxy(view), detail, screenX, screenY, clientX, clientY, ctrlKey, altKey, shiftKey, metaKey, button, convertedRelatedTarget);
}

glong webkit_dom_mouse_event_get_screen_x(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->screenX();
    return result;
}

glong webkit_dom_mouse_event_get_screen_y(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->screenY();
    return result;
}

glong webkit_dom_mouse_event_get_client_x(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->clientX();
    return result;
}

glong webkit_dom_mouse_event_get_client_y(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->clientY();
    return result;
}

gboolean webkit_dom_mouse_event_get_ctrl_key(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), FALSE);
    WebCore::MouseEvent* item = WebKit::core(self);
    gboolean result = item->ctrlKey();
    return result;
}

gboolean webkit_dom_mouse_event_get_shift_key(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), FALSE);
    WebCore::MouseEvent* item = WebKit::core(self);
    gboolean result = item->shiftKey();
    return result;
}

gboolean webkit_dom_mouse_event_get_alt_key(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), FALSE);
    WebCore::MouseEvent* item = WebKit::core(self);
    gboolean result = item->altKey();
    return result;
}

gboolean webkit_dom_mouse_event_get_meta_key(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), FALSE);
    WebCore::MouseEvent* item = WebKit::core(self);
    gboolean result = item->metaKey();
    return result;
}

gushort webkit_dom_mouse_event_get_button(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    gushort result = item->button();
    return result;
}

WebKitDOMEventTarget* webkit_dom_mouse_event_get_related_target(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    RefPtr<WebCore::EventTarget> gobjectResult = WTF::getPtr(item->relatedTarget());
    return WebKit::kit(gobjectResult.get());
}

glong webkit_dom_mouse_event_get_offset_x(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->offsetX();
    return result;
}

glong webkit_dom_mouse_event_get_offset_y(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->offsetY();
    return result;
}

glong webkit_dom_mouse_event_get_x(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->x();
    return result;
}

glong webkit_dom_mouse_event_get_y(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    glong result = item->y();
    return result;
}

WebKitDOMNode* webkit_dom_mouse_event_get_from_element(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->fromElement());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_mouse_event_get_to_element(WebKitDOMMouseEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_MOUSE_EVENT(self), 0);
    WebCore::MouseEvent* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->toElement());
    return WebKit::kit(gobjectResult.get());
}

G_GNUC_END_IGNORE_DEPRECATIONS;
