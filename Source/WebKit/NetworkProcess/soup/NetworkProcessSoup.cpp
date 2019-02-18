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
#include "ResourceCachesToClear.h"
#include "WebCookieManager.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
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
            builder.appendLiteral(",");

        builder.append(languages[i]);

        int quality = 100 - i * delta;
        if (quality > 0 && quality < 100) {
            builder.appendLiteral(";q=");
            char buffer[8];
            g_ascii_formatd(buffer, 8, "%.2f", quality / 100.0);
            builder.append(buffer);
        }
    }

    return builder.toString().utf8();
}

void NetworkProcess::userPreferredLanguagesChanged(const Vector<String>& languages)
{
    auto acceptLanguages = buildAcceptLanguages(languages);
    SoupNetworkSession::setInitialAcceptLanguages(acceptLanguages);
    forEachNetworkStorageSession([&acceptLanguages](const auto& session) {
        session.soupNetworkSession().setAcceptLanguages(acceptLanguages);
    });
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    if (parameters.proxySettings.mode != SoupNetworkProxySettings::Mode::Default)
        setNetworkProxySettings(parameters.proxySettings);

    ASSERT(!parameters.diskCacheDirectory.isEmpty());
    m_diskCacheDirectory = parameters.diskCacheDirectory;

    SoupNetworkSession::clearOldSoupCache(FileSystem::directoryName(m_diskCacheDirectory));

    OptionSet<NetworkCache::Cache::Option> cacheOptions { NetworkCache::Cache::Option::RegisterNotify };
    if (parameters.shouldEnableNetworkCacheEfficacyLogging)
        cacheOptions.add(NetworkCache::Cache::Option::EfficacyLogging);
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (parameters.shouldEnableNetworkCacheSpeculativeRevalidation)
        cacheOptions.add(NetworkCache::Cache::Option::SpeculativeRevalidation);
#endif

    m_cache = NetworkCache::Cache::open(*this, m_diskCacheDirectory, cacheOptions);

    supplement<WebCookieManager>()->setHTTPCookieAcceptPolicy(parameters.cookieAcceptPolicy, OptionalCallbackID());

    if (!parameters.languages.isEmpty())
        userPreferredLanguagesChanged(parameters.languages);

    setIgnoreTLSErrors(parameters.ignoreTLSErrors);
}

std::unique_ptr<WebCore::NetworkStorageSession> NetworkProcess::platformCreateDefaultStorageSession() const
{
    return std::make_unique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID(), std::make_unique<SoupNetworkSession>(PAL::SessionID::defaultSessionID()));
}

void NetworkProcess::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    SoupNetworkSession::setShouldIgnoreTLSErrors(ignoreTLSErrors);
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    SoupNetworkSession::allowSpecificHTTPSCertificateForHost(certificateInfo, host);
}

void NetworkProcess::clearCacheForAllOrigins(uint32_t cachesToClear)
{
    if (cachesToClear == InMemoryResourceCachesOnly)
        return;

    clearDiskCache(-WallTime::infinity(), [] { });
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    if (!m_cache) {
        completionHandler();
        return;
    }
    m_cache->clear(modifiedSince, WTFMove(completionHandler));
}

void NetworkProcess::platformTerminate()
{
    notImplemented();
}

void NetworkProcess::setNetworkProxySettings(const SoupNetworkProxySettings& settings)
{
    SoupNetworkSession::setProxySettings(settings);
    forEachNetworkStorageSession([](const auto& session) {
        session.soupNetworkSession().setupProxy();
    });
}

void NetworkProcess::platformPrepareToSuspend(CompletionHandler<void()>&& completionHandler)
{
    notImplemented();
    completionHandler();
}

void NetworkProcess::platformProcessDidResume()
{
    notImplemented();
}

void NetworkProcess::platformProcessDidTransitionToForeground()
{
    notImplemented();
}

void NetworkProcess::platformProcessDidTransitionToBackground()
{
    notImplemented();
}

} // namespace WebKit
