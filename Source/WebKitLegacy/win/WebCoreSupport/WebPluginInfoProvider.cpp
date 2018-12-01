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

#include "WebPluginInfoProvider.h"

#include "PluginDatabase.h"

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
    PluginDatabase::installedPlugins()->refresh();
}

Vector<WebCore::PluginInfo> WebPluginInfoProvider::pluginInfo(WebCore::Page& page, std::optional<Vector<WebCore::SupportedPluginIdentifier>>&)
{
    Vector<WebCore::PluginInfo> outPlugins;
    const Vector<PluginPackage*>& plugins = PluginDatabase::installedPlugins()->plugins();

    outPlugins.resize(plugins.size());

    for (size_t i = 0; i < plugins.size(); ++i) {
        PluginPackage* package = plugins[i];

        PluginInfo info;
        info.name = package->name();
        info.file = package->fileName();
        info.desc = package->description();

        const MIMEToDescriptionsMap& mimeToDescriptions = package->mimeToDescriptions();

        info.mimes.reserveCapacity(mimeToDescriptions.size());

        MIMEToDescriptionsMap::const_iterator end = mimeToDescriptions.end();
        for (MIMEToDescriptionsMap::const_iterator it = mimeToDescriptions.begin(); it != end; ++it) {
            MimeClassInfo mime;

            mime.type = it->key;
            mime.desc = it->value;
            mime.extensions = package->mimeToExtensions().get(mime.type);

            info.mimes.append(mime);
        }

        outPlugins[i] = info;
    }
    return outPlugins;
}

Vector<WebCore::PluginInfo> WebPluginInfoProvider::webVisiblePluginInfo(WebCore::Page& page, const URL&)
{
    std::optional<Vector<WebCore::SupportedPluginIdentifier>> supportedPluginNames;
    return pluginInfo(page, supportedPluginNames);
}
