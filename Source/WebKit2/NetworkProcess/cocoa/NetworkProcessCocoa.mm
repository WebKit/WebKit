/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkProcess.h"

#if ENABLE(NETWORK_PROCESS)

#import "NetworkProcessCreationParameters.h"
#import "NetworkResourceLoader.h"
#import "SandboxExtension.h"
#import <mach/host_info.h>
#import <mach/mach.h>
#import <mach/mach_error.h>

typedef const struct _CFURLCache* CFURLCacheRef;
extern "C" CFURLCacheRef CFURLCacheCopySharedURLCache();
extern "C" void _CFURLCachePurgeMemoryCache(CFURLCacheRef);
extern "C" void CFURLConnectionInvalidateConnectionCache();

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
extern "C" void _CFURLCacheSetMinSizeForVMCachedResource(CFURLCacheRef, CFIndex);
#endif

#if PLATFORM(IOS)
@interface NSURLCache (WKDetails)
-(id)_initWithMemoryCapacity:(NSUInteger)memoryCapacity diskCapacity:(NSUInteger)diskCapacity relativePath:(NSString *)path;
@end
#endif

namespace WebKit {

void NetworkProcess::platformLowMemoryHandler(bool)
{
    CFURLConnectionInvalidateConnectionCache();
    _CFURLCachePurgeMemoryCache(adoptCF(CFURLCacheCopySharedURLCache()).get());
}

void NetworkProcess::platformInitializeNetworkProcessCocoa(const NetworkProcessCreationParameters& parameters)
{
#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.hstsDatabasePathExtensionHandle);
    SandboxExtension::consumePermanently(parameters.parentBundleDirectoryExtensionHandle);
#endif
    m_diskCacheDirectory = parameters.diskCacheDirectory;

    if (!m_diskCacheDirectory.isNull()) {
        SandboxExtension::consumePermanently(parameters.diskCacheDirectoryExtensionHandle);
#if PLATFORM(IOS)
        [NSURLCache setSharedURLCache:adoptNS([[NSURLCache alloc]
            _initWithMemoryCapacity:parameters.nsURLCacheMemoryCapacity
            diskCapacity:parameters.nsURLCacheDiskCapacity
            relativePath:parameters.uiProcessBundleIdentifier]).get()];
#else
        [NSURLCache setSharedURLCache:adoptNS([[NSURLCache alloc]
            initWithMemoryCapacity:parameters.nsURLCacheMemoryCapacity
            diskCapacity:parameters.nsURLCacheDiskCapacity
            diskPath:parameters.diskCacheDirectory]).get()];
#endif
    }

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    RetainPtr<CFURLCacheRef> cache = adoptCF(CFURLCacheCopySharedURLCache());
    if (!cache)
        return;

    _CFURLCacheSetMinSizeForVMCachedResource(cache.get(), NetworkResourceLoader::fileBackedResourceMinimumSize());
#endif
}

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

static uint64_t volumeFreeSize(const String& path)
{
    NSDictionary *fileSystemAttributesDictionary = [[NSFileManager defaultManager] attributesOfFileSystemForPath:(NSString *)path error:NULL];
    return [[fileSystemAttributesDictionary objectForKey:NSFileSystemFreeSize] unsignedLongLongValue];
}

void NetworkProcess::platformSetCacheModel(CacheModel cacheModel)
{
    // As a fudge factor, use 1000 instead of 1024, in case the reported byte
    // count doesn't align exactly to a megabyte boundary.
    uint64_t memSize = memorySize() / 1024 / 1000;
    uint64_t diskFreeSize = volumeFreeSize(m_diskCacheDirectory) / 1024 / 1000;

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    auto deadDecodedDataDeletionInterval = std::chrono::seconds { 0 };
    unsigned pageCacheCapacity = 0;
    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
        pageCacheCapacity, urlCacheMemoryCapacity, urlCacheDiskCapacity);


    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];
    [nsurlCache setMemoryCapacity:urlCacheMemoryCapacity];
    [nsurlCache setDiskCapacity:std::max<unsigned long>(urlCacheDiskCapacity, [nsurlCache diskCapacity])]; // Don't shrink a big disk cache, since that would cause churn.
}

}

#endif
