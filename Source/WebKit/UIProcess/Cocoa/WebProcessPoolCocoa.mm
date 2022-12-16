/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#import "WebProcessPool.h"

#import "APINavigation.h"
#import "AccessibilityPreferences.h"
#import "AccessibilitySupportSPI.h"
#import "CookieStorageUtilsCF.h"
#import "DefaultWebBrowserChecks.h"
#import "LegacyCustomProtocolManagerClient.h"
#import "LockdownModeObserver.h"
#import "Logging.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkProcessMessages.h"
#import "NetworkProcessProxy.h"
#import "PreferenceObserver.h"
#import "ProcessThrottler.h"
#import "SandboxUtilities.h"
#import "TextChecker.h"
#import "UserInterfaceIdiom.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebBackForwardCache.h"
#import "WebMemoryPressureHandler.h"
#import "WebPageGroup.h"
#import "WebPreferencesKeys.h"
#import "WebProcessCache.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessMessages.h"
#import "WindowServerConnection.h"
#import "_WKSystemPreferencesInternal.h"
#import <WebCore/AGXCompilerService.h>
#import <WebCore/Color.h>
#import <WebCore/FontCacheCoreText.h>
#import <WebCore/LocalizedDeviceModel.h>
#import <WebCore/LowPowerModeNotifier.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PictureInPictureSupport.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/PowerSourceNotifier.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/UTIUtilities.h>
#import <objc/runtime.h>
#import <pal/Logging.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cf/CFNotificationCenterSPI.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <sys/param.h>
#import <wtf/FileSystem.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/TextStream.h>

#if ENABLE(REMOTE_INSPECTOR)
#import <JavaScriptCore/RemoteInspector.h>
#import <JavaScriptCore/RemoteInspectorConstants.h>
#endif

#if PLATFORM(MAC)
#import "WebInspectorPreferenceObserver.h"
#import <QuartzCore/CARemoteLayerServer.h>
#import <notify.h>
#import <notify_keys.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#else
#import "UIKitSPI.h"
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY)
#import <pal/spi/mac/PowerLogSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/spi/ios/MobileGestaltSPI.h>
#endif

#if PLATFORM(COCOA)
#import <WebCore/SystemBattery.h>
#endif

#if ENABLE(GPU_PROCESS)
#import "GPUProcessMessages.h"
#endif

#if HAVE(MOUSE_DEVICE_OBSERVATION)
#import "WKMouseDeviceObserver.h"
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
#import "WKStylusDeviceObserver.h"
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include <WebCore/CaptionUserPreferencesMediaAF.h>
#include <WebCore/MediaAccessibilitySoftLink.h>
#endif

#if ENABLE(WEBCONTENT_CRASH_TESTING)
#if __has_include(<WebKitAdditions/InternalBuildAdditions.h>)
#include <WebKitAdditions/InternalBuildAdditions.h>
#else
static bool isInternalBuild()
{
    return false;
}
#endif

#if __has_include(<WebKitAdditions/InternalCanaryState.h>)
#include <WebKitAdditions/InternalCanaryState.h>
#else
static bool canaryInBaseState()
{
    return true;
}
#endif

#include <OSAnalytics/OSASystemConfiguration_Public.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(OSAnalytics)

SOFT_LINK_CLASS(OSAnalytics, OSASystemConfiguration)
#endif

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>

NSString *WebServiceWorkerRegistrationDirectoryDefaultsKey = @"WebServiceWorkerRegistrationDirectory";
NSString *WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
NSString *WebKitJSCJITEnabledDefaultsKey = @"WebKitJSCJITEnabledDefaultsKey";
NSString *WebKitJSCFTLJITEnabledDefaultsKey = @"WebKitJSCFTLJITEnabledDefaultsKey";

#if !PLATFORM(IOS_FAMILY) || PLATFORM(MACCATALYST)
static NSString *WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification = @"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification";
static CFStringRef AppleColorPreferencesChangedNotification = CFSTR("AppleColorPreferencesChangedNotification");
#endif

static NSString * const WebKitSuppressMemoryPressureHandlerDefaultsKey = @"WebKitSuppressMemoryPressureHandler";

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
static NSString * const kPLTaskingStartNotificationGlobal = @"kPLTaskingStartNotificationGlobal";
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices, BKSDisplayBrightnessGetCurrent, float, (), ());
#endif

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
SOFT_LINK_LIBRARY_OPTIONAL(libAccessibility)
SOFT_LINK_OPTIONAL(libAccessibility, _AXSReduceMotionAutoplayAnimatedImagesEnabled, Boolean, (), ());
SOFT_LINK_CONSTANT_MAY_FAIL(libAccessibility, kAXSReduceMotionAutoplayAnimatedImagesChangedNotification, CFStringRef)
#endif

#define WEBPROCESSPOOL_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - WebProcessPool::" fmt, this, ##__VA_ARGS__)

@interface WKProcessPoolWeakObserver : NSObject {
    WeakPtr<WebKit::WebProcessPool> m_weakPtr;
}
@property (nonatomic, readonly, direct) RefPtr<WebKit::WebProcessPool> pool;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWeakPtr:(WeakPtr<WebKit::WebProcessPool>&&)weakPtr NS_DESIGNATED_INITIALIZER;
@end

NS_DIRECT_MEMBERS
@implementation WKProcessPoolWeakObserver
- (instancetype)initWithWeakPtr:(WeakPtr<WebKit::WebProcessPool>&&)weakPtr
{
    if ((self = [super init]))
        m_weakPtr = WTFMove(weakPtr);
    return self;
}

- (RefPtr<WebKit::WebProcessPool>)pool
{
    return m_weakPtr.get();
}
@end

namespace WebKit {
using namespace WebCore;

static void registerUserDefaultsIfNeeded()
{
    static bool didRegister;
    if (didRegister)
        return;

    didRegister = true;
    NSMutableDictionary *registrationDictionary = [NSMutableDictionary dictionary];
    
    [registrationDictionary setObject:@YES forKey:WebKitJSCJITEnabledDefaultsKey];
    [registrationDictionary setObject:@YES forKey:WebKitJSCFTLJITEnabledDefaultsKey];

    [[NSUserDefaults standardUserDefaults] registerDefaults:registrationDictionary];
}

static std::optional<bool>& cachedLockdownModeEnabledGlobally()
{
    static std::optional<bool> cachedLockdownModeEnabledGlobally;
    return cachedLockdownModeEnabledGlobally;
}

void WebProcessPool::updateProcessSuppressionState()
{
    WebsiteDataStore::forEachWebsiteDataStore([enabled = processSuppressionEnabled()] (WebsiteDataStore& dataStore) {
        if (auto* networkProcess = dataStore.networkProcessIfExists())
            networkProcess->setProcessSuppressionEnabled(enabled);
    });
}

NSMutableDictionary *WebProcessPool::ensureBundleParameters()
{
    if (!m_bundleParameters)
        m_bundleParameters = adoptNS([[NSMutableDictionary alloc] init]);

    return m_bundleParameters.get();
}

static AccessibilityPreferences accessibilityPreferences()
{
    AccessibilityPreferences preferences;
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    auto appId = WebCore::applicationBundleIdentifier().createCFString();

    preferences.reduceMotionEnabled = _AXSReduceMotionEnabledApp(appId.get());
    preferences.increaseButtonLegibility = _AXSIncreaseButtonLegibilityApp(appId.get());
    preferences.enhanceTextLegibility = _AXSEnhanceTextLegibilityEnabledApp(appId.get());
    preferences.darkenSystemColors = _AXDarkenSystemColorsApp(appId.get());
    preferences.invertColorsEnabled = _AXSInvertColorsEnabledApp(appId.get());
#endif
    preferences.enhanceTextLegibilityOverall = _AXSEnhanceTextLegibilityEnabled();
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (auto* functionPointer = _AXSReduceMotionAutoplayAnimatedImagesEnabledPtr())
        preferences.imageAnimationEnabled = functionPointer();
#endif
    return preferences;
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
void WebProcessPool::setMediaAccessibilityPreferences(WebProcessProxy& process)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [weakProcess = WeakPtr { process }] {
        auto captionDisplayMode = WebCore::CaptionUserPreferencesMediaAF::platformCaptionDisplayMode();
        auto preferredLanguages = WebCore::CaptionUserPreferencesMediaAF::platformPreferredLanguages();
        callOnMainRunLoop([weakProcess, captionDisplayMode, preferredLanguages = crossThreadCopy(WTFMove(preferredLanguages))] {
            if (weakProcess)
                weakProcess->send(Messages::WebProcess::SetMediaAccessibilityPreferences(captionDisplayMode, preferredLanguages), 0);
        });
    });
}
#endif

static void logProcessPoolState(const WebProcessPool& pool)
{
    for (const auto& process : pool.processes()) {
        WTF::TextStream stream;
        stream << process;

        String domain = process->optionalRegistrableDomain() ? process->optionalRegistrableDomain()->string() : "unknown"_s;
        RELEASE_LOG(Process, "WebProcessProxy %p - %" PUBLIC_LOG_STRING ", domain: %" PRIVATE_LOG_STRING, process.ptr(), stream.release().utf8().data(), domain.utf8().data());
    }
}

#if ENABLE(WEBCONTENT_CRASH_TESTING)
static bool determineIfWeShouldCrashWhenCreatingWebProcess()
{
    static bool shouldCrashResult { false };
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [&] {
            if (isInternalBuild()) {
                auto resultAutomatedDeviceGroup = [getOSASystemConfigurationClass() automatedDeviceGroup];

                RELEASE_LOG(Process, "shouldCrashWhenCreatingWebProcess: automatedDeviceGroup default: %s , canaryInBaseState: %s", resultAutomatedDeviceGroup ? [resultAutomatedDeviceGroup UTF8String] : "[nil]", canaryInBaseState() ? "true" : "false");

                if (![resultAutomatedDeviceGroup isEqualToString:@"CanaryExperimentOptOut"]
                    && !canaryInBaseState())
                    shouldCrashResult = true;
            }
        });

    return shouldCrashResult;
}
#endif

void WebProcessPool::platformInitialize()
{
    registerUserDefaultsIfNeeded();
    registerNotificationObservers();
    initializeClassesForParameterCoding();

    // FIXME: This should be able to share code with WebCore's MemoryPressureHandler (and be platform independent).
    // Right now it cannot because WebKit1 and WebKit2 need to be able to coexist in the UI process,
    // and you can only have one WebCore::MemoryPressureHandler.

    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitSuppressMemoryPressureHandler"])
        installMemoryPressureHandler();

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    if (!_MGCacheValid()) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [adoptNS([[objc_getClass("MobileGestaltHelperProxy") alloc] init]) proxyRebuildCache];
        });
    }
#endif

#if PLATFORM(MAC)
    [WKWebInspectorPreferenceObserver sharedInstance];
#endif

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        PAL::registerNotifyCallback("com.apple.WebKit.logProcessState"_s, ^{
            for (const auto& pool : WebProcessPool::allProcessPools())
                logProcessPoolState(pool.get());
        });
    });

#if ENABLE(WEBCONTENT_CRASH_TESTING)
#if PLATFORM(IOS_FAMILY)
    bool isSafari = WebCore::IOSApplication::isMobileSafari();
#elif PLATFORM(MAC)
    bool isSafari = WebCore::MacApplication::isSafari();
#endif

    if (isSafari)
        m_shouldCrashWhenCreatingWebProcess = determineIfWeShouldCrashWhenCreatingWebProcess();
#endif
}

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
    m_resolvedPaths.uiProcessBundleResourcePath = resolvePathForSandboxExtension(String { [[NSBundle mainBundle] resourcePath] });
}

void WebProcessPool::platformInitializeWebProcess(const WebProcessProxy& process, WebProcessCreationParameters& parameters)
{
    parameters.mediaMIMETypes = process.mediaMIMETypes();

#if PLATFORM(MAC)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    parameters.accessibilityEnhancedUserInterfaceEnabled = [[NSApp accessibilityAttributeValue:@"AXEnhancedUserInterface"] boolValue];
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    parameters.accessibilityEnhancedUserInterfaceEnabled = false;
#endif

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    parameters.shouldEnableJIT = [defaults boolForKey:WebKitJSCJITEnabledDefaultsKey];
    parameters.shouldEnableFTLJIT = [defaults boolForKey:WebKitJSCFTLJITEnabledDefaultsKey];
    parameters.shouldEnableMemoryPressureReliefLogging = [defaults boolForKey:@"LogMemoryJetsamDetails"];
    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

#if HAVE(HOSTED_CORE_ANIMATION)
    parameters.acceleratedCompositingPort = MachSendRight::create([CARemoteLayerServer sharedServer].serverPort);
#endif

    // FIXME: This should really be configurable; we shouldn't just blindly allow read access to the UI process bundle.
    parameters.uiProcessBundleResourcePath = m_resolvedPaths.uiProcessBundleResourcePath;
    if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.uiProcessBundleResourcePath, SandboxExtension::Type::ReadOnly))
        parameters.uiProcessBundleResourcePathExtensionHandle = WTFMove(*handle);

    parameters.uiProcessBundleIdentifier = applicationBundleIdentifier();

    parameters.latencyQOS = webProcessLatencyQOS();
    parameters.throughputQOS = webProcessThroughputQOS();

    if (m_configuration->presentingApplicationProcessToken()) {
        NSError *error = nil;
        auto bundleProxy = [LSBundleProxy bundleProxyWithAuditToken:*m_configuration->presentingApplicationProcessToken() error:&error];
        if (error)
            RELEASE_LOG_ERROR(WebRTC, "Failed to get attribution bundleID from audit token with error: %@.", error.localizedDescription);
        else
            parameters.presentingApplicationBundleIdentifier = bundleProxy.bundleIdentifier;
    }

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    if (WebProcessProxy::shouldEnableRemoteInspector()) {
        auto handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.webinspector"_s }, process.auditToken());
        parameters.enableRemoteWebInspectorExtensionHandles = WTFMove(handles);
    }
#endif

    parameters.fontAllowList = m_fontAllowList;

    if (m_bundleParameters) {
        auto keyedArchiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);

        @try {
            [keyedArchiver encodeObject:m_bundleParameters.get() forKey:@"parameters"];
            [keyedArchiver finishEncoding];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to encode bundle parameters: %@", exception);
        }

        auto data = keyedArchiver.get().encodedData;

        parameters.bundleParameterData = API::Data::createWithoutCopying(WTFMove(data));
    }
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

#if ENABLE(MEDIA_STREAM)
    // Allow microphone access if either preference is set because WebRTC requires microphone access.
    bool mediaDevicesEnabled = m_defaultPageGroup->preferences().mediaDevicesEnabled();

    bool isSafari = false;
#if PLATFORM(IOS_FAMILY)
    if (WebCore::IOSApplication::isMobileSafari())
        isSafari = true;
#elif PLATFORM(MAC)
    if (WebCore::MacApplication::isSafari())
        isSafari = true;
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    // FIXME: Remove this and related parameter when <rdar://problem/29448368> is fixed.
    if (isSafari && mediaDevicesEnabled && !m_defaultPageGroup->preferences().captureAudioInUIProcessEnabled() && !m_defaultPageGroup->preferences().captureAudioInGPUProcessEnabled()) {
        if (auto handle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone"_s))
            parameters.audioCaptureExtensionHandle = WTFMove(*handle);
    }
#else
    UNUSED_VARIABLE(mediaDevicesEnabled);
    UNUSED_VARIABLE(isSafari);
#endif
#endif

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
    parameters.shouldLogUserInteraction = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif

    auto screenProperties = WebCore::collectScreenProperties();
    parameters.screenProperties = WTFMove(screenProperties);
#if PLATFORM(MAC)
    parameters.useOverlayScrollbars = ([NSScroller preferredScrollerStyle] == NSScrollerStyleOverlay);
#endif
    
#if PLATFORM(IOS) && HAVE(AGX_COMPILER_SERVICE)
    if (WebCore::deviceHasAGXCompilerService())
        parameters.compilerServiceExtensionHandles = SandboxExtension::createHandlesForMachLookup(WebCore::agxCompilerServices(), std::nullopt);
#endif

#if PLATFORM(IOS_FAMILY) && HAVE(AGX_COMPILER_SERVICE)
    if (WebCore::deviceHasAGXCompilerService())
        parameters.dynamicIOKitExtensionHandles = SandboxExtension::createHandlesForIOKitClassExtensions(WebCore::agxCompilerClasses(), std::nullopt);
#endif

    parameters.systemHasBattery = systemHasBattery();
    parameters.systemHasAC = cachedSystemHasAC().value_or(true);

#if PLATFORM(IOS_FAMILY)
    parameters.currentUserInterfaceIdiomIsSmallScreen = currentUserInterfaceIdiomIsSmallScreen();
    parameters.supportsPictureInPicture = supportsPictureInPicture();
    parameters.cssValueToSystemColorMap = RenderThemeIOS::cssValueToSystemColorMap();
    parameters.focusRingColor = RenderThemeIOS::systemFocusRingColor();
    parameters.localizedDeviceModel = localizedDeviceModel();
    parameters.contentSizeCategory = contentSizeCategory();
#endif

    parameters.mobileGestaltExtensionHandle = process.createMobileGestaltSandboxExtensionIfNeeded();

#if PLATFORM(MAC)
    if (auto launchServicesExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.coreservices.launchservicesd"_s, std::nullopt))
        parameters.launchServicesExtensionHandle = WTFMove(*launchServicesExtensionHandle);
#endif

#if HAVE(VIDEO_RESTRICTED_DECODING)
#if PLATFORM(MAC)
    if (!isFullWebBrowser()) {
        if (auto trustdExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.trustd.agent"_s, std::nullopt))
            parameters.trustdExtensionHandle = WTFMove(*trustdExtensionHandle);
        parameters.enableDecodingHEIC = true;
        parameters.enableDecodingAVIF = true;
    }
#else
    parameters.enableDecodingHEIC = true;
    parameters.enableDecodingAVIF = true;
#endif // PLATFORM(MAC)
#endif // HAVE(VIDEO_RESTRICTED_DECODING)

#if PLATFORM(IOS_FAMILY) && ENABLE(CFPREFS_DIRECT_MODE)
    if ([UIApplication sharedApplication]) {
        auto state = [[UIApplication sharedApplication] applicationState];
        if (state == UIApplicationStateActive)
            startObservingPreferenceChanges();
    }
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    parameters.overrideUserInterfaceIdiomAndScale = { _UIApplicationCatalystUserInterfaceIdiom(), _UIApplicationCatalystScaleFactor() };
#endif

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    parameters.hasMouseDevice = [[WKMouseDeviceObserver sharedInstance] hasMouseDevice];
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    parameters.hasStylusDevice = [[WKStylusDeviceObserver sharedInstance] hasStylusDevice];
#endif

#if HAVE(IOSURFACE)
    parameters.maximumIOSurfaceSize = WebCore::IOSurface::maximumSize();
    parameters.bytesPerRowIOSurfaceAlignment = WebCore::IOSurface::bytesPerRowAlignment();
#endif

    parameters.accessibilityPreferences = accessibilityPreferences();
#if PLATFORM(IOS_FAMILY)
    parameters.applicationAccessibilityEnabled = _AXSApplicationAccessibilityEnabled();
#endif
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters& parameters)
{
    parameters.uiProcessBundleIdentifier = applicationBundleIdentifier();

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    parameters.uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage([[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]);
#endif

    parameters.enablePrivateClickMeasurement = ![defaults objectForKey:WebPreferencesKey::privateClickMeasurementEnabledKey()] || [defaults boolForKey:WebPreferencesKey::privateClickMeasurementEnabledKey()];
    parameters.ftpEnabled = [defaults objectForKey:WebPreferencesKey::ftpEnabledKey()] && [defaults boolForKey:WebPreferencesKey::ftpEnabledKey()];
}

void WebProcessPool::platformInvalidateContext()
{
    unregisterNotificationObservers();
}

#if PLATFORM(IOS_FAMILY)
void WebProcessPool::setJavaScriptConfigurationFileEnabledFromDefaults()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    setJavaScriptConfigurationFileEnabled([defaults boolForKey:@"WebKitJavaScriptCoreUseConfigFile"]);
}
#endif

bool WebProcessPool::omitPDFSupport()
{
    // Since this is a "secret default" we don't bother registering it.
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
}

bool WebProcessPool::processSuppressionEnabled() const
{
    return !m_userObservablePageCounter.value() && !m_processSuppressionDisabledForPageCounter.value();
}

static inline RefPtr<WebProcessPool> extractWebProcessPool(void* observer)
{
    RetainPtr strongObserver { dynamic_objc_cast<WKProcessPoolWeakObserver>(reinterpret_cast<id>(observer)) };
    if (!strongObserver)
        return nullptr;
    return [strongObserver pool];
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
float WebProcessPool::displayBrightness()
{
    return BKSDisplayBrightnessGetCurrent();
}

void WebProcessPool::backlightLevelDidChangeCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    auto pool = extractWebProcessPool(observer);
    if (!pool)
        return;
    pool->sendToAllProcesses(Messages::WebProcess::BacklightLevelDidChange(BKSDisplayBrightnessGetCurrent()));
}
#endif

void WebProcessPool::accessibilityPreferencesChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    auto pool = extractWebProcessPool(observer);
    if (!pool)
        return;
    pool->sendToAllProcesses(Messages::WebProcess::AccessibilityPreferencesDidChange(accessibilityPreferences()));
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
void WebProcessPool::mediaAccessibilityPreferencesChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    auto pool = extractWebProcessPool(observer);
    if (!pool)
        return;
    auto captionDisplayMode = WebCore::CaptionUserPreferencesMediaAF::platformCaptionDisplayMode();
    auto preferredLanguages = WebCore::CaptionUserPreferencesMediaAF::platformPreferredLanguages();
    pool->sendToAllProcesses(Messages::WebProcess::SetMediaAccessibilityPreferences(captionDisplayMode, preferredLanguages));
}
#endif

#if PLATFORM(MAC)
void WebProcessPool::colorPreferencesDidChangeCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    auto pool = extractWebProcessPool(observer);
    if (!pool)
        return;
    pool->sendToAllProcesses(Messages::WebProcess::ColorPreferencesDidChange());
}
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void WebProcessPool::hardwareConsoleStateChanged()
{
    for (auto& process : m_processes)
        process->hardwareConsoleStateChanged();
}
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
void WebProcessPool::remoteWebInspectorEnabledCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    auto pool = extractWebProcessPool(observer);
    if (!pool)
        return;
    for (auto& process : pool->m_processes)
        process->enableRemoteInspectorIfNeeded();
}
#endif

#if PLATFORM(COCOA)
void WebProcessPool::lockdownModeConfigurationUpdateCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    if (auto pool = extractWebProcessPool(observer))
        pool->lockdownModeStateChanged();
}
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
void WebProcessPool::powerLogTaskModeStartedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    if (auto* gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->enablePowerLogging();
}
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
void WebProcessPool::startObservingPreferenceChanges()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            // Start observing preference changes.
            [WKPreferenceObserver sharedInstance];
        });
    });
}
#endif

void WebProcessPool::addCFNotificationObserver(CFNotificationCallback callback, CFStringRef name, CFNotificationCenterRef center)
{
    auto coalesceBehavior = static_cast<CFNotificationSuspensionBehavior>(CFNotificationSuspensionBehaviorCoalesce | _CFNotificationObserverIsObjC);
    CFNotificationCenterAddObserver(center, (__bridge const void*)m_weakObserver.get(), callback, name, nullptr, coalesceBehavior);
}

void WebProcessPool::removeCFNotificationObserver(CFStringRef name, CFNotificationCenterRef center)
{
    CFNotificationCenterRemoveObserver(center, (__bridge const void*)m_weakObserver.get(), name, nullptr);
}

void WebProcessPool::registerNotificationObservers()
{
    m_weakObserver = adoptNS([[WKProcessPoolWeakObserver alloc] initWithWeakPtr:*this]);

#if !PLATFORM(IOS_FAMILY)
    m_powerObserver = makeUnique<WebCore::PowerObserver>([weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->sendToAllProcesses(Messages::WebProcess::SystemWillPowerOn());
    });
    m_systemSleepListener = PAL::SystemSleepListener::create(*this);
    // Listen for enhanced accessibility changes and propagate them to the WebProcess.
    m_enhancedAccessibilityObserver = [[NSNotificationCenter defaultCenter] addObserverForName:WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *note) {
        setEnhancedAccessibility([[[note userInfo] objectForKey:@"AXEnhancedUserInterface"] boolValue]);
    }];

    m_automaticTextReplacementNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSSpellCheckerDidChangeAutomaticTextReplacementNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        TextChecker::didChangeAutomaticTextReplacementEnabled();
        textCheckerStateChanged();
    }];
    
    m_automaticSpellingCorrectionNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSSpellCheckerDidChangeAutomaticSpellingCorrectionNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        TextChecker::didChangeAutomaticSpellingCorrectionEnabled();
        textCheckerStateChanged();
    }];

    m_automaticQuoteSubstitutionNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSSpellCheckerDidChangeAutomaticQuoteSubstitutionNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        TextChecker::didChangeAutomaticQuoteSubstitutionEnabled();
        textCheckerStateChanged();
    }];

    m_automaticDashSubstitutionNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSSpellCheckerDidChangeAutomaticDashSubstitutionNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        TextChecker::didChangeAutomaticDashSubstitutionEnabled();
        textCheckerStateChanged();
    }];

    m_accessibilityDisplayOptionsNotificationObserver = [[NSWorkspace.sharedWorkspace notificationCenter] addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        screenPropertiesStateChanged();
    }];

    m_scrollerStyleNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSPreferredScrollerStyleDidChangeNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        auto scrollbarStyle = [NSScroller preferredScrollerStyle];
        sendToAllProcesses(Messages::WebProcess::ScrollerStylePreferenceChanged(scrollbarStyle));
    }];

    m_activationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidBecomeActiveNotification object:NSApp queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
#if ENABLE(CFPREFS_DIRECT_MODE)
        startObservingPreferenceChanges();
#endif
        setApplicationIsActive(true);
    }];

    m_deactivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidResignActiveNotification object:NSApp queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        setApplicationIsActive(false);
    }];
    
    addCFNotificationObserver(colorPreferencesDidChangeCallback, AppleColorPreferencesChangedNotification, CFNotificationCenterGetDistributedCenter());

    const char* messages[] = { kNotifyDSCacheInvalidation, kNotifyDSCacheInvalidationGroup, kNotifyDSCacheInvalidationHost, kNotifyDSCacheInvalidationService, kNotifyDSCacheInvalidationUser };
    m_openDirectoryNotifyTokens.reserveInitialCapacity(std::size(messages));
    for (auto* message : messages) {
        int notifyToken;
        notify_register_dispatch(message, &notifyToken, dispatch_get_main_queue(), ^(int token) {
            RELEASE_LOG(Notifications, "OpenDirectory invalidated cache");
#if ENABLE(GPU_PROCESS)
            auto handle = SandboxExtension::createHandleForMachLookup("com.apple.system.opendirectoryd.libinfo"_s, std::nullopt);
            if (!handle)
                return;
            if (auto* gpuProcess = GPUProcessProxy::singletonIfCreated())
                gpuProcess->send(Messages::GPUProcess::OpenDirectoryCacheInvalidated(*handle), 0);
#endif
            for (auto& process : m_processes) {
                if (!process->canSendMessage())
                    continue;
                auto handle = SandboxExtension::createHandleForMachLookup("com.apple.system.opendirectoryd.libinfo"_s, std::nullopt);
                if (!handle)
                    continue;
                auto bootstrapHandle = SandboxExtension::createHandleForMachBootstrapExtension();
                process->send(Messages::WebProcess::OpenDirectoryCacheInvalidated(*handle, bootstrapHandle), 0);
            }
        });
        m_openDirectoryNotifyTokens.append(notifyToken);
    }
#elif !PLATFORM(MACCATALYST)
    addCFNotificationObserver(backlightLevelDidChangeCallback, (__bridge CFStringRef)UIBacklightLevelChangedNotification);
#if PLATFORM(IOS)
#if ENABLE(REMOTE_INSPECTOR)
    addCFNotificationObserver(remoteWebInspectorEnabledCallback, CFSTR(WIRServiceEnabledNotification));
#endif
#endif // PLATFORM(IOS)
#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY)
    m_accessibilityEnabledObserver = [[NSNotificationCenter defaultCenter] addObserverForName:(__bridge id)kAXSApplicationAccessibilityEnabledNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *) {
        if (!_AXSApplicationAccessibilityEnabled())
            return;
        for (auto& process : m_processes)
            process->unblockAccessibilityServerIfNeeded();
    }];
#if ENABLE(CFPREFS_DIRECT_MODE)
    m_activationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:@"UIApplicationDidBecomeActiveNotification" object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        startObservingPreferenceChanges();
    }];
#endif
    if (![UIApplication sharedApplication]) {
        m_applicationLaunchObserver = [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidFinishLaunchingNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
            if (WebKit::updateCurrentUserInterfaceIdiom())
                sendToAllProcesses(Messages::WebProcess::UserInterfaceIdiomDidChange(WebKit::currentUserInterfaceIdiomIsSmallScreen()));
        }];
    }
#endif

    m_powerSourceNotifier = WTF::makeUnique<WebCore::PowerSourceNotifier>([this] (bool hasAC) {
        sendToAllProcesses(Messages::WebProcess::PowerSourceDidChange(hasAC));
    });

#if PLATFORM(COCOA)
    addCFNotificationObserver(lockdownModeConfigurationUpdateCallback, (__bridge CFStringRef)WKLockdownModeContainerConfigurationChangedNotification);
#endif

#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    addCFNotificationObserver(accessibilityPreferencesChangedCallback, kAXSReduceMotionChangedNotification);
    addCFNotificationObserver(accessibilityPreferencesChangedCallback, kAXSIncreaseButtonLegibilityNotification);
    addCFNotificationObserver(accessibilityPreferencesChangedCallback, kAXSEnhanceTextLegibilityChangedNotification);
    addCFNotificationObserver(accessibilityPreferencesChangedCallback, kAXSDarkenSystemColorsEnabledNotification);
    addCFNotificationObserver(accessibilityPreferencesChangedCallback, kAXSInvertColorsEnabledNotification);
#endif
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (canLoadkAXSReduceMotionAutoplayAnimatedImagesChangedNotification())
        addCFNotificationObserver(accessibilityPreferencesChangedCallback, getkAXSReduceMotionAutoplayAnimatedImagesChangedNotification());
#endif
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    addCFNotificationObserver(mediaAccessibilityPreferencesChangedCallback, kMAXCaptionAppearanceSettingsChangedNotification);
#endif
#if HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
    addCFNotificationObserver(powerLogTaskModeStartedCallback, (__bridge CFStringRef)kPLTaskingStartNotificationGlobal);
#endif // HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
}

void WebProcessPool::unregisterNotificationObservers()
{
#if !PLATFORM(IOS_FAMILY)
    m_powerObserver = nullptr;
    m_systemSleepListener = nullptr;
    [[NSNotificationCenter defaultCenter] removeObserver:m_enhancedAccessibilityObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticTextReplacementNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticSpellingCorrectionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticQuoteSubstitutionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticDashSubstitutionNotificationObserver.get()];
    [[NSWorkspace.sharedWorkspace notificationCenter] removeObserver:m_accessibilityDisplayOptionsNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_scrollerStyleNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_deactivationObserver.get()];
    removeCFNotificationObserver(AppleColorPreferencesChangedNotification, CFNotificationCenterGetDistributedCenter());
    for (auto token : m_openDirectoryNotifyTokens)
        notify_cancel(token);
#elif !PLATFORM(MACCATALYST)
    removeCFNotificationObserver((__bridge CFStringRef)UIBacklightLevelChangedNotification);
#if PLATFORM(IOS)
#if ENABLE(REMOTE_INSPECTOR)
    removeCFNotificationObserver(CFSTR(WIRServiceEnabledNotification));
#endif
#endif // PLATFORM(IOS)
#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:m_accessibilityEnabledObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_applicationLaunchObserver.get()];
#endif

    [[NSNotificationCenter defaultCenter] removeObserver:m_activationObserver.get()];

    m_powerSourceNotifier = nullptr;
    
#if PLATFORM(COCOA)
    removeCFNotificationObserver((__bridge CFStringRef)WKLockdownModeContainerConfigurationChangedNotification);
#endif

#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    removeCFNotificationObserver(kAXSReduceMotionChangedNotification);
    removeCFNotificationObserver(kAXSIncreaseButtonLegibilityNotification);
    removeCFNotificationObserver(kAXSEnhanceTextLegibilityChangedNotification);
    removeCFNotificationObserver(kAXSDarkenSystemColorsEnabledNotification);
    removeCFNotificationObserver(kAXSInvertColorsEnabledNotification);
#endif
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    removeCFNotificationObserver(kMAXCaptionAppearanceSettingsChangedNotification);
#endif
#if HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
    removeCFNotificationObserver((__bridge CFStringRef)kPLTaskingStartNotificationGlobal);
#endif
    m_weakObserver = nil;
}

bool WebProcessPool::isURLKnownHSTSHost(const String& urlString) const
{
    RetainPtr<CFURLRef> url = URL { urlString }.createCFURL();

    return _CFNetworkIsKnownHSTSHostWithSession(url.get(), nullptr);
}

#if HAVE(CVDISPLAYLINK)

std::optional<unsigned> WebProcessPool::nominalFramesPerSecondForDisplay(WebCore::PlatformDisplayID displayID)
{
    if (auto* displayLink = m_displayLinks.displayLinkForDisplay(displayID))
        return displayLink->nominalFramesPerSecond();

    // Note that this creates a DisplayLink with no observers, but it's highly likely that we'll soon call startDisplayLink() for it.
    auto displayLink = makeUnique<DisplayLink>(displayID);
    auto frameRate = displayLink->nominalFramesPerSecond();
    m_displayLinks.add(WTFMove(displayLink));
    return frameRate;
}

void WebProcessPool::startDisplayLink(WebProcessProxy& processProxy, DisplayLinkObserverID observerID, PlatformDisplayID displayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    if (auto* displayLink = m_displayLinks.displayLinkForDisplay(displayID)) {
        displayLink->addObserver(processProxy.displayLinkClient(), observerID, preferredFramesPerSecond);
        return;
    }

    auto displayLink = makeUnique<DisplayLink>(displayID);
    displayLink->addObserver(processProxy.displayLinkClient(), observerID, preferredFramesPerSecond);
    m_displayLinks.add(WTFMove(displayLink));
}

void WebProcessPool::stopDisplayLink(WebProcessProxy& processProxy, DisplayLinkObserverID observerID, PlatformDisplayID displayID)
{
    if (auto* displayLink = m_displayLinks.displayLinkForDisplay(displayID))
        displayLink->removeObserver(processProxy.displayLinkClient(), observerID);
}

void WebProcessPool::stopDisplayLinks(WebProcessProxy& processProxy)
{
    for (auto& displayLink : m_displayLinks.displayLinks())
        displayLink->removeClient(processProxy.displayLinkClient());
}

void WebProcessPool::setDisplayLinkPreferredFramesPerSecond(WebProcessProxy& processProxy, DisplayLinkObserverID observerID, PlatformDisplayID displayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] WebProcessPool::setDisplayLinkPreferredFramesPerSecond - display " << displayID << " observer " << observerID << " fps " << preferredFramesPerSecond);

    if (auto* displayLink = m_displayLinks.displayLinkForDisplay(displayID))
        displayLink->setObserverPreferredFramesPerSecond(processProxy.displayLinkClient(), observerID, preferredFramesPerSecond);
}

void WebProcessPool::setDisplayLinkForDisplayWantsFullSpeedUpdates(WebProcessProxy& processProxy, WebCore::PlatformDisplayID displayID, bool wantsFullSpeedUpdates)
{
    if (auto* displayLink = m_displayLinks.displayLinkForDisplay(displayID)) {
        if (wantsFullSpeedUpdates)
            displayLink->incrementFullSpeedRequestClientCount(processProxy.displayLinkClient());
        else
            displayLink->decrementFullSpeedRequestClientCount(processProxy.displayLinkClient());
    }
}

#endif // HAVE(CVDISPLAYLINK)

// FIXME: Deprecated. Left here until a final decision is made.
void WebProcessPool::setCookieStoragePartitioningEnabled(bool enabled)
{
    m_cookieStoragePartitioningEnabled = enabled;
}

void WebProcessPool::clearPermanentCredentialsForProtectionSpace(WebCore::ProtectionSpace&& protectionSpace)
{
    auto sharedStorage = [NSURLCredentialStorage sharedCredentialStorage];
    auto credentials = [sharedStorage credentialsForProtectionSpace:protectionSpace.nsSpace()];
    for (NSString* user in credentials) {
        auto credential = credentials[user];
        if (credential.persistence == NSURLCredentialPersistencePermanent)
            [sharedStorage removeCredential:credentials[user] forProtectionSpace:protectionSpace.nsSpace()];
    }
}

int networkProcessLatencyQOS()
{
    static const int qos = [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitNetworkProcessLatencyQOS"];
    return qos;
}

int networkProcessThroughputQOS()
{
    static const int qos = [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitNetworkProcessThroughputQOS"];
    return qos;
}

int webProcessLatencyQOS()
{
    static const int qos = [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitWebProcessLatencyQOS"];
    return qos;
}

int webProcessThroughputQOS()
{
    static const int qos = [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitWebProcessThroughputQOS"];
    return qos;
}

static WeakHashSet<LockdownModeObserver>& lockdownModeObservers()
{
    RELEASE_ASSERT(isMainRunLoop());
    static NeverDestroyed<WeakHashSet<LockdownModeObserver>> observers;
    return observers;
}

static std::optional<bool>& isLockdownModeEnabledGloballyForTesting()
{
    static NeverDestroyed<std::optional<bool>> enabledForTesting;
    return enabledForTesting;
}

static bool isLockdownModeEnabledBySystemIgnoringCaching()
{
    if (auto& enabledForTesting = isLockdownModeEnabledGloballyForTesting())
        return *enabledForTesting;

    if (![_WKSystemPreferences isCaptivePortalModeEnabled])
        return false;

#if PLATFORM(IOS_FAMILY)
    if (processHasContainer() && [_WKSystemPreferences isCaptivePortalModeIgnored:pathForProcessContainer()])
        return false;
#endif
    
#if PLATFORM(MAC)
    if (!WebCore::MacApplication::isSafari() && !WebCore::MacApplication::isMiniBrowser())
        return false;
#endif
    
    return true;
}

void WebProcessPool::lockdownModeStateChanged()
{
    auto isNowEnabled = isLockdownModeEnabledBySystemIgnoringCaching();
    if (cachedLockdownModeEnabledGlobally() != isNowEnabled) {
        lockdownModeObservers().forEach([](auto& observer) { observer.willChangeLockdownMode(); });
        cachedLockdownModeEnabledGlobally() = isNowEnabled;
        lockdownModeObservers().forEach([](auto& observer) { observer.didChangeLockdownMode(); });
    }

    WEBPROCESSPOOL_RELEASE_LOG(Loading, "WebProcessPool::lockdownModeStateChanged() isNowEnabled=%d", isNowEnabled);

    for (auto& process : m_processes) {
        bool processHasLockdownModeEnabled = process->lockdownMode() == WebProcessProxy::LockdownMode::Enabled;
        if (processHasLockdownModeEnabled == isNowEnabled)
            continue;

        for (auto& page : process->pages()) {
            // When the Lockdown mode changes globally at system level, we reload every page that relied on the system setting (rather
            // than being explicitly opted in/out by the client app at navigation or PageConfiguration level).
            if (page->isLockdownModeExplicitlySet())
                continue;

            WEBPROCESSPOOL_RELEASE_LOG(Loading, "WebProcessPool::lockdownModeStateChanged() Reloading page with pageProxyID=%" PRIu64 " due to Lockdown mode change", page->identifier().toUInt64());
            page->reload({ });
        }
    }
}

void addLockdownModeObserver(LockdownModeObserver& observer)
{
    // Make sure cachedLockdownModeEnabledGlobally() gets initialized so lockdownModeStateChanged() can track changes.
    auto& cachedState = cachedLockdownModeEnabledGlobally();
    if (!cachedState)
        cachedState = isLockdownModeEnabledBySystemIgnoringCaching();

    lockdownModeObservers().add(observer);
}

void removeLockdownModeObserver(LockdownModeObserver& observer)
{
    lockdownModeObservers().remove(observer);
}

bool lockdownModeEnabledBySystem()
{
    auto& cachedState = cachedLockdownModeEnabledGlobally();
    if (!cachedState)
        cachedState = isLockdownModeEnabledBySystemIgnoringCaching();
    return *cachedState;
}

void setLockdownModeEnabledGloballyForTesting(std::optional<bool> enabledForTesting)
{
    if (isLockdownModeEnabledGloballyForTesting() == enabledForTesting)
        return;

    isLockdownModeEnabledGloballyForTesting() = enabledForTesting;

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->lockdownModeStateChanged();
}

#if PLATFORM(IOS_FAMILY)

void WebProcessPool::applicationIsAboutToSuspend()
{
    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "applicationIsAboutToSuspend: Terminating non-critical processes");

    m_backForwardCache->pruneToSize(1);
    m_webProcessCache->clear();
}

void WebProcessPool::notifyProcessPoolsApplicationIsAboutToSuspend()
{
    for (auto& processPool : allProcessPools())
        processPool->applicationIsAboutToSuspend();
}

void WebProcessPool::setProcessesShouldSuspend(bool shouldSuspend)
{
    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "setProcessesShouldSuspend: Processes should suspend %d", shouldSuspend);

    if (m_processesShouldSuspend == shouldSuspend)
        return;

    m_processesShouldSuspend = shouldSuspend;
    for (auto& process : m_processes)
        process->throttler().setAllowsActivities(!m_processesShouldSuspend);
}

#endif

void WebProcessPool::initializeClassesForParameterCoding()
{
    const auto& customClasses = m_configuration->customClassesForParameterCoder();
    if (customClasses.isEmpty())
        return;

    auto standardClasses = [NSSet setWithObjects:[NSArray class], [NSData class], [NSDate class], [NSDictionary class], [NSNull class],
        [NSNumber class], [NSSet class], [NSString class], [NSTimeZone class], [NSURL class], [NSUUID class], nil];
    
    auto mutableSet = adoptNS([standardClasses mutableCopy]);

    for (const auto& customClass : customClasses) {
        auto className = customClass.utf8();
        Class objectClass = objc_lookUpClass(className.data());
        if (!objectClass) {
            WTFLogAlways("InjectedBundle::extendClassesForParameterCoder - Class %s is not a valid Objective C class.\n", className.data());
            break;
        }

        [mutableSet.get() addObject:objectClass];
    }

    m_classesForParameterCoder = mutableSet;
}

NSSet *WebProcessPool::allowedClassesForParameterCoding() const
{
    return m_classesForParameterCoder.get();
}

#if ENABLE(CFPREFS_DIRECT_MODE)
void WebProcessPool::notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    for (auto process : m_processes)
        process->send(Messages::WebProcess::NotifyPreferencesChanged(domain, key, encodedValue), 0);

#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->send(Messages::GPUProcess::NotifyPreferencesChanged(domain, key, encodedValue), 0);
#endif
    
    WebsiteDataStore::forEachWebsiteDataStore([domain, key, encodedValue] (WebsiteDataStore& dataStore) {
        if (auto* networkProcess = dataStore.networkProcessIfExists())
            networkProcess->send(Messages::NetworkProcess::NotifyPreferencesChanged(domain, key, encodedValue), 0);
    });

    if (key == WKLockdownModeEnabledKey)
        lockdownModeStateChanged();
}
#endif // ENABLE(CFPREFS_DIRECT_MODE)

#if PLATFORM(MAC)
static void displayReconfigurationCallBack(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    RunLoop::main().dispatch([display, flags]() {
        auto screenProperties = WebCore::collectScreenProperties();
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            processPool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));
            processPool->sendToAllProcesses(Messages::WebProcess::DisplayConfigurationChanged(display, flags));
            if (auto gpuProcess = processPool->gpuProcess()) {
                gpuProcess->displayConfigurationChanged(display, flags);
                gpuProcess->setScreenProperties(screenProperties);
            }
        }
    });
}

void WebProcessPool::registerDisplayConfigurationCallback()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, nullptr);
        });
}

static void webProcessPoolHighDynamicRangeDidChangeCallback(CMNotificationCenterRef, const void*, CFStringRef notificationName, const void*, CFTypeRef)
{
    RunLoop::main().dispatch([] {
        auto properties = WebCore::collectScreenProperties();
        for (auto& pool : WebProcessPool::allProcessPools())
            pool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(properties));
    });
}

void WebProcessPool::registerHighDynamicRangeChangeCallback()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
        if (!PAL::isMediaToolboxFrameworkAvailable()
            || !PAL::canLoad_MediaToolbox_MTShouldPlayHDRVideo()
            || !PAL::canLoad_MediaToolbox_MT_GetShouldPlayHDRVideoNotificationSingleton()
            || !PAL::canLoad_MediaToolbox_kMTSupportNotification_ShouldPlayHDRVideoChanged())
            return;

        auto center = PAL::softLink_CoreMedia_CMNotificationCenterGetDefaultLocalCenter();
        auto notification = PAL::get_MediaToolbox_kMTSupportNotification_ShouldPlayHDRVideoChanged();
        auto object = PAL::softLinkMediaToolboxMT_GetShouldPlayHDRVideoNotificationSingleton();

        // Note: CMNotificationCenterAddListener requires a non-null listener pointer. Just use the singleton
        // object itself as the "listener", since the notification method is a static global and doesn't need
        // the listener pointer at all.
        PAL::softLink_CoreMedia_CMNotificationCenterAddListener(center, object, webProcessPoolHighDynamicRangeDidChangeCallback, notification, object, 0);
    });
}

void WebProcessPool::systemWillSleep()
{
    sendToAllProcesses(Messages::WebProcess::SystemWillSleep());
}

void WebProcessPool::systemDidWake()
{
    sendToAllProcesses(Messages::WebProcess::SystemDidWake());
}
#endif // PLATFORM(MAC)

#if PLATFORM(IOS)
void WebProcessPool::registerHighDynamicRangeChangeCallback()
{
    static NeverDestroyed<LowPowerModeNotifier> notifier { [](bool) {
        auto properties = WebCore::collectScreenProperties();
        for (auto& pool : WebProcessPool::allProcessPools())
            pool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(properties));
    } };
}
#endif // PLATFORM(MAC) || PLATFORM(IOS)

} // namespace WebKit
