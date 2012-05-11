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

#ifndef PluginInfoStore_h
#define PluginInfoStore_h

#include "PluginModuleInfo.h"

namespace WebCore {
    class KURL;
}

namespace WebKit {

class PluginInfoStore {
    WTF_MAKE_NONCOPYABLE(PluginInfoStore);

public:
    PluginInfoStore();

    void setAdditionalPluginsDirectories(const Vector<String>&);

    void refresh();
    Vector<PluginModuleInfo> plugins();

    // Returns the info for a plug-in that can handle the given MIME type.
    // If the MIME type is null, the file extension of the given url will be used to infer the
    // plug-in type. In that case, mimeType will be filled in with the right MIME type.
    PluginModuleInfo findPlugin(String& mimeType, const WebCore::KURL&);
    
    // Returns the info for the plug-in with the given path.
    PluginModuleInfo infoForPluginWithPath(const String& pluginPath) const;

    // Return whether this plug-in should be blocked from being instantiated.
    // Note that the plug-in will still be seen by e.g. navigator.plugins
    bool shouldBlockPlugin(const PluginModuleInfo&) const;

private:
    PluginModuleInfo findPluginForMIMEType(const String& mimeType) const;
    PluginModuleInfo findPluginForExtension(const String& extension, String& mimeType) const;

    void loadPluginsIfNecessary();
    static void loadPlugin(Vector<PluginModuleInfo>& plugins, const String& pluginPath);
    
    // Platform-specific member functions:

    // Returns paths to directories that should be searched for plug-ins (via pluginPathsInDirectory).
    static Vector<String> pluginsDirectories();

    // Returns paths to all plug-ins in the specified directory.
    static Vector<String> pluginPathsInDirectory(const String& directory);

    // Returns paths to individual plug-ins that won't be found via pluginsDirectories/pluginPathsInDirectory.
    static Vector<String> individualPluginPaths();

    // Load plug-in info for the plug-in with the specified path.
    static bool getPluginInfo(const String& pluginPath, PluginModuleInfo&);

    // Return whether this plug-in should be used (added to the list of plug-ins) or not.
    static bool shouldUsePlugin(Vector<PluginModuleInfo>& alreadyLoadedPlugins, const PluginModuleInfo&);

    // Get the MIME type for the given extension.
    static String getMIMETypeForExtension(const String& extension);

    Vector<String> m_additionalPluginsDirectories;
    Vector<PluginModuleInfo> m_plugins;
    bool m_pluginListIsUpToDate;
};
    
} // namespace WebKit

#endif // PluginInfoStore_h
