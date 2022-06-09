/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitPlugin.h"

#include <wtf/glib/WTFGType.h>

/**
 * SECTION: WebKitPlugin
 * @Short_description: Represents a plugin, enabling fine-grained control
 * @Title: WebKitPlugin
 *
 * This object represents a single plugin, found while scanning the
 * various platform plugin directories. This object can be used to get
 * more information about a plugin, and enable/disable it, allowing
 * fine-grained control of plugins. The list of available plugins can
 * be obtained from the #WebKitWebContext, with
 * webkit_web_context_get_plugins().
 *
 * Deprecated: 2.32
 */

struct _WebKitPluginPrivate {
};

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
WEBKIT_DEFINE_TYPE(WebKitPlugin, webkit_plugin, G_TYPE_OBJECT)
ALLOW_DEPRECATED_DECLARATIONS_END

static void webkit_plugin_class_init(WebKitPluginClass*)
{
}

/**
 * webkit_plugin_get_name:
 * @plugin: a #WebKitPlugin
 *
 * Returns: the name of the plugin.
 *
 * Deprecated: 2.32
 */
const char* webkit_plugin_get_name(WebKitPlugin*)
{
    return nullptr;
}

/**
 * webkit_plugin_get_description:
 * @plugin: a #WebKitPlugin
 *
 * Returns: the description of the plugin.
 *
 * Deprecated: 2.32
 */
const char* webkit_plugin_get_description(WebKitPlugin*)
{
    return nullptr;
}

/**
 * webkit_plugin_get_path:
 * @plugin: a #WebKitPlugin
 *
 * Returns: the absolute path where the plugin is installed.
 *
 * Deprecated: 2.32
 */
const char* webkit_plugin_get_path(WebKitPlugin*)
{
    return nullptr;
}

/**
 * webkit_plugin_get_mime_info_list:
 * @plugin: a #WebKitPlugin
 *
 * Get information about MIME types handled by the plugin,
 * as a list of #WebKitMimeInfo.
 *
 * Returns: (element-type WebKitMimeInfo) (transfer none): a #GList of #WebKitMimeInfo.
 *
 * Deprecated: 2.32
 */
GList* webkit_plugin_get_mime_info_list(WebKitPlugin*)
{
    return nullptr;
}
