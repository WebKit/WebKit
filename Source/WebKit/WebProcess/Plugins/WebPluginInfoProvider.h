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

#pragma once

#include <WebCore/PluginInfoProvider.h>
#include <wtf/HashMap.h>

namespace WebKit {

class WebPluginInfoProvider final : public WebCore::PluginInfoProvider {
    friend NeverDestroyed<WebPluginInfoProvider>;

public:
    static WebPluginInfoProvider& singleton();
    virtual ~WebPluginInfoProvider();

#if PLATFORM(MAC)
    void setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy, const String& host, const String& bundleIdentifier, const String& versionString);
    void clearPluginClientPolicies();
#endif

private:
    WebPluginInfoProvider();

    Vector<WebCore::PluginInfo> pluginInfo(WebCore::Page&, std::optional<Vector<WebCore::SupportedPluginIdentifier>>&) final;
    Vector<WebCore::PluginInfo> webVisiblePluginInfo(WebCore::Page&, const URL&) final;
    void refreshPlugins() override;

#if ENABLE(NETSCAPE_PLUGIN_API)
    void populatePluginCache(const WebCore::Page&);
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if PLATFORM(MAC)
    std::optional<WebCore::PluginLoadClientPolicy> pluginLoadClientPolicyForHost(const String&, const WebCore::PluginInfo&) const;
    String longestMatchedWildcardHostForHost(const String& host) const;
    bool replaceHostWithMatchedWildcardHost(String& host, const String& identifier) const;

    typedef HashMap<String, WebCore::PluginLoadClientPolicy> PluginLoadClientPoliciesByBundleVersion;
    typedef HashMap<String, PluginLoadClientPoliciesByBundleVersion> PluginPolicyMapsByIdentifier;
    HashMap<String, PluginPolicyMapsByIdentifier> m_hostsToPluginIdentifierData;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    bool m_pluginCacheIsPopulated { false };
    bool m_shouldRefreshPlugins { false };
    Vector<WebCore::PluginInfo> m_cachedPlugins;
    Vector<WebCore::PluginInfo> m_cachedApplicationPlugins;
    std::optional<Vector<WebCore::SupportedPluginIdentifier>> m_cachedSupportedPluginIdentifiers;
#endif
};

}
