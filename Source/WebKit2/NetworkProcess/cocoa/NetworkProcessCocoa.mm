/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#import "NetworkCache.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkResourceLoader.h"
#import "SandboxExtension.h"
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/PublicSuffix.h>
#import <WebCore/ResourceRequestCFNet.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <WebKitSystemInterface.h>
#import <wtf/RAMSize.h>

namespace WebKit {

void NetworkProcess::platformLowMemoryHandler(WebCore::Critical)
{
    CFURLConnectionInvalidateConnectionCache();
    _CFURLCachePurgeMemoryCache(adoptCF(CFURLCacheCopySharedURLCache()).get());
}

static void initializeNetworkSettings()
{
    static const unsigned preferredConnectionCount = 6;

    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPLoadWidth, preferredConnectionCount);

    Boolean keyExistsAndHasValidFormat = false;
    Boolean prefValue = CFPreferencesGetAppBooleanValue(CFSTR("WebKitEnableHTTPPipelining"), kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat)
        WebCore::ResourceRequest::setHTTPPipeliningEnabled(prefValue);

    if (WebCore::ResourceRequest::resourcePrioritiesEnabled()) {
        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, toPlatformRequestPriority(WebCore::ResourceLoadPriority::Highest));
#if PLATFORM(IOS)
        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(WebCore::ResourceLoadPriority::Medium));
#endif
    }
}

void NetworkProcess::platformInitializeNetworkProcessCocoa(const NetworkProcessCreationParameters& parameters)
{
#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.parentBundleDirectoryExtensionHandle);
#endif
    m_diskCacheDirectory = parameters.diskCacheDirectory;

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    _CFNetworkSetATSContext(parameters.networkATSContext.get());
#endif

    initializeNetworkSettings();

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    setSharedHTTPCookieStorage(parameters.uiProcessCookieStorageIdentifier);
#endif

    // FIXME: Most of what this function does for cache size gets immediately overridden by setCacheModel().
    // - memory cache size passed from UI process is always ignored;
    // - disk cache size passed from UI process is effectively a minimum size.
    // One non-obvious constraint is that we need to use -setSharedURLCache: even in testing mode, to prevent creating a default one on disk later, when some other code touches the cache.

    ASSERT(!m_diskCacheIsDisabledForTesting || !parameters.nsURLCacheDiskCapacity);

    if (!m_diskCacheDirectory.isNull()) {
        SandboxExtension::consumePermanently(parameters.diskCacheDirectoryExtensionHandle);
#if ENABLE(NETWORK_CACHE)
        if (parameters.shouldEnableNetworkCache) {
            NetworkCache::Cache::Parameters cacheParameters = {
                parameters.shouldEnableNetworkCacheEfficacyLogging
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
                , parameters.shouldEnableNetworkCacheSpeculativeRevalidation
#endif
            };
            if (NetworkCache::singleton().initialize(m_diskCacheDirectory, cacheParameters)) {
                auto urlCache(adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]));
                [NSURLCache setSharedURLCache:urlCache.get()];
                return;
            }
        }
#endif
        String nsURLCacheDirectory = m_diskCacheDirectory;
#if PLATFORM(IOS)
        // NSURLCache path is relative to network process cache directory.
        // This puts cache files under <container>/Library/Caches/com.apple.WebKit.Networking/
        nsURLCacheDirectory = ".";
#endif
        [NSURLCache setSharedURLCache:adoptNS([[NSURLCache alloc]
            initWithMemoryCapacity:parameters.nsURLCacheMemoryCapacity
            diskCapacity:parameters.nsURLCacheDiskCapacity
            diskPath:nsURLCacheDirectory]).get()];
    }

    RetainPtr<CFURLCacheRef> cache = adoptCF(CFURLCacheCopySharedURLCache());
    if (!cache)
        return;

    _CFURLCacheSetMinSizeForVMCachedResource(cache.get(), NetworkResourceLoader::fileBackedResourceMinimumSize());
}

static uint64_t volumeFreeSize(const String& path)
{
    NSDictionary *fileSystemAttributesDictionary = [[NSFileManager defaultManager] attributesOfFileSystemForPath:(NSString *)path error:NULL];
    return [[fileSystemAttributesDictionary objectForKey:NSFileSystemFreeSize] unsignedLongLongValue];
}

void NetworkProcess::platformSetCacheModel(CacheModel cacheModel)
{
    uint64_t memSize = ramSize() / 1024 / 1024;

    // As a fudge factor, use 1000 instead of 1024, in case the reported byte
    // count doesn't align exactly to a megabyte boundary.
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

    if (m_diskCacheSizeOverride >= 0)
        urlCacheDiskCapacity = m_diskCacheSizeOverride;

#if ENABLE(NETWORK_CACHE)
    auto& networkCache = NetworkCache::singleton();
    if (networkCache.isEnabled()) {
        networkCache.setCapacity(urlCacheDiskCapacity);
        return;
    }
#endif
    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];
    [nsurlCache setMemoryCapacity:urlCacheMemoryCapacity];
    if (!m_diskCacheIsDisabledForTesting)
        [nsurlCache setDiskCapacity:std::max<unsigned long>(urlCacheDiskCapacity, [nsurlCache diskCapacity])]; // Don't shrink a big disk cache, since that would cause churn.
}

static RetainPtr<CFStringRef> partitionName(CFStringRef domain)
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    String highLevel = WebCore::topPrivatelyControlledDomain(domain);
    if (highLevel.isNull())
        return 0;
    CString utf8String = highLevel.utf8();
    return adoptCF(CFStringCreateWithBytes(0, reinterpret_cast<const UInt8*>(utf8String.data()), utf8String.length(), kCFStringEncodingUTF8, false));
#else
    return domain;
#endif
}

Vector<Ref<WebCore::SecurityOrigin>> NetworkProcess::cfURLCacheOrigins()
{
    Vector<Ref<WebCore::SecurityOrigin>> result;

    WKCFURLCacheCopyAllPartitionNames([&result](CFArrayRef partitionNames) {
        RetainPtr<CFArrayRef> hostNamesInPersistentStore = adoptCF(WKCFURLCacheCopyAllHostNamesInPersistentStoreForPartition(CFSTR("")));
        RetainPtr<CFMutableArrayRef> hostNames = adoptCF(CFArrayCreateMutableCopy(0, 0, hostNamesInPersistentStore.get()));
        if (partitionNames) {
            CFArrayAppendArray(hostNames.get(), partitionNames, CFRangeMake(0, CFArrayGetCount(partitionNames)));
            CFRelease(partitionNames);
        }

        for (CFIndex i = 0, size = CFArrayGetCount(hostNames.get()); i < size; ++i) {
            CFStringRef host = static_cast<CFStringRef>(CFArrayGetValueAtIndex(hostNames.get(), i));

            result.append(WebCore::SecurityOrigin::create("http", host, 0));
        }
    });

    return result;
}

void NetworkProcess::clearCFURLCacheForOrigins(const Vector<WebCore::SecurityOriginData>& origins)
{
    auto hostNames = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
    for (auto& origin : origins)
        CFArrayAppendValue(hostNames.get(), origin.host.createCFString().get());

    WKCFURLCacheDeleteHostNamesInPersistentStore(hostNames.get());

    for (CFIndex i = 0, size = CFArrayGetCount(hostNames.get()); i < size; ++i) {
        RetainPtr<CFStringRef> partition = partitionName(static_cast<CFStringRef>(CFArrayGetValueAtIndex(hostNames.get(), i)));
        RetainPtr<CFArrayRef> partitionHostNames = adoptCF(WKCFURLCacheCopyAllHostNamesInPersistentStoreForPartition(partition.get()));
        WKCFURLCacheDeleteHostNamesInPersistentStoreForPartition(partitionHostNames.get(), partition.get());
    }
}

void NetworkProcess::clearHSTSCache(WebCore::NetworkStorageSession& session, std::chrono::system_clock::time_point modifiedSince)
{
    NSTimeInterval timeInterval = std::chrono::duration_cast<std::chrono::duration<double>>(modifiedSince.time_since_epoch()).count();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];

    _CFNetworkResetHSTSHostsSinceDate(session.platformSession(), (__bridge CFDateRef)date);
}

static void clearNSURLCache(dispatch_group_t group, std::chrono::system_clock::time_point modifiedSince, const std::function<void ()>& completionHandler)
{
    dispatch_group_async(group, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [modifiedSince, completionHandler] {
        NSURLCache *cache = [NSURLCache sharedURLCache];

        NSTimeInterval timeInterval = std::chrono::duration_cast<std::chrono::duration<double>>(modifiedSince.time_since_epoch()).count();
        NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
        [cache removeCachedResponsesSinceDate:date];

        dispatch_async(dispatch_get_main_queue(), [completionHandler] {
            completionHandler();
        });
    });
}

void NetworkProcess::clearDiskCache(std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler)
{
    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = dispatch_group_create();

#if ENABLE(NETWORK_CACHE)
    auto group = m_clearCacheDispatchGroup;
    dispatch_group_async(group, dispatch_get_main_queue(), [group, modifiedSince, completionHandler] {
        NetworkCache::singleton().clear(modifiedSince, [group, modifiedSince, completionHandler] {
            // FIXME: Probably not necessary.
            clearNSURLCache(group, modifiedSince, completionHandler);
        });
    });
#else
    clearNSURLCache(m_clearCacheDispatchGroup, modifiedSince, completionHandler);
#endif
}

}
