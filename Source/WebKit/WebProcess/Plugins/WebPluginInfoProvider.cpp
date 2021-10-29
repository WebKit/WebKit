/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebPluginInfoProvider.h"

#include "HangDetectionDisabler.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(MAC)
#include <WebCore/StringUtilities.h>
#endif

#if ENABLE(PDFKIT_PLUGIN)
#include "PDFPlugin.h"
#endif

namespace WebKit {
using namespace WebCore;

WebPluginInfoProvider& WebPluginInfoProvider::singleton()
{
    static WebPluginInfoProvider& pluginInfoProvider = adoptRef(*new WebPluginInfoProvider).leakRef();

    return pluginInfoProvider;
}

WebPluginInfoProvider::WebPluginInfoProvider()
{
}

WebPluginInfoProvider::~WebPluginInfoProvider()
{
}

void WebPluginInfoProvider::refreshPlugins()
{
}

Vector<PluginInfo> WebPluginInfoProvider::pluginInfo(Page& page, std::optional<Vector<SupportedPluginIdentifier>>& supportedPluginIdentifiers)
{
#if ENABLE(PDFKIT_PLUGIN)
    UNUSED_PARAM(page);
    UNUSED_PARAM(supportedPluginIdentifiers);
    return { PDFPlugin::pluginInfo() };
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(supportedPluginIdentifiers);
    return { };
#endif // ENABLE(PDFKIT_PLUGIN)
}

Vector<WebCore::PluginInfo> WebPluginInfoProvider::webVisiblePluginInfo(Page& page, const URL& url)
{
    std::optional<Vector<WebCore::SupportedPluginIdentifier>> supportedPluginIdentifiers;
    auto plugins = pluginInfo(page, supportedPluginIdentifiers);

    plugins.removeAllMatching([&] (auto& plugin) {
        return supportedPluginIdentifiers && !isSupportedPlugin(*supportedPluginIdentifiers, url, plugin.bundleIdentifier);
    });

#if PLATFORM(MAC)
    if (LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol().toString()))
        return plugins;

    for (int32_t i = plugins.size() - 1; i >= 0; --i) {
        auto& info = plugins.at(i);

        // Allow built-in plugins. Also tentatively allow plugins that the client might later selectively permit.
        if (info.isApplicationPlugin || info.clientLoadPolicy == WebCore::PluginLoadClientPolicy::Ask)
            continue;

        if (info.clientLoadPolicy == WebCore::PluginLoadClientPolicy::Block)
            plugins.remove(i);
    }
#endif
    return plugins;
}

}
