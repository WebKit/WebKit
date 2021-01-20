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

#ifndef WebKitDOMEvent_h
#define WebKitDOMEvent_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_EVENT            (webkit_dom_event_get_type())
#define WEBKIT_DOM_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_EVENT, WebKitDOMEvent))
#define WEBKIT_DOM_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_EVENT, WebKitDOMEventClass)
#define WEBKIT_DOM_IS_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_EVENT))
#define WEBKIT_DOM_IS_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_EVENT))
#define WEBKIT_DOM_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_EVENT, WebKitDOMEventClass))

#ifndef WEBKIT_DISABLE_DEPRECATED

/**
 * WEBKIT_DOM_EVENT_NONE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_NONE 0

/**
 * WEBKIT_DOM_EVENT_CAPTURING_PHASE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_CAPTURING_PHASE 1

/**
 * WEBKIT_DOM_EVENT_AT_TARGET:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_AT_TARGET 2

/**
 * WEBKIT_DOM_EVENT_BUBBLING_PHASE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_BUBBLING_PHASE 3

/**
 * WEBKIT_DOM_EVENT_MOUSEDOWN:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_MOUSEDOWN 1

/**
 * WEBKIT_DOM_EVENT_MOUSEUP:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_MOUSEUP 2

/**
 * WEBKIT_DOM_EVENT_MOUSEOVER:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_MOUSEOVER 4

/**
 * WEBKIT_DOM_EVENT_MOUSEOUT:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_MOUSEOUT 8

/**
 * WEBKIT_DOM_EVENT_MOUSEMOVE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_MOUSEMOVE 16

/**
 * WEBKIT_DOM_EVENT_MOUSEDRAG:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_MOUSEDRAG 32

/**
 * WEBKIT_DOM_EVENT_CLICK:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_CLICK 64

/**
 * WEBKIT_DOM_EVENT_DBLCLICK:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_DBLCLICK 128

/**
 * WEBKIT_DOM_EVENT_KEYDOWN:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_KEYDOWN 256

/**
 * WEBKIT_DOM_EVENT_KEYUP:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_KEYUP 512

/**
 * WEBKIT_DOM_EVENT_KEYPRESS:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_KEYPRESS 1024

/**
 * WEBKIT_DOM_EVENT_DRAGDROP:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_DRAGDROP 2048

/**
 * WEBKIT_DOM_EVENT_FOCUS:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_FOCUS 4096

/**
 * WEBKIT_DOM_EVENT_BLUR:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_BLUR 8192

/**
 * WEBKIT_DOM_EVENT_SELECT:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_SELECT 16384

/**
 * WEBKIT_DOM_EVENT_CHANGE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_EVENT_CHANGE 32768

#endif /* WEBKIT_DISABLE_DEPRECATED */

struct _WebKitDOMEvent {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMEventClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_event_get_type(void);

/**
 * webkit_dom_event_stop_propagation:
 * @self: A #WebKitDOMEvent
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_event_stop_propagation(WebKitDOMEvent* self);

/**
 * webkit_dom_event_prevent_default:
 * @self: A #WebKitDOMEvent
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_event_prevent_default(WebKitDOMEvent* self);

/**
 * webkit_dom_event_init_event:
 * @self: A #WebKitDOMEvent
 * @eventTypeArg: A #gchar
 * @canBubbleArg: A #gboolean
 * @cancelableArg: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_event_init_event(WebKitDOMEvent* self, const gchar* eventTypeArg, gboolean canBubbleArg, gboolean cancelableArg);

/**
 * webkit_dom_event_get_event_type:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_event_get_event_type(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_target:
 * @self: A #WebKitDOMEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMEventTarget*
webkit_dom_event_get_target(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_current_target:
 * @self: A #WebKitDOMEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMEventTarget*
webkit_dom_event_get_current_target(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_event_phase:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gushort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gushort
webkit_dom_event_get_event_phase(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_bubbles:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_event_get_bubbles(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_cancelable:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_event_get_cancelable(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_time_stamp:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #guint32
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED guint32
webkit_dom_event_get_time_stamp(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_src_element:
 * @self: A #WebKitDOMEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMEventTarget*
webkit_dom_event_get_src_element(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_return_value:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_event_get_return_value(WebKitDOMEvent* self);

/**
 * webkit_dom_event_set_return_value:
 * @self: A #WebKitDOMEvent
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_event_set_return_value(WebKitDOMEvent* self, gboolean value);

/**
 * webkit_dom_event_get_cancel_bubble:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_event_get_cancel_bubble(WebKitDOMEvent* self);

/**
 * webkit_dom_event_set_cancel_bubble:
 * @self: A #WebKitDOMEvent
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_event_set_cancel_bubble(WebKitDOMEvent* self, gboolean value);

G_END_DECLS

#endif /* WebKitDOMEvent_h */
