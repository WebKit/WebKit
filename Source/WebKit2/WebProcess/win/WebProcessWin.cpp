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

#include "config.h"
#include "WebProcess.h"

#include "WebCookieManager.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/FileSystem.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/PageCache.h>
#include <WebCore/Settings.h>
#include <wtf/text/WTFString.h>

#if USE(CFNETWORK)
#include <CFNetwork/CFURLCachePriv.h>
#include <CFNetwork/CFURLProtocolPriv.h>
#include <WebCore/CookieStorageCFNet.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h> 
#include <wtf/RetainPtr.h>
#endif

using namespace WebCore;
using namespace std;

namespace WebKit {

static uint64_t memorySize()
{
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return statex.ullTotalPhys;
}

static uint64_t volumeFreeSize(CFStringRef cfstringPath)
{
    WTF::String path(cfstringPath);
    ULARGE_INTEGER freeBytesToCaller;
    BOOL result = GetDiskFreeSpaceExW((LPCWSTR)path.charactersWithNullTermination(), &freeBytesToCaller, 0, 0);
    if (!result)
        return 0;
    return freeBytesToCaller.QuadPart;
}

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
#if USE(CFNETWORK)
    RetainPtr<CFStringRef> cfurlCacheDirectory(AdoptCF, wkCopyFoundationCacheDirectory());
    if (!cfurlCacheDirectory)
        cfurlCacheDirectory.adoptCF(WebCore::localUserSpecificStorageDirectory().createCFString());

    // As a fudge factor, use 1000 instead of 1024, in case the reported byte 
    // count doesn't align exactly to a megabyte boundary.
    uint64_t memSize = memorySize() / 1024 / 1000;
    uint64_t diskFreeSize = volumeFreeSize(cfurlCacheDirectory.get()) / 1024 / 1000;

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    double deadDecodedDataDeletionInterval = 0;
    unsigned pageCacheCapacity = 0;
    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
        pageCacheCapacity, urlCacheMemoryCapacity, urlCacheDiskCapacity);

    memoryCache()->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache()->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    pageCache()->setCapacity(pageCacheCapacity);

    RetainPtr<CFURLCacheRef> cfurlCache(AdoptCF, CFURLCacheCopySharedURLCache());
    CFURLCacheSetMemoryCapacity(cfurlCache.get(), urlCacheMemoryCapacity);
    CFURLCacheSetDiskCapacity(cfurlCache.get(), max<unsigned long>(urlCacheDiskCapacity, CFURLCacheDiskCapacity(cfurlCache.get()))); // Don't shrink a big disk cache, since that would cause churn.
#endif
}

void WebProcess::platformClearResourceCaches(ResourceCachesToClear cachesToClear)
{
#if USE(CFNETWORK)
    if (cachesToClear == InMemoryResourceCachesOnly)
        return;
    CFURLCacheRemoveAllCachedResponses(RetainPtr<CFURLCacheRef>(AdoptCF, CFURLCacheCopySharedURLCache()).get());
#endif
}

void WebProcess::platformInitializeWebProcess(const WebProcessCreationParameters& parameters, CoreIPC::ArgumentDecoder*)
{
    setShouldPaintNativeControls(parameters.shouldPaintNativeControls);

#if USE(CFNETWORK)
    RetainPtr<CFStringRef> cachePath(AdoptCF, parameters.cfURLCachePath.createCFString());
    if (!cachePath)
        return;

    CFIndex cacheDiskCapacity = parameters.cfURLCacheDiskCapacity;
    CFIndex cacheMemoryCapacity = parameters.cfURLCacheMemoryCapacity;
    RetainPtr<CFURLCacheRef> uiProcessCache(AdoptCF, CFURLCacheCreate(kCFAllocatorDefault, cacheMemoryCapacity, cacheDiskCapacity, cachePath.get()));
    CFURLCacheSetSharedURLCache(uiProcessCache.get());
#endif

    WebCookieManager::shared().setHTTPCookieAcceptPolicy(parameters.initialHTTPCookieAcceptPolicy);
}

void WebProcess::platformTerminate()
{
}

void WebProcess::setShouldPaintNativeControls(bool shouldPaintNativeControls)
{
#if USE(SAFARI_THEME)
    Settings::setShouldPaintNativeControls(shouldPaintNativeControls);
#endif
}

} // namespace WebKit
