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

#include "SandboxExtension.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/MemoryCache.h>
#include <WebCore/PageCache.h>
#include <WebKitSystemInterface.h>
#include <algorithm>
#include <dispatch/dispatch.h>
#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#if ENABLE(WEB_PROCESS_SANDBOX)
#include <sandbox.h>
#include <stdlib.h>
#include <sysexits.h>
#endif

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

static uint64_t volumeFreeSize(NSString *path)
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
    uint64_t diskFreeSize = volumeFreeSize(nsurlCacheDirectory.get()) / 1024 / 1000;

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

    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];
    [nsurlCache setMemoryCapacity:urlCacheMemoryCapacity];
    [nsurlCache setDiskCapacity:max<unsigned long>(urlCacheDiskCapacity, [nsurlCache diskCapacity])]; // Don't shrink a big disk cache, since that would cause churn.
}

void WebProcess::platformClearResourceCaches()
{
    [[NSURLCache sharedURLCache] removeAllCachedResponses];
}

static void initializeSandbox(const WebProcessCreationParameters& parameters)
{
#if ENABLE(WEB_PROCESS_SANDBOX)
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"DisableSandbox"]) {
        fprintf(stderr, "Bypassing sandbox due to DisableSandbox user default.\n");
        return;
    }

    char* errorBuf;
    char tmpPath[PATH_MAX];
    char tmpRealPath[PATH_MAX];
    char cachePath[PATH_MAX];
    char cacheRealPath[PATH_MAX];
    const char* frameworkPath = [[[[NSBundle bundleForClass:NSClassFromString(@"WKView")] bundlePath] stringByDeletingLastPathComponent] UTF8String];
    const char* profilePath = [[[NSBundle mainBundle] pathForResource:@"com.apple.WebProcess" ofType:@"sb"] UTF8String];

    if (confstr(_CS_DARWIN_USER_TEMP_DIR, tmpPath, PATH_MAX) <= 0 || !realpath(tmpPath, tmpRealPath))
        tmpRealPath[0] = '\0';

    if (confstr(_CS_DARWIN_USER_CACHE_DIR, cachePath, PATH_MAX) <= 0 || !realpath(cachePath, cacheRealPath))
        cacheRealPath[0] = '\0';

    const char* const sandboxParam[] = {
        "WEBKIT2_FRAMEWORK_DIR", frameworkPath,
        "DARWIN_USER_TEMP_DIR", (const char*)tmpRealPath,
        "DARWIN_USER_CACHE_DIR", (const char*)cacheRealPath,
        "NSURL_CACHE_DIR", (const char*)parameters.nsURLCachePath.data(),
        "UI_PROCESS_BUNDLE_RESOURCE_DIR", (const char*)parameters.uiProcessBundleResourcePath.data(),
        NULL
    };

    if (sandbox_init_with_parameters(profilePath, SANDBOX_NAMED_EXTERNAL, sandboxParam, &errorBuf)) {
        fprintf(stderr, "WebProcess: couldn't initialize sandbox profile [%s] with framework path [%s], tmp path [%s], cache path [%s]: %s\n", profilePath, frameworkPath, tmpRealPath, cacheRealPath, errorBuf);
        exit(EX_NOPERM);
    }
#endif
}

void WebProcess::platformInitializeWebProcess(const WebProcessCreationParameters& parameters, CoreIPC::ArgumentDecoder*)
{
    initializeSandbox(parameters);

    if (!parameters.nsURLCachePath.isNull()) {
        NSUInteger cacheMemoryCapacity = parameters.nsURLCacheMemoryCapacity;
        NSUInteger cacheDiskCapacity = parameters.nsURLCacheDiskCapacity;

        NSString *nsCachePath = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:parameters.nsURLCachePath.data() length:parameters.nsURLCachePath.length()];
        RetainPtr<NSURLCache> parentProcessURLCache(AdoptNS, [[NSURLCache alloc] initWithMemoryCapacity:cacheMemoryCapacity diskCapacity:cacheDiskCapacity diskPath:nsCachePath]);
        [NSURLCache setSharedURLCache:parentProcessURLCache.get()];
    }

    m_compositingRenderServerPort = parameters.acceleratedCompositingPort.port();
}

void WebProcess::platformShutdown()
{
}

} // namespace WebKit
