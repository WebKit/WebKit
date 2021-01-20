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

#ifndef WebKitDOMCharacterData_h
#define WebKitDOMCharacterData_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMNode.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_CHARACTER_DATA            (webkit_dom_character_data_get_type())
#define WEBKIT_DOM_CHARACTER_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_CHARACTER_DATA, WebKitDOMCharacterData))
#define WEBKIT_DOM_CHARACTER_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_CHARACTER_DATA, WebKitDOMCharacterDataClass)
#define WEBKIT_DOM_IS_CHARACTER_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_CHARACTER_DATA))
#define WEBKIT_DOM_IS_CHARACTER_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_CHARACTER_DATA))
#define WEBKIT_DOM_CHARACTER_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_CHARACTER_DATA, WebKitDOMCharacterDataClass))

struct _WebKitDOMCharacterData {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMCharacterDataClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_character_data_get_type(void);

/**
 * webkit_dom_character_data_substring_data:
 * @self: A #WebKitDOMCharacterData
 * @offset: A #gulong
 * @length: A #gulong
 * @error: #GError
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_character_data_substring_data(WebKitDOMCharacterData* self, gulong offset, gulong length, GError** error);

/**
 * webkit_dom_character_data_append_data:
 * @self: A #WebKitDOMCharacterData
 * @data: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_character_data_append_data(WebKitDOMCharacterData* self, const gchar* data, GError** error);

/**
 * webkit_dom_character_data_insert_data:
 * @self: A #WebKitDOMCharacterData
 * @offset: A #gulong
 * @data: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_character_data_insert_data(WebKitDOMCharacterData* self, gulong offset, const gchar* data, GError** error);

/**
 * webkit_dom_character_data_delete_data:
 * @self: A #WebKitDOMCharacterData
 * @offset: A #gulong
 * @length: A #gulong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_character_data_delete_data(WebKitDOMCharacterData* self, gulong offset, gulong length, GError** error);

/**
 * webkit_dom_character_data_replace_data:
 * @self: A #WebKitDOMCharacterData
 * @offset: A #gulong
 * @length: A #gulong
 * @data: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_character_data_replace_data(WebKitDOMCharacterData* self, gulong offset, gulong length, const gchar* data, GError** error);

/**
 * webkit_dom_character_data_get_data:
 * @self: A #WebKitDOMCharacterData
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_character_data_get_data(WebKitDOMCharacterData* self);

/**
 * webkit_dom_character_data_set_data:
 * @self: A #WebKitDOMCharacterData
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_character_data_set_data(WebKitDOMCharacterData* self, const gchar* value, GError** error);

/**
 * webkit_dom_character_data_get_length:
 * @self: A #WebKitDOMCharacterData
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_character_data_get_length(WebKitDOMCharacterData* self);

G_END_DECLS

#endif /* WebKitDOMCharacterData_h */
