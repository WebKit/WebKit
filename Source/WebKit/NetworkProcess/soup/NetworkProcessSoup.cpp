/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Comapny 100 Inc.
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
#include "NetworkProcess.h"

#include "NetworkCache.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSessionSoup.h"
#include "WebCookieManager.h"
#include "WebKitCachedResolver.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/FileSystem.h>
#include <wtf/RAMSize.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

static CString buildAcceptLanguages(const Vector<String>& languages)
{
    size_t languagesCount = languages.size();

    // Ignore "C" locale.
    size_t cLocalePosition = languages.find("c");
    if (cLocalePosition != notFound)
        languagesCount--;

    // Fallback to "en" if the list is empty.
    if (!languagesCount)
        return "en";

    // Calculate deltas for the quality values.
    int delta;
    if (languagesCount < 10)
        delta = 10;
    else if (languagesCount < 20)
        delta = 5;
    else
        delta = 1;

    // Set quality values for each language.
    StringBuilder builder;
    for (size_t i = 0; i < languages.size(); ++i) {
        if (i == cLocalePosition)
            continue;

        if (i)
            builder.append(",");

        builder.append(languages[i]);

        int quality = 100 - i * delta;
        if (quality > 0 && quality < 100) {
            char buffer[8];
            g_ascii_formatd(buffer, 8, "%.2f", quality / 100.0);
            builder.append(";q=", buffer);
        }
    }

    return builder.toString().utf8();
}

HashSet<String> NetworkProcess::hostNamesWithHSTSCache(PAL::SessionID sessionID) const
{
    HashSet<String> hostNames;
    const auto* session = static_cast<NetworkSessionSoup*>(networkSession(sessionID));
    session->soupNetworkSession().getHostNamesWithHSTSCache(hostNames);
    return hostNames;
}

void NetworkProcess::deleteHSTSCacheForHostNames(PAL::SessionID sessionID, const Vector<String>& hostNames)
{
    const auto* session = static_cast<NetworkSessionSoup*>(networkSession(sessionID));
    session->soupNetworkSession().deleteHSTSCacheForHostNames(hostNames);
}

void NetworkProcess::clearHSTSCache(PAL::SessionID sessionID, WallTime modifiedSince)
{
    const auto* session = static_cast<NetworkSessionSoup*>(networkSession(sessionID));
    session->soupNetworkSession().clearHSTSCache(modifiedSince);
}

void NetworkProcess::userPreferredLanguagesChanged(const Vector<String>& languages)
{
    auto acceptLanguages = buildAcceptLanguages(languages);
    SoupNetworkSession::setInitialAcceptLanguages(acceptLanguages);
    forEachNetworkSession([&acceptLanguages](const auto& session) {
        static_cast<const NetworkSessionSoup&>(session).soupNetworkSession().setAcceptLanguages(acceptLanguages);
    });
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    GRefPtr<GResolver> cachedResolver = adoptGRef(webkitCachedResolverNew(adoptGRef(g_resolver_get_default())));
    g_resolver_set_default(cachedResolver.get());

    m_cacheOptions = { NetworkCache::CacheOption::RegisterNotify };
    supplement<WebCookieManager>()->setHTTPCookieAcceptPolicy(parameters.cookieAcceptPolicy, []() { });

    if (!parameters.languages.isEmpty())
        userPreferredLanguagesChanged(parameters.languages);

#if ENABLE(PERIODIC_MEMORY_MONITOR)
    // The periodic memory monitor is disabled by default in the network process. Enable
    // it only if MemoryPressureHandler is not suppressed and there is a custom configuration
    // for it.
    if (!parameters.shouldSuppressMemoryPressureHandler && parameters.memoryPressureHandlerConfiguration) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setConfiguration(*parameters.memoryPressureHandlerConfiguration);
        memoryPressureHandler.setShouldUsePeriodicMemoryMonitor(true);
        memoryPressureHandler.setMemoryKillCallback([this] () {
            parentProcessConnection()->send(Messages::NetworkProcessProxy::DidExceedMemoryLimit(), 0);
        });
    }
#endif
}

void NetworkProcess::setIgnoreTLSErrors(PAL::SessionID sessionID, bool ignoreTLSErrors)
{
    if (auto* session = networkSession(sessionID))
        static_cast<NetworkSessionSoup&>(*session).setIgnoreTLSErrors(ignoreTLSErrors);
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    SoupNetworkSession::allowSpecificHTTPSCertificateForHost(certificateInfo, host);
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachNetworkSession([modifiedSince, &aggregator](NetworkSession& session) {
        if (auto* cache = session.cache())
            cache->clear(modifiedSince, [aggregator] () { });
    });
}

void NetworkProcess::platformTerminate()
{
    notImplemented();
}

void NetworkProcess::setNetworkProxySettings(PAL::SessionID sessionID, SoupNetworkProxySettings&& settings)
{
    if (auto* session = networkSession(sessionID))
        static_cast<NetworkSessionSoup&>(*session).setProxySettings(settings);
}

void NetworkProcess::setPersistentCredentialStorageEnabled(PAL::SessionID sessionID, bool enabled)
{
    if (auto* session = networkSession(sessionID))
        static_cast<NetworkSessionSoup&>(*session).setPersistentCredentialStorageEnabled(enabled);
}

} // namespace WebKit
