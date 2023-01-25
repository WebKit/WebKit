/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#import "AccessibilitySupportSPI.h"
#import "DefaultWebBrowserChecks.h"
#import "LegacyCustomProtocolManager.h"
#import "LogInitialization.h"
#import "Logging.h"
#import "NetworkConnectionToWebProcessMessages.h"
#import "NetworkProcessConnection.h"
#import "ObjCObjectGraph.h"
#import "ProcessAssertion.h"
#import "SandboxExtension.h"
#import "SandboxInitializationParameters.h"
#import "SharedBufferReference.h"
#import "WKAPICast.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WKTypeRefWrapper.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WebFrame.h"
#import "WebInspector.h"
#import "WebPage.h"
#import "WebPageGroupProxy.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessDataStoreParameters.h"
#import "WebProcessMessages.h"
#import "WebProcessProxyMessages.h"
#import "WebSleepDisablerClient.h"
#import "WebSystemSoundDelegate.h"
#import "WebsiteDataStoreParameters.h"
#import <JavaScriptCore/ConfigFile.h>
#import <JavaScriptCore/Options.h>
#import <WebCore/AVAssetMIMETypeCache.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/CPUMonitor.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/DisplayRefreshMonitorManager.h>
#import <WebCore/FontCache.h>
#import <WebCore/FontCacheCoreText.h>
#import <WebCore/FontCascade.h>
#import <WebCore/FrameView.h>
#import <WebCore/HistoryController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/IOSurface.h>
#import <WebCore/ImageDecoderCG.h>
#import <WebCore/LocalizedDeviceModel.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/MainThreadSharedTimer.h>
#import <WebCore/MemoryRelease.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/PageGroup.h>
#import <WebCore/PerformanceLogging.h>
#import <WebCore/PictureInPictureSupport.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ProcessCapabilities.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SWContextManager.h>
#import <WebCore/SystemBattery.h>
#import <WebCore/SystemSoundManager.h>
#import <WebCore/UTIUtilities.h>
#import <WebCore/WebMAudioUtilitiesCocoa.h>
#import <algorithm>
#import <dispatch/dispatch.h>
#import <mach/mach.h>
#import <objc/runtime.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/pthreadSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <stdio.h>
#import <wtf/FileSystem.h>
#import <wtf/Language.h>
#import <wtf/LogInitialization.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/OSLogSPI.h>

#if ENABLE(REMOTE_INSPECTOR)
#import <JavaScriptCore/RemoteInspector.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <bmalloc/MemoryStatusSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "RunningBoardServicesSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import <MobileCoreServices/MobileCoreServices.h>
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
#import "AppKitSPI.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WebSwitchingGPUClient.h"
#import <WebCore/DisplayConfigurationMonitor.h>
#import <WebCore/ScrollbarThemeMac.h>
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#import "WebCaptionPreferencesDelegate.h"
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#endif

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
#import <pal/spi/ios/DataDetectorsUISPI.h>
#endif

#import <WebCore/MediaAccessibilitySoftLink.h>
#import <pal/cf/VideoToolboxSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/VideoToolboxAdditions.h>
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
// FIXME: This is only for binary compatibility with versions of UIKit in macOS 11 that are missing the change in <rdar://problem/68524148>.
SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, UIKit, _UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor, void, (UIUserInterfaceIdiom idiom, CGFloat scaleFactor), (idiom, scaleFactor))
#endif

#define RELEASE_LOG_SESSION_ID (m_sessionID ? m_sessionID->toUInt64() : 0)
#define WEBPROCESS_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define WEBPROCESS_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)

#if PLATFORM(MAC)
SOFT_LINK_FRAMEWORK_IN_UMBRELLA(ApplicationServices, HIServices)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, HIServices, _AXSetAuditTokenIsAuthenticatedCallback, void, (AXAuditTokenIsAuthenticatedCallback callback), (callback))
#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(MAC)
static const Seconds cpuMonitoringInterval { 8_min };
static const double serviceWorkerCPULimit { 0.5 }; // 50% average CPU usage over 8 minutes.
#endif

void WebProcess::platformSetCacheModel(CacheModel)
{
}

#if USE(APPKIT)
static id NSApplicationAccessibilityFocusedUIElement(NSApplication*, SEL)
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([] () -> RetainPtr<id> {
        WebPage* page = WebProcess::singleton().focusedWebPage();
        if (!page || !page->accessibilityRemoteObject())
            return nil;
        return [page->accessibilityRemoteObject() accessibilityFocusedUIElement];
    });
}

#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
static void preventAppKitFromContactingLaunchServices(NSApplication*, SEL)
{
    // WebKit prohibits communication with Launch Services after entering the sandbox. This method override
    // prevents AppKit from attempting to update application information with Launch Services from the WebContent process.
}
#endif
#endif


#if PLATFORM(MAC)
static Boolean isAXAuthenticatedCallback(audit_token_t auditToken)
{
    bool authenticated = false;
    // IPC must be done on the main runloop, so dispatch it to avoid crashes when the secondary AX thread handles this callback.
    callOnMainRunLoopAndWait([&authenticated, auditToken] {
        auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::IsAXAuthenticated(auditToken), 0);
        std::tie(authenticated) = sendResult.takeReplyOr(false);
    });
    return authenticated;
}
#endif

static void softlinkDataDetectorsFrameworks()
{
#if ENABLE(DATA_DETECTION)
    PAL::isDataDetectorsCoreFrameworkAvailable();
#if PLATFORM(IOS_FAMILY)
    DataDetectorsUILibrary();
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(DATA_DETECTION)
}

static void initializeLogd()
{
    os_trace_set_mode(OS_TRACE_MODE_INFO | OS_TRACE_MODE_DEBUG);

    // Log a long message to make sure the XPC connection to the log daemon for oversized messages is opened.
    // This is needed to block launchd after the WebContent process has launched, since access to launchd is
    // required when opening new XPC connections.
    char stringWithSpaces[1024];
    memset(stringWithSpaces, ' ', sizeof(stringWithSpaces));
    stringWithSpaces[sizeof(stringWithSpaces) - 1] = 0;
    RELEASE_LOG(Process, "Initialized logd %s", stringWithSpaces);
}

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    WEBPROCESS_RELEASE_LOG(Process, "WebProcess::platformInitializeWebProcess");

    initializeLogd();
    
    applyProcessCreationParameters(parameters.auxiliaryProcessParameters);

    setQOS(parameters.latencyQOS, parameters.throughputQOS);
    
#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    if (canLoad_UIKit__UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor()) {
        auto [overrideUserInterfaceIdiom, overrideScaleFactor] = parameters.overrideUserInterfaceIdiomAndScale;
        softLink_UIKit__UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor(static_cast<UIUserInterfaceIdiom>(overrideUserInterfaceIdiom), overrideScaleFactor);
    }
#endif

    populateMobileGestaltCache(WTFMove(parameters.mobileGestaltExtensionHandle));

    m_uiProcessBundleIdentifier = parameters.uiProcessBundleIdentifier;

    WebCore::setPresentingApplicationBundleIdentifier(parameters.presentingApplicationBundleIdentifier);

#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::consumePermanently(parameters.uiProcessBundleResourcePathExtensionHandle);
#if ENABLE(MEDIA_STREAM)
    SandboxExtension::consumePermanently(parameters.audioCaptureExtensionHandle);
#endif
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    Inspector::RemoteInspector::setNeedMachSandboxExtension(!SandboxExtension::consumePermanently(parameters.enableRemoteWebInspectorExtensionHandles));
#endif
#endif

#if HAVE(VIDEO_RESTRICTED_DECODING)
#if PLATFORM(MAC)
    SandboxExtension::consumePermanently(parameters.trustdExtensionHandle);
#endif // PLATFORM(MAC)
#if USE(APPLE_INTERNAL_SDK)
    OptionSet<VideoDecoderBehavior> videoDecoderBehavior;
    
    if (parameters.enableDecodingHEIC) {
        ProcessCapabilities::setHEICDecodingEnabled(true);
        videoDecoderBehavior.add(VideoDecoderBehavior::EnableHEIC);
    }

    if (parameters.enableDecodingAVIF) {
        ProcessCapabilities::setAVIFDecodingEnabled(true);
        videoDecoderBehavior.add(VideoDecoderBehavior::EnableAVIF);
    }

    if (videoDecoderBehavior) {
        videoDecoderBehavior.add({ VideoDecoderBehavior::AvoidIOSurface, VideoDecoderBehavior::AvoidHardware });
        setVideoDecoderBehaviors(videoDecoderBehavior);
    }
#endif
#endif // HAVE(VIDEO_RESTRICTED_DECODING)

    // Disable NSURLCache.
    auto urlCache = adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]);
    [NSURLCache setSharedURLCache:urlCache.get()];

#if PLATFORM(MAC)
    WebCore::FontCache::setFontAllowlist(parameters.fontAllowList);
#endif

    m_compositingRenderServerPort = WTFMove(parameters.acceleratedCompositingPort);

    WebCore::registerMemoryReleaseNotifyCallbacks();
    MemoryPressureHandler::ReliefLogger::setLoggingEnabled(parameters.shouldEnableMemoryPressureReliefLogging);

    setEnhancedAccessibility(parameters.accessibilityEnhancedUserInterfaceEnabled);

#if PLATFORM(IOS_FAMILY)
    setCurrentUserInterfaceIdiomIsSmallScreen(parameters.currentUserInterfaceIdiomIsSmallScreen);
    setLocalizedDeviceModel(parameters.localizedDeviceModel);
    setContentSizeCategory(parameters.contentSizeCategory);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    setSupportsPictureInPicture(parameters.supportsPictureInPicture);
#endif
#endif

#if USE(APPKIT)
    // We don't need to talk to the Dock.
    [NSApplication _preventDockConnections];

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"NSApplicationCrashOnExceptions": @YES,
        @"ApplePersistence": @(-1) // Number -1 means NO + no logging to log and console. See <rdar://7749927>.
    }];

    // rdar://9118639 accessibilityFocusedUIElement in NSApplication defaults to use the keyWindow. Since there's
    // no window in WK2, NSApplication needs to use the focused page's focused element.
    Method methodToPatch = class_getInstanceMethod([NSApplication class], @selector(accessibilityFocusedUIElement));
    method_setImplementation(methodToPatch, (IMP)NSApplicationAccessibilityFocusedUIElement);
#endif

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
    RefPtr<SandboxExtension> launchServicesExtension;
    if (parameters.launchServicesExtensionHandle) {
        if ((launchServicesExtension = SandboxExtension::create(WTFMove(*parameters.launchServicesExtensionHandle)))) {
            bool ok = launchServicesExtension->consume();
            ASSERT_UNUSED(ok, ok);
        }
    }

    // Register the application. This will also check in with Launch Services.
    _RegisterApplication(nullptr, nullptr);
    
    // Update process name while holding the Launch Services sandbox extension
    updateProcessName(IsInProcessInitialization::Yes);

#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    // Disable relaunch on login. This is also done from -[NSApplication init] by dispatching -[NSApplication disableRelaunchOnLogin] on a non-main thread.
    // This will be in a race with the closing of the Launch Services connection, so call it synchronously here.
    // The cost of calling this should be small, and it is not expected to have any impact on performance.
    _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSPersistenceSuppressRelaunchAtLoginKey, kCFBooleanTrue, nullptr);
    
    // This is being called under WebPage::platformInitialize(), and may reach out to the Launch Services daemon once in the lifetime of the process.
    // Call this synchronously here while a sandbox extension to Launch Services is being held.
    [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:adoptNS([[WKAccessibilityWebPageObject alloc] init]).get()];

    auto method = class_getInstanceMethod([NSApplication class], @selector(_updateCanQuitQuietlyAndSafely));
    method_setImplementation(method, (IMP)preventAppKitFromContactingLaunchServices);

    // FIXME: Replace the constant 4 with kLSServerConnectionStatusReleaseNotificationsMask when available in the SDK, see <https://bugs.webkit.org/show_bug.cgi?id=220988>.
    _LSSetApplicationLaunchServicesServerConnectionStatus(kLSServerConnectionStatusDoNotConnectToServerMask | /*kLSServerConnectionStatusReleaseNotificationsMask*/ 4, nullptr);
#endif

    if (launchServicesExtension)
        launchServicesExtension->revoke();
#endif

#if PLATFORM(MAC)
    // App nap must be manually enabled when not running the NSApplication run loop.
    __CFRunLoopSetOptionsReason(__CFRunLoopOptionsEnableAppNap, CFSTR("Finished checkin as application - enable app nap"));
#endif

#if !ENABLE(CFPREFS_DIRECT_MODE)
    WTF::listenForLanguageChangeNotifications();
#endif

#if TARGET_OS_IPHONE
    // Priority decay on iOS 9 is impacting page load time so we fix the priority of the WebProcess' main thread (rdar://problem/22003112).
    pthread_set_fixedpriority_self();
#endif

#if ENABLE(WEBM_FORMAT_READER)
    PlatformMediaSessionManager::setWebMFormatReaderEnabled(DeprecatedGlobalSettings::webMFormatReaderEnabled());
#endif

#if ENABLE(VORBIS)
    PlatformMediaSessionManager::setVorbisDecoderEnabled(DeprecatedGlobalSettings::vorbisDecoderEnabled());
#endif

#if ENABLE(OPUS)
    PlatformMediaSessionManager::setOpusDecoderEnabled(DeprecatedGlobalSettings::opusDecoderEnabled());
#endif

    if (!parameters.mediaMIMETypes.isEmpty())
        setMediaMIMETypes(parameters.mediaMIMETypes);
    else {
        AVAssetMIMETypeCache::singleton().setCacheMIMETypesCallback([this](const Vector<String>& types) {
            parentProcessConnection()->send(Messages::WebProcessProxy::CacheMediaMIMETypes(types), 0);
        });
    }

    WebCore::setScreenProperties(parameters.screenProperties);

#if PLATFORM(MAC)
    scrollerStylePreferenceChanged(parameters.useOverlayScrollbars);
#endif

#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.compilerServiceExtensionHandles);
#endif

#if PLATFORM(IOS_FAMILY)
    SandboxExtension::consumePermanently(parameters.dynamicIOKitExtensionHandles);
#endif
    
    setSystemHasBattery(parameters.systemHasBattery);
    setSystemHasAC(parameters.systemHasAC);

#if PLATFORM(IOS_FAMILY)
    RenderThemeIOS::setCSSValueToSystemColorMap(WTFMove(parameters.cssValueToSystemColorMap));
    RenderThemeIOS::setFocusRingColor(parameters.focusRingColor);
#endif

    WebCore::sleepDisablerClient() = makeUnique<WebSleepDisablerClient>();

#if HAVE(FIG_PHOTO_DECOMPRESSION_SET_HARDWARE_CUTOFF) && !ENABLE(HARDWARE_JPEG)
    if (PAL::isMediaToolboxFrameworkAvailable() && PAL::canLoad_MediaToolbox_FigPhotoDecompressionSetHardwareCutoff())
        PAL::softLinkMediaToolboxFigPhotoDecompressionSetHardwareCutoff(kPALFigPhotoContainerFormat_JFIF, INT_MAX);
#endif

    SystemSoundManager::singleton().setSystemSoundDelegate(makeUnique<WebSystemSoundDelegate>());

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    WebCore::CaptionUserPreferencesMediaAF::setCaptionPreferencesDelegate(makeUnique<WebCaptionPreferencesDelegate>());
#endif

#if PLATFORM(MAC)
    if (canLoad_HIServices__AXSetAuditTokenIsAuthenticatedCallback())
        softLink_HIServices__AXSetAuditTokenIsAuthenticatedCallback(isAXAuthenticatedCallback);
#endif

#if HAVE(IOSURFACE)
    WebCore::IOSurface::setMaximumSize(parameters.maximumIOSurfaceSize);
    WebCore::IOSurface::setBytesPerRowAlignment(parameters.bytesPerRowIOSurfaceAlignment);
#endif

    accessibilityPreferencesDidChange(parameters.accessibilityPreferences);
#if PLATFORM(IOS_FAMILY)
    _AXSApplicationAccessibilitySetEnabled(parameters.applicationAccessibilityEnabled);
#endif

    disableURLSchemeCheckInDataDetectors();

    // Soft link frameworks related to Data Detection before we disconnect from launchd because these frameworks connect to
    // launchd temporarily at link time to register XPC services. See rdar://93598951 (my feature request to stop doing that)
    softlinkDataDetectorsFrameworks();
}

void WebProcess::platformSetWebsiteDataStoreParameters(WebProcessDataStoreParameters&& parameters)
{
#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::consumePermanently(parameters.applicationCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaKeyStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.javaScriptConfigurationDirectoryExtensionHandle);
#if ENABLE(ARKIT_INLINE_PREVIEW)
    SandboxExtension::consumePermanently(parameters.modelElementCacheDirectoryExtensionHandle);
#endif
#endif
#if PLATFORM(IOS_FAMILY)
    // FIXME: Does the web process still need access to the cookie storage directory? Probably not.
    if (auto& handle = parameters.cookieStorageDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
    if (auto& handle = parameters.containerCachesDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
    if (auto& handle = parameters.containerTemporaryDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
#endif

    if (!parameters.javaScriptConfigurationDirectory.isEmpty()) {
        String javaScriptConfigFile = parameters.javaScriptConfigurationDirectory + "/JSC.config";
        JSC::processConfigFile(javaScriptConfigFile.latin1().data(), "com.apple.WebKit.WebContent", m_uiProcessBundleIdentifier.latin1().data());
    }
}

void WebProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC)
    m_uiProcessName = parameters.uiProcessName;
#else
    UNUSED_PARAM(parameters);
#endif
}

std::optional<audit_token_t> WebProcess::auditTokenForSelf()
{
    audit_token_t auditToken = { 0 };
    mach_msg_type_number_t info_size = TASK_AUDIT_TOKEN_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN, reinterpret_cast<integer_t *>(&auditToken), &info_size);
    if (kr != KERN_SUCCESS) {
        WEBPROCESS_RELEASE_LOG_ERROR(Process, "Unable to get audit token for self. Error: %{public}s (%x)", mach_error_string(kr), kr);
        return std::nullopt;
    }
    return auditToken;
}

void WebProcess::updateProcessName(IsInProcessInitialization isInProcessInitialization)
{
#if PLATFORM(MAC)
    RetainPtr<NSString> applicationName;
    switch (m_processType) {
    case ProcessType::Inspector:
        applicationName = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ Web Inspector", "Visible name of Web Inspector's web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::ServiceWorker:
        applicationName = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ Service Worker (%@)", "Visible name of Service Worker process. The argument is the application name."), (NSString *)m_uiProcessName, (NSString *)m_registrableDomain.string()];
        break;
    case ProcessType::PrewarmedWebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ Web Content (Prewarmed)", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::CachedWebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ Web Content (Cached)", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::WebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ Web Content", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    }

#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    // During WebProcess initialization, we are still able to talk to LaunchServices to set the process name so there is no need to go
    // via the NetworkProcess. Prewarmed WebProcesses also do not have a network process connection until they are actually used by
    // a page.

    if (isInProcessInitialization == IsInProcessInitialization::No) {
        auto auditToken = auditTokenForSelf();
        if (!auditToken)
            return;
        String displayName = applicationName.get();
        ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UpdateActivePages(displayName, Vector<String>(), *auditToken), 0);
        return;
    }
#endif // ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)

    // Note that it is important for _RegisterApplication() to have been called before setting the display name.
    auto error = _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName.get(), nullptr);
    ASSERT(!error);
    if (error) {
        WEBPROCESS_RELEASE_LOG_ERROR(Process, "updateProcessName: Failed to set the display name of the WebContent process, error code=%ld", static_cast<long>(error));
        return;
    }
#if ASSERT_ENABLED
    // It is possible for _LSSetApplicationInformationItem() to return 0 and yet fail to set the display name so we make sure the display name has actually been set.
    String actualApplicationName = adoptCF((CFStringRef)_LSCopyApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey)).get();
    ASSERT(!actualApplicationName.isEmpty());
#endif
#else
    UNUSED_PARAM(isInProcessInitialization);
#endif // PLATFORM(MAC)
}

#if PLATFORM(IOS_FAMILY)
static NSString *webProcessLoaderAccessibilityBundlePath()
{
    NSString *path = (__bridge NSString *)GSSystemRootDirectory();
#if PLATFORM(MACCATALYST)
    path = [path stringByAppendingPathComponent:@"System/iOSSupport"];
#endif
    return [path stringByAppendingPathComponent:@"System/Library/AccessibilityBundles/WebProcessLoader.axbundle"];
}
#endif

static void registerWithAccessibility()
{
#if USE(APPKIT)
    [NSAccessibilityRemoteUIElement setRemoteUIApp:YES];
#endif

#if PLATFORM(IOS_FAMILY)
    NSString *bundlePath = webProcessLoaderAccessibilityBundlePath();
    NSError *error = nil;
    if (![[NSBundle bundleWithPath:bundlePath] loadAndReturnError:&error])
        LOG_ERROR("Failed to load accessibility bundle at %@: %@", bundlePath, error);
#endif
}

#if USE(OS_STATE)

RetainPtr<NSDictionary> WebProcess::additionalStateForDiagnosticReport() const
{
    auto stateDictionary = adoptNS([[NSMutableDictionary alloc] init]);
    {
        auto memoryUsageStats = adoptNS([[NSMutableDictionary alloc] init]);
        for (auto& it : PerformanceLogging::memoryUsageStatistics(ShouldIncludeExpensiveComputations::Yes)) {
            auto keyString = adoptNS([[NSString alloc] initWithUTF8String:it.key]);
            [memoryUsageStats setObject:@(it.value) forKey:keyString.get()];
        }
        [stateDictionary setObject:memoryUsageStats.get() forKey:@"Memory Usage Stats"];
    }
    {
        auto jsObjectCounts = adoptNS([[NSMutableDictionary alloc] init]);
        for (auto& it : PerformanceLogging::javaScriptObjectCounts()) {
            auto keyString = adoptNS([[NSString alloc] initWithUTF8String:it.key]);
            [jsObjectCounts setObject:@(it.value) forKey:keyString.get()];
        }
        [stateDictionary setObject:jsObjectCounts.get() forKey:@"JavaScript Object Counts"];
    }
    auto pageLoadTimes = createNSArray(m_pageMap.values(), [] (auto& page) -> id {
        if (page->usesEphemeralSession())
            return nil;

        return [NSDate dateWithTimeIntervalSince1970:page->loadCommitTime().secondsSinceEpoch().seconds()];
    });
    auto websamStateDescription = MemoryPressureHandler::processStateDescription().createNSString();
    [stateDictionary setObject:websamStateDescription.get() forKey:@"Websam State"];

    // Adding an empty array to the process state may provide an
    // indication of the existance of private sessions, which we'd like
    // to hide, so don't add empty arrays.
    if ([pageLoadTimes count])
        [stateDictionary setObject:pageLoadTimes.get() forKey:@"Page Load Times"];

    // --- Possibly add other state here as other entries in the dictionary. ---
    return stateDictionary;
}

#endif // USE(OS_STATE)

#if ENABLE(WEBCONTENT_CRASH_TESTING)
NEVER_INLINE NO_RETURN_DUE_TO_CRASH static void deliberateCrashForTesting()
{
    CRASH();
}
#endif

void WebProcess::platformInitializeProcess(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC)
    // Deny the WebContent process access to the WindowServer.
    // This call will not succeed if there are open WindowServer connections at this point.
    auto retval = CGSSetDenyWindowServerConnections(true);
    RELEASE_ASSERT(retval == kCGErrorSuccess);

    SwitchingGPUClient::setSingleton(WebSwitchingGPUClient::singleton());
    MainThreadSharedTimer::shouldSetupPowerObserver() = false;
#endif // PLATFORM(MAC)

    if (parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("inspector-process"_s) == "1"_s)
        m_processType = ProcessType::Inspector;
#if ENABLE(SERVICE_WORKER)
    else if (parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("service-worker-process"_s) == "1"_s) {
        m_processType = ProcessType::ServiceWorker;
#if PLATFORM(MAC)
        m_registrableDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("registrable-domain"_s));
#endif
    }
#endif
    else if (parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("is-prewarmed"_s) == "1"_s)
        m_processType = ProcessType::PrewarmedWebContent;
    else
        m_processType = ProcessType::WebContent;

#if ENABLE(WEBCONTENT_CRASH_TESTING)
    if (parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("is-webcontent-crashy"_s) == "1"_s)
        deliberateCrashForTesting();
#endif

#if USE(OS_STATE)
    registerWithStateDumper("WebContent state"_s);
#endif

#if HAVE(APP_SSO) || PLATFORM(MACCATALYST)
    [NSURLSession _disableAppSSO];
#endif

#if HAVE(CSCHECKFIXDISABLE)
    // _CSCheckFixDisable() needs to be called before checking in with Launch Services. The WebContent process is checking in
    // with Launch Services in WebProcess::platformInitializeWebProcess when calling +[NSApplication _accessibilityInitialize].
    _CSCheckFixDisable();
#endif
}

#if USE(APPKIT)
void WebProcess::stopRunLoop()
{
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
    AuxiliaryProcess::stopNSRunLoop();
#else
    AuxiliaryProcess::stopNSAppRunLoop();
#endif
}
#endif

void WebProcess::platformTerminate()
{
    AVAssetMIMETypeCache::singleton().setCacheMIMETypesCallback(nullptr);
}

RetainPtr<CFDataRef> WebProcess::sourceApplicationAuditData() const
{
#if USE(SOURCE_APPLICATION_AUDIT_DATA)
    ASSERT(parentProcessConnection());
    if (!parentProcessConnection())
        return nullptr;
    std::optional<audit_token_t> auditToken = parentProcessConnection()->getAuditToken();
    if (!auditToken)
        return nullptr;
    return adoptCF(CFDataCreate(nullptr, (const UInt8*)&*auditToken, sizeof(*auditToken)));
#else
    return nullptr;
#endif
}

void WebProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    // Need to override the default, because service has a different bundle ID.
    auto webKitBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];

#if defined(USE_VORBIS_AUDIOCOMPONENT_WORKAROUND)
    // We need to initialize the Vorbis decoder before the sandbox gets setup; this is a one off action.
    WebCore::registerVorbisDecoderIfNeeded();
#endif

    sandboxParameters.setOverrideSandboxProfilePath(makeString(String([webKitBundle resourcePath]), "/com.apple.WebProcess.sb"));

    AuxiliaryProcess::initializeSandbox(parameters, sandboxParameters);
#endif
}

#if PLATFORM(MAC)

static NSURL *origin(WebPage& page)
{
    auto& mainFrame = page.mainWebFrame();

    URL mainFrameURL = mainFrame.url();
    Ref<SecurityOrigin> mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
    String mainFrameOriginString;
    if (!mainFrameOrigin->isOpaque())
        mainFrameOriginString = mainFrameOrigin->toRawString();
    else
        mainFrameOriginString = makeString(mainFrameURL.protocol(), ':'); // toRawString() is not supposed to work with opaque origins, and would just return "://".

    // +[NSURL URLWithString:] returns nil when its argument is malformed. It's unclear when we would have a malformed URL here,
    // but it happens in practice according to <rdar://problem/14173389>. Leaving an assertion in to catch a reproducible case.
    ASSERT([NSURL URLWithString:mainFrameOriginString]);

    return [NSURL URLWithString:mainFrameOriginString];
}

#endif

#if PLATFORM(MAC)
static Vector<String> activePagesOrigins(const HashMap<PageIdentifier, RefPtr<WebPage>>& pageMap)
{
    Vector<String> origins;
    for (auto& page : pageMap.values()) {
        if (page->usesEphemeralSession())
            continue;

        NSURL *originAsURL = origin(*page);
        if (!originAsURL)
            continue;

        origins.append(WTF::userVisibleString(originAsURL));
    }
    return origins;
}
#endif

void WebProcess::getProcessDisplayName(CompletionHandler<void(String&&)>&& completionHandler)
{
#if PLATFORM(MAC)
    auto auditToken = auditTokenForSelf();
    if (!auditToken)
        return completionHandler({ });
    ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::GetProcessDisplayName(*auditToken), WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

void WebProcess::updateActivePages(const String& overrideDisplayName)
{
#if PLATFORM(MAC)
#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    auto auditToken = auditTokenForSelf();
    if (!auditToken)
        return;
    ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UpdateActivePages(overrideDisplayName, activePagesOrigins(m_pageMap), *auditToken), 0);
#else
    if (!overrideDisplayName) {
        RunLoop::main().dispatch([activeOrigins = activePagesOrigins(m_pageMap)] {
            _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), CFSTR("LSActivePageUserVisibleOriginsKey"), (__bridge CFArrayRef)createNSArray(activeOrigins).get(), nullptr);
        });
    } else {
        RunLoop::main().dispatch([name = overrideDisplayName.createCFString()] {
            _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, name.get(), nullptr);
        });
    }
#endif
#endif
}

void WebProcess::getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
#if PLATFORM(MAC)
    completionHandler(activePagesOrigins(m_pageMap));
#else
    completionHandler({ });
#endif
}

void WebProcess::updateCPULimit()
{
#if PLATFORM(MAC)
    std::optional<double> cpuLimit;
    if (m_processType == ProcessType::ServiceWorker)
        cpuLimit = serviceWorkerCPULimit;
    else {
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
        m_cpuMonitor = makeUnique<CPUMonitor>(cpuMonitoringInterval, [this](double cpuUsage) {
            if (m_processType == ProcessType::ServiceWorker)
                WEBPROCESS_RELEASE_LOG_ERROR(ProcessSuspension, "updateCPUMonitorState: Service worker process exceeded CPU limit of %.1f%% (was using %.1f%%)", m_cpuLimit.value() * 100, cpuUsage * 100);
            else
                WEBPROCESS_RELEASE_LOG_ERROR(ProcessSuspension, "updateCPUMonitorState: WebProcess exceeded CPU limit of %.1f%% (was using %.1f%%) hasVisiblePages? %d", m_cpuLimit.value() * 100, cpuUsage * 100, hasVisibleWebPage());
            parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedCPULimit(), 0);
        });
    } else if (reason == CPUMonitorUpdateReason::VisibilityHasChanged) {
        // If the visibility has changed, stop the CPU monitor before setting its limit. This is needed because the CPU usage can vary wildly based on visibility and we would
        // not want to report that a process has exceeded its background CPU limit even though most of the CPU time was used while the process was visible.
        m_cpuMonitor->setCPULimit(std::nullopt);
    }
    m_cpuMonitor->setCPULimit(m_cpuLimit);
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
            if (dynamic_objc_cast<WKBrowsingContextHandle>(object))
                return true;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
            ALLOW_DEPRECATED_DECLARATIONS_END
            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
            if (auto* handle = dynamic_objc_cast<WKBrowsingContextHandle>(object)) {
                if (auto* webPage = m_webProcess.webPage(makeObjectIdentifier<WebCore::PageIdentifierType>(handle._webPageID)))
                    return wrapper(*webPage);

                return [NSNull null];
            }

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(m_webProcess.transformHandlesToObjects(toImpl(wrapper.object)).get())]);
            ALLOW_DEPRECATED_DECLARATIONS_END
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
            if (dynamic_objc_cast<WKWebProcessPlugInBrowserContextController>(object))
                return true;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
            ALLOW_DEPRECATED_DECLARATIONS_END
            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
            if (auto* controller = dynamic_objc_cast<WKWebProcessPlugInBrowserContextController>(object))
                return controller.handle;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(transformObjectsToHandles(toImpl(wrapper.object)).get())]);
            ALLOW_DEPRECATED_DECLARATIONS_END
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
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "destroyRenderingResources: took %.2fms", (endTime - startTime).milliseconds());
}

#if PLATFORM(IOS_FAMILY)

void WebProcess::userInterfaceIdiomDidChange(bool isSmallScreen)
{
    WebKit::setCurrentUserInterfaceIdiomIsSmallScreen(isSmallScreen);
}

bool WebProcess::shouldFreezeOnSuspension() const
{
    switch (m_processType) {
    case ProcessType::Inspector:
    case ProcessType::ServiceWorker:
    case ProcessType::PrewarmedWebContent:
    case ProcessType::CachedWebContent:
        return false;
    case ProcessType::WebContent:
        break;
    }

    for (auto& page : m_pageMap.values()) {
        if (!page->isSuspended())
            return true;
    }

    // Since all of the pages in this process were suspended, we should not bother freezing it.
    return false;
}

void WebProcess::updateFreezerStatus()
{
    bool isFreezable = shouldFreezeOnSuspension();
    auto result = memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE, getpid(), isFreezable ? 1 : 0, nullptr, 0);
    if (result)
        WEBPROCESS_RELEASE_LOG_ERROR(ProcessSuspension, "updateFreezerStatus: isFreezable=%d, error=%d", isFreezable, result);
    else
        WEBPROCESS_RELEASE_LOG(ProcessSuspension, "updateFreezerStatus: isFreezable=%d, success", isFreezable);
}

#endif

#if PLATFORM(MAC)
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

void WebProcess::displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags flags)
{
    DisplayConfigurationMonitor::singleton().dispatchDisplayWasReconfigured(flags);
}
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
static float currentBacklightLevel()
{
    return WebProcess::singleton().backlightLevel();
}

void WebProcess::backlightLevelDidChange(float backlightLevel)
{
    m_backlightLevel = backlightLevel;

    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            Method methodToPatch = class_getInstanceMethod([UIDevice class], @selector(_backlightLevel));
            method_setImplementation(methodToPatch, reinterpret_cast<IMP>(currentBacklightLevel));
        });
}
#endif

void WebProcess::accessibilityPreferencesDidChange(const AccessibilityPreferences& preferences)
{
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    auto appID = CFSTR("com.apple.WebKit.WebContent");
    auto reduceMotionEnabled = preferences.reduceMotionEnabled;
    if (_AXSReduceMotionEnabledApp(appID) != reduceMotionEnabled)
        _AXSSetReduceMotionEnabledApp(reduceMotionEnabled, appID);
    auto increaseButtonLegibility = preferences.increaseButtonLegibility;
    if (_AXSIncreaseButtonLegibilityApp(appID) != increaseButtonLegibility)
        _AXSSetIncreaseButtonLegibilityApp(increaseButtonLegibility, appID);
    auto enhanceTextLegibility = preferences.enhanceTextLegibility;
    if (_AXSEnhanceTextLegibilityEnabledApp(appID) != enhanceTextLegibility)
        _AXSSetEnhanceTextLegibilityEnabledApp(enhanceTextLegibility, appID);
    auto darkenSystemColors = preferences.darkenSystemColors;
    if (_AXDarkenSystemColorsApp(appID) != darkenSystemColors)
        _AXSSetDarkenSystemColorsApp(darkenSystemColors, appID);
    auto invertColorsEnabled = preferences.invertColorsEnabled;
    if (_AXSInvertColorsEnabledApp(appID) != invertColorsEnabled)
        _AXSInvertColorsSetEnabledApp(invertColorsEnabled, appID);
#endif
    setOverrideEnhanceTextLegibility(preferences.enhanceTextLegibilityOverall);
    FontCache::invalidateAllFontCaches();
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    m_imageAnimationEnabled = preferences.imageAnimationEnabled;
    for (auto& page : m_pageMap.values())
        page->updateImageAnimationEnabled();
#endif
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
void WebProcess::setMediaAccessibilityPreferences(WebCore::CaptionUserPreferences::CaptionDisplayMode captionDisplayMode, const Vector<String>& preferredLanguages)
{
    WebCore::CaptionUserPreferencesMediaAF::setCachedCaptionDisplayMode(captionDisplayMode);
    WebCore::CaptionUserPreferencesMediaAF::setCachedPreferredLanguages(preferredLanguages);

    for (auto& pageGroup : m_pageGroupMap.values()) {
        if (auto* captionPreferences = pageGroup->corePageGroup()->captionPreferences())
            captionPreferences->captionPreferencesChanged();
    }
}
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void WebProcess::colorPreferencesDidChange()
{
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), CFSTR("NSColorLocalPreferencesChangedNotification"), nullptr, nullptr, true);
}
#endif

#if ENABLE(REMOTE_INSPECTOR)
void WebProcess::enableRemoteWebInspector()
{
    Inspector::RemoteInspector::singleton();
}
#endif

void WebProcess::setMediaMIMETypes(const Vector<String> types)
{
    auto& cache = AVAssetMIMETypeCache::singleton();
    if (cache.isEmpty())
        cache.addSupportedTypes(types);
}

#if ENABLE(CFPREFS_DIRECT_MODE)

#if USE(APPKIT)
static const WTF::String& userAccentColorPreferenceKey()
{
    static NeverDestroyed<WTF::String> userAccentColorPreferenceKey(MAKE_STATIC_STRING_IMPL("AppleAccentColor"));
    return userAccentColorPreferenceKey;
}

static const WTF::String& userHighlightColorPreferenceKey()
{
    static NeverDestroyed<WTF::String> userHighlightColorPreferenceKey(MAKE_STATIC_STRING_IMPL("AppleHighlightColor"));
    return userHighlightColorPreferenceKey;
}

static const WTF::String& invertColorsPreferenceKey()
{
    static NeverDestroyed<WTF::String> key(MAKE_STATIC_STRING_IMPL("whiteOnBlack"));
    return key;
}
#endif

static const WTF::String& captionProfilePreferenceKey()
{
    static NeverDestroyed<WTF::String> key(MAKE_STATIC_STRING_IMPL("MACaptionActiveProfile"));
    return key;
}

void WebProcess::dispatchSimulatedNotificationsForPreferenceChange(const String& key)
{
#if USE(APPKIT)
    // Ordinarily, other parts of the system ensure that this notification is posted after this default is changed.
    // However, since we synchronize defaults to the Web Content process using a mechanism not known to the rest
    // of the system, we must re-post the notification in the Web Content process after updating the default.
    
    if (key == userAccentColorPreferenceKey()) {
        auto notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter postNotificationName:@"kCUINotificationAquaColorVariantChanged" object:nil];
        [notificationCenter postNotificationName:@"NSSystemColorsWillChangeNotification" object:nil];
        [notificationCenter postNotificationName:NSSystemColorsDidChangeNotification object:nil];
    } else if (key == userHighlightColorPreferenceKey()) {
        auto notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter postNotificationName:@"NSSystemColorsWillChangeNotification" object:nil];
        [notificationCenter postNotificationName:NSSystemColorsDidChangeNotification object:nil];
    }
#endif
    if (key == captionProfilePreferenceKey()) {
        auto notificationCenter = CFNotificationCenterGetLocalCenter();
        CFNotificationCenterPostNotification(notificationCenter, kMAXCaptionAppearanceSettingsChangedNotification, nullptr, nullptr, true);
    }
}

void WebProcess::handlePreferenceChange(const String& domain, const String& key, id value)
{
    if (key == "AppleLanguages"_s) {
        // We need to set AppleLanguages for the volatile domain, similarly to what we do in XPCServiceMain.mm.
        NSDictionary *existingArguments = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
        RetainPtr<NSMutableDictionary> newArguments = adoptNS([existingArguments mutableCopy]);
        [newArguments setValue:value forKey:@"AppleLanguages"];
        [[NSUserDefaults standardUserDefaults] setVolatileDomain:newArguments.get() forName:NSArgumentDomain];

        WTF::languageDidChange();
    }

#if USE(APPKIT)
    auto cfKey = key.createCFString();
    if (CFEqual(cfKey.get(), kAXInterfaceReduceMotionKey) || CFEqual(cfKey.get(), kAXInterfaceIncreaseContrastKey) || key == invertColorsPreferenceKey()) {
        [NSWorkspace _invalidateAccessibilityDisplayValues];
        for (auto& page : m_pageMap.values())
            page->accessibilitySettingsDidChange();
    }
#endif

    AuxiliaryProcess::handlePreferenceChange(domain, key, value);
}

void WebProcess::notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    preferenceDidUpdate(domain, key, encodedValue);
}
#endif

void WebProcess::grantAccessToAssetServices(Vector<WebKit::SandboxExtension::Handle>&& assetServicesHandles)
{
    if (m_assetServicesExtensions.size())
        return;
    for (auto& handle : assetServicesHandles) {
        auto extension = SandboxExtension::create(WTFMove(handle));
        if (!extension)
            continue;
        extension->consume();
        m_assetServicesExtensions.append(extension);
    }
}

void WebProcess::revokeAccessToAssetServices()
{
    for (auto extension : m_assetServicesExtensions)
        extension->revoke();
    m_assetServicesExtensions.clear();
}

void WebProcess::disableURLSchemeCheckInDataDetectors() const
{
#if HAVE(DDRESULT_DISABLE_URL_SCHEME_CHECKING)
    if (PAL::canLoad_DataDetectorsCore_DDResultDisableURLSchemeChecking())
        PAL::softLinkDataDetectorsCoreDDResultDisableURLSchemeChecking();
#endif
}

void WebProcess::switchFromStaticFontRegistryToUserFontRegistry(Vector<WebKit::SandboxExtension::Handle>&& fontMachExtensionHandles)
{
    SandboxExtension::consumePermanently(fontMachExtensionHandles);
#if HAVE(STATIC_FONT_REGISTRY)
    CTFontManagerEnableAllUserFonts(true);
#endif
}

void WebProcess::setScreenProperties(const WebCore::ScreenProperties& properties)
{
    WebCore::setScreenProperties(properties);
    for (auto& page : m_pageMap.values())
        page->screenPropertiesDidChange();
#if PLATFORM(MAC)
    updatePageScreenProperties();
#endif
}

#if PLATFORM(MAC)
void WebProcess::updatePageScreenProperties()
{
#if !HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
    // Only override HDR support at the MediaToolbox level if AVPlayer.videoRangeOverride support is
    // not present, as the MediaToolbox override functionality is both duplicative and process global.
    if (hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer)) {
        setShouldOverrideScreenSupportsHighDynamicRange(false, false);
        return;
    }

    bool allPagesAreOnHDRScreens = allOf(m_pageMap.values(), [] (auto& page) {
        return page && screenSupportsHighDynamicRange(page->mainFrameView());
    });
    setShouldOverrideScreenSupportsHighDynamicRange(true, allPagesAreOnHDRScreens);
#endif
}
#endif

void WebProcess::unblockServicesRequiredByAccessibility(const Vector<SandboxExtension::Handle>& handleArray)
{
#if PLATFORM(IOS_FAMILY)
    bool consumed = SandboxExtension::consumePermanently(handleArray);
    ASSERT_UNUSED(consumed, consumed);
#endif
    registerWithAccessibility();
}

void WebProcess::powerSourceDidChange(bool hasAC)
{
    setSystemHasAC(hasAC);
}

void WebProcess::willWriteToPasteboardAsynchronously(const String& pasteboardName)
{
    m_pendingPasteboardWriteCounts.add(pasteboardName);
}

void WebProcess::didWriteToPasteboardAsynchronously(const String& pasteboardName)
{
    m_pendingPasteboardWriteCounts.remove(pasteboardName);
}

void WebProcess::waitForPendingPasteboardWritesToFinish(const String& pasteboardName)
{
    while (m_pendingPasteboardWriteCounts.contains(pasteboardName)) {
        if (!parentProcessConnection()->waitForAndDispatchImmediately<Messages::WebProcess::DidWriteToPasteboardAsynchronously>(0, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives)) {
            m_pendingPasteboardWriteCounts.removeAll(pasteboardName);
            break;
        }
    }
}

#if PLATFORM(MAC)
void WebProcess::systemWillPowerOn()
{
    MainThreadSharedTimer::restartSharedTimer();
}

void WebProcess::systemWillSleep()
{
    if (PlatformMediaSessionManager::sharedManagerIfExists())
        PlatformMediaSessionManager::sharedManager().processSystemWillSleep();
}

void WebProcess::systemDidWake()
{
    if (PlatformMediaSessionManager::sharedManagerIfExists())
        PlatformMediaSessionManager::sharedManager().processSystemDidWake();
}
#endif

#if PLATFORM(MAC)
void WebProcess::openDirectoryCacheInvalidated(SandboxExtension::Handle&& handle, SandboxExtension::Handle&& machBootstrapHandle)
{
    auto bootstrapExtension = SandboxExtension::create(WTFMove(machBootstrapHandle));

    if (bootstrapExtension)
        bootstrapExtension->consume();
    
    AuxiliaryProcess::openDirectoryCacheInvalidated(WTFMove(handle));
    
    if (bootstrapExtension)
        bootstrapExtension->revoke();
}
#endif

} // namespace WebKit

#undef RELEASE_LOG_SESSION_ID
#undef WEBPROCESS_RELEASE_LOG
#undef WEBPROCESS_RELEASE_LOG_ERROR
