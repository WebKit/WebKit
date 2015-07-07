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
    return ASCIILiteral("PluginInformationBundleIdentifier");
}

String pluginInformationBundleVersionKey()
{
    return ASCIILiteral("PluginInformationBundleVersion");
}

String pluginInformationBundleShortVersionKey()
{
    return ASCIILiteral("PluginInformationBundleShortVersion");
}

String pluginInformationPathKey()
{
    return ASCIILiteral("PluginInformationPath");
}

String pluginInformationDisplayNameKey()
{
    return ASCIILiteral("PluginInformationDisplayName");
}

String pluginInformationDefaultLoadPolicyKey()
{
    return ASCIILiteral("PluginInformationDefaultLoadPolicy");
}

String pluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey()
{
    return ASCIILiteral("PluginInformationUpdatePastLastBlockedVersionIsKnownAvailable");
}

String pluginInformationHasSandboxProfileKey()
{
    return ASCIILiteral("PluginInformationHasSandboxProfile");
}

String pluginInformationFrameURLKey()
{
    return ASCIILiteral("PluginInformationFrameURL");
}

String pluginInformationMIMETypeKey()
{
    return ASCIILiteral("PluginInformationMIMEType");
}

String pluginInformationPageURLKey()
{
    return ASCIILiteral("PluginInformationPageURL");
}

String pluginInformationPluginspageAttributeURLKey()
{
    return ASCIILiteral("PluginInformationPluginspageAttributeURL");
}

String pluginInformationPluginURLKey()
{
    return ASCIILiteral("PluginInformationPluginURL");
}

String plugInInformationReplacementObscuredKey()
{
    return ASCIILiteral("PlugInInformationReplacementObscured");
}

void getPluginModuleInformation(const PluginModuleInfo& plugin, ImmutableDictionary::MapType& map)
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

PassRefPtr<ImmutableDictionary> createPluginInformationDictionary(const PluginModuleInfo& plugin)
{
    ImmutableDictionary::MapType map;
    getPluginModuleInformation(plugin, map);

    return ImmutableDictionary::create(WTF::move(map));
}

PassRefPtr<ImmutableDictionary> createPluginInformationDictionary(const PluginModuleInfo& plugin, const String& frameURLString, const String& mimeType, const String& pageURLString, const String& pluginspageAttributeURLString, const String& pluginURLString, bool replacementObscured)
{
    ImmutableDictionary::MapType map;
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

    return ImmutableDictionary::create(WTF::move(map));
}

PassRefPtr<ImmutableDictionary> createPluginInformationDictionary(const String& mimeType, const String& frameURLString, const String& pageURLString)
{
    ImmutableDictionary::MapType map;

    if (!frameURLString.isEmpty())
        map.set(pluginInformationFrameURLKey(), API::URL::create(frameURLString));
    if (!mimeType.isEmpty())
        map.set(pluginInformationMIMETypeKey(), API::String::create(mimeType));
    if (!pageURLString.isEmpty())
        map.set(pluginInformationPageURLKey(), API::URL::create(pageURLString));

    return ImmutableDictionary::create(WTF::move(map));
}

#if !PLATFORM(COCOA)
void getPlatformPluginModuleInformation(const PluginModuleInfo&, ImmutableDictionary::MapType&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
