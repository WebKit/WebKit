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

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <webkitdom/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMMouseEvent_h
#define WebKitDOMMouseEvent_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMUIEvent.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_MOUSE_EVENT            (webkit_dom_mouse_event_get_type())
#define WEBKIT_DOM_MOUSE_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_MOUSE_EVENT, WebKitDOMMouseEvent))
#define WEBKIT_DOM_MOUSE_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_MOUSE_EVENT, WebKitDOMMouseEventClass)
#define WEBKIT_DOM_IS_MOUSE_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_MOUSE_EVENT))
#define WEBKIT_DOM_IS_MOUSE_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_MOUSE_EVENT))
#define WEBKIT_DOM_MOUSE_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_MOUSE_EVENT, WebKitDOMMouseEventClass))

struct _WebKitDOMMouseEvent {
    WebKitDOMUIEvent parent_instance;
};

struct _WebKitDOMMouseEventClass {
    WebKitDOMUIEventClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_mouse_event_get_type(void);

/**
 * webkit_dom_mouse_event_init_mouse_event:
 * @self: A #WebKitDOMMouseEvent
 * @type: A #gchar
 * @canBubble: A #gboolean
 * @cancelable: A #gboolean
 * @view: A #WebKitDOMDOMWindow
 * @detail: A #glong
 * @screenX: A #glong
 * @screenY: A #glong
 * @clientX: A #glong
 * @clientY: A #glong
 * @ctrlKey: A #gboolean
 * @altKey: A #gboolean
 * @shiftKey: A #gboolean
 * @metaKey: A #gboolean
 * @button: A #gushort
 * @relatedTarget: A #WebKitDOMEventTarget
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_mouse_event_init_mouse_event(WebKitDOMMouseEvent* self, const gchar* type, gboolean canBubble, gboolean cancelable, WebKitDOMDOMWindow* view, glong detail, glong screenX, glong screenY, glong clientX, glong clientY, gboolean ctrlKey, gboolean altKey, gboolean shiftKey, gboolean metaKey, gushort button, WebKitDOMEventTarget* relatedTarget);

/**
 * webkit_dom_mouse_event_get_screen_x:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_screen_x(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_screen_y:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_screen_y(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_client_x:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_client_x(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_client_y:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_client_y(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_ctrl_key:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_mouse_event_get_ctrl_key(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_shift_key:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_mouse_event_get_shift_key(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_alt_key:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_mouse_event_get_alt_key(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_meta_key:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_mouse_event_get_meta_key(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_button:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #gushort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gushort
webkit_dom_mouse_event_get_button(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_related_target:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMEventTarget*
webkit_dom_mouse_event_get_related_target(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_offset_x:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_offset_x(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_offset_y:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_offset_y(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_x:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_x(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_y:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_mouse_event_get_y(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_from_element:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_mouse_event_get_from_element(WebKitDOMMouseEvent* self);

/**
 * webkit_dom_mouse_event_get_to_element:
 * @self: A #WebKitDOMMouseEvent
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_mouse_event_get_to_element(WebKitDOMMouseEvent* self);

G_END_DECLS

#endif /* WebKitDOMMouseEvent_h */
