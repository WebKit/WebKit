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
#include "WebKitDOMKeyboardEvent.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMKeyboardEventPrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMKeyboardEvent* kit(WebCore::KeyboardEvent* obj)
{
    return WEBKIT_DOM_KEYBOARD_EVENT(kit(static_cast<WebCore::Event*>(obj)));
}

WebCore::KeyboardEvent* core(WebKitDOMKeyboardEvent* request)
{
    return request ? static_cast<WebCore::KeyboardEvent*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMKeyboardEvent* wrapKeyboardEvent(WebCore::KeyboardEvent* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_KEYBOARD_EVENT(g_object_new(WEBKIT_DOM_TYPE_KEYBOARD_EVENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMKeyboardEvent, webkit_dom_keyboard_event, WEBKIT_DOM_TYPE_UI_EVENT)

enum {
    DOM_KEYBOARD_EVENT_PROP_0,
    DOM_KEYBOARD_EVENT_PROP_KEY_IDENTIFIER,
    DOM_KEYBOARD_EVENT_PROP_KEY_LOCATION,
    DOM_KEYBOARD_EVENT_PROP_CTRL_KEY,
    DOM_KEYBOARD_EVENT_PROP_SHIFT_KEY,
    DOM_KEYBOARD_EVENT_PROP_ALT_KEY,
    DOM_KEYBOARD_EVENT_PROP_META_KEY,
    DOM_KEYBOARD_EVENT_PROP_ALT_GRAPH_KEY,
};

static void webkit_dom_keyboard_event_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMKeyboardEvent* self = WEBKIT_DOM_KEYBOARD_EVENT(object);

    switch (propertyId) {
    case DOM_KEYBOARD_EVENT_PROP_KEY_IDENTIFIER:
        g_value_take_string(value, webkit_dom_keyboard_event_get_key_identifier(self));
        break;
    case DOM_KEYBOARD_EVENT_PROP_KEY_LOCATION:
        g_value_set_ulong(value, webkit_dom_keyboard_event_get_key_location(self));
        break;
    case DOM_KEYBOARD_EVENT_PROP_CTRL_KEY:
        g_value_set_boolean(value, webkit_dom_keyboard_event_get_ctrl_key(self));
        break;
    case DOM_KEYBOARD_EVENT_PROP_SHIFT_KEY:
        g_value_set_boolean(value, webkit_dom_keyboard_event_get_shift_key(self));
        break;
    case DOM_KEYBOARD_EVENT_PROP_ALT_KEY:
        g_value_set_boolean(value, webkit_dom_keyboard_event_get_alt_key(self));
        break;
    case DOM_KEYBOARD_EVENT_PROP_META_KEY:
        g_value_set_boolean(value, webkit_dom_keyboard_event_get_meta_key(self));
        break;
    case DOM_KEYBOARD_EVENT_PROP_ALT_GRAPH_KEY:
        g_value_set_boolean(value, webkit_dom_keyboard_event_get_alt_graph_key(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_keyboard_event_class_init(WebKitDOMKeyboardEventClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_keyboard_event_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_KEY_IDENTIFIER,
        g_param_spec_string(
            "key-identifier",
            "KeyboardEvent:key-identifier",
            "read-only gchar* KeyboardEvent:key-identifier",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_KEY_LOCATION,
        g_param_spec_ulong(
            "key-location",
            "KeyboardEvent:key-location",
            "read-only gulong KeyboardEvent:key-location",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_CTRL_KEY,
        g_param_spec_boolean(
            "ctrl-key",
            "KeyboardEvent:ctrl-key",
            "read-only gboolean KeyboardEvent:ctrl-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_SHIFT_KEY,
        g_param_spec_boolean(
            "shift-key",
            "KeyboardEvent:shift-key",
            "read-only gboolean KeyboardEvent:shift-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_ALT_KEY,
        g_param_spec_boolean(
            "alt-key",
            "KeyboardEvent:alt-key",
            "read-only gboolean KeyboardEvent:alt-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_META_KEY,
        g_param_spec_boolean(
            "meta-key",
            "KeyboardEvent:meta-key",
            "read-only gboolean KeyboardEvent:meta-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_KEYBOARD_EVENT_PROP_ALT_GRAPH_KEY,
        g_param_spec_boolean(
            "alt-graph-key",
            "KeyboardEvent:alt-graph-key",
            "read-only gboolean KeyboardEvent:alt-graph-key",
            FALSE,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_keyboard_event_init(WebKitDOMKeyboardEvent* request)
{
    UNUSED_PARAM(request);
}

gboolean webkit_dom_keyboard_event_get_modifier_state(WebKitDOMKeyboardEvent* self, const gchar* keyIdentifierArg)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), FALSE);
    g_return_val_if_fail(keyIdentifierArg, FALSE);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    WTF::String convertedKeyIdentifierArg = WTF::String::fromUTF8(keyIdentifierArg);
    gboolean result = item->getModifierState(convertedKeyIdentifierArg);
    return result;
}

void webkit_dom_keyboard_event_init_keyboard_event(WebKitDOMKeyboardEvent* self, const gchar* type, gboolean canBubble, gboolean cancelable, WebKitDOMDOMWindow* view, const gchar* keyIdentifier, gulong location, gboolean ctrlKey, gboolean altKey, gboolean shiftKey, gboolean metaKey, gboolean altGraphKey)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self));
    g_return_if_fail(type);
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(view));
    g_return_if_fail(keyIdentifier);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    WTF::String convertedType = WTF::String::fromUTF8(type);
    WTF::String convertedKeyIdentifier = WTF::String::fromUTF8(keyIdentifier);
    item->initKeyboardEvent(convertedType, canBubble, cancelable, WebKit::toWindowProxy(view), convertedKeyIdentifier, location, ctrlKey, altKey, shiftKey, metaKey, altGraphKey);
}

gchar* webkit_dom_keyboard_event_get_key_identifier(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), 0);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->keyIdentifier());
    return result;
}

gulong webkit_dom_keyboard_event_get_key_location(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), 0);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gulong result = item->location();
    return result;
}

gboolean webkit_dom_keyboard_event_get_ctrl_key(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), FALSE);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gboolean result = item->ctrlKey();
    return result;
}

gboolean webkit_dom_keyboard_event_get_shift_key(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), FALSE);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gboolean result = item->shiftKey();
    return result;
}

gboolean webkit_dom_keyboard_event_get_alt_key(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), FALSE);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gboolean result = item->altKey();
    return result;
}

gboolean webkit_dom_keyboard_event_get_meta_key(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), FALSE);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gboolean result = item->metaKey();
    return result;
}

gboolean webkit_dom_keyboard_event_get_alt_graph_key(WebKitDOMKeyboardEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_KEYBOARD_EVENT(self), FALSE);
    WebCore::KeyboardEvent* item = WebKit::core(self);
    gboolean result = item->altGraphKey();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
