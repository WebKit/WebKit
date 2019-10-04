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
#include <WebCore/SubframeLoader.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(MAC)
#include <WebCore/StringUtilities.h>
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

#if PLATFORM(MAC)
void WebPluginInfoProvider::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy clientPolicy, const String& host, const String& bundleIdentifier, const String& versionString)
{
    String hostToSet = host.isNull() || !host.length() ? "*" : host;
    String bundleIdentifierToSet = bundleIdentifier.isNull() || !bundleIdentifier.length() ? "*" : bundleIdentifier;
    String versionStringToSet = versionString.isNull() || !versionString.length() ? "*" : versionString;

    PluginPolicyMapsByIdentifier policiesByIdentifier;
    if (m_hostsToPluginIdentifierData.contains(hostToSet))
        policiesByIdentifier = m_hostsToPluginIdentifierData.get(hostToSet);

    PluginLoadClientPoliciesByBundleVersion versionsToPolicies;
    if (policiesByIdentifier.contains(bundleIdentifierToSet))
        versionsToPolicies = policiesByIdentifier.get(bundleIdentifierToSet);

    versionsToPolicies.set(versionStringToSet, clientPolicy);
    policiesByIdentifier.set(bundleIdentifierToSet, WTFMove(versionsToPolicies));
    m_hostsToPluginIdentifierData.set(hostToSet, policiesByIdentifier);

    clearPagesPluginData();
}

void WebPluginInfoProvider::clearPluginClientPolicies()
{
    m_hostsToPluginIdentifierData.clear();
    clearPagesPluginData();
}
#endif

void WebPluginInfoProvider::refreshPlugins()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_cachedPlugins.clear();
    m_pluginCacheIsPopulated = false;
    m_shouldRefreshPlugins = true;
#endif
}

Vector<PluginInfo> WebPluginInfoProvider::pluginInfo(Page& page, Optional<Vector<SupportedPluginIdentifier>>& supportedPluginIdentifiers)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    populatePluginCache(page);

    if (m_cachedSupportedPluginIdentifiers)
        supportedPluginIdentifiers = *m_cachedSupportedPluginIdentifiers;

    return page.mainFrame().loader().subframeLoader().allowPlugins() ? m_cachedPlugins : m_cachedApplicationPlugins;
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(supportedPluginIdentifiers);
    return { };
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

Vector<WebCore::PluginInfo> WebPluginInfoProvider::webVisiblePluginInfo(Page& page, const URL& url)
{
    Optional<Vector<WebCore::SupportedPluginIdentifier>> supportedPluginIdentifiers;
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
        if (info.isApplicationPlugin || info.clientLoadPolicy == WebCore::PluginLoadClientPolicyAsk)
            continue;

        if (info.clientLoadPolicy == WebCore::PluginLoadClientPolicyBlock)
            plugins.remove(i);
    }
#endif
    return plugins;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebPluginInfoProvider::populatePluginCache(const WebCore::Page& page)
{
    if (!m_pluginCacheIsPopulated) {
#if PLATFORM(COCOA)
        // Application plugins are not affected by enablePlugins setting, so we always need to scan plugins to get them.
        bool shouldScanPlugins = true;
#else
        bool shouldScanPlugins = page.mainFrame().loader().subframeLoader().allowPlugins();
#endif
        if (shouldScanPlugins) {
            HangDetectionDisabler hangDetectionDisabler;
            if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::GetPlugins(m_shouldRefreshPlugins),
                Messages::WebProcessProxy::GetPlugins::Reply(m_cachedPlugins, m_cachedApplicationPlugins, m_cachedSupportedPluginIdentifiers), 0))
                return;
        }

        m_shouldRefreshPlugins = false;
        m_pluginCacheIsPopulated = true;
    }

#if PLATFORM(MAC)
    String pageHost = page.mainFrame().loader().documentLoader()->responseURL().host().toString();
    if (pageHost.isNull())
        return;
    for (auto& info : m_cachedPlugins) {
        if (auto clientPolicy = pluginLoadClientPolicyForHost(pageHost, info))
            info.clientLoadPolicy = *clientPolicy;
    }
#else
    UNUSED_PARAM(page);
#endif // not PLATFORM(MAC)
}
#endif

#if PLATFORM(MAC)
Optional<WebCore::PluginLoadClientPolicy> WebPluginInfoProvider::pluginLoadClientPolicyForHost(const String& host, const WebCore::PluginInfo& info) const
{
    String hostToLookUp = host;
    String identifier = info.bundleIdentifier;

    auto policiesByIdentifierIterator = m_hostsToPluginIdentifierData.find(hostToLookUp);

    if (!identifier.isNull() && policiesByIdentifierIterator == m_hostsToPluginIdentifierData.end()) {
        if (!replaceHostWithMatchedWildcardHost(hostToLookUp, identifier))
            hostToLookUp = "*";
        policiesByIdentifierIterator = m_hostsToPluginIdentifierData.find(hostToLookUp);
        if (hostToLookUp != "*" && policiesByIdentifierIterator == m_hostsToPluginIdentifierData.end()) {
            hostToLookUp = "*";
            policiesByIdentifierIterator = m_hostsToPluginIdentifierData.find(hostToLookUp);
        }
    }
    if (policiesByIdentifierIterator == m_hostsToPluginIdentifierData.end())
        return WTF::nullopt;

    auto& policiesByIdentifier = policiesByIdentifierIterator->value;

    if (!identifier)
        identifier = "*";

    auto identifierPolicyIterator = policiesByIdentifier.find(identifier);
    if (identifier != "*" && identifierPolicyIterator == policiesByIdentifier.end()) {
        identifier = "*";
        identifierPolicyIterator = policiesByIdentifier.find(identifier);
    }

    if (identifierPolicyIterator == policiesByIdentifier.end())
        return WTF::nullopt;

    auto& versionsToPolicies = identifierPolicyIterator->value;

    String version = info.versionString;
    if (!version)
        version = "*";
    auto policyIterator = versionsToPolicies.find(version);
    if (version != "*" && policyIterator == versionsToPolicies.end()) {
        version = "*";
        policyIterator = versionsToPolicies.find(version);
    }

    if (policyIterator == versionsToPolicies.end())
        return WTF::nullopt;

    return policyIterator->value;
}

String WebPluginInfoProvider::longestMatchedWildcardHostForHost(const String& host) const
{
    String longestMatchedHost;

    for (auto& key : m_hostsToPluginIdentifierData.keys()) {
        if (key.contains('*') && key != "*" && stringMatchesWildcardString(host, key)) {
            if (key.length() > longestMatchedHost.length())
                longestMatchedHost = key;
            else if (key.length() == longestMatchedHost.length() && codePointCompareLessThan(key, longestMatchedHost))
                longestMatchedHost = key;
        }
    }

    return longestMatchedHost;
}

bool WebPluginInfoProvider::replaceHostWithMatchedWildcardHost(String& host, const String& identifier) const
{
    String matchedWildcardHost = longestMatchedWildcardHostForHost(host);

    if (matchedWildcardHost.isNull())
        return false;

    auto plugInIdentifierData = m_hostsToPluginIdentifierData.find(matchedWildcardHost);
    if (plugInIdentifierData == m_hostsToPluginIdentifierData.end() || !plugInIdentifierData->value.contains(identifier))
        return false;

    host = matchedWildcardHost;
    return true;
}
#endif

}
