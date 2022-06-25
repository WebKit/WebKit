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

#ifndef WebKitDOMUIEvent_h
#define WebKitDOMUIEvent_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMEvent.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_UI_EVENT            (webkit_dom_ui_event_get_type())
#define WEBKIT_DOM_UI_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_UI_EVENT, WebKitDOMUIEvent))
#define WEBKIT_DOM_UI_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_UI_EVENT, WebKitDOMUIEventClass)
#define WEBKIT_DOM_IS_UI_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_UI_EVENT))
#define WEBKIT_DOM_IS_UI_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_UI_EVENT))
#define WEBKIT_DOM_UI_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_UI_EVENT, WebKitDOMUIEventClass))

struct _WebKitDOMUIEvent {
    WebKitDOMEvent parent_instance;
};

struct _WebKitDOMUIEventClass {
    WebKitDOMEventClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_ui_event_get_type(void);

/**
 * webkit_dom_ui_event_init_ui_event:
 * @self: A #WebKitDOMUIEvent
 * @type: A #gchar
 * @canBubble: A #gboolean
 * @cancelable: A #gboolean
 * @view: A #WebKitDOMDOMWindow
 * @detail: A #glong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_ui_event_init_ui_event(WebKitDOMUIEvent* self, const gchar* type, gboolean canBubble, gboolean cancelable, WebKitDOMDOMWindow* view, glong detail);

/**
 * webkit_dom_ui_event_get_view:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_ui_event_get_view(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_detail:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_detail(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_key_code:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_key_code(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_char_code:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_char_code(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_layer_x:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_layer_x(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_layer_y:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_layer_y(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_page_x:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_page_x(WebKitDOMUIEvent* self);

/**
 * webkit_dom_ui_event_get_page_y:
 * @self: A #WebKitDOMUIEvent
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_ui_event_get_page_y(WebKitDOMUIEvent* self);

G_END_DECLS

#endif /* WebKitDOMUIEvent_h */
