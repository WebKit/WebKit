/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#import "WebProcessCocoa.h"

#import "LegacyCustomProtocolManager.h"
#import "LogInitialization.h"
#import "Logging.h"
#import "ObjCObjectGraph.h"
#import "SandboxExtension.h"
#import "SandboxInitializationParameters.h"
#import "SessionTracker.h"
#import "WKAPICast.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKCrashReporter.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WKTypeRefWrapper.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WebFrame.h"
#import "WebInspector.h"
#import "WebPage.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessProxyMessages.h"
#import "WebsiteDataStoreParameters.h"
#import <JavaScriptCore/ConfigFile.h>
#import <JavaScriptCore/Options.h>
#import <WebCore/AVFoundationMIMETypeCache.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/CPUMonitor.h>
#import <WebCore/DisplayRefreshMonitorManager.h>
#import <WebCore/FileSystem.h>
#import <WebCore/FontCache.h>
#import <WebCore/FontCascade.h>
#import <WebCore/HistoryController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/MemoryRelease.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/PerformanceLogging.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <algorithm>
#import <dispatch/dispatch.h>
#import <objc/runtime.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/pthreadSPI.h>
#import <pal/spi/mac/NSAccessibilitySPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <stdio.h>
#import <wtf/cocoa/NSURLExtras.h>

#if PLATFORM(IOS_FAMILY)
#import "WKAccessibilityWebPageObjectIOS.h"
#import <UIKit/UIAccessibility.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#endif

#if PLATFORM(IOS_FAMILY) && USE(APPLE_INTERNAL_SDK)
#import <AXRuntime/AXDefines.h>
#import <AXRuntime/AXNotificationConstants.h>
#endif

#if PLATFORM(IOS_FAMILY) && !USE(APPLE_INTERNAL_SDK)
#define kAXPidStatusChangedNotification 0
#endif

#if PLATFORM(MAC)
#import "WKAccessibilityWebPageObjectMac.h"
#import "WebSwitchingGPUClient.h"
#import <WebCore/GraphicsContext3DManager.h>
#import <WebCore/ScrollbarThemeMac.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#endif

#if USE(OS_STATE)
#import <os/state_private.h>
#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(MAC)
static const Seconds cpuMonitoringInterval { 8_min };
#endif

void WebProcess::platformSetCacheModel(CacheModel)
{
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
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WebCore::initializeLogChannelsIfNecessary(parameters.webCoreLoggingChannels);
    WebKit::initializeLogChannelsIfNecessary(parameters.webKitLoggingChannels);
#endif

    WebCore::setApplicationBundleIdentifier(parameters.uiProcessBundleIdentifier);
    WebCore::setApplicationSDKVersion(parameters.uiProcessSDKVersion);

    SessionTracker::setIdentifierBase(parameters.uiProcessBundleIdentifier);

#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::consumePermanently(parameters.uiProcessBundleResourcePathExtensionHandle);
    SandboxExtension::consumePermanently(parameters.webSQLDatabaseDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.applicationCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaKeyStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.javaScriptConfigurationDirectoryExtensionHandle);
#if ENABLE(MEDIA_STREAM)
    SandboxExtension::consumePermanently(parameters.audioCaptureExtensionHandle);
#endif
#if PLATFORM(IOS_FAMILY)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerTemporaryDirectoryExtensionHandle);
#endif
#endif

    if (!parameters.javaScriptConfigurationDirectory.isEmpty()) {
        String javaScriptConfigFile = parameters.javaScriptConfigurationDirectory + "/JSC.config";
        JSC::processConfigFile(javaScriptConfigFile.latin1().data(), "com.apple.WebKit.WebContent", parameters.uiProcessBundleIdentifier.latin1().data());
    }

    // Disable NSURLCache.
    auto urlCache = adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]);
    [NSURLCache setSharedURLCache:urlCache.get()];

#if PLATFORM(MAC)
    WebCore::FontCache::setFontWhitelist(parameters.fontWhitelist);
#endif

    m_compositingRenderServerPort = WTFMove(parameters.acceleratedCompositingPort);

    WebCore::registerMemoryReleaseNotifyCallbacks();
    MemoryPressureHandler::ReliefLogger::setLoggingEnabled(parameters.shouldEnableMemoryPressureReliefLogging);

    setEnhancedAccessibility(parameters.accessibilityEnhancedUserInterfaceEnabled);

#if USE(APPKIT)
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{ @"NSApplicationCrashOnExceptions" : @YES }];

    // rdar://9118639 accessibilityFocusedUIElement in NSApplication defaults to use the keyWindow. Since there's
    // no window in WK2, NSApplication needs to use the focused page's focused element.
    Method methodToPatch = class_getInstanceMethod([NSApplication class], @selector(accessibilityFocusedUIElement));
    method_setImplementation(methodToPatch, (IMP)NSApplicationAccessibilityFocusedUIElement);
#endif
    
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
    // Need to initialize accessibility for VoiceOver to work when the WebContent process is using NSRunLoop.
    // Currently, it is also needed to allocate and initialize an NSApplication object.
    [NSApplication _accessibilityInitialize];
#endif

#if TARGET_OS_IPHONE
    // Priority decay on iOS 9 is impacting page load time so we fix the priority of the WebProcess' main thread (rdar://problem/22003112).
    pthread_set_fixedpriority_self();
#endif

    if (!parameters.mediaMIMETypes.isEmpty())
        setMediaMIMETypes(parameters.mediaMIMETypes);
    else {
        AVFoundationMIMETypeCache::singleton().setCacheMIMETypesCallback([this](const Vector<String>& types) {
            parentProcessConnection()->send(Messages::WebProcessProxy::CacheMediaMIMETypes(types), 0);
        });
    }

#if PLATFORM(MAC)
    WebCore::setScreenProperties(parameters.screenProperties);
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    scrollerStylePreferenceChanged(parameters.useOverlayScrollbars);
#endif
#endif
}

void WebProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
#if PLATFORM(MAC)
    updateProcessName();
#endif
}

#if PLATFORM(MAC)
void WebProcess::updateProcessName()
{
    NSString *applicationName;
    switch (m_processType) {
    case ProcessType::Inspector:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Inspector", "Visible name of Web Inspector's web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::ServiceWorker:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Service Worker (%@)", "Visible name of Service Worker process. The argument is the application name."), (NSString *)m_uiProcessName, (NSString *)m_securityOrigin];
        break;
    case ProcessType::PrewarmedWebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content (Prewarmed)", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::WebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        // Note that it is important for _RegisterApplication() to have been called before setting the display name.
        auto error = _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName, nullptr);
        ASSERT(!error);
        if (error) {
            RELEASE_LOG_ERROR(Process, "Failed to set the display name of the WebContent process, error code: %ld", static_cast<long>(error));
            return;
        }
#if !ASSERT_DISABLED
        // It is possible for _LSSetApplicationInformationItem() to return 0 and yet fail to set the display name so we make sure the display name has actually been set.
        String actualApplicationName = adoptCF((CFStringRef)_LSCopyApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey)).get();
        ASSERT(!actualApplicationName.isEmpty());
#endif
    });
}
#endif // PLATFORM(MAC)

static void registerWithAccessibility()
{
#if USE(APPKIT)
    [NSAccessibilityRemoteUIElement setRemoteUIApp:YES];
#endif
#if PLATFORM(IOS_FAMILY)
    NSString *accessibilityBundlePath = [(NSString *)GSSystemRootDirectory() stringByAppendingString:@"/System/Library/AccessibilityBundles/WebProcessLoader.axbundle"];
    NSError *error = nil;
    if (![[NSBundle bundleWithPath:accessibilityBundlePath] loadAndReturnError:&error])
        LOG_ERROR("Failed to load accessibility bundle at %@: %@", accessibilityBundlePath, error);
#endif
}

#if USE(OS_STATE)
void WebProcess::registerWithStateDumper()
{
    os_state_add_handler(dispatch_get_main_queue(), ^(os_state_hints_t hints) {

        @autoreleasepool {
            os_state_data_t os_state = nil;

            // Only gather state on faults and sysdiagnose. It's overkill for
            // general error messages.
            if (hints->osh_api == OS_STATE_API_ERROR)
                return os_state;

            // Create a dictionary to contain the collected state. This
            // dictionary will be serialized and passed back to os_state.
            auto stateDict = adoptNS([[NSMutableDictionary alloc] init]);

            {
                auto memoryUsageStats = adoptNS([[NSMutableDictionary alloc] init]);
                for (auto& it : PerformanceLogging::memoryUsageStatistics(ShouldIncludeExpensiveComputations::Yes)) {
                    auto keyString = adoptNS([[NSString alloc] initWithUTF8String:it.key]);
                    [memoryUsageStats setObject:@(it.value) forKey:keyString.get()];
                }
                [stateDict setObject:memoryUsageStats.get() forKey:@"Memory Usage Stats"];
            }

            {
                auto jsObjectCounts = adoptNS([[NSMutableDictionary alloc] init]);
                for (auto& it : PerformanceLogging::javaScriptObjectCounts()) {
                    auto keyString = adoptNS([[NSString alloc] initWithUTF8String:it.key]);
                    [jsObjectCounts setObject:@(it.value) forKey:keyString.get()];
                }
                [stateDict setObject:jsObjectCounts.get() forKey:@"JavaScript Object Counts"];
            }

            auto pageLoadTimes = adoptNS([[NSMutableArray alloc] init]);
            for (auto& page : m_pageMap.values()) {
                if (page->usesEphemeralSession())
                    continue;

                NSDate* date = [NSDate dateWithTimeIntervalSince1970:page->loadCommitTime().secondsSinceEpoch().seconds()];
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
                strlcpy(os_state->osd_title, "WebContent state", sizeof(os_state->osd_title));
                memcpy(os_state->osd_data, data.bytes, data.length);
            }

            return os_state;
        }
    });
}
#endif

void WebProcess::platformInitializeProcess(const ChildProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC)
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    // Deny the WebContent process access to the WindowServer.
    // This call will not succeed if there are open WindowServer connections at this point.
    auto retval = CGSSetDenyWindowServerConnections(true);
    RELEASE_ASSERT(retval == kCGErrorSuccess);
    // Make sure that we close any WindowServer connections after checking in with Launch Services.
    CGSShutdownServerConnections();

    SwitchingGPUClient::setSingleton(WebSwitchingGPUClient::singleton());

    // This is necessary so that we are able to set the process' display name.
    _RegisterApplication(nullptr, nullptr);

#else

    if (![NSApp isRunning]) {
        // This call is needed when the WebProcess is not running the NSApplication event loop.
        // Otherwise, calling enableSandboxStyleFileQuarantine() will fail.
        launchServicesCheckIn();
    }
#endif // ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)

    if (parameters.extraInitializationData.get("inspector-process"_s) == "1")
        m_processType = ProcessType::Inspector;
#if ENABLE(SERVICE_WORKER)
    else if (parameters.extraInitializationData.get("service-worker-process"_s) == "1") {
        m_processType = ProcessType::ServiceWorker;
        m_securityOrigin = parameters.extraInitializationData.get("security-origin"_s);
    }
#endif
    else if (parameters.extraInitializationData.get("is-prewarmed"_s) == "1")
        m_processType = ProcessType::PrewarmedWebContent;
    else
        m_processType = ProcessType::WebContent;

    m_uiProcessName = parameters.uiProcessName;
#else
    UNUSED_PARAM(parameters);
#endif // PLATFORM(MAC)

    registerWithAccessibility();

#if USE(OS_STATE)
    registerWithStateDumper();
#endif
}

#if USE(APPKIT)
void WebProcess::stopRunLoop()
{
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
    ChildProcess::stopNSRunLoop();
#else
    ChildProcess::stopNSAppRunLoop();
#endif
}
#endif

void WebProcess::platformTerminate()
{
    AVFoundationMIMETypeCache::singleton().setCacheMIMETypesCallback(nullptr);
}

RetainPtr<CFDataRef> WebProcess::sourceApplicationAuditData() const
{
#if PLATFORM(IOS_FAMILY)
    audit_token_t auditToken;
    ASSERT(parentProcessConnection());
    if (!parentProcessConnection() || !parentProcessConnection()->getAuditToken(auditToken))
        return nullptr;
    return adoptCF(CFDataCreate(nullptr, (const UInt8*)&auditToken, sizeof(auditToken)));
#else
    return nullptr;
#endif
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
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOSMAC)
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

    URL mainFrameURL = { URL(), mainFrame->url() };
    if (page.isSuspended()) {
        // Suspended page are navigated to about:blank upon suspension so we really want to report the previous URL.
        if (auto* coreFrame = mainFrame->coreFrame()) {
            if (auto* backHistoryItem = coreFrame->loader().history().previousItem())
                mainFrameURL = { URL(), backHistoryItem->urlString() };
        }
    }

    Ref<SecurityOrigin> mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
    String mainFrameOriginString;
    if (!mainFrameOrigin->isUnique())
        mainFrameOriginString = mainFrameOrigin->toRawString();
    else
        mainFrameOriginString = makeString(mainFrameURL.protocol(), ':'); // toRawString() is not supposed to work with unique origins, and would just return "://".

    // +[NSURL URLWithString:] returns nil when its argument is malformed. It's unclear when we would have a malformed URL here,
    // but it happens in practice according to <rdar://problem/14173389>. Leaving an assertion in to catch a reproducible case.
    ASSERT([NSURL URLWithString:mainFrameOriginString]);

    return [NSURL URLWithString:mainFrameOriginString];
}

#endif

#if PLATFORM(MAC)
static RetainPtr<NSArray<NSString *>> activePagesOrigins(const HashMap<uint64_t, RefPtr<WebPage>>& pageMap)
{
    RetainPtr<NSMutableArray<NSString *>> activeOrigins = adoptNS([[NSMutableArray alloc] init]);

    for (auto& page : pageMap.values()) {
        if (page->usesEphemeralSession())
            continue;

        if (NSURL *originAsURL = origin(*page))
            [activeOrigins addObject:WTF::userVisibleString(originAsURL)];
    }

    return activeOrigins;
}
#endif

void WebProcess::updateActivePages()
{
#if PLATFORM(MAC)
    auto activeOrigins = activePagesOrigins(m_pageMap);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [activeOrigins = WTFMove(activeOrigins)] {
        _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), CFSTR("LSActivePageUserVisibleOriginsKey"), (__bridge CFArrayRef)activeOrigins.get(), nullptr);
    });
#endif
}

void WebProcess::getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
#if PLATFORM(MAC)
    auto activeOriginsAsNSStrings = activePagesOrigins(m_pageMap);
    Vector<String> activeOrigins;
    activeOrigins.reserveInitialCapacity([activeOriginsAsNSStrings count]);
    for (NSString* activeOrigin in activeOriginsAsNSStrings.get())
        activeOrigins.uncheckedAppend(activeOrigin);
    completionHandler(WTFMove(activeOrigins));
#else
    completionHandler({ });
#endif
}

void WebProcess::updateCPULimit()
{
#if PLATFORM(MAC)
    std::optional<double> cpuLimit;

    // Use the largest limit among all pages in this process.
    for (auto& page : m_pageMap.values()) {
        auto pageCPULimit = page->cpuLimit();
        if (!pageCPULimit) {
            cpuLimit = std::nullopt;
            break;
        }
        if (!cpuLimit || pageCPULimit > cpuLimit.value())
            cpuLimit = pageCPULimit;
    }

    if (m_cpuLimit == cpuLimit)
        return;

    m_cpuLimit = cpuLimit;
    updateCPUMonitorState(CPUMonitorUpdateReason::LimitHasChanged);
#endif
}

void WebProcess::updateCPUMonitorState(CPUMonitorUpdateReason reason)
{
#if PLATFORM(MAC)
    if (!m_cpuLimit) {
        if (m_cpuMonitor)
            m_cpuMonitor->setCPULimit(std::nullopt);
        return;
    }

    if (!m_cpuMonitor) {
        m_cpuMonitor = std::make_unique<CPUMonitor>(cpuMonitoringInterval, [this](double cpuUsage) {
            RELEASE_LOG(PerformanceLogging, "%p - WebProcess exceeded CPU limit of %.1f%% (was using %.1f%%) hasVisiblePages? %d", this, m_cpuLimit.value() * 100, cpuUsage * 100, hasVisibleWebPage());
            parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedCPULimit(), 0);
        });
    } else if (reason == CPUMonitorUpdateReason::VisibilityHasChanged) {
        // If the visibility has changed, stop the CPU monitor before setting its limit. This is needed because the CPU usage can vary wildly based on visibility and we would
        // not want to report that a process has exceeded its background CPU limit even though most of the CPU time was used while the process was visible.
        m_cpuMonitor->setCPULimit(std::nullopt);
    }
    m_cpuMonitor->setCPULimit(m_cpuLimit.value());
#else
    UNUSED_PARAM(reason);
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

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
            ALLOW_DEPRECATED_DECLARATIONS_END
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

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(m_webProcess.transformHandlesToObjects(toImpl(wrapper.object)).get())]);
            ALLOW_DEPRECATED_DECLARATIONS_END
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

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif

            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
#if WK_API_ENABLED
            if (auto* controller = dynamic_objc_cast<WKWebProcessPlugInBrowserContextController>(object))
                return controller.handle;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(transformObjectsToHandles(toImpl(wrapper.object)).get())]);
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            return object;
        }
    };

    return ObjCObjectGraph::create(ObjCObjectGraph::transform(objectGraph.rootObject(), Transformer()).get());
}

void WebProcess::destroyRenderingResources()
{
#if !RELEASE_LOG_DISABLED
    MonotonicTime startTime = MonotonicTime::now();
#endif
    CABackingStoreCollectBlocking();
#if !RELEASE_LOG_DISABLED
    MonotonicTime endTime = MonotonicTime::now();
#endif
    RELEASE_LOG(ProcessSuspension, "%p - WebProcess::destroyRenderingResources() took %.2fms", this, (endTime - startTime).milliseconds());
}

// FIXME: This should live somewhere else, and it should have the implementation in line instead of calling out to WKSI.
void _WKSetCrashReportApplicationSpecificInformation(NSString *infoString)
{
    return setCrashReportApplicationSpecificInformation((__bridge CFStringRef)infoString);
}

#if PLATFORM(IOS_FAMILY)
void WebProcess::accessibilityProcessSuspendedNotification(bool suspended)
{
    UIAccessibilityPostNotification(kAXPidStatusChangedNotification, @{ @"pid" : @(getpid()), @"suspended" : @(suspended) });
}
#endif

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
void WebProcess::scrollerStylePreferenceChanged(bool useOverlayScrollbars)
{
    ScrollerStyle::setUseOverlayScrollbars(useOverlayScrollbars);

    ScrollbarTheme& theme = ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    static_cast<ScrollbarThemeMac&>(theme).preferencesChanged();
    
    NSScrollerStyle style = useOverlayScrollbars ? NSScrollerStyleOverlay : NSScrollerStyleLegacy;
    [NSScrollerImpPair _updateAllScrollerImpPairsForNewRecommendedScrollerStyle:style];
}

void WebProcess::displayConfigurationChanged(CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags)
{
    GraphicsContext3DManager::displayWasReconfigured(displayID, flags, nullptr);
}
    
void WebProcess::displayWasRefreshed(CGDirectDisplayID displayID)
{
    DisplayRefreshMonitorManager::sharedManager().displayWasUpdated(displayID);
}
#endif

void WebProcess::setMediaMIMETypes(const Vector<String> types)
{
    AVFoundationMIMETypeCache::singleton().setSupportedTypes(types);
}

} // namespace WebKit
