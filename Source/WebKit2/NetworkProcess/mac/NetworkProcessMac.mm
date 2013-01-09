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

#import "config.h"
#import "NetworkProcess.h"

#if ENABLE(NETWORK_PROCESS)

#import "NetworkProcessCreationParameters.h"
#import "PlatformCertificateInfo.h"
#import "SandboxExtension.h"
#import "StringUtilities.h"
#import <WebCore/FileSystem.h>
#import <WebCore/LocalizedStrings.h>
#import <WebKitSystemInterface.h>
#import <mach/host_info.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <sysexits.h>
#import <wtf/text/WTFString.h>

#if USE(SECURITY_FRAMEWORK)
#import "SecItemShim.h"
#endif

// Define this to 1 to bypass the sandbox for debugging purposes.
#define DEBUG_BYPASS_SANDBOX 0

using namespace WebCore;

@interface NSURLRequest (Details) 
+ (void)setAllowsSpecificHTTPSCertificate:(NSArray *)allow forHost:(NSString *)host;
@end

namespace WebKit {

void NetworkProcess::initializeProcessName(const ChildProcessInitializationParameters& parameters)
{
    if (!parameters.uiProcessName.isNull()) {
        NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Networking", "visible name of the network process. The argument is the application name."), (NSString *)parameters.uiProcessName];
        WKSetVisibleApplicationName((CFStringRef)applicationName);
    }
}

void NetworkProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters)
{
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];

#if DEBUG_BYPASS_SANDBOX
    WTFLogAlways("Bypassing network process sandbox.\n");
    return;
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    // Use private temporary and cache directories.
    String systemDirectorySuffix = "com.apple.WebKit.NetworkProcess+" + parameters.clientIdentifier;
    setenv("DIRHELPER_USER_DIR_SUFFIX", fileSystemRepresentation(systemDirectorySuffix).data(), 0);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("NetworkProcess: couldn't retrieve private temporary directory path: %d\n", errno);
        exit(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);
#endif

    // FIXME (NetworkProcess): <rdar://problem/12772605> Actually initialize the sandbox.

    // This will override LSFileQuarantineEnabled from Info.plist unless sandbox quarantine is globally disabled.
    OSStatus error = WKEnableSandboxStyleFileQuarantine();
    if (error) {
        WTFLogAlways("NetworkProcess: Couldn't enable sandbox style file quarantine: %ld\n", (long)error);
        exit(EX_NOPERM);
    }
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
static void overrideSystemProxies(const String& httpProxy, const String& httpsProxy)
{
    NSMutableDictionary *proxySettings = [NSMutableDictionary dictionary];

    if (!httpProxy.isNull()) {
        KURL httpProxyURL(KURL(), httpProxy);
        if (httpProxyURL.isValid()) {
            [proxySettings setObject:nsStringFromWebCoreString(httpProxyURL.host()) forKey:(NSString *)kCFNetworkProxiesHTTPProxy];
            if (httpProxyURL.hasPort()) {
                NSNumber *port = [NSNumber numberWithInt:httpProxyURL.port()];
                [proxySettings setObject:port forKey:(NSString *)kCFNetworkProxiesHTTPPort];
            }
        }
        else
            NSLog(@"Malformed HTTP Proxy URL '%s'.  Expected 'http://<hostname>[:<port>]'\n", httpProxy.utf8().data());
    }

    if (!httpsProxy.isNull()) {
        KURL httpsProxyURL(KURL(), httpsProxy);
        if (httpsProxyURL.isValid()) {
            [proxySettings setObject:nsStringFromWebCoreString(httpsProxyURL.host()) forKey:(NSString *)kCFNetworkProxiesHTTPSProxy];
            if (httpsProxyURL.hasPort()) {
                NSNumber *port = [NSNumber numberWithInt:httpsProxyURL.port()];
                [proxySettings setObject:port forKey:(NSString *)kCFNetworkProxiesHTTPSPort];
            }
        } else
            NSLog(@"Malformed HTTPS Proxy URL '%s'.  Expected 'https://<hostname>[:<port>]'\n", httpsProxy.utf8().data());
    }

    if ([proxySettings count] > 0)
        WKCFNetworkSetOverrideSystemProxySettings((CFDictionaryRef)proxySettings);
}
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    m_diskCacheDirectory = parameters.diskCacheDirectory;

    if (!m_diskCacheDirectory.isNull()) {
        SandboxExtension::consumePermanently(parameters.diskCacheDirectoryExtensionHandle);
        NSUInteger cacheMemoryCapacity = parameters.nsURLCacheMemoryCapacity;
        NSUInteger cacheDiskCapacity = parameters.nsURLCacheDiskCapacity;

        RetainPtr<NSURLCache> parentProcessURLCache(AdoptNS, [[NSURLCache alloc] initWithMemoryCapacity:cacheMemoryCapacity diskCapacity:cacheDiskCapacity diskPath:parameters.diskCacheDirectory]);
        [NSURLCache setSharedURLCache:parentProcessURLCache.get()];
    }

#if USE(SECURITY_FRAMEWORK)
    SecItemShim::shared().initialize(this);
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    if (!parameters.httpProxy.isNull() || !parameters.httpsProxy.isNull())
        overrideSystemProxies(parameters.httpProxy, parameters.httpsProxy);
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
    double deadDecodedDataDeletionInterval = 0;
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

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const PlatformCertificateInfo& certificateInfo, const String& host)
{
    [NSURLRequest setAllowsSpecificHTTPSCertificate:(NSArray *)certificateInfo.certificateChain() forHost:(NSString *)host];
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
