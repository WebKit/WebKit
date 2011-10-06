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

#define LIBSOUP_USE_UNSTABLE_REQUEST_API

#include "WebProcessCreationParameters.h"
#include <WebCore/FileSystem.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PageCache.h>
#include <WebCore/ResourceHandle.h>
#include <libsoup/soup-cache.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

namespace WebKit {

static uint64_t getCacheDiskFreeSize(SoupCache* cache)
{
    if (!cache)
        return 0;

    GOwnPtr<char> cacheDir;
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

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    double deadDecodedDataDeletionInterval = 0;
    unsigned pageCacheCapacity = 0;

    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

    SoupSession* session = WebCore::ResourceHandle::defaultSession();
    SoupCache* cache = reinterpret_cast<SoupCache*>(soup_session_get_feature(session, SOUP_TYPE_CACHE));
    uint64_t diskFreeSize = getCacheDiskFreeSize(cache);

    uint64_t memSize = getMemorySize();
    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
                        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
                        pageCacheCapacity, urlCacheMemoryCapacity, urlCacheDiskCapacity);

    WebCore::memoryCache()->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    WebCore::memoryCache()->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    WebCore::pageCache()->setCapacity(pageCacheCapacity);

    if (cache) {
        if (urlCacheDiskCapacity > soup_cache_get_max_size(cache))
            soup_cache_set_max_size(cache, urlCacheDiskCapacity);
    }
}

void WebProcess::platformClearResourceCaches(ResourceCachesToClear)
{
    notImplemented();
}

void WebProcess::platformInitializeWebProcess(const WebProcessCreationParameters&, CoreIPC::ArgumentDecoder*)
{
    notImplemented();
}

void WebProcess::platformTerminate()
{
}

} // namespace WebKit
