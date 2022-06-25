/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebKitDOMEventTarget_h
#define WebKitDOMEventTarget_h

#include <glib-object.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_EVENT_TARGET            (webkit_dom_event_target_get_type ())
#define WEBKIT_DOM_EVENT_TARGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_DOM_TYPE_EVENT_TARGET, WebKitDOMEventTarget))
#define WEBKIT_DOM_EVENT_TARGET_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), WEBKIT_DOM_TYPE_EVENT_TARGET, WebKitDOMEventTargetIface))
#define WEBKIT_DOM_IS_EVENT_TARGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_DOM_TYPE_EVENT_TARGET))
#define WEBKIT_DOM_EVENT_TARGET_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), WEBKIT_DOM_TYPE_EVENT_TARGET, WebKitDOMEventTargetIface))

struct _WebKitDOMEventTargetIface {
    GTypeInterface gIface;

    /* virtual table */
    gboolean      (* dispatch_event)(WebKitDOMEventTarget *target,
                                     WebKitDOMEvent       *event,
                                     GError              **error);

    gboolean      (* add_event_listener)(WebKitDOMEventTarget *target,
                                         const char           *event_name,
                                         GClosure             *handler,
                                         gboolean              use_capture);
    gboolean      (* remove_event_listener)(WebKitDOMEventTarget *target,
                                            const char           *event_name,
                                            GClosure             *handler,
                                            gboolean              use_capture);

    void (*_webkitdom_reserved0) (void);
    void (*_webkitdom_reserved1) (void);
    void (*_webkitdom_reserved2) (void);
    void (*_webkitdom_reserved3) (void);
};


WEBKIT_DEPRECATED GType     webkit_dom_event_target_get_type(void) G_GNUC_CONST;

/**
 * webkit_dom_event_target_dispatch_event:
 * @target: A #WebKitDOMEventTarget
 * @event: A #WebKitDOMEvent
 * @error: return location for an error or %NULL
 *
 * Returns: a #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean  webkit_dom_event_target_dispatch_event(WebKitDOMEventTarget *target,
                                                            WebKitDOMEvent       *event,
                                                            GError              **error);

/**
 * webkit_dom_event_target_add_event_listener:
 * @target: A #WebKitDOMEventTarget
 * @event_name: A #gchar
 * @handler: (scope async): A #GCallback
 * @use_capture: A #gboolean
 * @user_data: A #gpointer
 *
 * Returns: a #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean  webkit_dom_event_target_add_event_listener(WebKitDOMEventTarget *target,
                                                                const char           *event_name,
                                                                GCallback             handler,
                                                                gboolean              use_capture,
                                                                gpointer              user_data);

/**
 * webkit_dom_event_target_remove_event_listener:
 * @target: A #WebKitDOMEventTarget
 * @event_name: A #gchar
 * @handler: (scope call): A #GCallback
 * @use_capture: A #gboolean
 *
 * Returns: a #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean  webkit_dom_event_target_remove_event_listener(WebKitDOMEventTarget *target,
                                                                   const char           *event_name,
                                                                   GCallback             handler,
                                                                   gboolean              use_capture);

/**
 * webkit_dom_event_target_add_event_listener_with_closure: (rename-to webkit_dom_event_target_add_event_listener)
 * @target: A #WebKitDOMEventTarget
 * @event_name: A #gchar
 * @handler: A #GClosure
 * @use_capture: A #gboolean
 *
 * Version of webkit_dom_event_target_add_event_listener() using a closure
 * instead of a callbacks for easier binding in other languages.
 *
 * Returns: a #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean webkit_dom_event_target_add_event_listener_with_closure(WebKitDOMEventTarget *target,
                                                                            const char           *event_name,
                                                                            GClosure             *handler,
                                                                            gboolean              use_capture);

/**
 * webkit_dom_event_target_remove_event_listener_with_closure: (rename-to webkit_dom_event_target_remove_event_listener)
 * @target: A #WebKitDOMEventTarget
 * @event_name: A #gchar
 * @handler: A #GClosure
 * @use_capture: A #gboolean
 *
 * Version of webkit_dom_event_target_remove_event_listener() using a closure
 * instead of a callbacks for easier binding in other languages.
 *
 * Returns: a #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean webkit_dom_event_target_remove_event_listener_with_closure(WebKitDOMEventTarget *target,
                                                                               const char           *event_name,
                                                                               GClosure             *handler,
                                                                               gboolean              use_capture);


G_END_DECLS

#endif /* WebKitDOMEventTarget_h */
