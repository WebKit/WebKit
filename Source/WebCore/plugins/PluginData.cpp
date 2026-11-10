/*
    Copyright (C) 2000 Harri Porten (porten@kde.org)
    Copyright (C) 2000 Daniel Molkentin (molkentin@kde.org)
    Copyright (C) 2000 Stefan Schimanski (schimmi@kde.org)
    Copyright (C) 2003, 2004, 2005, 2006, 2007, 2015 Apple Inc. All rights reserved.
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
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PluginInfoProvider.h"

namespace WebCore {

#if PLATFORM(COCOA)
static inline bool isBuiltInPDFPlugIn(const PluginInfo& plugin)
{
    return equalLettersIgnoringASCIICase(plugin.bundleIdentifier, "com.apple.webkit.builtinpdfplugin"_s);
}
#else
static inline bool isBuiltInPDFPlugIn(const PluginInfo&)
{
    return false;
}
#endif

PluginData::PluginData(Page& page)
    : m_page(page)
{
    initPlugins();
}

void PluginData::initPlugins()
{
    ASSERT(m_plugins.isEmpty());
    Ref page = m_page.get();
    m_plugins = protect(page->pluginInfoProvider())->pluginInfo(page.get(), m_supportedPluginIdentifiers);

    for (auto& plugin : m_plugins) {
        if (isBuiltInPDFPlugIn(plugin)) {
            m_builtInPDFPluginInfo = plugin;
            break;
        }
    }
}

const Vector<PluginInfo>& PluginData::webVisiblePlugins() const
{
    Ref page = m_page.get();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    auto documentURL = localMainFrame && localMainFrame->document() ? protect(localMainFrame->document())->url() : URL { };
    if (!documentURL.isNull() && !protocolHostAndPortAreEqual(m_cachedVisiblePlugins.pageURL, documentURL)) {
        m_cachedVisiblePlugins.pageURL = WTF::move(documentURL);
        m_cachedVisiblePlugins.pluginList = std::nullopt;
    }

    if (!m_cachedVisiblePlugins.pluginList)
        m_cachedVisiblePlugins.pluginList = protect(page->pluginInfoProvider())->webVisiblePluginInfo(page.get(), m_cachedVisiblePlugins.pageURL);

    return *m_cachedVisiblePlugins.pluginList;
}

Vector<MimeClassInfo> PluginData::webVisibleMimeTypes() const
{
    Vector<MimeClassInfo> result;
    for (auto& plugin : webVisiblePlugins())
        result.appendVector(plugin.mimes);
    return result;
}

NODELETE static bool supportsMimeTypeForPlugins(StringImpl* mimeType, const PluginData::AllowedPluginTypes allowedPluginTypes, const Vector<PluginInfo>& plugins)
{
    for (auto& plugin : plugins) {
        for (auto& type : plugin.mimes) {
            if (equal(type.type, mimeType) && (allowedPluginTypes == PluginData::AllPlugins || plugin.isApplicationPlugin))
                return true;
        }
    }
    return false;
}

bool PluginData::supportsMimeType(StringImpl* mimeType, const AllowedPluginTypes allowedPluginTypes) const
{
    return supportsMimeTypeForPlugins(mimeType, allowedPluginTypes, plugins());
}

bool PluginData::supportsWebVisibleMimeType(StringImpl* mimeType, const AllowedPluginTypes allowedPluginTypes) const
{
    return supportsMimeTypeForPlugins(mimeType, allowedPluginTypes, webVisiblePlugins());
}

bool PluginData::supportsWebVisibleMimeTypeForURL(StringImpl* mimeType, const AllowedPluginTypes allowedPluginTypes, const URL& url) const
{
    if (!protocolHostAndPortAreEqual(m_cachedVisiblePlugins.pageURL, url)) {
        Ref page = m_page.get();
        m_cachedVisiblePlugins = { url, protect(page->pluginInfoProvider())->webVisiblePluginInfo(page.get(), url) };
    }
    if (!m_cachedVisiblePlugins.pluginList)
        return false;
    return supportsMimeTypeForPlugins(mimeType, allowedPluginTypes, *m_cachedVisiblePlugins.pluginList);
}

RefPtr<StringImpl> PluginData::pluginFileForWebVisibleMimeType(StringImpl* mimeType) const
{
    for (auto& plugin : webVisiblePlugins()) {
        for (auto& type : plugin.mimes) {
            if (equal(type.type, mimeType))
                return plugin.file;
        }
    }
    return { };
}

PluginInfo PluginData::dummyPDFPluginInfo()
{
    PluginInfo info;

    info.name = "Dummy Plugin"_si;
    info.desc = pdfDocumentTypeDescription();
    info.file = "internal-pdf-viewer"_si;
    info.isApplicationPlugin = true;

    MimeClassInfo pdfMimeClassInfo;
    pdfMimeClassInfo.type = "application/pdf"_atom;
    pdfMimeClassInfo.desc = pdfDocumentTypeDescription();
    pdfMimeClassInfo.extensions.append("pdf"_s);
    info.mimes.append(pdfMimeClassInfo);

    MimeClassInfo textPDFMimeClassInfo;
    textPDFMimeClassInfo.type = "text/pdf"_atom;
    textPDFMimeClassInfo.desc = pdfDocumentTypeDescription();
    textPDFMimeClassInfo.extensions.append("pdf"_s);
    info.mimes.append(textPDFMimeClassInfo);

    return info;
}

} // namespace WebCore
