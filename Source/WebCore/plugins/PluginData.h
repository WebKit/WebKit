/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef PluginData_h
#define PluginData_h

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
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

// FIXME: merge with PluginDatabase in the future
class PluginData : public RefCounted<PluginData> {
public:
    static Ref<PluginData> create(const Page* page) { return adoptRef(*new PluginData(page)); }

    const Vector<PluginInfo>& plugins() const { return m_plugins; }
    Vector<PluginInfo> webVisiblePlugins() const;
    WEBCORE_EXPORT void getWebVisibleMimesAndPluginIndices(Vector<MimeClassInfo>&, Vector<size_t>&) const;

    enum AllowedPluginTypes {
        AllPlugins,
        OnlyApplicationPlugins
    };

    WEBCORE_EXPORT bool supportsWebVisibleMimeType(const String& mimeType, const AllowedPluginTypes) const;
    String pluginNameForWebVisibleMimeType(const String& mimeType) const;
    String pluginFileForWebVisibleMimeType(const String& mimeType) const;

    static void refresh();

private:
    explicit PluginData(const Page*);
    void initPlugins();
    const PluginInfo* pluginInfoForWebVisibleMimeType(const String& mimeType) const;

protected:
#if defined ENABLE_WEB_REPLAY && ENABLE_WEB_REPLAY
    PluginData(Vector<PluginInfo> plugins)
        : m_plugins(plugins)
    {
    }
#endif

    const Page* m_page;
    Vector<PluginInfo> m_plugins;
};

}

#endif
