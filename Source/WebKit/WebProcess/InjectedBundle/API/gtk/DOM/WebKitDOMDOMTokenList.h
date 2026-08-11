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

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(BUILDING_WEBKIT) && !defined(WEBKIT_DOM_USE_UNSTABLE_API)
#error "Only <webkitdom/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMDOMTokenList_h
#define WebKitDOMDOMTokenList_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_DOM_TOKEN_LIST            (webkit_dom_dom_token_list_get_type())
#define WEBKIT_DOM_DOM_TOKEN_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_DOM_TOKEN_LIST, WebKitDOMDOMTokenList))
#define WEBKIT_DOM_DOM_TOKEN_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_DOM_TOKEN_LIST, WebKitDOMDOMTokenListClass)
#define WEBKIT_DOM_IS_DOM_TOKEN_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_DOM_TOKEN_LIST))
#define WEBKIT_DOM_IS_DOM_TOKEN_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_DOM_TOKEN_LIST))
#define WEBKIT_DOM_DOM_TOKEN_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_DOM_TOKEN_LIST, WebKitDOMDOMTokenListClass))

struct _WebKitDOMDOMTokenList {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMDOMTokenListClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_dom_token_list_get_type(void);

/**
 * webkit_dom_dom_token_list_item:
 * @self: A #WebKitDOMDOMTokenList
 * @index: A #gulong
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_dom_token_list_item(WebKitDOMDOMTokenList* self, gulong index);

/**
 * webkit_dom_dom_token_list_contains:
 * @self: A #WebKitDOMDOMTokenList
 * @token: A #gchar
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_token_list_contains(WebKitDOMDOMTokenList* self, const gchar* token);

/**
 * webkit_dom_dom_token_list_add:
 * @self: A #WebKitDOMDOMTokenList
 * @error: #GError
 * @...: list of #gchar ended by %NULL.
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_token_list_add(WebKitDOMDOMTokenList* self, GError** error, ...);

/**
 * webkit_dom_dom_token_list_remove:
 * @self: A #WebKitDOMDOMTokenList
 * @error: #GError
 * @...: list of #gchar ended by %NULL.
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_token_list_remove(WebKitDOMDOMTokenList* self, GError** error, ...);

/**
 * webkit_dom_dom_token_list_toggle:
 * @self: A #WebKitDOMDOMTokenList
 * @token: A #gchar
 * @force: A #gboolean
 * @error: #GError
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gboolean
webkit_dom_dom_token_list_toggle(WebKitDOMDOMTokenList* self, const gchar* token, gboolean force, GError** error);

/**
 * webkit_dom_dom_token_list_replace:
 * @self: A #WebKitDOMDOMTokenList
 * @token: A #gchar
 * @newToken: A #gchar
 * @error: #GError
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_token_list_replace(WebKitDOMDOMTokenList* self, const gchar* token, const gchar* newToken, GError** error);

/**
 * webkit_dom_dom_token_list_get_length:
 * @self: A #WebKitDOMDOMTokenList
 *
 * Returns: A #gulong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gulong
webkit_dom_dom_token_list_get_length(WebKitDOMDOMTokenList* self);

/**
 * webkit_dom_dom_token_list_get_value:
 * @self: A #WebKitDOMDOMTokenList
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_dom_token_list_get_value(WebKitDOMDOMTokenList* self);

/**
 * webkit_dom_dom_token_list_set_value:
 * @self: A #WebKitDOMDOMTokenList
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED void
webkit_dom_dom_token_list_set_value(WebKitDOMDOMTokenList* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMDOMTokenList_h */
