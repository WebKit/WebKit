/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebProcess.h"

#if PLATFORM(EFL)
#include "SeccompFiltersWebProcessEfl.h"
#endif

#include "CertificateInfo.h"
#include "WebCookieManager.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/FileSystem.h>
#include <WebCore/Language.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/PageCache.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/RAMSize.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

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

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    auto deadDecodedDataDeletionInterval = std::chrono::seconds { 0 };
    unsigned pageCacheSize = 0;

    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

    uint64_t diskFreeSize = 0;
    SoupCache* cache = nullptr;

    if (!usesNetworkProcess()) {
        cache = WebCore::SoupNetworkSession::defaultSession().cache();
        diskFreeSize = getCacheDiskFreeSize(cache) / WTF::MB;
    }

    uint64_t memSize = WTF::ramSize() / WTF::MB;
    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
                        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
                        pageCacheSize, urlCacheMemoryCapacity, urlCacheDiskCapacity);

    auto& memoryCache = WebCore::MemoryCache::singleton();
    memoryCache.setDisabled(cacheModel == CacheModelDocumentViewer);
    memoryCache.setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache.setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    WebCore::PageCache::singleton().setMaxSize(pageCacheSize);

#if PLATFORM(GTK)
    WebCore::PageCache::singleton().setShouldClearBackingStores(true);
#endif

    if (!usesNetworkProcess()) {
        if (urlCacheDiskCapacity > soup_cache_get_max_size(cache))
            soup_cache_set_max_size(cache, urlCacheDiskCapacity);
    }
}

void WebProcess::platformClearResourceCaches(ResourceCachesToClear cachesToClear)
{
    if (cachesToClear == InMemoryResourceCachesOnly)
        return;

    // If we're using the network process then it is the only one that needs to clear the disk cache.
    if (usesNetworkProcess())
        return;

    soup_cache_clear(WebCore::SoupNetworkSession::defaultSession().cache());
}

static void setSoupSessionAcceptLanguage(const Vector<String>& languages)
{
    WebCore::SoupNetworkSession::defaultSession().setAcceptLanguages(languages);
}

static void languageChanged(void*)
{
    setSoupSessionAcceptLanguage(WebCore::userPreferredLanguages());
}

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters&& parameters)
{
#if ENABLE(SECCOMP_FILTERS)
    {
#if PLATFORM(EFL)
        SeccompFiltersWebProcessEfl seccompFilters(parameters);
#endif
        seccompFilters.initialize();
    }
#endif

    if (usesNetworkProcess())
        return;

    ASSERT(!parameters.diskCacheDirectory.isEmpty());

    // We used to use the given cache directory for the soup cache, but now we use a subdirectory to avoid
    // conflicts with other cache files in the same directory. Remove the old cache files if they still exist.
    WebCore::SoupNetworkSession::defaultSession().clearCache(WebCore::directoryName(parameters.diskCacheDirectory));

#if ENABLE(NETWORK_CACHE)
    // When network cache is enabled, the disk cache directory is the network process one.
    CString diskCachePath = WebCore::pathByAppendingComponent(WebCore::directoryName(parameters.diskCacheDirectory), "webkit").utf8();
#else
    CString diskCachePath = parameters.diskCacheDirectory.utf8();
#endif

    GRefPtr<SoupCache> soupCache = adoptGRef(soup_cache_new(diskCachePath.data(), SOUP_CACHE_SINGLE_USER));
    WebCore::SoupNetworkSession::defaultSession().setCache(soupCache.get());
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
        setSoupSessionAcceptLanguage(parameters.languages);

    setIgnoreTLSErrors(parameters.ignoreTLSErrors);

    WebCore::addLanguageChangeObserver(this, languageChanged);
}

void WebProcess::platformTerminate()
{
    if (!usesNetworkProcess())
        WebCore::removeLanguageChangeObserver(this);
}

void WebProcess::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    ASSERT(!usesNetworkProcess());
    WebCore::ResourceHandle::setIgnoreSSLErrors(ignoreTLSErrors);
}

void WebProcess::allowSpecificHTTPSCertificateForHost(const WebCore::CertificateInfo& certificateInfo, const String& host)
{
    ASSERT(!usesNetworkProcess());
    WebCore::ResourceHandle::setClientCertificate(host, certificateInfo.certificate());
}

} // namespace WebKit
