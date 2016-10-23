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

/**
 * WEBKIT_DOM_EVENT_NONE:
 */
#define WEBKIT_DOM_EVENT_NONE 0

/**
 * WEBKIT_DOM_EVENT_CAPTURING_PHASE:
 */
#define WEBKIT_DOM_EVENT_CAPTURING_PHASE 1

/**
 * WEBKIT_DOM_EVENT_AT_TARGET:
 */
#define WEBKIT_DOM_EVENT_AT_TARGET 2

/**
 * WEBKIT_DOM_EVENT_BUBBLING_PHASE:
 */
#define WEBKIT_DOM_EVENT_BUBBLING_PHASE 3

/**
 * WEBKIT_DOM_EVENT_MOUSEDOWN:
 */
#define WEBKIT_DOM_EVENT_MOUSEDOWN 1

/**
 * WEBKIT_DOM_EVENT_MOUSEUP:
 */
#define WEBKIT_DOM_EVENT_MOUSEUP 2

/**
 * WEBKIT_DOM_EVENT_MOUSEOVER:
 */
#define WEBKIT_DOM_EVENT_MOUSEOVER 4

/**
 * WEBKIT_DOM_EVENT_MOUSEOUT:
 */
#define WEBKIT_DOM_EVENT_MOUSEOUT 8

/**
 * WEBKIT_DOM_EVENT_MOUSEMOVE:
 */
#define WEBKIT_DOM_EVENT_MOUSEMOVE 16

/**
 * WEBKIT_DOM_EVENT_MOUSEDRAG:
 */
#define WEBKIT_DOM_EVENT_MOUSEDRAG 32

/**
 * WEBKIT_DOM_EVENT_CLICK:
 */
#define WEBKIT_DOM_EVENT_CLICK 64

/**
 * WEBKIT_DOM_EVENT_DBLCLICK:
 */
#define WEBKIT_DOM_EVENT_DBLCLICK 128

/**
 * WEBKIT_DOM_EVENT_KEYDOWN:
 */
#define WEBKIT_DOM_EVENT_KEYDOWN 256

/**
 * WEBKIT_DOM_EVENT_KEYUP:
 */
#define WEBKIT_DOM_EVENT_KEYUP 512

/**
 * WEBKIT_DOM_EVENT_KEYPRESS:
 */
#define WEBKIT_DOM_EVENT_KEYPRESS 1024

/**
 * WEBKIT_DOM_EVENT_DRAGDROP:
 */
#define WEBKIT_DOM_EVENT_DRAGDROP 2048

/**
 * WEBKIT_DOM_EVENT_FOCUS:
 */
#define WEBKIT_DOM_EVENT_FOCUS 4096

/**
 * WEBKIT_DOM_EVENT_BLUR:
 */
#define WEBKIT_DOM_EVENT_BLUR 8192

/**
 * WEBKIT_DOM_EVENT_SELECT:
 */
#define WEBKIT_DOM_EVENT_SELECT 16384

/**
 * WEBKIT_DOM_EVENT_CHANGE:
 */
#define WEBKIT_DOM_EVENT_CHANGE 32768

struct _WebKitDOMEvent {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMEventClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_API GType
webkit_dom_event_get_type(void);

/**
 * webkit_dom_event_stop_propagation:
 * @self: A #WebKitDOMEvent
 *
**/
WEBKIT_API void
webkit_dom_event_stop_propagation(WebKitDOMEvent* self);

/**
 * webkit_dom_event_prevent_default:
 * @self: A #WebKitDOMEvent
 *
**/
WEBKIT_API void
webkit_dom_event_prevent_default(WebKitDOMEvent* self);

/**
 * webkit_dom_event_init_event:
 * @self: A #WebKitDOMEvent
 * @eventTypeArg: A #gchar
 * @canBubbleArg: A #gboolean
 * @cancelableArg: A #gboolean
 *
**/
WEBKIT_API void
webkit_dom_event_init_event(WebKitDOMEvent* self, const gchar* eventTypeArg, gboolean canBubbleArg, gboolean cancelableArg);

/**
 * webkit_dom_event_get_event_type:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gchar
**/
WEBKIT_API gchar*
webkit_dom_event_get_event_type(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_target:
 * @self: A #WebKitDOMEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
**/
WEBKIT_API WebKitDOMEventTarget*
webkit_dom_event_get_target(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_current_target:
 * @self: A #WebKitDOMEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
**/
WEBKIT_API WebKitDOMEventTarget*
webkit_dom_event_get_current_target(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_event_phase:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gushort
**/
WEBKIT_API gushort
webkit_dom_event_get_event_phase(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_bubbles:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
**/
WEBKIT_API gboolean
webkit_dom_event_get_bubbles(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_cancelable:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
**/
WEBKIT_API gboolean
webkit_dom_event_get_cancelable(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_time_stamp:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #guint32
**/
WEBKIT_API guint32
webkit_dom_event_get_time_stamp(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_src_element:
 * @self: A #WebKitDOMEvent
 *
 * Returns: (transfer full): A #WebKitDOMEventTarget
**/
WEBKIT_API WebKitDOMEventTarget*
webkit_dom_event_get_src_element(WebKitDOMEvent* self);

/**
 * webkit_dom_event_get_return_value:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
**/
WEBKIT_API gboolean
webkit_dom_event_get_return_value(WebKitDOMEvent* self);

/**
 * webkit_dom_event_set_return_value:
 * @self: A #WebKitDOMEvent
 * @value: A #gboolean
 *
**/
WEBKIT_API void
webkit_dom_event_set_return_value(WebKitDOMEvent* self, gboolean value);

/**
 * webkit_dom_event_get_cancel_bubble:
 * @self: A #WebKitDOMEvent
 *
 * Returns: A #gboolean
**/
WEBKIT_API gboolean
webkit_dom_event_get_cancel_bubble(WebKitDOMEvent* self);

/**
 * webkit_dom_event_set_cancel_bubble:
 * @self: A #WebKitDOMEvent
 * @value: A #gboolean
 *
**/
WEBKIT_API void
webkit_dom_event_set_cancel_bubble(WebKitDOMEvent* self, gboolean value);

G_END_DECLS

#endif /* WebKitDOMEvent_h */
