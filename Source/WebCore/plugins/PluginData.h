/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2015 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include "SecurityOriginData.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Page;
struct PluginInfo;

enum PluginLoadClientPolicy : uint8_t {
    // No client-specific plug-in load policy has been defined. The plug-in should be visible in navigator.plugins and WebKit should synchronously
    // ask the client whether the plug-in should be loaded.
    PluginLoadClientPolicyUndefined = 0,

    // The plug-in module should be blocked from being instantiated. The plug-in should be hidden in navigator.plugins.
    PluginLoadClientPolicyBlock,

    // WebKit should synchronously ask the client whether the plug-in should be loaded. The plug-in should be visible in navigator.plugins.
    PluginLoadClientPolicyAsk,

    // The plug-in module may be loaded if WebKit is not blocking it.
    PluginLoadClientPolicyAllow,

    // The plug-in module should be loaded irrespective of whether WebKit has asked it to be blocked.
    PluginLoadClientPolicyAllowAlways,

    PluginLoadClientPolicyMaximum = PluginLoadClientPolicyAllowAlways
};

struct MimeClassInfo {
    String type;
    String desc;
    Vector<String> extensions;
};

inline bool operator==(const MimeClassInfo& a, const MimeClassInfo& b)
{
    return a.type == b.type && a.desc == b.desc && a.extensions == b.extensions;
}

struct PluginInfo {
    String name;
    String file;
    String desc;
    Vector<MimeClassInfo> mimes;
    bool isApplicationPlugin;

    PluginLoadClientPolicy clientLoadPolicy;

#if PLATFORM(MAC)
    String bundleIdentifier;
    String versionString;
#endif
};

inline bool operator==(PluginInfo& a, PluginInfo& b)
{
    bool result = a.name == b.name && a.file == b.file && a.desc == b.desc && a.mimes == b.mimes && a.isApplicationPlugin == b.isApplicationPlugin && a.clientLoadPolicy == b.clientLoadPolicy;
#if PLATFORM(MAC)
    result = result && a.bundleIdentifier == b.bundleIdentifier && a.versionString == b.versionString;
#endif
    return result;
}

struct SupportedPluginNames {
    HashSet<String> allOriginPlugins;
    HashMap<SecurityOriginData, HashSet<String>> originSpecificPlugins;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SupportedPluginNames> decode(Decoder&);
};

// FIXME: merge with PluginDatabase in the future
class PluginData : public RefCounted<PluginData> {
public:
    static Ref<PluginData> create(Page& page) { return adoptRef(*new PluginData(page)); }

    const Vector<PluginInfo>& plugins() const { return m_plugins; }
    Vector<PluginInfo> webVisiblePlugins() const;
    Vector<PluginInfo> publiclyVisiblePlugins() const;
    WEBCORE_EXPORT void getWebVisibleMimesAndPluginIndices(Vector<MimeClassInfo>&, Vector<size_t>&) const;

    enum AllowedPluginTypes {
        AllPlugins,
        OnlyApplicationPlugins
    };

    WEBCORE_EXPORT bool supportsWebVisibleMimeType(const String& mimeType, const AllowedPluginTypes) const;
    String pluginFileForWebVisibleMimeType(const String& mimeType) const;

    WEBCORE_EXPORT bool supportsMimeType(const String& mimeType, const AllowedPluginTypes) const;

private:
    explicit PluginData(Page&);
    void initPlugins();
    bool getPluginInfoForWebVisibleMimeType(const String& mimeType, PluginInfo&) const;
    void getMimesAndPluginIndices(Vector<MimeClassInfo>&, Vector<size_t>&) const;
    void getMimesAndPluginIndiciesForPlugins(const Vector<PluginInfo>&, Vector<MimeClassInfo>&, Vector<size_t>&) const;

protected:
    Page& m_page;
    Vector<PluginInfo> m_plugins;
    std::optional<SupportedPluginNames> m_supportedPluginNames;
};

inline bool isSupportedPlugin(SupportedPluginNames& pluginNames, SecurityOriginData& origin, const String& pluginName)
{
    auto iterator = pluginNames.originSpecificPlugins.find(origin);
    if (iterator != pluginNames.originSpecificPlugins.end()) {
        if (iterator->value.contains(pluginName))
            return true;
    }

    return pluginNames.allOriginPlugins.contains(pluginName);
}

template<class Decoder> inline std::optional<SupportedPluginNames> SupportedPluginNames::decode(Decoder& decoder)
{
    std::optional<HashSet<String>> allOriginPlugins;
    decoder >> allOriginPlugins;
    if (!allOriginPlugins)
        return std::nullopt;

    std::optional<HashMap<SecurityOriginData, HashSet<String>>> originSpecificPlugins;
    decoder >> originSpecificPlugins;
    if (!originSpecificPlugins)
        return std::nullopt;

    return SupportedPluginNames { WTFMove(allOriginPlugins.value()), WTFMove(originSpecificPlugins.value()) };
}

template<class Encoder> inline void SupportedPluginNames::encode(Encoder& encoder) const
{
    encoder << allOriginPlugins;
    encoder << originSpecificPlugins;
}

} // namespace WebCore
