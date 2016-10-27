/*
 *  Copyright (C) 2014 Igalia S.L.
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

#ifndef WebKitDOMCustomUnstable_h
#define WebKitDOMCustomUnstable_h

#ifdef WEBKIT_DOM_USE_UNSTABLE_API

#include <webkitdom/webkitdomdefines-unstable.h>

G_BEGIN_DECLS

/**
 * webkit_dom_dom_window_get_webkit_namespace:
 * @self: A #WebKitDOMDOMWindow
 *
 * Returns: (transfer full): A #WebKitDOMWebKitNamespace
 *
 * Stability: Unstable
 * Since: 2.8
 */
WEBKIT_API WebKitDOMWebKitNamespace *
webkit_dom_dom_window_get_webkit_namespace(WebKitDOMDOMWindow* self);

/**
 * webkit_dom_user_message_handlers_namespace_get_handler:
 * @self: A #WebKitDOMUserMessageHandlersNamespace
 * @name: a #gchar
 *
 * Returns: (transfer full): A #WebKitDOMUserMessageHandler
 *
 * Stability: Unstable
 * Since: 2.8
 */
WEBKIT_API WebKitDOMUserMessageHandler *
webkit_dom_user_message_handlers_namespace_get_handler(WebKitDOMUserMessageHandlersNamespace* self, const gchar* name);

/**
 * webkit_dom_html_link_element_set_sizes:
 * @self: A #WebKitDOMHTMLLinkElement
 * @value: a #gchar
 *
 * Stability: Unstable
 * Since: 2.8
 */
WEBKIT_API void
webkit_dom_html_link_element_set_sizes(WebKitDOMHTMLLinkElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_auto_filled:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Stability: Unstable
 * Since: 2.14.2
 */
WEBKIT_API gboolean
webkit_dom_html_input_element_get_auto_filled(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_auto_filled:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 * Stability: Unstable
 * Since: 2.14.2
 */
WEBKIT_API void
webkit_dom_html_input_element_set_auto_filled(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_set_editing_value:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 * Stability: Unstable
 * Since: 2.14.2
 */
WEBKIT_API void
webkit_dom_html_input_element_set_editing_value(WebKitDOMHTMLInputElement* self, const gchar* value);

G_END_DECLS

#endif /* WEBKIT_DOM_USE_UNSTABLE_API */

#endif
