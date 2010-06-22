/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PluginInfoStore.h"

#include <wtf/StdLibExtras.h>

using namespace WebCore;

namespace WebKit {

PluginInfoStore& PluginInfoStore::shared()
{
    DEFINE_STATIC_LOCAL(PluginInfoStore, pluginInfoStore, ());
    return pluginInfoStore;
}

PluginInfoStore::PluginInfoStore()
    : m_pluginListIsUpToDate(false)
{
}

void PluginInfoStore::refresh()
{
    m_pluginListIsUpToDate = false;
}

void PluginInfoStore::loadPluginsIfNecessary()
{
    if (m_pluginListIsUpToDate)
        return;

    m_plugins.clear();

    Vector<String> directories = pluginDirectories();
    for (size_t i = 0; i < directories.size(); ++i)
        loadPluginsInDirectory(directories[i]);

    m_pluginListIsUpToDate = true;
}

void PluginInfoStore::loadPluginsInDirectory(const String& directory)
{
    Vector<String> pluginPaths = pluginPathsInDirectory(directory);
    for (size_t i = 0; i < pluginPaths.size(); ++i)
        loadPlugin(pluginPaths[i]);
}

void PluginInfoStore::loadPlugin(const String& pluginPath)
{
    Plugin plugin;
    
    if (!getPluginInfo(pluginPath, plugin))
        return;

    if (!shouldUsePlugin(plugin, m_plugins))
        return;
    
    // Add the plug-in.
    m_plugins.append(plugin);
}

void PluginInfoStore::getPlugins(Vector<WebCore::PluginInfo>& plugins)
{
    loadPluginsIfNecessary();

    for (size_t i = 0; i < m_plugins.size(); ++i)
        plugins.append(m_plugins[i].info);
}

} // namespace WebKit
