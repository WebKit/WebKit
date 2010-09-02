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

// Note: this file is only for UNIX. On other platforms we can reuse the native implementation.

#include "PluginInfoStore.h"

#include "PluginDatabase.h"
#include "PluginPackage.h"

using namespace WebCore;

namespace WebKit {

Vector<String> PluginInfoStore::pluginsDirectories()
{
    return PluginDatabase::defaultPluginDirectories();
}

Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> result;
    Vector<String> pluginPaths = listDirectory(directory, String("*.so"));
    Vector<String>::const_iterator end = pluginPaths.end();
    for (Vector<String>::const_iterator it = pluginPaths.begin(); it != end; ++it) {
        if (fileExists(*it))
            result.append(*it);
    }

    return result;
}

bool PluginInfoStore::getPluginInfo(const String& pluginPath, Plugin& plugin)
{
    // We are loading the plugin here since it does not seem to be a standardized way to
    // get the needed informations from a UNIX plugin without loading it.

    RefPtr<PluginPackage> package = PluginPackage::createPackage(pluginPath, 0 /*lastModified*/);
    if (!package)
        return false;

    plugin.path = pluginPath;
    plugin.info.desc = package->description();
    plugin.info.file = package->fileName();

    const MIMEToDescriptionsMap& descriptions = package->mimeToDescriptions();
    const MIMEToExtensionsMap& extensions = package->mimeToExtensions();
    Vector<MimeClassInfo> mimes(descriptions.size());
    MIMEToDescriptionsMap::const_iterator descEnd = descriptions.end();
    unsigned i = 0;
    for (MIMEToDescriptionsMap::const_iterator it = descriptions.begin(); it != descEnd; ++it) {
        MimeClassInfo& mime = mimes[i++];
        mime.type = it->first;
        mime.desc = it->second;
        MIMEToExtensionsMap::const_iterator extensionIt = extensions.find(it->first);
        ASSERT(extensionIt != extensions.end());
        mime.extensions = extensionIt->second;
    }

    package->unload();
    return true;
}

bool PluginInfoStore::shouldUsePlugin(const Plugin& plugin, const Vector<Plugin>& loadedPlugins)
{
    // We do not do any black-listing presently.
    return true;
}

} // namespace WebKit
