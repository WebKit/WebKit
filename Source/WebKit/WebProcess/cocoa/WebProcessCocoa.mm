/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#import "GPUProcessConnectionParameters.h"
#import "LegacyCustomProtocolManager.h"
#import "LogInitialization.h"
#import "Logging.h"
#import "NetworkConnectionToWebProcessMessages.h"
#import "NetworkProcessConnection.h"
#import "ObjCObjectGraph.h"
#import "ProcessAssertion.h"
#import "SandboxExtension.h"
#import "SandboxInitializationParameters.h"
#import "WKAPICast.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WKTypeRefWrapper.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WebFrame.h"
#import "WebInspector.h"
#import "WebPage.h"
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
#import <WebCore/DisplayRefreshMonitorManager.h>
#import <WebCore/FontCache.h>
#import <WebCore/FontCascade.h>
#import <WebCore/HistoryController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/IOSurface.h>
#import <WebCore/LocalizedDeviceModel.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/MainThreadSharedTimer.h>
#import <WebCore/MemoryRelease.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/PerformanceLogging.h>
#import <WebCore/PictureInPictureSupport.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/SWContextManager.h>
#import <WebCore/SystemBattery.h>
#import <WebCore/SystemSoundManager.h>
#import <WebCore/UTIUtilities.h>
#import <WebCore/VersionChecks.h>
#import <algorithm>
#import <dispatch/dispatch.h>
#import <objc/runtime.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/pthreadSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <stdio.h>
#import <wtf/FileSystem.h>
#import <wtf/Language.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(REMOTE_INSPECTOR)
#import <JavaScriptCore/RemoteInspector.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <bmalloc/MemoryStatusSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "AccessibilitySupportSPI.h"
#import "RunningBoardServicesSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIAccessibility.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
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
#import <WebCore/GraphicsContextGLOpenGLManager.h>
#import <WebCore/ScrollbarThemeMac.h>
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#endif

#if USE(OS_STATE)
#import <os/state_private.h>
#endif

#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
// FIXME: This is only for binary compatibility with versions of UIKit in macOS 11 that are missing the change in <rdar://problem/68524148>.
SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, UIKit, _UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor, void, (UIUserInterfaceIdiom idiom, CGFloat scaleFactor), (idiom, scaleFactor))
#endif

SOFT_LINK_FRAMEWORK(CoreServices)
SOFT_LINK_CLASS(CoreServices, _LSDService)
SOFT_LINK_CLASS(CoreServices, _LSDOpenService)

#define RELEASE_LOG_SESSION_ID (m_sessionID ? m_sessionID->toUInt64() : 0)
#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)

#if PLATFORM(MAC)
SOFT_LINK_FRAMEWORK_IN_UMBRELLA(ApplicationServices, HIServices)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, HIServices, _AXSetAuditTokenIsAuthenticatedCallback, void, (AXAuditTokenIsAuthenticatedCallback callback), (callback))
#endif

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_LIBRARY(libAccessibility)
SOFT_LINK_OPTIONAL(libAccessibility, _AXSUpdateWebAccessibilitySettings, void, (), ());
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
    WebPage* page = WebProcess::singleton().focusedWebPage();
    if (!page || !page->accessibilityRemoteObject())
        return 0;

    return [page->accessibilityRemoteObject() accessibilityFocusedUIElement];
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
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::IsAXAuthenticated(auditToken), Messages::WebProcessProxy::IsAXAuthenticated::Reply(authenticated), 0);
    return authenticated;
}
#endif

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    setQOS(parameters.latencyQOS, parameters.throughputQOS);
    
    SandboxExtension::consumePermanently(parameters.diagnosticsExtensionHandles);

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    if (canLoad_UIKit__UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor()) {
        auto [overrideUserInterfaceIdiom, overrideScaleFactor] = parameters.overrideUserInterfaceIdiomAndScale;
        softLink_UIKit__UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor(static_cast<UIUserInterfaceIdiom>(overrideUserInterfaceIdiom), overrideScaleFactor);
    }
#endif

    if (parameters.mobileGestaltExtensionHandle) {
        if (auto extension = SandboxExtension::create(WTFMove(*parameters.mobileGestaltExtensionHandle))) {
            bool ok = extension->consume();
            ASSERT_UNUSED(ok, ok);
            // If we have an extension handle for MobileGestalt, it means the MobileGestalt cache is invalid.
            // In this case, we perform a set of MobileGestalt queries while having access to the daemon,
            // which will populate the MobileGestalt in-memory cache with correct values.
            // The set of queries below was determined by finding all possible queries that have cachable
            // values, and would reach out to the daemon for the answer. That way, the in-memory cache
            // should be identical to a valid MobileGestalt cache after having queried all of these values.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
            MGGetFloat32Answer(kMGQMainScreenScale, 0);
            MGGetSInt32Answer(kMGQMainScreenPitch, 0);
            MGGetSInt32Answer(kMGQMainScreenClass, MGScreenClassPad2);
            MGGetBoolAnswer(kMGQAppleInternalInstallCapability);
            MGGetBoolAnswer(kMGQiPadCapability);
            auto deviceName = adoptCF(MGCopyAnswer(kMGQDeviceName, nullptr));
            MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid);
            MGGetBoolAnswer(kMGQHasExtendedColorDisplay);
            MGGetFloat32Answer(kMGQDeviceCornerRadius, 0);
            MGGetBoolAnswer(kMGQSupportsForceTouch);

            auto answer = adoptCF(MGCopyAnswer(kMGQBluetoothCapability, nullptr));
            answer = MGCopyAnswer(kMGQDeviceProximityCapability, nullptr);
            answer = MGCopyAnswer(kMGQDeviceSupportsARKit, nullptr);
            answer = MGCopyAnswer(kMGQTimeSyncCapability, nullptr);
            answer = MGCopyAnswer(kMGQWAPICapability, nullptr);
            answer = MGCopyAnswer(kMGQMainDisplayRotation, nullptr);
#endif
            ok = extension->revoke();
            ASSERT_UNUSED(ok, ok);
        }
    }

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WebCore::initializeLogChannelsIfNecessary(parameters.webCoreLoggingChannels);
    WebKit::initializeLogChannelsIfNecessary(parameters.webKitLoggingChannels);
#endif

    m_uiProcessBundleIdentifier = parameters.uiProcessBundleIdentifier;

#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::consumePermanently(parameters.uiProcessBundleResourcePathExtensionHandle);
#if ENABLE(MEDIA_STREAM)
    SandboxExtension::consumePermanently(parameters.audioCaptureExtensionHandle);
#endif
#if PLATFORM(IOS_FAMILY)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerTemporaryDirectoryExtensionHandle);
#endif
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    if (SandboxExtension::consumePermanently(parameters.enableRemoteWebInspectorExtensionHandle))
        Inspector::RemoteInspector::setNeedMachSandboxExtension(false);
#endif
#endif

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
    setCurrentUserInterfaceIdiomIsPadOrMac(parameters.currentUserInterfaceIdiomIsPad);
    setLocalizedDeviceModel(parameters.localizedDeviceModel);
    RenderThemeIOS::setContentSizeCategory(parameters.contentSizeCategory);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    setSupportsPictureInPicture(parameters.supportsPictureInPicture);
#endif
#endif

#if USE(APPKIT)
    // We don't need to talk to the Dock.
    [NSApplication _preventDockConnections];

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{ @"NSApplicationCrashOnExceptions" : @YES, @"ApplePersistence" : @NO }];

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

    // Need to initialize accessibility for VoiceOver to work when the WebContent process is using NSRunLoop.
    // Currently, it is also needed to allocate and initialize an NSApplication object.
    // This method call will also call RegisterApplication, so there is no need for us to call this or
    // check in with Launch Services
    [NSApplication _accessibilityInitialize];

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
    PlatformMediaSessionManager::setWebMFormatReaderEnabled(RuntimeEnabledFeatures::sharedFeatures().webMFormatReaderEnabled());
#endif

#if ENABLE(VORBIS) && PLATFORM(MAC)
    PlatformMediaSessionManager::setVorbisDecoderEnabled(RuntimeEnabledFeatures::sharedFeatures().vorbisDecoderEnabled());
#endif

#if ENABLE(OPUS) && PLATFORM(MAC)
    PlatformMediaSessionManager::setOpusDecoderEnabled(RuntimeEnabledFeatures::sharedFeatures().opusDecoderEnabled());
#endif

    if (!parameters.mediaMIMETypes.isEmpty())
        setMediaMIMETypes(parameters.mediaMIMETypes);
    else {
        AVAssetMIMETypeCache::singleton().setCacheMIMETypesCallback([this](const Vector<String>& types) {
            parentProcessConnection()->send(Messages::WebProcessProxy::CacheMediaMIMETypes(types), 0);
        });
    }

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) && PLATFORM(IOS)
    m_hasMouseDevice = parameters.hasMouseDevice;
#endif

    WebCore::setScreenProperties(parameters.screenProperties);

#if PLATFORM(MAC)
    scrollerStylePreferenceChanged(parameters.useOverlayScrollbars);
#endif

#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.compilerServiceExtensionHandles);
#endif

    if (parameters.containerManagerExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.containerManagerExtensionHandle);
    
#if PLATFORM(IOS_FAMILY)
    SandboxExtension::consumePermanently(parameters.dynamicMachExtensionHandles);
    SandboxExtension::consumePermanently(parameters.dynamicIOKitExtensionHandles);
#endif
    
    setSystemHasBattery(parameters.systemHasBattery);
    setSystemHasAC(parameters.systemHasAC);

#if PLATFORM(IOS_FAMILY)
    RenderThemeIOS::setCSSValueToSystemColorMap(WTFMove(parameters.cssValueToSystemColorMap));
    RenderThemeIOS::setFocusRingColor(parameters.focusRingColor);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    if (parameters.preferencesExtensionHandles) {
        SandboxExtension::consumePermanently(*parameters.preferencesExtensionHandles);
        _CFPrefsSetDirectModeEnabled(false);
    }
#endif
        
    WebCore::sleepDisablerClient() = makeUnique<WebSleepDisablerClient>();

#if HAVE(FIG_PHOTO_DECOMPRESSION_SET_HARDWARE_CUTOFF) && !ENABLE(HARDWARE_JPEG)
    if (PAL::isMediaToolboxFrameworkAvailable() && PAL::canLoad_MediaToolbox_FigPhotoDecompressionSetHardwareCutoff())
        PAL::softLinkMediaToolboxFigPhotoDecompressionSetHardwareCutoff(kPALFigPhotoContainerFormat_JFIF, INT_MAX);
#endif

    SystemSoundManager::singleton().setSystemSoundDelegate(makeUnique<WebSystemSoundDelegate>());

#if PLATFORM(MAC)
    if (canLoad_HIServices__AXSetAuditTokenIsAuthenticatedCallback())
        softLink_HIServices__AXSetAuditTokenIsAuthenticatedCallback(isAXAuthenticatedCallback);
#endif
    
    if (!parameters.maximumIOSurfaceSize.isEmpty())
        WebCore::IOSurface::setMaximumSize(parameters.maximumIOSurfaceSize);
}

void WebProcess::platformSetWebsiteDataStoreParameters(WebProcessDataStoreParameters&& parameters)
{
#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtension::consumePermanently(parameters.webSQLDatabaseDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.applicationCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaCacheDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.mediaKeyStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.javaScriptConfigurationDirectoryExtensionHandle);
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

void WebProcess::updateProcessName(IsInProcessInitialization isInProcessInitialization)
{
#if PLATFORM(MAC)
    RetainPtr<NSString> applicationName;
    switch (m_processType) {
    case ProcessType::Inspector:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Inspector", "Visible name of Web Inspector's web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::ServiceWorker:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Service Worker (%@)", "Visible name of Service Worker process. The argument is the application name."), (NSString *)m_uiProcessName, (NSString *)m_registrableDomain.string()];
        break;
    case ProcessType::PrewarmedWebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content (Prewarmed)", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::CachedWebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content (Cached)", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    case ProcessType::WebContent:
        applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content", "Visible name of the web process. The argument is the application name."), (NSString *)m_uiProcessName];
        break;
    }

#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    // During WebProcess initialization, we are still able to talk to LaunchServices to set the process name so there is no need to go
    // via the NetworkProcess. Prewarmed WebProcesses also do not have a network process connection until they are actually used by
    // a page.
    if (isInProcessInitialization == IsInProcessInitialization::No) {
        audit_token_t auditToken = { 0 };
        mach_msg_type_number_t info_size = TASK_AUDIT_TOKEN_COUNT;
        kern_return_t kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN, reinterpret_cast<integer_t *>(&auditToken), &info_size);
        if (kr != KERN_SUCCESS) {
            RELEASE_LOG_ERROR_IF_ALLOWED(Process, "updateProcessName: Unable to get audit token for self. Error: %{public}s (%x)", mach_error_string(kr), kr);
            return;
        }
        String displayName = applicationName.get();
        ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UpdateActivePages(displayName, Vector<String>(), auditToken), 0);
        return;
    }
#endif // ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)

    // Note that it is important for _RegisterApplication() to have been called before setting the display name.
    auto error = _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName.get(), nullptr);
    ASSERT(!error);
    if (error) {
        RELEASE_LOG_ERROR_IF_ALLOWED(Process, "updateProcessName: Failed to set the display name of the WebContent process, error code=%ld", static_cast<long>(error));
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
#if HAVE(ACCESSIBILITY_BUNDLES_PATH)
    return adoptCF(_AXSCopyPathForAccessibilityBundle(CFSTR("WebProcessLoader"))).bridgingAutorelease();
#else
    NSString *path = (__bridge NSString *)GSSystemRootDirectory();
#if PLATFORM(MACCATALYST)
    path = [path stringByAppendingPathComponent:@"System/iOSSupport"];
#endif
    return [path stringByAppendingPathComponent:@"System/Library/AccessibilityBundles/WebProcessLoader.axbundle"];
#endif // HAVE(ACCESSIBILITY_BUNDLES_PATH)
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

            auto pageLoadTimes = createNSArray(m_pageMap.values(), [] (auto& page) -> id {
                if (page->usesEphemeralSession())
                    return nil;

                return [NSDate dateWithTimeIntervalSince1970:page->loadCommitTime().secondsSinceEpoch().seconds()];
            });

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

void WebProcess::platformInitializeProcess(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC)
    // Deny the WebContent process access to the WindowServer.
    // This call will not succeed if there are open WindowServer connections at this point.
    auto retval = CGSSetDenyWindowServerConnections(true);
    RELEASE_ASSERT(retval == kCGErrorSuccess);
    // Make sure that we close any WindowServer connections after checking in with Launch Services.
    CGSShutdownServerConnections();

    SwitchingGPUClient::setSingleton(WebSwitchingGPUClient::singleton());
    MainThreadSharedTimer::shouldSetupPowerObserver() = false;
#endif // PLATFORM(MAC)

    if (parameters.extraInitializationData.get("inspector-process"_s) == "1")
        m_processType = ProcessType::Inspector;
#if ENABLE(SERVICE_WORKER)
    else if (parameters.extraInitializationData.get("service-worker-process"_s) == "1") {
        m_processType = ProcessType::ServiceWorker;
#if PLATFORM(MAC)
        m_registrableDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(parameters.extraInitializationData.get("registrable-domain"_s));
#endif
    }
#endif
    else if (parameters.extraInitializationData.get("is-prewarmed"_s) == "1")
        m_processType = ProcessType::PrewarmedWebContent;
    else
        m_processType = ProcessType::WebContent;

#if USE(OS_STATE)
    registerWithStateDumper();
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
    Optional<audit_token_t> auditToken = parentProcessConnection()->getAuditToken();
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

    sandboxParameters.setOverrideSandboxProfilePath(makeString(String([webKitBundle resourcePath]), "/com.apple.WebProcess.sb"));

    bool enableMessageFilter = false;
#if HAVE(SANDBOX_MESSAGE_FILTERING)
    enableMessageFilter = WTF::processHasEntitlement("com.apple.private.security.message-filter");
#endif
    sandboxParameters.addParameter("ENABLE_SANDBOX_MESSAGE_FILTER", enableMessageFilter ? "YES" : "NO");

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

void WebProcess::updateActivePages(const String& overrideDisplayName)
{
#if PLATFORM(MAC)
#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    audit_token_t auditToken = { 0 };
    mach_msg_type_number_t info_size = TASK_AUDIT_TOKEN_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN, reinterpret_cast<integer_t *>(&auditToken), &info_size);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR_IF_ALLOWED(Process, "updateActivePages: Unable to get audit token for self. Error: %{public}s (%x)", mach_error_string(kr), kr);
        return;
    }

    ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UpdateActivePages(overrideDisplayName, activePagesOrigins(m_pageMap), auditToken), 0);
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
    Optional<double> cpuLimit;
    if (m_processType == ProcessType::ServiceWorker)
        cpuLimit = serviceWorkerCPULimit;
    else {
        // Use the largest limit among all pages in this process.
        for (auto& page : m_pageMap.values()) {
            auto pageCPULimit = page->cpuLimit();
            if (!pageCPULimit) {
                cpuLimit = WTF::nullopt;
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
            m_cpuMonitor->setCPULimit(WTF::nullopt);
        return;
    }

    if (!m_cpuMonitor) {
        m_cpuMonitor = makeUnique<CPUMonitor>(cpuMonitoringInterval, [this](double cpuUsage) {
            if (m_processType == ProcessType::ServiceWorker)
                RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSuspension, "updateCPUMonitorState: Service worker process exceeded CPU limit of %.1f%% (was using %.1f%%)", m_cpuLimit.value() * 100, cpuUsage * 100);
            else
                RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSuspension, "updateCPUMonitorState: WebProcess exceeded CPU limit of %.1f%% (was using %.1f%%) hasVisiblePages? %d", m_cpuLimit.value() * 100, cpuUsage * 100, hasVisibleWebPage());
            parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedCPULimit(), 0);
        });
    } else if (reason == CPUMonitorUpdateReason::VisibilityHasChanged) {
        // If the visibility has changed, stop the CPU monitor before setting its limit. This is needed because the CPU usage can vary wildly based on visibility and we would
        // not want to report that a process has exceeded its background CPU limit even though most of the CPU time was used while the process was visible.
        m_cpuMonitor->setCPULimit(WTF::nullopt);
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
                if (auto* webPage = m_webProcess.webPage(handle._webPageID))
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
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "destroyRenderingResources: took %.2fms", (endTime - startTime).milliseconds());
}

#if PLATFORM(IOS_FAMILY)
void WebProcess::accessibilityProcessSuspendedNotification(bool suspended)
{
    UIAccessibilityPostNotification(kAXPidStatusChangedNotification, @{ @"pid" : @(getpid()), @"suspended" : @(suspended) });
}

void WebProcess::userInterfaceIdiomDidChange(bool isPadOrMac)
{
    WebKit::setCurrentUserInterfaceIdiomIsPadOrMac(isPadOrMac);
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
        RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSuspension, "updateFreezerStatus: isFreezable=%d, error=%d", isFreezable, result);
    else
        RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "updateFreezerStatus: isFreezable=%d, success", isFreezable);
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

void WebProcess::displayConfigurationChanged(CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags)
{
    GraphicsContextGLOpenGLManager::displayWasReconfigured(displayID, flags, nullptr);
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

static const WTF::String& reduceMotionPreferenceKey()
{
    static NeverDestroyed<WTF::String> key(MAKE_STATIC_STRING_IMPL("reduceMotion"));
    return key;
}
#endif

static void dispatchSimulatedNotificationsForPreferenceChange(const String& key)
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
    } else if (key == reduceMotionPreferenceKey()) {
        auto notificationCenter = CFNotificationCenterGetDistributedCenter();
        CFNotificationCenterPostNotification(notificationCenter, kAXInterfaceReduceMotionStatusDidChangeNotification, nullptr, nullptr, true);
    }
#endif
}

static void setPreferenceValue(const String& domain, const String& key, id value)
{
    if (domain.isEmpty()) {
        CFPreferencesSetValue(key.createCFString().get(), (__bridge CFPropertyListRef)value, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
#if ASSERT_ENABLED
        id valueAfterSetting = [[NSUserDefaults standardUserDefaults] objectForKey:key];
        ASSERT(valueAfterSetting == value || [valueAfterSetting isEqual:value] || key == "AppleLanguages");
#endif
    } else
        CFPreferencesSetValue(key.createCFString().get(), (__bridge CFPropertyListRef)value, domain.createCFString().get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    if (key == "AppleLanguages") {
        // We need to set AppleLanguages for the volatile domain, similarly to what we do in XPCServiceMain.mm.
        NSDictionary *existingArguments = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
        RetainPtr<NSMutableDictionary> newArguments = adoptNS([existingArguments mutableCopy]);
        [newArguments setValue:value forKey:@"AppleLanguages"];
        [[NSUserDefaults standardUserDefaults] setVolatileDomain:newArguments.get() forName:NSArgumentDomain];

        WTF::languageDidChange();
    }

#if PLATFORM(IOS_FAMILY)
    if (domain == String(kAXSAccessibilityPreferenceDomain)) {
        if (_AXSUpdateWebAccessibilitySettingsPtr())
            _AXSUpdateWebAccessibilitySettingsPtr()();
    }
#endif
}

void WebProcess::notifyPreferencesChanged(const String& domain, const String& key, const Optional<String>& encodedValue)
{
    if (!encodedValue.hasValue()) {
        setPreferenceValue(domain, key, nil);
        dispatchSimulatedNotificationsForPreferenceChange(key);
        return;
    }
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:*encodedValue options:0]);
    if (!encodedData)
        return;
    NSError *err = nil;
    auto classes = [NSSet setWithArray:@[[NSString class], [NSNumber class], [NSDate class], [NSDictionary class], [NSArray class], [NSData class]]];
    id object = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes fromData:encodedData.get() error:&err];
    ASSERT(!err);
    if (err)
        return;
    setPreferenceValue(domain, key, object);
    dispatchSimulatedNotificationsForPreferenceChange(key);
}

void WebProcess::unblockPreferenceService(SandboxExtension::HandleArray&& handleArray)
{
    SandboxExtension::consumePermanently(handleArray);
    _CFPrefsSetDirectModeEnabled(false);
}
#endif

void WebProcess::grantAccessToAssetServices(WebKit::SandboxExtension::Handle&& mobileAssetV2Handle)
{
    if (m_assetServiceV2Extension)
        return;
    m_assetServiceV2Extension = SandboxExtension::create(WTFMove(mobileAssetV2Handle));
    m_assetServiceV2Extension->consume();
}

void WebProcess::revokeAccessToAssetServices()
{
    if (!m_assetServiceV2Extension)
        return;
    m_assetServiceV2Extension->revoke();
    m_assetServiceV2Extension = nullptr;
}

void WebProcess::switchFromStaticFontRegistryToUserFontRegistry(WebKit::SandboxExtension::Handle&& fontMachExtensionHandle)
{
    if (m_fontMachExtension)
        return;
    m_fontMachExtension = SandboxExtension::create(WTFMove(fontMachExtensionHandle));
    m_fontMachExtension->consume();
#if HAVE(STATIC_FONT_REGISTRY)
    CTFontManagerEnableAllUserFonts(true);
#endif
}

void WebProcess::setScreenProperties(const ScreenProperties& properties)
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

void WebProcess::unblockServicesRequiredByAccessibility(const SandboxExtension::HandleArray& handleArray)
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

#if ENABLE(GPU_PROCESS)
void WebProcess::platformInitializeGPUProcessConnectionParameters(GPUProcessConnectionParameters& parameters)
{
#if HAVE(TASK_IDENTITY_TOKEN)
    task_id_token_t identityToken;
    kern_return_t kr = task_create_identity_token(mach_task_self(), &identityToken);
    if (kr == KERN_SUCCESS)
        parameters.webProcessIdentityToken = MachSendRight::adopt(identityToken);
    else
        RELEASE_LOG_ERROR(Process, "Call to task_create_identity_token() failed: %{private}s (%x)", mach_error_string(kr), kr);
#endif

    parameters.overrideLanguages = userPreferredLanguagesOverride();
}
#endif

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

void WebProcess::consumeAudioComponentRegistrations(const IPC::DataReference& data)
{
    using namespace PAL;

    if (!isAudioToolboxCoreFrameworkAvailable() || !canLoad_AudioToolboxCore_AudioComponentApplyServerRegistrations())
        return;

    auto registrations = adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size()));
    if (!registrations)
        return;

    auto err = AudioComponentApplyServerRegistrations(registrations.get());
    if (noErr != err)
        RELEASE_LOG_ERROR_IF_ALLOWED(Process, "Could not apply AudioComponent registrations, err(%ld)", static_cast<long>(err));
}

} // namespace WebKit

#undef RELEASE_LOG_SESSION_ID
#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
