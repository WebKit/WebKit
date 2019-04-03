/*
 * Copyright (C) 2010, 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PluginInfoStore.h"

#if PLATFORM(MAC) && ENABLE(NETSCAPE_PLUGIN_API)

#import "Logging.h"
#import "NetscapePluginModule.h"
#import "SandboxUtilities.h"
#import <WebCore/PluginBlacklist.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <pwd.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

namespace WebKit {
using namespace WebCore;

Vector<String> PluginInfoStore::pluginsDirectories()
{
    Vector<String> pluginsDirectories;
    pluginsDirectories.reserveInitialCapacity(2);

    ASCIILiteral pluginPath { "/Library/Internet Plug-Ins"_s };

    if (auto* pw = getpwuid(getuid()))
        pluginsDirectories.uncheckedAppend(makeString(pw->pw_dir, pluginPath));
    pluginsDirectories.uncheckedAppend(pluginPath);
    
    return pluginsDirectories;
}
    
Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> pluginPaths;

    RetainPtr<CFStringRef> directoryCFString = directory.createCFString();
    NSArray *filenames = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:(__bridge NSString *)directoryCFString.get() error:nil];
    for (NSString *filename in filenames)
        pluginPaths.append([(__bridge NSString *)directoryCFString.get() stringByAppendingPathComponent:filename]);
    
    return pluginPaths;
}

Vector<String> PluginInfoStore::individualPluginPaths()
{
    return Vector<String>();
}

bool PluginInfoStore::getPluginInfo(const String& pluginPath, PluginModuleInfo& plugin)
{
    return NetscapePluginModule::getPluginInfo(pluginPath, plugin);
}

static bool shouldBlockPlugin(const PluginModuleInfo& plugin)
{
    PluginModuleLoadPolicy loadPolicy = PluginInfoStore::defaultLoadPolicyForPlugin(plugin);
    return loadPolicy == PluginModuleBlockedForSecurity || loadPolicy == PluginModuleBlockedForCompatibility;
}

bool PluginInfoStore::shouldUsePlugin(Vector<PluginModuleInfo>& alreadyLoadedPlugins, const PluginModuleInfo& plugin)
{
    for (size_t i = 0; i < alreadyLoadedPlugins.size(); ++i) {
        const PluginModuleInfo& loadedPlugin = alreadyLoadedPlugins[i];

        // If a plug-in with the same bundle identifier already exists, we don't want to load it.
        // However, if the already existing plug-in is blocked we want to replace it with the new plug-in.
        if (loadedPlugin.bundleIdentifier == plugin.bundleIdentifier) {
            if (!shouldBlockPlugin(loadedPlugin))
                return false;

            alreadyLoadedPlugins.remove(i);
            break;
        }
    }

    if (plugin.bundleIdentifier == "com.apple.java.JavaAppletPlugin") {
        LOG(Plugins, "Ignoring com.apple.java.JavaAppletPlugin");
        return false;
    }

    if (currentProcessIsSandboxed() && !plugin.hasSandboxProfile) {
        LOG(Plugins, "Ignoring unsandboxed plug-in %s", plugin.bundleIdentifier.utf8().data());
        return false;
    }

    return true;
}

PluginModuleLoadPolicy PluginInfoStore::defaultLoadPolicyForPlugin(const PluginModuleInfo& plugin)
{
    switch (PluginBlacklist::loadPolicyForPluginVersion(plugin.bundleIdentifier, plugin.versionString)) {
    case PluginBlacklist::LoadPolicy::LoadNormally:
        return PluginModuleLoadNormally;
    case PluginBlacklist::LoadPolicy::BlockedForSecurity:
        return PluginModuleBlockedForSecurity;
    case PluginBlacklist::LoadPolicy::BlockedForCompatibility:
        return PluginModuleBlockedForCompatibility;
    }
}

PluginModuleInfo PluginInfoStore::findPluginWithBundleIdentifier(const String& bundleIdentifier)
{
    loadPluginsIfNecessary();

    for (size_t i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins[i].bundleIdentifier == bundleIdentifier)
            return m_plugins[i];
    }
    
    return PluginModuleInfo();
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(NETSCAPE_PLUGIN_API)
