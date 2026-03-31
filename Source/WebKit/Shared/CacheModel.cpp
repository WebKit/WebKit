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
        // back/forward cache capacity (in pages)
        backForwardCacheCapacity = 0;
        cacheTotalCapacity = 0 * MB;
        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;
        deadDecodedDataDeletionInterval = 1_s;

}

uint64_t calculateURLCacheDiskCapacity(CacheModel cacheModel, uint64_t diskFreeSize)
{
    uint64_t urlCacheDiskCapacity;

    switch (cacheModel) {
    case CacheModel::DocumentViewer: {
        // Disk cache capacity (in bytes)
        urlCacheDiskCapacity = 0;

        break;
    }
    case CacheModel::DocumentBrowser: {
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
        // Disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            urlCacheDiskCapacity = 1 * GB;
        else if (diskFreeSize >= 8192)
            urlCacheDiskCapacity = 500 * MB;
        else if (diskFreeSize >= 4096)
            urlCacheDiskCapacity = 250 * MB;
        else if (diskFreeSize >= 2048)
            urlCacheDiskCapacity = 200 * MB;
        else if (diskFreeSize >= 1024)
            urlCacheDiskCapacity = 150 * MB;
        else
            urlCacheDiskCapacity = 100 * MB;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };

    return urlCacheDiskCapacity;
}

} // namespace WebKit
