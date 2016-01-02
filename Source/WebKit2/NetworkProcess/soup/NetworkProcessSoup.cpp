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
#include <WebCore/FileSystem.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/RAMSize.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebCore;

namespace WebKit {

#if !ENABLE(NETWORK_CACHE)
static uint64_t getCacheDiskFreeSize(SoupCache* cache)
{
    ASSERT(cache);

    GUniqueOutPtr<char> cacheDir;
    g_object_get(G_OBJECT(cache), "cache-dir", &cacheDir.outPtr(), NULL);
    if (!cacheDir)
        return 0;

    return WebCore::getVolumeFreeSizeForPath(cacheDir.get());
}
#endif

void NetworkProcess::userPreferredLanguagesChanged(const Vector<String>& languages)
{
    SoupNetworkSession::defaultSession().setAcceptLanguages(languages);
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    ASSERT(!parameters.diskCacheDirectory.isEmpty());
    m_diskCacheDirectory = parameters.diskCacheDirectory;

#if ENABLE(NETWORK_CACHE)
    // Clear the old soup cache if it exists.
    SoupNetworkSession::defaultSession().clearCache(WebCore::directoryName(m_diskCacheDirectory));

    NetworkCache::singleton().initialize(m_diskCacheDirectory, { parameters.shouldEnableNetworkCacheEfficacyLogging });
#else
    // We used to use the given cache directory for the soup cache, but now we use a subdirectory to avoid
    // conflicts with other cache files in the same directory. Remove the old cache files if they still exist.
    SoupNetworkSession::defaultSession().clearCache(WebCore::directoryName(m_diskCacheDirectory));

    GRefPtr<SoupCache> soupCache = adoptGRef(soup_cache_new(m_diskCacheDirectory.utf8().data(), SOUP_CACHE_SINGLE_USER));
    SoupNetworkSession::defaultSession().setCache(soupCache.get());
    // Set an initial huge max_size for the SoupCache so the call to soup_cache_load() won't evict any cached
    // resource. The final size of the cache will be set by NetworkProcess::platformSetCacheModel().
    unsigned initialMaxSize = soup_cache_get_max_size(soupCache.get());
    soup_cache_set_max_size(soupCache.get(), G_MAXUINT);
    soup_cache_load(soupCache.get());
    soup_cache_set_max_size(soupCache.get(), initialMaxSize);
#endif

    if (!parameters.cookiePersistentStoragePath.isEmpty()) {
        supplement<WebCookieManager>()->setCookiePersistentStorage(parameters.cookiePersistentStoragePath,
            parameters.cookiePersistentStorageType);
    }
    supplement<WebCookieManager>()->setHTTPCookieAcceptPolicy(parameters.cookieAcceptPolicy);

    if (!parameters.languages.isEmpty())
        userPreferredLanguagesChanged(parameters.languages);

    setIgnoreTLSErrors(parameters.ignoreTLSErrors);
}

void NetworkProcess::platformSetCacheModel(CacheModel cacheModel)
{
    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    auto deadDecodedDataDeletionInterval = std::chrono::seconds { 0 };
    unsigned pageCacheCapacity = 0;

    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

#if ENABLE(NETWORK_CACHE)
    uint64_t diskFreeSize = WebCore::getVolumeFreeSizeForPath(m_diskCacheDirectory.utf8().data()) / WTF::MB;
#else
    SoupCache* cache = SoupNetworkSession::defaultSession().cache();
    uint64_t diskFreeSize = getCacheDiskFreeSize(cache) / WTF::MB;
#endif

    uint64_t memSize = WTF::ramSize() / WTF::MB;
    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
        pageCacheCapacity, urlCacheMemoryCapacity, urlCacheDiskCapacity);

#if ENABLE(NETWORK_CACHE)
    auto& networkCache = NetworkCache::singleton();
    if (networkCache.isEnabled())
        networkCache.setCapacity(urlCacheDiskCapacity);
#else
    if (urlCacheDiskCapacity > soup_cache_get_max_size(cache))
        soup_cache_set_max_size(cache, urlCacheDiskCapacity);
#endif
}

void NetworkProcess::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    ResourceHandle::setIgnoreSSLErrors(ignoreTLSErrors);
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    ResourceHandle::setClientCertificate(host, certificateInfo.certificate());
}

void NetworkProcess::clearCacheForAllOrigins(uint32_t cachesToClear)
{
    if (cachesToClear == InMemoryResourceCachesOnly)
        return;

    clearDiskCache(std::chrono::system_clock::time_point::min(), [] { });
}

void NetworkProcess::clearDiskCache(std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler)
{
#if ENABLE(NETWORK_CACHE)
    NetworkCache::singleton().clear(modifiedSince, WTFMove(completionHandler));
#else
    UNUSED_PARAM(modifiedSince);
    UNUSED_PARAM(completionHandler);
    soup_cache_clear(SoupNetworkSession::defaultSession().cache());
#endif
}

void NetworkProcess::platformTerminate()
{
    notImplemented();
}

} // namespace WebKit
