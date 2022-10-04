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
    m_plugins = m_page.pluginInfoProvider().pluginInfo(m_page, m_supportedPluginIdentifiers);

    for (auto& plugin : m_plugins) {
        if (isBuiltInPDFPlugIn(plugin)) {
            m_builtInPDFPluginInfo = plugin;
            break;
        }
    }
}

const Vector<PluginInfo>& PluginData::webVisiblePlugins() const
{
    auto documentURL = m_page.mainFrame().document() ? m_page.mainFrame().document()->url() : URL { };
    if (!documentURL.isNull() && !protocolHostAndPortAreEqual(m_cachedVisiblePlugins.pageURL, documentURL)) {
        m_cachedVisiblePlugins.pageURL = WTFMove(documentURL);
        m_cachedVisiblePlugins.pluginList = std::nullopt;
    }

    if (!m_cachedVisiblePlugins.pluginList)
        m_cachedVisiblePlugins.pluginList = m_page.pluginInfoProvider().webVisiblePluginInfo(m_page, m_cachedVisiblePlugins.pageURL);

    return *m_cachedVisiblePlugins.pluginList;
}

Vector<MimeClassInfo> PluginData::webVisibleMimeTypes() const
{
    Vector<MimeClassInfo> result;
    for (auto& plugin : webVisiblePlugins())
        result.appendVector(plugin.mimes);
    return result;
}

static bool supportsMimeTypeForPlugins(const String& mimeType, const PluginData::AllowedPluginTypes allowedPluginTypes, const Vector<PluginInfo>& plugins)
{
    for (auto& plugin : plugins) {
        for (auto& type : plugin.mimes) {
            if (type.type == mimeType && (allowedPluginTypes == PluginData::AllPlugins || plugin.isApplicationPlugin))
                return true;
        }
    }
    return false;
}

bool PluginData::supportsMimeType(const String& mimeType, const AllowedPluginTypes allowedPluginTypes) const
{
    return supportsMimeTypeForPlugins(mimeType, allowedPluginTypes, plugins());
}

bool PluginData::supportsWebVisibleMimeType(const String& mimeType, const AllowedPluginTypes allowedPluginTypes) const
{
    return supportsMimeTypeForPlugins(mimeType, allowedPluginTypes, webVisiblePlugins());
}

bool PluginData::supportsWebVisibleMimeTypeForURL(const String& mimeType, const AllowedPluginTypes allowedPluginTypes, const URL& url) const
{
    if (!protocolHostAndPortAreEqual(m_cachedVisiblePlugins.pageURL, url))
        m_cachedVisiblePlugins = { url, m_page.pluginInfoProvider().webVisiblePluginInfo(m_page, url) };
    if (!m_cachedVisiblePlugins.pluginList)
        return false;
    return supportsMimeTypeForPlugins(mimeType, allowedPluginTypes, *m_cachedVisiblePlugins.pluginList);
}

String PluginData::pluginFileForWebVisibleMimeType(const String& mimeType) const
{
    for (auto& plugin : webVisiblePlugins()) {
        for (auto& type : plugin.mimes) {
            if (type.type == mimeType)
                return plugin.file;
        }
    }
    return { };
}

PluginInfo PluginData::dummyPDFPluginInfo()
{
    PluginInfo info;

    info.name = "Dummy Plugin"_s;
    info.desc = pdfDocumentTypeDescription();
    info.file = "internal-pdf-viewer"_s;
    info.isApplicationPlugin = true;
    info.clientLoadPolicy = PluginLoadClientPolicy::Undefined;

    MimeClassInfo pdfMimeClassInfo;
    pdfMimeClassInfo.type = "application/pdf"_s;
    pdfMimeClassInfo.desc = pdfDocumentTypeDescription();
    pdfMimeClassInfo.extensions.append("pdf"_s);
    info.mimes.append(pdfMimeClassInfo);

    MimeClassInfo textPDFMimeClassInfo;
    textPDFMimeClassInfo.type = "text/pdf"_s;
    textPDFMimeClassInfo.desc = pdfDocumentTypeDescription();
    textPDFMimeClassInfo.extensions.append("pdf"_s);
    info.mimes.append(textPDFMimeClassInfo);

    return info;
}

} // namespace WebCore
