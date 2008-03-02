/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginDatabase.h"

#include "Frame.h"
#include "KURL.h"
#include "PluginPackage.h"
#include <stdlib.h>

namespace WebCore {

PluginDatabase* PluginDatabase::installedPlugins()
{
    static PluginDatabase* plugins = 0;
    
    if (!plugins) {
        plugins = new PluginDatabase;
        plugins->setPluginPaths(PluginDatabase::defaultPluginPaths());
        plugins->refresh();
    }

    return plugins;
}

bool PluginDatabase::isMIMETypeRegistered(const String& mimeType)
{
    if (mimeType.isNull())
        return false;
    if (m_registeredMIMETypes.contains(mimeType))
        return true;
    // No plugin was found, try refreshing the database and searching again
    return (refresh() && m_registeredMIMETypes.contains(mimeType));
}

void PluginDatabase::addExtraPluginPath(const String& path)
{
    m_pluginPaths.append(path);
    refresh();
}

bool PluginDatabase::refresh()
{   
    PluginSet newPlugins;

    bool pluginSetChanged = false;

    // Create a new set of plugins
    newPlugins = getPluginsInPaths();

    if (!m_plugins.isEmpty()) {
        m_registeredMIMETypes.clear();

        PluginSet pluginsToUnload = m_plugins;

        PluginSet::const_iterator end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            pluginsToUnload.remove(*it);

        end = m_plugins.end();
        for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
            newPlugins.remove(*it);

        // Unload plugins
        end = pluginsToUnload.end();
        for (PluginSet::const_iterator it = pluginsToUnload.begin(); it != end; ++it)
            m_plugins.remove(*it);

        // Add new plugins
        end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            m_plugins.add(*it);

        pluginSetChanged = !pluginsToUnload.isEmpty() || !newPlugins.isEmpty();
    } else {
        m_plugins = newPlugins;
        PluginSet::const_iterator end = newPlugins.end();
        for (PluginSet::const_iterator it = newPlugins.begin(); it != end; ++it)
            m_plugins.add(*it);

        pluginSetChanged = !newPlugins.isEmpty();
    }

    // Register plug-in MIME types
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        // Get MIME types
        MIMEToDescriptionsMap::const_iterator map_end = (*it)->mimeToDescriptions().end();
        for (MIMEToDescriptionsMap::const_iterator map_it = (*it)->mimeToDescriptions().begin(); map_it != map_end; ++map_it) {
            m_registeredMIMETypes.add(map_it->first);
        }
    }

    return pluginSetChanged;
}

Vector<PluginPackage*> PluginDatabase::plugins() const
{
    Vector<PluginPackage*> result;

    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
        result.append((*it).get());

    return result;
}

int PluginDatabase::preferredPluginCompare(const void* a, const void* b)
{
    PluginPackage* pluginA = *static_cast<PluginPackage* const*>(a);
    PluginPackage* pluginB = *static_cast<PluginPackage* const*>(b);

    return pluginA->compare(*pluginB);
}

PluginPackage* PluginDatabase::pluginForMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return 0;

    String key = mimeType.lower();
    PluginSet::const_iterator end = m_plugins.end();

    Vector<PluginPackage*, 2> pluginChoices;

    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        if ((*it)->mimeToDescriptions().contains(key))
            pluginChoices.append((*it).get());
    }

    if (pluginChoices.isEmpty())
        return 0;

    qsort(pluginChoices.data(), pluginChoices.size(), sizeof(PluginPackage*), PluginDatabase::preferredPluginCompare);

    return pluginChoices[0];
}

String PluginDatabase::MIMETypeForExtension(const String& extension) const
{
    if (extension.isEmpty())
        return String();

    PluginSet::const_iterator end = m_plugins.end();
    String mimeType;
    Vector<PluginPackage*, 2> pluginChoices;
    HashMap<PluginPackage*, String> mimeTypeForPlugin;

    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        MIMEToExtensionsMap::const_iterator mime_end = (*it)->mimeToExtensions().end();

        for (MIMEToExtensionsMap::const_iterator mime_it = (*it)->mimeToExtensions().begin(); mime_it != mime_end; ++mime_it) {
            const Vector<String>& extensions = mime_it->second;
            bool foundMapping = false;
            for (unsigned i = 0; i < extensions.size(); i++) {
                if (equalIgnoringCase(extensions[i], extension)) {
                    PluginPackage* plugin = (*it).get();
                    pluginChoices.append(plugin);
                    mimeTypeForPlugin.add(plugin, mime_it->first);
                    foundMapping = true;
                    break;
                }
            }
            if (foundMapping)
                break;
        }
    }

    if (pluginChoices.isEmpty())
        return String();

    qsort(pluginChoices.data(), pluginChoices.size(), sizeof(PluginPackage*), PluginDatabase::preferredPluginCompare);

    return mimeTypeForPlugin.get(pluginChoices[0]);
}

PluginPackage* PluginDatabase::findPlugin(const KURL& url, String& mimeType)
{
    PluginPackage* plugin = pluginForMIMEType(mimeType);
    String filename = url.string();
    
    if (!plugin) {
        String filename = url.lastPathComponent();
        if (!filename.endsWith("/")) {
            int extensionPos = filename.reverseFind('.');
            if (extensionPos != -1) {
                String extension = filename.substring(extensionPos + 1);

                mimeType = MIMETypeForExtension(extension);
                plugin = pluginForMIMEType(mimeType);
            }
        }
    }

    // FIXME: if no plugin could be found, query Windows for the mime type 
    // corresponding to the extension.

    return plugin;
}

}
