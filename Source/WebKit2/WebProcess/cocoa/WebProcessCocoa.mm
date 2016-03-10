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

#import "CustomProtocolManager.h"
#import "ObjCObjectGraph.h"
#import "SandboxExtension.h"
#import "SandboxInitializationParameters.h"
#import "SecItemShim.h"
#import "WKAPICast.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WKTypeRefWrapper.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WebFrame.h"
#import "WebInspector.h"
#import "WebPage.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessProxyMessages.h"
#import <JavaScriptCore/Options.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/FileSystem.h>
#import <WebCore/FontCache.h>
#import <WebCore/FontCascade.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MemoryCache.h>
#import <WebCore/MemoryPressureHandler.h>
#import <WebCore/NSAccessibilitySPI.h>
#import <WebCore/PageCache.h>
#import <WebCore/pthreadSPI.h>
#import <WebCore/VNodeTracker.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <WebKitSystemInterface.h>
#import <algorithm>
#import <dispatch/dispatch.h>
#import <objc/runtime.h>
#import <stdio.h>
#import <wtf/RAMSize.h>

#if PLATFORM(IOS)
#import <WebCore/GraphicsServicesSPI.h>
#endif

#if USE(OS_STATE)
#include <os/state_private.h>
#endif

using namespace WebCore;

namespace WebKit {

static uint64_t volumeFreeSize(NSString *path)
{
    NSDictionary *fileSystemAttributesDictionary = [[NSFileManager defaultManager] attributesOfFileSystemForPath:path error:NULL];
    return [[fileSystemAttributesDictionary objectForKey:NSFileSystemFreeSize] unsignedLongLongValue];
}

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
    RetainPtr<NSString> nsurlCacheDirectory = adoptNS((NSString *)WKCopyFoundationCacheDirectory());
    if (!nsurlCacheDirectory)
        nsurlCacheDirectory = NSHomeDirectory();

    uint64_t memSize = ramSize() / 1024 / 1024;

    // As a fudge factor, use 1000 instead of 1024, in case the reported byte 
    // count doesn't align exactly to a megabyte boundary.
    uint64_t diskFreeSize = volumeFreeSize(nsurlCacheDirectory.get()) / 1024 / 1000;

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    auto deadDecodedDataDeletionInterval = std::chrono::seconds { 0 };
    unsigned pageCacheSize = 0;
    unsigned long urlCacheMemoryCapacity = 0;
    unsigned long urlCacheDiskCapacity = 0;

    calculateCacheSizes(cacheModel, memSize, diskFreeSize,
        cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval,
        pageCacheSize, urlCacheMemoryCapacity, urlCacheDiskCapacity);

    auto& memoryCache = MemoryCache::singleton();
    memoryCache.setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache.setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    PageCache::singleton().setMaxSize(pageCacheSize);
}

void WebProcess::platformClearResourceCaches(ResourceCachesToClear cachesToClear)
{
    // FIXME: Remove this.
}

#if USE(APPKIT)
static id NSApplicationAccessibilityFocusedUIElement(NSApplication*, SEL)
{
    WebPage* page = WebProcess::singleton().focusedWebPage();
    if (!page || !page->accessibilityRemoteObject())
        return 0;

    return [page->accessibilityRemoteObject() accessibilityFocusedUIElement];
}
#endif

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters&& parameters)
{
#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::consumePermanently(parameters.uiProcessBundleResourcePathExtensionHandle);
    SandboxExtension::consumePermanently(parameters.webSQLDatabaseDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.applicationCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaKeyStorageDirectoryExtensionHandle);
#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerTemporaryDirectoryExtensionHandle);
#endif
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    setSharedHTTPCookieStorage(parameters.uiProcessCookieStorageIdentifier);
#endif

    auto urlCache = adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]);
    [NSURLCache setSharedURLCache:urlCache.get()];

#if PLATFORM(MAC)
    WebCore::FontCache::setFontWhitelist(parameters.fontWhitelist);
#endif

    m_compositingRenderServerPort = WTFMove(parameters.acceleratedCompositingPort);
    m_presenterApplicationPid = parameters.presenterApplicationPid;

    MemoryPressureHandler::ReliefLogger::setLoggingEnabled(parameters.shouldEnableMemoryPressureReliefLogging);

#if PLATFORM(IOS)
    // Track the number of vnodes we are using on iOS and make sure we only use a
    // reasonable amount because limits are fairly low on iOS devices and we can
    // get killed when reaching the limit.
    VNodeTracker::singleton().setPressureHandler([] (Critical critical) {
        MemoryPressureHandler::singleton().releaseMemory(critical);
    });
#endif

    setEnhancedAccessibility(parameters.accessibilityEnhancedUserInterfaceEnabled);

#if USE(APPKIT)
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{ @"NSApplicationCrashOnExceptions" : @YES }];

    // rdar://9118639 accessibilityFocusedUIElement in NSApplication defaults to use the keyWindow. Since there's
    // no window in WK2, NSApplication needs to use the focused page's focused element.
    Method methodToPatch = class_getInstanceMethod([NSApplication class], @selector(accessibilityFocusedUIElement));
    method_setImplementation(methodToPatch, (IMP)NSApplicationAccessibilityFocusedUIElement);
#endif
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    _CFNetworkSetATSContext(parameters.networkATSContext.get());
#endif

#if TARGET_OS_IPHONE
    // Priority decay on iOS 9 is impacting page load time so we fix the priority of the WebProcess' main thread (rdar://problem/22003112).
    pthread_set_fixedpriority_self();
#endif
}

void WebProcess::initializeProcessName(const ChildProcessInitializationParameters& parameters)
{
#if !PLATFORM(IOS)
    NSString *applicationName;
    if (parameters.extraInitializationData.get(ASCIILiteral("inspector-process")) == "1")
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Inspector", "Visible name of Web Inspector's web process. The argument is the application name."), (NSString *)parameters.uiProcessName];
    else
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content", "Visible name of the web process. The argument is the application name."), (NSString *)parameters.uiProcessName];
    WKSetVisibleApplicationName((CFStringRef)applicationName);
#endif
}

static void registerWithAccessibility()
{
#if USE(APPKIT)
    [NSAccessibilityRemoteUIElement setRemoteUIApp:YES];
#endif
#if PLATFORM(IOS)
    NSString *accessibilityBundlePath = [(NSString *)GSSystemRootDirectory() stringByAppendingString:@"/System/Library/AccessibilityBundles/WebProcessLoader.axbundle"];
    NSError *error = nil;
    if (![[NSBundle bundleWithPath:accessibilityBundlePath] loadAndReturnError:&error])
        LOG_ERROR("Failed to load accessibility bundle at %@: %@", accessibilityBundlePath, error);
#endif
}

#if USE(OS_STATE)
void WebProcess::registerWithStateDumper()
{
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    os_state_add_handler(queue, ^(os_state_hints_t hints) {

        @autoreleasepool {
            os_state_data_t os_state = nil;

            // Only gather state on faults and sysdiagnose. It's overkill for
            // general error messages.
            if (hints->osh_api == OS_STATE_API_ERROR)
                return os_state;

            // Create a dictionary to contain the collected state. This
            // dictionary will be serialized and passed back to os_state.
            auto stateDict = adoptNS([[NSMutableDictionary alloc] init]);

            auto pageLoadTimes = adoptNS([[NSMutableArray alloc] init]);
            for (auto& page : m_pageMap.values()) {
                if (page->usesEphemeralSession())
                    continue;

                NSDate* date = [NSDate dateWithTimeIntervalSince1970:std::chrono::system_clock::to_time_t(page->loadCommitTime())];
                [pageLoadTimes addObject:date];
            }

            // Adding an empty array to the process state may provide an
            // indication of the existance of private sessions, which we'd like
            // to hide, so don't add empty arrays.
            if ([pageLoadTimes count])
                [stateDict setObject:pageLoadTimes.get() forKey:@"Page Load Times"];

            // --- Possibly add other state here as other entries in the dictionary. ---

            // Submitting an empty process state object may provide an
            // indication of the existance of private sessions, which we'd like
            // to hide, so don't return empty dictionaries.
            if (![stateDict count])
                return os_state;

            // Serialize the accumulated process state so that we can put the
            // result in an os_state_data_t structure.
            NSError* error = nil;
            NSData* data = [NSPropertyListSerialization dataWithPropertyList:stateDict.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];

            if (!data) {
                ASSERT(data);
                return os_state;
            }

            size_t neededSize = OS_STATE_DATA_SIZE_NEEDED(data.length);
            os_state = (os_state_data_t)malloc(neededSize);
            if (os_state) {
                memset(os_state, 0, neededSize);
                os_state->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
                os_state->osd_data_size = data.length;
                strcpy(os_state->osd_title, "WebContent state"); // NB: Only 64 bytes of buffer here.
                memcpy(os_state->osd_data, data.bytes, data.length);
            }

            return os_state;
        }
    });
}
#endif

void WebProcess::platformInitializeProcess(const ChildProcessInitializationParameters&)
{
    registerWithAccessibility();

#if USE(OS_STATE)
    registerWithStateDumper();
#endif

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShim::singleton().initialize(this);
#endif
}

#if USE(APPKIT)
void WebProcess::stopRunLoop()
{
    ChildProcess::stopNSAppRunLoop();
}
#endif

void WebProcess::platformTerminate()
{
}

void WebProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if ENABLE(WEB_PROCESS_SANDBOX)
#if ENABLE(MANUAL_SANDBOXING)
    // Need to override the default, because service has a different bundle ID.
#if WK_API_ENABLED
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
#else
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
#endif
#if PLATFORM(IOS)
    sandboxParameters.setOverrideSandboxProfilePath([webKit2Bundle pathForResource:@"com.apple.WebKit.WebContent" ofType:@"sb"]);
#else
    sandboxParameters.setOverrideSandboxProfilePath([webKit2Bundle pathForResource:@"com.apple.WebProcess" ofType:@"sb"]);
#endif
    ChildProcess::initializeSandbox(parameters, sandboxParameters);
#endif
#else
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(sandboxParameters);
#endif
}

#if PLATFORM(MAC)

static NSURL *origin(WebPage& page)
{
    WebFrame* mainFrame = page.mainWebFrame();
    if (!mainFrame)
        return nil;

    URL mainFrameURL(URL(), mainFrame->url());
    Ref<SecurityOrigin> mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
    String mainFrameOriginString;
    if (!mainFrameOrigin->isUnique())
        mainFrameOriginString = mainFrameOrigin->toRawString();
    else
        mainFrameOriginString = mainFrameURL.protocol() + ':'; // toRawString() is not supposed to work with unique origins, and would just return "://".

    // +[NSURL URLWithString:] returns nil when its argument is malformed. It's unclear when we would have a malformed URL here,
    // but it happens in practice according to <rdar://problem/14173389>. Leaving an assertion in to catch a reproducible case.
    ASSERT([NSURL URLWithString:mainFrameOriginString]);

    return [NSURL URLWithString:mainFrameOriginString];
}

#endif

void WebProcess::updateActivePages()
{
#if PLATFORM(MAC)
    auto activePageURLs = adoptNS([[NSMutableArray alloc] init]);

    for (auto& page : m_pageMap.values()) {
        if (page->usesEphemeralSession())
            continue;

        if (NSURL *originAsURL = origin(*page))
            [activePageURLs addObject:userVisibleString(originAsURL)];
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [activePageURLs] {
        WKSetApplicationInformationItem(CFSTR("LSActivePageUserVisibleOriginsKey"), (__bridge CFArrayRef)activePageURLs.get());
    });
#endif
}

RefPtr<ObjCObjectGraph> WebProcess::transformHandlesToObjects(ObjCObjectGraph& objectGraph)
{
    struct Transformer final : ObjCObjectGraph::Transformer {
        Transformer(WebProcess& webProcess)
            : m_webProcess(webProcess)
        {
        }

        bool shouldTransformObject(id object) const override
        {
#if WK_API_ENABLED
            if (dynamic_objc_cast<WKBrowsingContextHandle>(object))
                return true;

            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
#endif
            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
#if WK_API_ENABLED
            if (auto* handle = dynamic_objc_cast<WKBrowsingContextHandle>(object)) {
                if (auto* webPage = m_webProcess.webPage(handle._pageID))
                    return wrapper(*webPage);

                return [NSNull null];
            }

            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(m_webProcess.transformHandlesToObjects(toImpl(wrapper.object)).get())]);
#endif
            return object;
        }

        WebProcess& m_webProcess;
    };

    return ObjCObjectGraph::create(ObjCObjectGraph::transform(objectGraph.rootObject(), Transformer(*this)).get());
}

RefPtr<ObjCObjectGraph> WebProcess::transformObjectsToHandles(ObjCObjectGraph& objectGraph)
{
    struct Transformer final : ObjCObjectGraph::Transformer {
        bool shouldTransformObject(id object) const override
        {
#if WK_API_ENABLED
            if (dynamic_objc_cast<WKWebProcessPlugInBrowserContextController>(object))
                return true;

            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
#endif

            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
#if WK_API_ENABLED
            if (auto* controller = dynamic_objc_cast<WKWebProcessPlugInBrowserContextController>(object))
                return controller.handle;

            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(transformObjectsToHandles(toImpl(wrapper.object)).get())]);
#endif
            return object;
        }
    };

    return ObjCObjectGraph::create(ObjCObjectGraph::transform(objectGraph.rootObject(), Transformer()).get());
}

void WebProcess::destroyRenderingResources()
{
    WKDestroyRenderingResources();
}

} // namespace WebKit
