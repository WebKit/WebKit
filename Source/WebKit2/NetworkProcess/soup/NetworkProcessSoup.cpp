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
#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcess.h"

#include "NetworkProcessCreationParameters.h"
#include "ResourceCachesToClear.h"
#include "WebCookieManager.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/FileSystem.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/gobject/GUniquePtr.h>

using namespace WebCore;

namespace WebKit {

static uint64_t getCacheDiskFreeSize(SoupCache* cache)
{
    ASSERT(cache);

    GUniqueOutPtr<char> cacheDir;
    g_object_get(G_OBJECT(cache), "cache-dir", &cacheDir.outPtr(), NULL);
    if (!cacheDir)
        return 0;

    return WebCore::getVolumeFreeSizeForPath(cacheDir.get());
}

static uint64_t getMemorySize()
{
    static uint64_t kDefaultMemorySize = 512;
#if !OS(WINDOWS)
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize == -1)
        return kDefaultMemorySize;

    long physPages = sysconf(_SC_PHYS_PAGES);
    if (physPages == -1)
        return kDefaultMemorySize;

    return ((pageSize / 1024) * physPages) / 1024;
#else
    // Fallback to default for other platforms.
    return kDefaultMemorySize;
#endif
}

void NetworkProcess::userPreferredLanguagesChanged(const Vector<String>& languages)
{
    SoupNetworkSession::defaultSession().setAcceptLanguages(languages);
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    ASSERT(!parameters.diskCacheDirectory.isEmpty());
    GRefPtr<SoupCache> soupCache = adoptGRef(soup_cache_new(parameters.diskCacheDirectory.utf8().data(), SOUP_CACHE_SINGLE_USER));
    SoupNetworkSession::defaultSession().setCache(soupCache.get());
    // Set an initial huge max_size for the SoupCache so the call to soup_cache_load() won't evict any cached
    // resource. The final size of the cache will be set by NetworkProcess::platformSetCacheModel().
    unsigned initialMaxSize = soup_cache_get_max_size(soupCache.get());
    soup_cache_set_max_size(soupCache.get(), G_MAXUINT);
    soup_cache_load(soupCache.get());
    soup_cache_set_max_size(soupCache.get(), initialMaxSize);

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
    double deadDecodedDataDeletionInterval = 0;
    unsigned pageCacheCapacity = 0;

    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

    SoupCache* cache = SoupNetworkSession::defaultSession().cache();
    uint64_t diskFreeSize = getCacheDiskFreeSize(cache) / 1024 / 1024;

    uint64_t memSize = getMemorySize();
    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
        pageCacheCapacity, urlCacheMemoryCapacity, urlCacheDiskCapacity);

    if (urlCacheDiskCapacity > soup_cache_get_max_size(cache))
        soup_cache_set_max_size(cache, urlCacheDiskCapacity);
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

    soup_cache_clear(SoupNetworkSession::defaultSession().cache());
}

void NetworkProcess::platformTerminate()
{
    notImplemented();
}

} // namespace WebKit

#endif
