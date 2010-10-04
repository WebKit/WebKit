/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebProcess.h"

#include <WebCore/Cache.h>
#include <WebCore/PageCache.h>
#include <WebKitSystemInterface.h>
#include <algorithm>

using namespace WebCore;
using namespace std;

namespace WebKit {

static uint64_t memorySize()
{
    static host_basic_info_data_t hostInfo;

    static dispatch_once_t once;
    dispatch_once(&once, ^() {
        mach_port_t host = mach_host_self();
        mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
        kern_return_t r = host_info(host, HOST_BASIC_INFO, (host_info_t)&hostInfo, &count);
        mach_port_deallocate(mach_task_self(), host);

        if (r != KERN_SUCCESS)
            LOG_ERROR("%s : host_info(%d) : %s.\n", __FUNCTION__, r, mach_error_string(r));
    });

    return hostInfo.max_mem;
}

static unsigned long long volumeFreeSize(NSString *path)
{
    NSDictionary *fileSystemAttributesDictionary = [[NSFileManager defaultManager] attributesOfFileSystemForPath:path error:NULL];
    return [[fileSystemAttributesDictionary objectForKey:NSFileSystemFreeSize] unsignedLongLongValue];
}

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
    RetainPtr<NSString> nsurlCacheDirectory(AdoptNS, (NSString *)WKCopyFoundationCacheDirectory());
    if (!nsurlCacheDirectory)
        nsurlCacheDirectory.adoptNS(NSHomeDirectory());

    // As a fudge factor, use 1000 instead of 1024, in case the reported byte 
    // count doesn't align exactly to a megabyte boundary.
    uint64_t memSize = memorySize() / 1024 / 1000;
    unsigned long long diskFreeSize = volumeFreeSize(nsurlCacheDirectory.get()) / 1024 / 1000;
    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    double deadDecodedDataDeletionInterval = 0;

    unsigned pageCacheCapacity = 0;

    NSUInteger nsurlCacheMemoryCapacity = 0;
    NSUInteger nsurlCacheDiskCapacity = 0;

    switch (cacheModel) {
    case CacheModelDocumentViewer: {
        // Page cache capacity (in pages)
        pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;

        // Foundation memory cache capacity (in bytes)
        nsurlCacheMemoryCapacity = 0;

        // Foundation disk cache capacity (in bytes)
        nsurlCacheDiskCapacity = [nsurlCache diskCapacity];

        break;
    }
    case CacheModelDocumentBrowser: {
        // Page cache capacity (in pages)
        if (memSize >= 1024)
            pageCacheCapacity = 3;
        else if (memSize >= 512)
            pageCacheCapacity = 2;
        else if (memSize >= 256)
            pageCacheCapacity = 1;
        else
            pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;

        // Foundation memory cache capacity (in bytes)
        if (memSize >= 2048)
            nsurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 512)
            nsurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            nsurlCacheDiskCapacity = 50 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            nsurlCacheDiskCapacity = 40 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            nsurlCacheDiskCapacity = 30 * 1024 * 1024;
        else
            nsurlCacheDiskCapacity = 20 * 1024 * 1024;

        break;
    }
    case CacheModelPrimaryWebBrowser: {
        // Page cache capacity (in pages)
        // (Research indicates that value / page drops substantially after 3 pages.)
        if (memSize >= 2048)
            pageCacheCapacity = 5;
        else if (memSize >= 1024)
            pageCacheCapacity = 4;
        else if (memSize >= 512)
            pageCacheCapacity = 3;
        else if (memSize >= 256)
            pageCacheCapacity = 2;
        else
            pageCacheCapacity = 1;

        // Object cache capacities (in bytes)
        // (Testing indicates that value / MB depends heavily on content and
        // browsing pattern. Even growth above 128MB can have substantial 
        // value / MB for some content / browsing patterns.)
        if (memSize >= 2048)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 32 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;

        // This code is here to avoid a PLT regression. We can remove it if we
        // can prove that the overall system gain would justify the regression.
        cacheMaxDeadCapacity = max(24u, cacheMaxDeadCapacity);

        deadDecodedDataDeletionInterval = 60;

        // Foundation memory cache capacity (in bytes)
        // (These values are small because WebCore does most caching itself.)
        if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 512)
            nsurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 256)
            nsurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            nsurlCacheDiskCapacity = 175 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            nsurlCacheDiskCapacity = 150 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            nsurlCacheDiskCapacity = 125 * 1024 * 1024;
        else if (diskFreeSize >= 2048)
            nsurlCacheDiskCapacity = 100 * 1024 * 1024;
        else if (diskFreeSize >= 1024)
            nsurlCacheDiskCapacity = 75 * 1024 * 1024;
        else
            nsurlCacheDiskCapacity = 50 * 1024 * 1024;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };

    // Don't shrink a big disk cache, since that would cause churn.
    nsurlCacheDiskCapacity = max(nsurlCacheDiskCapacity, [nsurlCache diskCapacity]);

    cache()->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    cache()->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    pageCache()->setCapacity(pageCacheCapacity);
    [nsurlCache setMemoryCapacity:nsurlCacheMemoryCapacity];
    [nsurlCache setDiskCapacity:nsurlCacheDiskCapacity];
}

} // namespace WebKit
