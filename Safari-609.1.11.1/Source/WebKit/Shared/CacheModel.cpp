/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "CacheModel.h"

#include <algorithm>
#include <wtf/RAMSize.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {

void calculateMemoryCacheSizes(CacheModel cacheModel, unsigned& cacheTotalCapacity, unsigned& cacheMinDeadCapacity, unsigned& cacheMaxDeadCapacity, Seconds& deadDecodedDataDeletionInterval, unsigned& backForwardCacheCapacity)
{
    uint64_t memorySize = ramSize() / MB;

    switch (cacheModel) {
    case CacheModel::DocumentViewer: {
        // back/forward cache capacity (in pages)
        backForwardCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memorySize >= 2048)
            cacheTotalCapacity = 96 * MB;
        else if (memorySize >= 1536)
            cacheTotalCapacity = 64 * MB;
        else if (memorySize >= 1024)
            cacheTotalCapacity = 32 * MB;
        else if (memorySize >= 512)
            cacheTotalCapacity = 16 * MB;
        else
            cacheTotalCapacity = 8 * MB;

        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;

        break;
    }
    case CacheModel::DocumentBrowser: {
        // back/forward cache capacity (in pages)
        if (memorySize >= 512)
            backForwardCacheCapacity = 2;
        else if (memorySize >= 256)
            backForwardCacheCapacity = 1;
        else
            backForwardCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memorySize >= 2048)
            cacheTotalCapacity = 96 * MB;
        else if (memorySize >= 1536)
            cacheTotalCapacity = 64 * MB;
        else if (memorySize >= 1024)
            cacheTotalCapacity = 32 * MB;
        else if (memorySize >= 512)
            cacheTotalCapacity = 16 * MB;
        else
            cacheTotalCapacity = 8 * MB;

        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;

        break;
    }
    case CacheModel::PrimaryWebBrowser: {
        // back/forward cache capacity (in pages)
        if (memorySize >= 512)
            backForwardCacheCapacity = 2;
        else if (memorySize >= 256)
            backForwardCacheCapacity = 1;
        else
            backForwardCacheCapacity = 0;

        // Object cache capacities (in bytes)
        // (Testing indicates that value / MB depends heavily on content and
        // browsing pattern. Even growth above 128MB can have substantial
        // value / MB for some content / browsing patterns.)
        if (memorySize >= 2048)
            cacheTotalCapacity = 128 * MB;
        else if (memorySize >= 1536)
            cacheTotalCapacity = 96 * MB;
        else if (memorySize >= 1024)
            cacheTotalCapacity = 64 * MB;
        else if (memorySize >= 512)
            cacheTotalCapacity = 32 * MB;
        else
            cacheTotalCapacity = 16 * MB;

        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;

        // This code is here to avoid a PLT regression. We can remove it if we
        // can prove that the overall system gain would justify the regression.
        cacheMaxDeadCapacity = std::max(24u, cacheMaxDeadCapacity);

        deadDecodedDataDeletionInterval = 60_s;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };
}

void calculateURLCacheSizes(CacheModel cacheModel, uint64_t diskFreeSize, unsigned& urlCacheMemoryCapacity, uint64_t& urlCacheDiskCapacity)
{
    switch (cacheModel) {
    case CacheModel::DocumentViewer: {
        // Foundation memory cache capacity (in bytes)
        urlCacheMemoryCapacity = 0;

        // Disk cache capacity (in bytes)
        urlCacheDiskCapacity = 0;

        break;
    }
    case CacheModel::DocumentBrowser: {
        uint64_t memorySize = ramSize() / MB;

        // Foundation memory cache capacity (in bytes)
        if (memorySize >= 2048)
            urlCacheMemoryCapacity = 4 * MB;
        else if (memorySize >= 1024)
            urlCacheMemoryCapacity = 2 * MB;
        else if (memorySize >= 512)
            urlCacheMemoryCapacity = 1 * MB;
        else
            urlCacheMemoryCapacity = 512 * KB;

        // Disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            urlCacheDiskCapacity = 75 * MB;
        else if (diskFreeSize >= 8192)
            urlCacheDiskCapacity = 40 * MB;
        else if (diskFreeSize >= 4096)
            urlCacheDiskCapacity = 30 * MB;
        else
            urlCacheDiskCapacity = 20 * MB;

        break;
    }
    case CacheModel::PrimaryWebBrowser: {
        uint64_t memorySize = ramSize() / MB;

#if PLATFORM(IOS_FAMILY)
        if (memorySize >= 1024)
            urlCacheMemoryCapacity = 16 * MB;
        else
            urlCacheMemoryCapacity = 8 * MB;
#else
        // Foundation memory cache capacity (in bytes)
        // (These values are small because WebCore does most caching itself.)
        if (memorySize >= 1024)
            urlCacheMemoryCapacity = 4 * MB;
        else if (memorySize >= 512)
            urlCacheMemoryCapacity = 2 * MB;
        else if (memorySize >= 256)
            urlCacheMemoryCapacity = 1 * MB;
        else
            urlCacheMemoryCapacity = 512 * KB;
#endif

        // Disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            urlCacheDiskCapacity = 500 * MB;
        else if (diskFreeSize >= 8192)
            urlCacheDiskCapacity = 250 * MB;
        else if (diskFreeSize >= 4096)
            urlCacheDiskCapacity = 125 * MB;
        else if (diskFreeSize >= 2048)
            urlCacheDiskCapacity = 100 * MB;
        else if (diskFreeSize >= 1024)
            urlCacheDiskCapacity = 75 * MB;
        else
            urlCacheDiskCapacity = 50 * MB;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };
}

} // namespace WebKit
