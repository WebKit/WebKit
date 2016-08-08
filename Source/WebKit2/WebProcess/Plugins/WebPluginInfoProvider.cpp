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

// FIXME: We shouldn't call out to the platform strategy for this.
#include "WebPlatformStrategies.h"
#include <WebCore/PluginStrategy.h>

namespace WebKit {

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
    WebCore::platformStrategies()->pluginStrategy()->refreshPlugins();
}

void WebPluginInfoProvider::getPluginInfo(WebCore::Page& page, Vector<WebCore::PluginInfo>& plugins)
{
    WebCore::platformStrategies()->pluginStrategy()->getPluginInfo(&page, plugins);
}

void WebPluginInfoProvider::getWebVisiblePluginInfo(WebCore::Page& page, Vector<WebCore::PluginInfo>& plugins)
{
    WebCore::platformStrategies()->pluginStrategy()->getWebVisiblePluginInfo(&page, plugins);
}

#if PLATFORM(MAC)
void WebPluginInfoProvider::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy pluginLoadClientPolicy, const String& host, const String& bundleIdentifier, const String& versionString)
{
    WebCore::platformStrategies()->pluginStrategy()->setPluginLoadClientPolicy(pluginLoadClientPolicy, host, bundleIdentifier, versionString);
}

void WebPluginInfoProvider::clearPluginClientPolicies()
{
    WebCore::platformStrategies()->pluginStrategy()->clearPluginClientPolicies();
}
#endif

}
