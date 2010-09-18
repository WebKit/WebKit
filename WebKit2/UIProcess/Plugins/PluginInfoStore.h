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

#include <WebCore/PluginData.h>

namespace WebCore {
    class KURL;
}

namespace WebKit {

class PluginInfoStore {
public:
    PluginInfoStore();

    void setAdditionalPluginsDirectories(const Vector<String>&);

    void refresh();
    void getPlugins(Vector<WebCore::PluginInfo>& plugins);
    
    // Represents a single plug-in.
    struct Plugin {
        String path;
        WebCore::PluginInfo info;
#if PLATFORM(MAC)
        cpu_type_t pluginArchitecture;
        String bundleIdentifier;
        unsigned versionNumber;
#endif
    };

    // Returns the info for a plug-in that can handle the given MIME type.
    // If the MIME type is null, the file extension of the given url will be used to infer the
    // plug-in type. In that case, mimeType will be filled in with the right MIME type.
    Plugin findPlugin(String& mimeType, const WebCore::KURL& url);
    
private:

    Plugin findPluginForMIMEType(const String& mimeType);
    Plugin findPluginForExtension(const String& extension, String& mimeType);

    void loadPluginsIfNecessary();
    void loadPluginsInDirectory(const String& directory);
    void loadPlugin(const String& pluginPath);
    
    // Platform specific member functions.
    static Vector<String> pluginsDirectories();
    static Vector<String> pluginPathsInDirectory(const String& directory);
    static bool getPluginInfo(const String& pluginPath, Plugin& plugin);
    static bool shouldUsePlugin(const Plugin& plugin, const Vector<Plugin>& loadedPlugins);
    static String getMIMETypeForExtension(const String& extension);

    Vector<String> m_additionalPluginsDirectories;
    Vector<Plugin> m_plugins;
    bool m_pluginListIsUpToDate;
};
    
} // namespace WebKit

#endif // PluginInfoStore_h
