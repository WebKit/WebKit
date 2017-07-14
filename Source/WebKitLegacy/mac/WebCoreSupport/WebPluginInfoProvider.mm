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

#import "WebPluginInfoProvider.h"

#import "WebPluginDatabase.h"
#import "WebPluginPackage.h"
#import <WebCore/FrameLoader.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <WebCore/SubframeLoader.h>
#import <wtf/BlockObjCExceptions.h>

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
    [[WebPluginDatabase sharedDatabaseIfExists] refresh];
}

void WebPluginInfoProvider::getPluginInfo(WebCore::Page& page, Vector<WebCore::PluginInfo>& plugins)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // WebKit1 has no application plug-ins, so we don't need to add them here.
    if (!page.mainFrame().loader().subframeLoader().allowPlugins())
        return;

    for (WebPluginPackage *plugin in [WebPluginDatabase sharedDatabase].plugins)
        plugins.append(plugin.pluginInfo);

    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebPluginInfoProvider::getWebVisiblePluginInfo(WebCore::Page& page, Vector<WebCore::PluginInfo>& plugins)
{
    getPluginInfo(page, plugins);
}
