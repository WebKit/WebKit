/*
    Copyright (C) 2000 Harri Porten (porten@kde.org)
    Copyright (C) 2000 Daniel Molkentin (molkentin@kde.org)
    Copyright (C) 2000 Stefan Schimanski (schimmi@kde.org)
    Copyright (C) 2003, 2004, 2005, 2006, 2007, 2015 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "PluginData.h"

#include "Document.h"
#include "Frame.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PluginInfoProvider.h"

namespace WebCore {

PluginData::PluginData(Page& page)
    : m_page(page)
{
    initPlugins();
}

const Vector<PluginInfo>& PluginData::webVisiblePlugins() const
{
    auto documentURL = m_page.mainFrame().document() ? m_page.mainFrame().document()->url() : URL { };
    if (!documentURL.isNull() && !protocolHostAndPortAreEqual(m_cachedVisiblePlugins.pageURL, documentURL)) {
        m_cachedVisiblePlugins.pageURL = WTFMove(documentURL);
        m_cachedVisiblePlugins.pluginList = WTF::nullopt;
    }

    if (!m_cachedVisiblePlugins.pluginList)
        m_cachedVisiblePlugins.pluginList = m_page.pluginInfoProvider().webVisiblePluginInfo(m_page, m_cachedVisiblePlugins.pageURL);

    return *m_cachedVisiblePlugins.pluginList;
}

#if PLATFORM(COCOA)
static inline bool isBuiltInPDFPlugIn(const PluginInfo& plugin)
{
    return equalLettersIgnoringASCIICase(plugin.bundleIdentifier, "com.apple.webkit.builtinpdfplugin");
}
#else
static inline bool isBuiltInPDFPlugIn(const PluginInfo&)
{
    return false;
}
#endif

static bool shouldBePubliclyVisible(const PluginInfo& plugin)
{
    // We can greatly reduce fingerprinting opportunities by only advertising plug-ins
    // that are widely needed for general website compatibility. Since many users
    // will have these plug-ins, we are not revealing much user-specific information.
    //
    // Web compatibility data indicate that Flash, QuickTime, Java, and PDF support
    // are frequently accessed through the bad practice of iterating over the contents
    // of the navigator.plugins list. Luckily, these plug-ins happen to be the least
    // user-specific.
    return plugin.name.containsIgnoringASCIICase("Shockwave")
        || plugin.name.containsIgnoringASCIICase("QuickTime")
        || plugin.name.containsIgnoringASCIICase("Java")
        || isBuiltInPDFPlugIn(plugin);
}

Vector<PluginInfo> PluginData::publiclyVisiblePlugins() const
{
    auto plugins = webVisiblePlugins();

    if (m_page.showAllPlugins())
        return plugins;
    
    plugins.removeAllMatching([](auto& plugin) {
        return !shouldBePubliclyVisible(plugin);
    });

    std::sort(plugins.begin(), plugins.end(), [](const PluginInfo& a, const PluginInfo& b) {
        return codePointCompareLessThan(a.name, b.name);
    });
    return plugins;
}

void PluginData::getWebVisibleMimesAndPluginIndices(Vector<MimeClassInfo>& mimes, Vector<size_t>& mimePluginIndices) const
{
    getMimesAndPluginIndiciesForPlugins(webVisiblePlugins(), mimes, mimePluginIndices);
}

void PluginData::getMimesAndPluginIndices(Vector<MimeClassInfo>& mimes, Vector<size_t>& mimePluginIndices) const
{
    getMimesAndPluginIndiciesForPlugins(plugins(), mimes, mimePluginIndices);
}

void PluginData::getMimesAndPluginIndiciesForPlugins(const Vector<PluginInfo>& plugins, Vector<MimeClassInfo>& mimes, Vector<size_t>& mimePluginIndices) const
{
    ASSERT_ARG(mimes, mimes.isEmpty());
    ASSERT_ARG(mimePluginIndices, mimePluginIndices.isEmpty());

    for (unsigned i = 0; i < plugins.size(); ++i) {
        const PluginInfo& plugin = plugins[i];
        for (auto& mime : plugin.mimes) {
            mimes.append(mime);
            mimePluginIndices.append(i);
        }
    }
}

bool PluginData::supportsWebVisibleMimeTypeForURL(const String& mimeType, const AllowedPluginTypes allowedPluginTypes, const URL& url) const
{
    if (!protocolHostAndPortAreEqual(m_cachedVisiblePlugins.pageURL, url))
        m_cachedVisiblePlugins = { url, m_page.pluginInfoProvider().webVisiblePluginInfo(m_page, url) };
    if (!m_cachedVisiblePlugins.pluginList)
        return false;
    return supportsWebVisibleMimeType(mimeType, allowedPluginTypes, *m_cachedVisiblePlugins.pluginList);
}

bool PluginData::supportsWebVisibleMimeType(const String& mimeType, const AllowedPluginTypes allowedPluginTypes) const
{
    return supportsWebVisibleMimeType(mimeType, allowedPluginTypes, webVisiblePlugins());
}

bool PluginData::supportsWebVisibleMimeType(const String& mimeType, const AllowedPluginTypes allowedPluginTypes, const Vector<PluginInfo>& plugins) const
{
    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    getMimesAndPluginIndiciesForPlugins(plugins, mimes, mimePluginIndices);

    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i].type == mimeType && (allowedPluginTypes == AllPlugins || plugins[mimePluginIndices[i]].isApplicationPlugin))
            return true;
    }
    return false;
}

bool PluginData::getPluginInfoForWebVisibleMimeType(const String& mimeType, PluginInfo& pluginInfoRef) const
{
    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    const Vector<PluginInfo>& plugins = webVisiblePlugins();
    getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);

    for (unsigned i = 0; i < mimes.size(); ++i) {
        const MimeClassInfo& info = mimes[i];

        if (info.type == mimeType) {
            pluginInfoRef = plugins[mimePluginIndices[i]];
            return true;
        }
    }

    return false;
}

String PluginData::pluginFileForWebVisibleMimeType(const String& mimeType) const
{
    PluginInfo info;
    if (getPluginInfoForWebVisibleMimeType(mimeType, info))
        return info.file;
    return String();
}

bool PluginData::supportsMimeType(const String& mimeType, const AllowedPluginTypes allowedPluginTypes) const
{
    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    const Vector<PluginInfo>& plugins = this->plugins();
    getMimesAndPluginIndices(mimes, mimePluginIndices);

    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i].type == mimeType && (allowedPluginTypes == AllPlugins || plugins[mimePluginIndices[i]].isApplicationPlugin))
            return true;
    }
    return false;
}

void PluginData::initPlugins()
{
    ASSERT(m_plugins.isEmpty());

    m_plugins = m_page.pluginInfoProvider().pluginInfo(m_page, m_supportedPluginIdentifiers);
}

} // namespace WebCore
