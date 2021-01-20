/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "PluginInformation.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "APINumber.h"
#import "APIString.h"
#import "PluginModuleInfo.h"
#import "StringUtilities.h"
#import <WebCore/PluginBlocklist.h>

namespace WebKit {
using namespace WebCore;

void getPlatformPluginModuleInformation(const PluginModuleInfo& plugin, API::Dictionary::MapType& map)
{
    map.set(pluginInformationBundleIdentifierKey(), API::String::create(plugin.bundleIdentifier));
    map.set(pluginInformationBundleVersionKey(), API::String::create(plugin.versionString));
    map.set(pluginInformationBundleShortVersionKey(), API::String::create(plugin.shortVersionString));
    map.set(pluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey(), API::Boolean::create(WebCore::PluginBlocklist::isPluginUpdateAvailable(nsStringFromWebCoreString(plugin.bundleIdentifier))));
    map.set(pluginInformationHasSandboxProfileKey(), API::Boolean::create(plugin.hasSandboxProfile));
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
