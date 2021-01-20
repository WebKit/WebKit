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

#include "config.h"
#include "PluginInformation.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "APINumber.h"
#include "APIString.h"
#include "APIURL.h"
#include "PluginInfoStore.h"
#include "PluginModuleInfo.h"
#include "WKAPICast.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

String pluginInformationBundleIdentifierKey()
{
    return "PluginInformationBundleIdentifier"_s;
}

String pluginInformationBundleVersionKey()
{
    return "PluginInformationBundleVersion"_s;
}

String pluginInformationBundleShortVersionKey()
{
    return "PluginInformationBundleShortVersion"_s;
}

String pluginInformationPathKey()
{
    return "PluginInformationPath"_s;
}

String pluginInformationDisplayNameKey()
{
    return "PluginInformationDisplayName"_s;
}

String pluginInformationDefaultLoadPolicyKey()
{
    return "PluginInformationDefaultLoadPolicy"_s;
}

String pluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey()
{
    return "PluginInformationUpdatePastLastBlockedVersionIsKnownAvailable"_s;
}

String pluginInformationHasSandboxProfileKey()
{
    return "PluginInformationHasSandboxProfile"_s;
}

String pluginInformationFrameURLKey()
{
    return "PluginInformationFrameURL"_s;
}

String pluginInformationMIMETypeKey()
{
    return "PluginInformationMIMEType"_s;
}

String pluginInformationPageURLKey()
{
    return "PluginInformationPageURL"_s;
}

String pluginInformationPluginspageAttributeURLKey()
{
    return "PluginInformationPluginspageAttributeURL"_s;
}

String pluginInformationPluginURLKey()
{
    return "PluginInformationPluginURL"_s;
}

String plugInInformationReplacementObscuredKey()
{
    return "PlugInInformationReplacementObscured"_s;
}

void getPluginModuleInformation(const PluginModuleInfo& plugin, API::Dictionary::MapType& map)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    map.set(pluginInformationPathKey(), API::String::create(plugin.path));
    map.set(pluginInformationDisplayNameKey(), API::String::create(plugin.info.name));
    map.set(pluginInformationDefaultLoadPolicyKey(), API::UInt64::create(toWKPluginLoadPolicy(PluginInfoStore::defaultLoadPolicyForPlugin(plugin))));

    getPlatformPluginModuleInformation(plugin, map);
#else
    UNUSED_PARAM(plugin);
    UNUSED_PARAM(map);
#endif
}

Ref<API::Dictionary> createPluginInformationDictionary(const PluginModuleInfo& plugin)
{
    API::Dictionary::MapType map;
    getPluginModuleInformation(plugin, map);

    return API::Dictionary::create(WTFMove(map));
}

Ref<API::Dictionary> createPluginInformationDictionary(const PluginModuleInfo& plugin, const String& frameURLString, const String& mimeType, const String& pageURLString, const String& pluginspageAttributeURLString, const String& pluginURLString, bool replacementObscured)
{
    API::Dictionary::MapType map;
    getPluginModuleInformation(plugin, map);

    if (!frameURLString.isEmpty())
        map.set(pluginInformationFrameURLKey(), API::URL::create(frameURLString));
    if (!mimeType.isEmpty())
        map.set(pluginInformationMIMETypeKey(), API::String::create(mimeType));
    if (!pageURLString.isEmpty())
        map.set(pluginInformationPageURLKey(), API::URL::create(pageURLString));
    if (!pluginspageAttributeURLString.isEmpty())
        map.set(pluginInformationPluginspageAttributeURLKey(), API::URL::create(pluginspageAttributeURLString));
    if (!pluginURLString.isEmpty())
        map.set(pluginInformationPluginURLKey(), API::URL::create(pluginURLString));
    map.set(plugInInformationReplacementObscuredKey(), API::Boolean::create(replacementObscured));

    return API::Dictionary::create(WTFMove(map));
}

Ref<API::Dictionary> createPluginInformationDictionary(const String& mimeType, const String& frameURLString, const String& pageURLString)
{
    API::Dictionary::MapType map;

    if (!frameURLString.isEmpty())
        map.set(pluginInformationFrameURLKey(), API::URL::create(frameURLString));
    if (!mimeType.isEmpty())
        map.set(pluginInformationMIMETypeKey(), API::String::create(mimeType));
    if (!pageURLString.isEmpty())
        map.set(pluginInformationPageURLKey(), API::URL::create(pageURLString));

    return API::Dictionary::create(WTFMove(map));
}

#if !PLATFORM(COCOA)
void getPlatformPluginModuleInformation(const PluginModuleInfo&, API::Dictionary::MapType&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
