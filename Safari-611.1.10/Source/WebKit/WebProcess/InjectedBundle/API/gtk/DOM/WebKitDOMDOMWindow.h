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

#ifndef WebKitDOMDOMWindow_h
#define WebKitDOMDOMWindow_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_DOM_WINDOW            (webkit_dom_dom_window_get_type())
#define WEBKIT_DOM_DOM_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_DOM_WINDOW, WebKitDOMDOMWindow))
#define WEBKIT_DOM_DOM_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_DOM_WINDOW, WebKitDOMDOMWindowClass)
#define WEBKIT_DOM_IS_DOM_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_DOM_WINDOW))
#define WEBKIT_DOM_IS_DOM_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_DOM_WINDOW))
#define WEBKIT_DOM_DOM_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_DOM_WINDOW, WebKitDOMDOMWindowClass))

struct _WebKitDOMDOMWindow {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMDOMWindowClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_dom_window_get_type(void);

/**
 * webkit_dom_dom_window_get_selection:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMSelection
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMSelection*
webkit_dom_dom_window_get_selection(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_focus:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_focus(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_blur:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_blur(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_close:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_close(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_print:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_print(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_stop:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_stop(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_alert:
 * @self: A #WebKitDOMDOMWindow
 * @message: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_alert(WebKitDOMDOMWindow* self, const gchar* message);

/**
 * webkit_dom_dom_window_confirm:
 * @self: A #WebKitDOMDOMWindow
 * @message: A #gchar
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_window_confirm(WebKitDOMDOMWindow* self, const gchar* message);

/**
 * webkit_dom_dom_window_prompt:
 * @self: A #WebKitDOMDOMWindow
 * @message: A #gchar
 * @defaultValue: A #gchar
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_dom_window_prompt(WebKitDOMDOMWindow* self, const gchar* message, const gchar* defaultValue);

/**
 * webkit_dom_dom_window_find:
 * @self: A #WebKitDOMDOMWindow
 * @string: A #gchar
 * @caseSensitive: A #gboolean
 * @backwards: A #gboolean
 * @wrap: A #gboolean
 * @wholeWord: A #gboolean
 * @searchInFrames: A #gboolean
 * @showDialog: A #gboolean
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_window_find(WebKitDOMDOMWindow* self, const gchar* string, gboolean caseSensitive, gboolean backwards, gboolean wrap, gboolean wholeWord, gboolean searchInFrames, gboolean showDialog);

/**
 * webkit_dom_dom_window_scroll_by:
 * @self: A #WebKitDOMDOMWindow
 * @x: A #gdouble
 * @y: A #gdouble
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_scroll_by(WebKitDOMDOMWindow* self, gdouble x, gdouble y);

/**
 * webkit_dom_dom_window_scroll_to:
 * @self: A #WebKitDOMDOMWindow
 * @x: A #gdouble
 * @y: A #gdouble
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_scroll_to(WebKitDOMDOMWindow* self, gdouble x, gdouble y);

/**
 * webkit_dom_dom_window_move_by:
 * @self: A #WebKitDOMDOMWindow
 * @x: A #gfloat
 * @y: A #gfloat
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_move_by(WebKitDOMDOMWindow* self, gfloat x, gfloat y);

/**
 * webkit_dom_dom_window_move_to:
 * @self: A #WebKitDOMDOMWindow
 * @x: A #gfloat
 * @y: A #gfloat
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_move_to(WebKitDOMDOMWindow* self, gfloat x, gfloat y);

/**
 * webkit_dom_dom_window_resize_by:
 * @self: A #WebKitDOMDOMWindow
 * @x: A #gfloat
 * @y: A #gfloat
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_resize_by(WebKitDOMDOMWindow* self, gfloat x, gfloat y);

/**
 * webkit_dom_dom_window_resize_to:
 * @self: A #WebKitDOMDOMWindow
 * @width: A #gfloat
 * @height: A #gfloat
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_resize_to(WebKitDOMDOMWindow* self, gfloat width, gfloat height);

/**
 * webkit_dom_dom_window_get_computed_style:
 * @self: A #WebKitDOMDOMWindow
 * @element: A #WebKitDOMElement
 * @pseudoElement: (allow-none): A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMCSSStyleDeclaration
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMCSSStyleDeclaration*
webkit_dom_dom_window_get_computed_style(WebKitDOMDOMWindow* self, WebKitDOMElement* element, const gchar* pseudoElement);

/**
 * webkit_dom_dom_window_capture_events:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_capture_events(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_release_events:
 * @self: A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_release_events(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_frame_element:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_dom_window_get_frame_element(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_offscreen_buffering:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_window_get_offscreen_buffering(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_outer_height:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_outer_height(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_outer_width:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_outer_width(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_inner_height:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_inner_height(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_inner_width:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_inner_width(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_screen_x:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_screen_x(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_screen_y:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_screen_y(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_screen_left:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_screen_left(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_screen_top:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_screen_top(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_scroll_x:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_scroll_x(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_scroll_y:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_scroll_y(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_page_x_offset:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_page_x_offset(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_page_y_offset:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_page_y_offset(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_closed:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_window_get_closed(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_length:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gulong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gulong
webkit_dom_dom_window_get_length(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_name:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_dom_window_get_name(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_set_name:
 * @self: A #WebKitDOMDOMWindow
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_set_name(WebKitDOMDOMWindow* self, const gchar* value);

/**
 * webkit_dom_dom_window_get_status:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_dom_window_get_status(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_set_status:
 * @self: A #WebKitDOMDOMWindow
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_set_status(WebKitDOMDOMWindow* self, const gchar* value);

/**
 * webkit_dom_dom_window_get_default_status:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_dom_window_get_default_status(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_set_default_status:
 * @self: A #WebKitDOMDOMWindow
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_window_set_default_status(WebKitDOMDOMWindow* self, const gchar* value);

/**
 * webkit_dom_dom_window_get_self:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_dom_window_get_self(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_window:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_dom_window_get_window(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_frames:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_dom_window_get_frames(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_opener:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_dom_window_get_opener(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_parent:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_dom_window_get_parent(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_top:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_dom_window_get_top(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_document:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer none): A #WebKitDOMDocument
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED WebKitDOMDocument*
webkit_dom_dom_window_get_document(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_device_pixel_ratio:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #gdouble
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gdouble
webkit_dom_dom_window_get_device_pixel_ratio(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_get_orientation:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: A #glong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED glong
webkit_dom_dom_window_get_orientation(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_dom_window_post_user_message:
 * @window: A #WebKitDOMDOMWindow
 * @handler: Name of the user message handler.
 * @message: JavaScript value to be sent.
 *
 * Returns: Whether the message was successfully sent.
 *
 * Since: 2.8
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_window_webkit_message_handlers_post_message(WebKitDOMDOMWindow* window, const gchar* handler, const gchar* message);

G_END_DECLS

#endif /* WebKitDOMDOMWindow_h */
