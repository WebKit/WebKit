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

#include "config.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "PluginInfoStore.h"

#include "NetscapePluginModule.h"
#include "PluginSearchPath.h"
#include "ProcessExecutablePath.h"
#include <WebCore/PlatformDisplay.h>
#include <limits.h>
#include <stdlib.h>
#include <wtf/FileSystem.h>

#if PLATFORM(GTK)
#include "PluginInfoCache.h"
#endif

namespace WebKit {
using namespace WebCore;

Vector<String> PluginInfoStore::pluginsDirectories()
{
    return WebKit::pluginsDirectories();
}

Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> result;
    char normalizedPath[PATH_MAX];
    for (const auto& path : FileSystem::listDirectory(directory, String("*.so"))) {
        CString filename = FileSystem::fileSystemRepresentation(path);
        if (realpath(filename.data(), normalizedPath))
            result.append(FileSystem::stringFromFileSystemRepresentation(normalizedPath));
    }

    return result;
}

Vector<String> PluginInfoStore::individualPluginPaths()
{
    return Vector<String>();
}

bool PluginInfoStore::getPluginInfo(const String& pluginPath, PluginModuleInfo& plugin)
{
#if PLATFORM(GTK)
    if (PluginInfoCache::singleton().getPluginInfo(pluginPath, plugin))
        return true;

    if (NetscapePluginModule::getPluginInfo(pluginPath, plugin)) {
        PluginInfoCache::singleton().updatePluginInfo(pluginPath, plugin);
        return true;
    }
    return false;
#else
    return NetscapePluginModule::getPluginInfo(pluginPath, plugin);
#endif
}

bool PluginInfoStore::shouldUsePlugin(Vector<PluginModuleInfo>& /*alreadyLoadedPlugins*/, const PluginModuleInfo& /*plugin*/)
{
    // We do not do any black-listing presently.
    return true;
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
