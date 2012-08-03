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

#import "config.h"
#import "WebProcess.h"

#import "SandboxExtension.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WebInspector.h"
#import "WebPage.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessProxyMessages.h"
#import <WebCore/FileSystem.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MemoryCache.h>
#import <WebCore/PageCache.h>
#import <WebKitSystemInterface.h>
#import <algorithm>
#import <dispatch/dispatch.h>
#import <mach/host_info.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <objc/runtime.h>
#import <stdio.h>

#if defined(BUILDING_ON_SNOW_LEOPARD)
#import "KeychainItemShimMethods.h"
#else
#import "SecItemShimMethods.h"
#endif

#if ENABLE(WEB_PROCESS_SANDBOX)
#import <stdlib.h>
#import <sysexits.h>

// We have to #undef __APPLE_API_PRIVATE to prevent sandbox.h from looking for a header file that does not exist (<rdar://problem/9679211>). 
#undef __APPLE_API_PRIVATE
#import <sandbox.h>

#define SANDBOX_NAMED_EXTERNAL 0x0003
extern "C" int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf);

// Define this to 1 to bypass the sandbox for debugging purposes. 
#define DEBUG_BYPASS_SANDBOX 0

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
        nsurlCacheDirectory = NSHomeDirectory();

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

void WebProcess::platformClearResourceCaches(ResourceCachesToClear cachesToClear)
{
    if (cachesToClear == InMemoryResourceCachesOnly)
        return;

    if (!m_clearResourceCachesDispatchGroup)
        m_clearResourceCachesDispatchGroup = dispatch_group_create();

    dispatch_group_async(m_clearResourceCachesDispatchGroup, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[NSURLCache sharedURLCache] removeAllCachedResponses];
    });
}

#if ENABLE(WEB_PROCESS_SANDBOX)
static void appendSandboxParameterPathInternal(Vector<const char*>& vector, const char* name, const char* path)
{
    char normalizedPath[PATH_MAX];
    if (!realpath(path, normalizedPath))
        normalizedPath[0] = '\0';

    vector.append(name);
    vector.append(fastStrDup(normalizedPath));
}

static void appendReadwriteConfDirectory(Vector<const char*>& vector, const char* name, int confID)
{
    char path[PATH_MAX];
    if (confstr(confID, path, PATH_MAX) <= 0)
        path[0] = '\0';

    appendSandboxParameterPathInternal(vector, name, path);
}

static void appendReadonlySandboxDirectory(Vector<const char*>& vector, const char* name, NSString *path)
{
    appendSandboxParameterPathInternal(vector, name, [path length] ? [(NSString *)path fileSystemRepresentation] : "");
}

static void appendReadwriteSandboxDirectory(Vector<const char*>& vector, const char* name, NSString *path)
{
    NSError *error = nil;

    // This is very unlikely to fail, but in case it actually happens, we'd like some sort of output in the console.
    if (![[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&error])
        NSLog(@"could not create \"%@\", error %@", path, error);

    appendSandboxParameterPathInternal(vector, name, [(NSString *)path fileSystemRepresentation]);
}

#endif

static void initializeSandbox(const WebProcessCreationParameters& parameters)
{
#if ENABLE(WEB_PROCESS_SANDBOX)

#if DEBUG_BYPASS_SANDBOX 
    WTFLogAlways("Bypassing web process sandbox.\n"); 
    return; 
 #endif

#if !defined(BUILDING_ON_LION)
    // Use private temporary and cache directories.
    String systemDirectorySuffix = "com.apple.WebProcess+" + parameters.uiProcessBundleIdentifier;
    setenv("DIRHELPER_USER_DIR_SUFFIX", fileSystemRepresentation(systemDirectorySuffix).data(), 0);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("WebProcess: couldn't retrieve private temporary directory path: %d\n", errno);
        exit(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);
#endif

    Vector<const char*> sandboxParameters;

    // These are read-only.
    appendReadonlySandboxDirectory(sandboxParameters, "WEBKIT2_FRAMEWORK_DIR", [[[NSBundle bundleForClass:NSClassFromString(@"WKView")] bundlePath] stringByDeletingLastPathComponent]);
    appendReadonlySandboxDirectory(sandboxParameters, "UI_PROCESS_BUNDLE_RESOURCE_DIR", parameters.uiProcessBundleResourcePath);
    appendReadonlySandboxDirectory(sandboxParameters, "WEBKIT_WEB_INSPECTOR_DIR", parameters.webInspectorBaseDirectory);

    // These are read-write getconf paths.
    appendReadwriteConfDirectory(sandboxParameters, "DARWIN_USER_TEMP_DIR", _CS_DARWIN_USER_TEMP_DIR);
    appendReadwriteConfDirectory(sandboxParameters, "DARWIN_USER_CACHE_DIR", _CS_DARWIN_USER_CACHE_DIR);

    // These are read-write paths.
    appendReadwriteSandboxDirectory(sandboxParameters, "HOME_DIR", NSHomeDirectory());
    appendReadwriteSandboxDirectory(sandboxParameters, "WEBKIT_DATABASE_DIR", parameters.databaseDirectory);
    appendReadwriteSandboxDirectory(sandboxParameters, "WEBKIT_LOCALSTORAGE_DIR", parameters.localStorageDirectory);
    appendReadwriteSandboxDirectory(sandboxParameters, "WEBKIT_APPLICATION_CACHE_DIR", parameters.applicationCacheDirectory);
    appendReadwriteSandboxDirectory(sandboxParameters, "NSURL_CACHE_DIR", parameters.nsURLCachePath);

    sandboxParameters.append(static_cast<const char*>(0));

    const char* profilePath = [[[NSBundle mainBundle] pathForResource:@"com.apple.WebProcess" ofType:@"sb"] fileSystemRepresentation];

    char* errorBuf;
    if (sandbox_init_with_parameters(profilePath, SANDBOX_NAMED_EXTERNAL, sandboxParameters.data(), &errorBuf)) {
        WTFLogAlways("WebProcess: couldn't initialize sandbox profile [%s] error '%s'\n", profilePath, errorBuf);
        for (size_t i = 0; sandboxParameters[i]; i += 2)
            WTFLogAlways("%s=%s\n", sandboxParameters[i], sandboxParameters[i + 1]);
        exit(EX_NOPERM);
    }

    for (size_t i = 0; sandboxParameters[i]; i += 2)
        fastFree(const_cast<char*>(sandboxParameters[i + 1]));

    // This will override LSFileQuarantineEnabled from Info.plist unless sandbox quarantine is globally disabled.
    OSStatus error = WKEnableSandboxStyleFileQuarantine();
    if (error) {
        WTFLogAlways("WebProcess: couldn't enable sandbox style file quarantine: %ld\n", (long)error);
        exit(EX_NOPERM);
    }
#endif
}

static id NSApplicationAccessibilityFocusedUIElement(NSApplication*, SEL)
{
    WebPage* page = WebProcess::shared().focusedWebPage();
    if (!page || !page->accessibilityRemoteObject())
        return 0;

    return [page->accessibilityRemoteObject() accessibilityFocusedUIElement];
}
    
void WebProcess::platformInitializeWebProcess(const WebProcessCreationParameters& parameters, CoreIPC::ArgumentDecoder*)
{
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];

    initializeSandbox(parameters);

    if (!parameters.parentProcessName.isNull()) {
        NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content", "Visible name of the web process. The argument is the application name."), (NSString *)parameters.parentProcessName];
        WKSetVisibleApplicationName((CFStringRef)applicationName);
    }

    if (!parameters.nsURLCachePath.isNull()) {
        NSUInteger cacheMemoryCapacity = parameters.nsURLCacheMemoryCapacity;
        NSUInteger cacheDiskCapacity = parameters.nsURLCacheDiskCapacity;

        RetainPtr<NSURLCache> parentProcessURLCache(AdoptNS, [[NSURLCache alloc] initWithMemoryCapacity:cacheMemoryCapacity diskCapacity:cacheDiskCapacity diskPath:parameters.nsURLCachePath]);
        [NSURLCache setSharedURLCache:parentProcessURLCache.get()];
    }

    m_compositingRenderServerPort = parameters.acceleratedCompositingPort.port();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    m_notificationManager.initialize(parameters.notificationPermissions);
#endif

    // rdar://9118639 accessibilityFocusedUIElement in NSApplication defaults to use the keyWindow. Since there's
    // no window in WK2, NSApplication needs to use the focused page's focused element.
    Method methodToPatch = class_getInstanceMethod([NSApplication class], @selector(accessibilityFocusedUIElement));
    method_setImplementation(methodToPatch, (IMP)NSApplicationAccessibilityFocusedUIElement);
}

void WebProcess::initializeShim()
{
#if defined(BUILDING_ON_SNOW_LEOPARD)
    initializeKeychainItemShim();
#else
    initializeSecItemShim();
#endif
}

void WebProcess::platformTerminate()
{
    if (m_clearResourceCachesDispatchGroup) {
        dispatch_group_wait(m_clearResourceCachesDispatchGroup, DISPATCH_TIME_FOREVER);
        dispatch_release(m_clearResourceCachesDispatchGroup);
        m_clearResourceCachesDispatchGroup = 0;
    }
}

void WebProcess::secItemResponse(CoreIPC::Connection*, uint64_t requestID, const SecItemResponseData& response)
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    didReceiveSecItemResponse(requestID, response);
#endif
}

void WebProcess::secKeychainItemResponse(CoreIPC::Connection*, uint64_t requestID, const SecKeychainItemResponseData& response)
{
#if defined(BUILDING_ON_SNOW_LEOPARD)
    didReceiveSecKeychainItemResponse(requestID, response);
#endif
}

} // namespace WebKit
