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

#include "config.h"
#include "webkitwebplugin.h"

#include "PluginPackage.h"
#include "webkitprivate.h"
#include "webkitwebpluginprivate.h"
#include <glib/gi18n-lib.h>

/**
 * SECTION:webkitwebplugin
 * @short_description: Represents a plugin, enabling fine-grained control
 * @see_also: #WebKitWebPluginDatabase
 *
 * This object represents a single plugin, found by WebKitGTK+ while
 * scanning the various usual directories. This object can be used to
 * get more information about a plugin, and enable/disable it,
 * allowing fine-grained control of plugins. The list of available
 * plugins can be obtained from the #WebKitWebPluginDatabase object.
 */

using namespace WebCore;

enum {
    PROP_0,

    PROP_ENABLED
};

G_DEFINE_TYPE(WebKitWebPlugin, webkit_web_plugin, G_TYPE_OBJECT)

static void freeMIMEType(WebKitWebPluginMIMEType* mimeType)
{
    if (mimeType->name)
        g_free(mimeType->name);
    if (mimeType->description)
        g_free(mimeType->description);
    if (mimeType->extensions)
        g_strfreev(mimeType->extensions);
    g_slice_free(WebKitWebPluginMIMEType, mimeType);
}

static void webkit_web_plugin_finalize(GObject* object)
{
    WebKitWebPlugin* plugin = WEBKIT_WEB_PLUGIN(object);
    WebKitWebPluginPrivate* priv = plugin->priv;

    g_slist_foreach(priv->mimeTypes, (GFunc)freeMIMEType, 0);
    g_slist_free(priv->mimeTypes);

    delete plugin->priv;

    G_OBJECT_CLASS(webkit_web_plugin_parent_class)->finalize(object);
}

static void webkit_web_plugin_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebPlugin* plugin = WEBKIT_WEB_PLUGIN(object);

    switch (prop_id) {
    case PROP_ENABLED:
        g_value_set_boolean(value, webkit_web_plugin_get_enabled(plugin));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, paramSpec);
    }
}

static void webkit_web_plugin_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebPlugin* plugin = WEBKIT_WEB_PLUGIN(object);

    switch (prop_id) {
    case PROP_ENABLED:
        webkit_web_plugin_set_enabled(plugin, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, paramSpec);
    }
}

static void webkit_web_plugin_class_init(WebKitWebPluginClass* klass)
{
    webkit_init();

    GObjectClass* gobjectClass = reinterpret_cast<GObjectClass*>(klass);

    gobjectClass->finalize = webkit_web_plugin_finalize;
    gobjectClass->get_property = webkit_web_plugin_get_property;
    gobjectClass->set_property = webkit_web_plugin_set_property;

    g_object_class_install_property(gobjectClass,
                                    PROP_ENABLED,
                                    g_param_spec_boolean("enabled",
                                                         _("Enabled"),
                                                         _("Whether the plugin is enabled"),
                                                         FALSE,
                                                         WEBKIT_PARAM_READWRITE));
}

static void webkit_web_plugin_init(WebKitWebPlugin *plugin)
{
    plugin->priv = new WebKitWebPluginPrivate();
    plugin->priv->mimeTypes = 0;
}

namespace WebKit {
WebKitWebPlugin* kitNew(WebCore::PluginPackage* package)
{
    WebKitWebPlugin* plugin = WEBKIT_WEB_PLUGIN(g_object_new(WEBKIT_TYPE_WEB_PLUGIN, 0));

    plugin->priv->corePlugin = package;

    return plugin;
}
}

/**
 * webkit_web_plugin_get_name:
 * @plugin: a #WebKitWebPlugin
 *
 * Returns: the name string for @plugin.
 *
 * Since: 1.3.8
 */
const char* webkit_web_plugin_get_name(WebKitWebPlugin* plugin)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PLUGIN(plugin), 0);

    WebKitWebPluginPrivate* priv = plugin->priv;

    if (!priv->name.length())
        priv->name = priv->corePlugin->name().utf8();

    return priv->name.data();
}

/**
 * webkit_web_plugin_get_description:
 * @plugin: a #WebKitWebPlugin
 *
 * Returns: the description string for @plugin.
 *
 * Since: 1.3.8
 */
const char* webkit_web_plugin_get_description(WebKitWebPlugin* plugin)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PLUGIN(plugin), 0);

    WebKitWebPluginPrivate* priv = plugin->priv;

    if (!priv->description.length())
        priv->description = priv->corePlugin->description().utf8();

    return priv->description.data();
}

/**
 * webkit_web_plugin_get_mimetypes:
 * @plugin: a #WebKitWebPlugin
 *
 * Returns all the #WebKitWebPluginMIMEType that @plugin is handling
 * at the moment.
 *
 * Returns: (transfer none) (element-type WebKitWebPluginMIMEType): a #GSList of #WebKitWebPluginMIMEType
 *
 * Since: 1.3.8
 */
GSList* webkit_web_plugin_get_mimetypes(WebKitWebPlugin* plugin)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PLUGIN(plugin), 0);

    WebKitWebPluginPrivate* priv = plugin->priv;

    if (priv->mimeTypes)
        return priv->mimeTypes;

    const MIMEToDescriptionsMap& mimeToDescriptions = priv->corePlugin->mimeToDescriptions();
    MIMEToDescriptionsMap::const_iterator end = mimeToDescriptions.end();

    for (MIMEToDescriptionsMap::const_iterator it = mimeToDescriptions.begin(); it != end; ++it) {
        WebKitWebPluginMIMEType* mimeType = g_slice_new0(WebKitWebPluginMIMEType);
        mimeType->name = g_strdup(it->first.utf8().data());
        mimeType->description = g_strdup(it->second.utf8().data());

        Vector<String> extensions = priv->corePlugin->mimeToExtensions().get(it->first);
        mimeType->extensions = static_cast<gchar**>(g_malloc0(extensions.size() + 1));
        for (unsigned i = 0; i < extensions.size(); i++)
            mimeType->extensions[i] = g_strdup(extensions[i].utf8().data());

        priv->mimeTypes = g_slist_append(priv->mimeTypes, mimeType);
    }

    return priv->mimeTypes;
}

/**
 * webkit_web_plugin_set_enabled:
 * @plugin: a #WebKitWebPlugin
 * @enabled: whether to enable the plugin
 *
 * Sets the enabled status of the @plugin.
 *
 * Since: 1.3.8
 */
void webkit_web_plugin_set_enabled(WebKitWebPlugin* plugin, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_WEB_PLUGIN(plugin));
    WebKitWebPluginPrivate* priv = plugin->priv;

    ASSERT(priv->corePlugin);
    if (priv->corePlugin->isEnabled() == enabled)
        return;

    priv->corePlugin->setEnabled(enabled);

    g_object_notify(G_OBJECT(plugin), "enabled");
}

/**
 * webkit_web_plugin_get_enabled:
 * @plugin: a #WebKitWebPlugin
 *
 * Returns: %TRUE if the plugin is enabled, %FALSE otherwise
 *
 * Since: 1.3.8
 */
gboolean webkit_web_plugin_get_enabled(WebKitWebPlugin* plugin)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PLUGIN(plugin), FALSE);

    ASSERT(plugin->priv->corePlugin);
    return plugin->priv->corePlugin->isEnabled();
}
