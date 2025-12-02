/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#import "AdditionalFonts.h"
#import "ArgumentCodersCocoa.h"
#import "CookieStorageUtilsCF.h"
#import "DefaultWebBrowserChecks.h"
#import "ExtensionCapabilityGranter.h"
#import "LegacyCustomProtocolManagerClient.h"
#import "LockdownModeObserver.h"
#import "Logging.h"
#import "MediaCapability.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkProcessMessages.h"
#import "NetworkProcessProxy.h"
#import "PreferenceObserver.h"
#import "ProcessThrottler.h"
#import "SandboxExtension.h"
#import "SandboxUtilities.h"
#import "TextChecker.h"
#import "WKContentRuleListInternal.h"
#import "WKContentRuleListStore.h"
#import "WebBackForwardCache.h"
#import "WebCompiledContentRuleList.h"
#import "WebMemoryPressureHandler.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPreferencesDefaultValues.h"
#import "WebPreferencesKeys.h"
#import "WebPrivacyHelpers.h"
#import "WebProcessCache.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessMessages.h"
#import "WindowServerConnection.h"
#import "_WKSystemPreferencesInternal.h"
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <WebCore/Color.h>
#import <WebCore/FontCacheCoreText.h>
#import <WebCore/LocalizedDeviceModel.h>
#import <WebCore/LowPowerModeNotifier.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PictureInPictureSupport.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/PowerSourceNotifier.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/UTIUtilities.h>
#import <objc/runtime.h>
#import <pal/Logging.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cf/CFNotificationCenterSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <sys/param.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/FileSystem.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/NotificationCenterCF.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>
#import <wtf/spi/cocoa/XTSPI.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/TextStream.h>

#if ENABLE(NOTIFY_BLOCKING) || PLATFORM(MAC)
#include <notify.h>
#endif

#if ENABLE(REMOTE_INSPECTOR)
#import <JavaScriptCore/RemoteInspector.h>
#import <JavaScriptCore/RemoteInspectorConstants.h>
#endif

#if PLATFORM(MAC)
#import "WebInspectorPreferenceObserver.h"
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
#import <pal/spi/ios/GraphicsServicesSPI.h>
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

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>

#if __has_include(<WebKitAdditions/WebProcessPoolAdditions.h>)
#import <WebKitAdditions/WebProcessPoolAdditions.h>
#endif

static NSString * const WebServiceWorkerRegistrationDirectoryDefaultsKey = @"WebServiceWorkerRegistrationDirectory";
static NSString * const WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
static NSString * const WebKitJSCJITEnabledDefaultsKey = @"WebKitJSCJITEnabledDefaultsKey";
static NSString * const WebKitJSCFTLJITEnabledDefaultsKey = @"WebKitJSCFTLJITEnabledDefaultsKey";

#if !PLATFORM(IOS_FAMILY) || PLATFORM(MACCATALYST)
static NSString * const WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification = @"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification";
static CFStringRef AppleColorPreferencesChangedNotification = CFSTR("AppleColorPreferencesChangedNotification");
#endif

static NSString * const WebKitSuppressMemoryPressureHandlerDefaultsKey = @"WebKitSuppressMemoryPressureHandler";

static NSString * const WebKitMediaStreamingActivity = @"WebKitMediaStreamingActivity";

#if !RELEASE_LOG_DISABLED
static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
static NSString * const kPLTaskingStartNotificationGlobal = @"kPLTaskingStartNotificationGlobal";
#endif

#if ENABLE(CONTENT_EXTENSIONS)
static NSString * const WebKitResourceMonitorURLsForTestingIdentifier = @"com.apple.WebPrivacy.ResourceMonitorURLsForTesting";
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices, BKSDisplayBrightnessGetCurrent, float, (), ());
#endif

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
SOFT_LINK_LIBRARY_OPTIONAL(libAccessibility)
SOFT_LINK_CONSTANT_MAY_FAIL(libAccessibility, kAXSReduceMotionAutoplayAnimatedImagesChangedNotification, CFStringRef)
#endif

#if PLATFORM(MAC)
SOFT_LINK_LIBRARY_WITH_PATH(libFontRegistry, "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/ATS.framework/Versions/A/Resources/")
SOFT_LINK(libFontRegistry, XTCopyPropertiesForAllFontsWithOptions, CFArrayRef, (CFSetRef propertyKeys, XTScope scope, XTOptions options), (propertyKeys, scope, options));
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

static void registerUserDefaults()
{
    RetainPtr registrationDictionary = adoptNS([[NSMutableDictionary alloc] init]);
    
    [registrationDictionary setObject:@YES forKey:WebKitJSCJITEnabledDefaultsKey];
    [registrationDictionary setObject:@YES forKey:WebKitJSCFTLJITEnabledDefaultsKey];
    [registrationDictionary setObject:@YES forKey:@"WebKitPrefersFullScreenDimming"];

    [[NSUserDefaults standardUserDefaults] registerDefaults:registrationDictionary.get()];
}

static std::optional<bool>& cachedLockdownModeEnabledGlobally()
{
    static std::optional<bool> cachedLockdownModeEnabledGlobally;
    return cachedLockdownModeEnabledGlobally;
}

#if PLATFORM(MAC)
static NSApplication* NSAppSingleton()
{
    return NSApp;
}
#endif

void WebProcessPool::updateProcessSuppressionState()
{
    bool enabled = processSuppressionEnabled();
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->setProcessSuppressionEnabled(enabled);
}

NSMutableDictionary *WebProcessPool::ensureBundleParameters()
{
    if (!m_bundleParameters)
        m_bundleParameters = adoptNS([[NSMutableDictionary alloc] init]);

    return m_bundleParameters.get();
}

RetainPtr<NSMutableDictionary> WebProcessPool::ensureProtectedBundleParameters()
{
    return ensureBundleParameters();
}

RetainPtr<NSMutableDictionary> WebProcessPool::protectedBundleParameters()
{
    return bundleParameters();
}

static AccessibilityPreferences accessibilityPreferences()
{
    AccessibilityPreferences preferences;

#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    preferences.reduceMotionEnabled = AXPreferenceHelpers::reduceMotionEnabled();
    preferences.increaseButtonLegibility = AXPreferenceHelpers::increaseButtonLegibility();
    preferences.enhanceTextLegibility = AXPreferenceHelpers::enhanceTextLegibility();
    preferences.darkenSystemColors = AXPreferenceHelpers::darkenSystemColors();
    preferences.invertColorsEnabled = AXPreferenceHelpers::invertColorsEnabled();
#endif
    preferences.enhanceTextLegibilityOverall = AXPreferenceHelpers::enhanceTextLegibilityOverall();
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    preferences.imageAnimationEnabled = AXPreferenceHelpers::imageAnimationEnabled();
#endif
#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    preferences.prefersNonBlinkingCursor = AXPreferenceHelpers::prefersNonBlinkingCursor();
#endif
    return preferences;
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
void WebProcessPool::setMediaAccessibilityPreferences(WebProcessProxy& process)
{
    static NeverDestroyed<OSObjectPtr<dispatch_queue_t>> mediaAccessibilityQueue = adoptOSObject(dispatch_queue_create("MediaAccessibility queue", DISPATCH_QUEUE_SERIAL));

    dispatch_async(mediaAccessibilityQueue.get().get(), [weakProcess = WeakPtr { process }] {
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
    for (Ref process : pool.processes()) {
        WTF::TextStream processDescription;
        processDescription << process;

        RegistrableDomain domain = process->site() ? process->site()->domain() : RegistrableDomain();
        String domainString = domain.isEmpty() ? "unknown"_s : domain.string();

        WTF::TextStream pageURLs;
        auto pages = process->pages();
        if (pages.isEmpty())
            pageURLs << "none";
        else {
            bool isFirst = true;
            for (auto& page : pages) {
                pageURLs << (isFirst ? "" : ", ") << page->currentURL();
                isFirst = false;
            }
        }

        RELEASE_LOG(Process, "WebProcessProxy %p - %" PUBLIC_LOG_STRING ", domain: %" PRIVATE_LOG_STRING ", pageURLs: %" SENSITIVE_LOG_STRING, process.ptr(), processDescription.release().utf8().data(), domainString.utf8().data(), pageURLs.release().utf8().data());
    }
}

void WebProcessPool::platformInitialize(NeedsGlobalStaticInitialization needsGlobalStaticInitialization)
{
#if PLATFORM(IOS_FAMILY)
    initializeHardwareKeyboardAvailability();
#endif

    registerNotificationObservers();

    if (needsGlobalStaticInitialization == NeedsGlobalStaticInitialization::No)
        return;

    registerUserDefaults();

    // FIXME: This should be able to share code with WebCore's MemoryPressureHandler (and be platform independent).
    // Right now it cannot because WebKit1 and WebKit2 need to be able to coexist in the UI process,
    // and you can only have one WebCore::MemoryPressureHandler.
    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitSuppressMemoryPressureHandler"])
        installMemoryPressureHandler();

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    if (!_MGCacheValid()) {
        dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [adoptNS([[objc_getClass("MobileGestaltHelperProxy") alloc] init]) proxyRebuildCache];
        });
    }
#endif

#if PLATFORM(MAC)
    [WKWebInspectorPreferenceObserver sharedInstance];
#endif

    PAL::registerNotifyCallback("com.apple.WebKit.logProcessState"_s, [] {
        for (const auto& pool : WebProcessPool::allProcessPools())
            logProcessPoolState(pool.get());
    });

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    PAL::registerNotifyCallback("com.apple.WebKit.restrictedDomains"_s, [] {
        RestrictedOpenerDomainsController::singleton();
    });
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
    parameters.accessibilityEnhancedUserInterfaceEnabled = [[NSAppSingleton() accessibilityAttributeValue:@"AXEnhancedUserInterface"] boolValue];
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    parameters.accessibilityEnhancedUserInterfaceEnabled = false;
#endif

    RetainPtr defaults = [NSUserDefaults standardUserDefaults];

    parameters.shouldEnableJIT = [defaults boolForKey:WebKitJSCJITEnabledDefaultsKey];
    parameters.shouldEnableFTLJIT = [defaults boolForKey:WebKitJSCFTLJITEnabledDefaultsKey];
    parameters.shouldEnableMemoryPressureReliefLogging = [defaults boolForKey:@"LogMemoryJetsamDetails"];
    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

    // FIXME: This should really be configurable; we shouldn't just blindly allow read access to the UI process bundle.
    parameters.uiProcessBundleResourcePath = m_resolvedPaths.uiProcessBundleResourcePath;
    if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.uiProcessBundleResourcePath, SandboxExtension::Type::ReadOnly))
        parameters.uiProcessBundleResourcePathExtensionHandle = WTFMove(*handle);

    parameters.uiProcessBundleIdentifier = applicationBundleIdentifier();

    parameters.latencyQOS = webProcessLatencyQOS();
    parameters.throughputQOS = webProcessThroughputQOS();

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    if (WebProcessProxy::shouldEnableRemoteInspector()) {
        auto handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.webinspector"_s }, process.auditToken());
        parameters.enableRemoteWebInspectorExtensionHandles = WTFMove(handles);

#if ENABLE(GPU_PROCESS)
        if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated()) {
            if (!gpuProcess->hasSentGPUToolsSandboxExtensions()) {
                auto gpuToolsHandle = GPUProcessProxy::createGPUToolsSandboxExtensionHandlesIfNeeded();
                gpuProcess->send(Messages::GPUProcess::UpdateSandboxAccess(WTFMove(gpuToolsHandle)), 0);
            }
        }
#endif
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

        RetainPtr<NSData> data = keyedArchiver.get().encodedData;

        parameters.bundleParameterData = API::Data::createWithoutCopying(data.get());
    }
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

#if !RELEASE_LOG_DISABLED
    parameters.shouldLogUserInteraction = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif

    auto screenProperties = WebCore::collectScreenProperties();
    parameters.screenProperties = WTFMove(screenProperties);
#if PLATFORM(MAC)
    parameters.useOverlayScrollbars = ([NSScroller preferredScrollerStyle] == NSScrollerStyleOverlay);
#endif

#if PLATFORM(VISION)
    auto metalDirectory = WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.WebContent/com.apple.metal"_s);
    if (auto metalDirectoryHandle = SandboxExtension::createHandleForReadWriteDirectory(metalDirectory))
        parameters.metalCacheDirectoryExtensionHandles.append(WTFMove(*metalDirectoryHandle));
    auto metalFEDirectory = WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.WebContent/com.apple.metalfe"_s);
    if (auto metalFEDirectoryHandle = SandboxExtension::createHandleForReadWriteDirectory(metalFEDirectory))
        parameters.metalCacheDirectoryExtensionHandles.append(WTFMove(*metalFEDirectoryHandle));
    auto gpuArchiverDirectory = WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.WebContent/com.apple.gpuarchiver"_s);
    if (auto gpuArchiverDirectoryHandle = SandboxExtension::createHandleForReadWriteDirectory(gpuArchiverDirectory))
        parameters.metalCacheDirectoryExtensionHandles.append(WTFMove(*gpuArchiverDirectoryHandle));
#endif

    parameters.systemHasBattery = systemHasBattery();
    parameters.systemHasAC = cachedSystemHasAC().value_or(true);

#if PLATFORM(IOS_FAMILY)
    parameters.currentUserInterfaceIdiom = PAL::currentUserInterfaceIdiom();
    parameters.supportsPictureInPicture = supportsPictureInPicture();
    parameters.cssValueToSystemColorMap = RenderThemeIOS::cssValueToSystemColorMap();
    parameters.focusRingColor = RenderThemeIOS::systemFocusRingColor();
    parameters.localizedDeviceModel = localizedDeviceModel();
    parameters.contentSizeCategory = contentSizeCategory();
#endif

    parameters.mobileGestaltExtensionHandle = process.createMobileGestaltSandboxExtensionIfNeeded();

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (auto launchServicesExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.coreservices.launchservicesd"_s, std::nullopt))
        parameters.launchServicesExtensionHandle = WTFMove(*launchServicesExtensionHandle);
#endif

#if HAVE(VIDEO_RESTRICTED_DECODING)
#if (PLATFORM(MAC) || PLATFORM(MACCATALYST)) && !ENABLE(TRUSTD_BLOCKING_IN_WEBCONTENT)
    // FIXME: this will not be needed when rdar://74144544 is fixed.
    if (auto trustdExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.trustd.agent"_s, std::nullopt))
        parameters.trustdExtensionHandle = WTFMove(*trustdExtensionHandle);
#endif
    parameters.enableDecodingHEIC = true;
    parameters.enableDecodingAVIF = true;
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

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    // FIXME: Filter by process's site when site isolation is enabled
    parameters.storageAccessUserAgentStringQuirksData = StorageAccessUserAgentStringQuirkController::sharedSingleton().cachedListData();

    for (auto&& entry : StorageAccessPromptQuirkController::sharedSingleton().cachedListData()) {
        if (!entry.triggerPages.isEmpty()) {
            for (auto&& page : entry.triggerPages)
                parameters.storageAccessPromptQuirksDomains.add(RegistrableDomain { page });
            continue;
        }
        for (auto&& domain : entry.quirkDomains.keys())
            parameters.storageAccessPromptQuirksDomains.add(domain);
    }

    parameters.scriptTrackingPrivacyRules = ScriptTrackingPrivacyController::sharedSingleton().cachedListData();
#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

#if ENABLE(NOTIFY_BLOCKING)
    parameters.notifyState = WTF::map(m_notifyState, [] (auto&& item) {
        return std::make_pair(item.key, item.value);
    });
#endif

#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    parameters.shouldInitializeAccessibility = m_hasReceivedAXRequestInUIProcess;
#endif

#if HAVE(LIQUID_GLASS)
    parameters.isLiquidGlassEnabled = isLiquidGlassEnabled();
#endif

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    parameters.isDebugLoggingEnabled = os_log_debug_enabled(OS_LOG_DEFAULT);
#endif
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters& parameters)
{
    parameters.uiProcessBundleIdentifier = applicationBundleIdentifier();

    RetainPtr defaults = [NSUserDefaults standardUserDefaults];

    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    parameters.uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage([[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]);
#endif

    parameters.enablePrivateClickMeasurement = ![defaults objectForKey:WebPreferencesKey::privateClickMeasurementEnabledKey().createNSString().get()] || [defaults boolForKey:WebPreferencesKey::privateClickMeasurementEnabledKey().createNSString().get()];
    parameters.ftpEnabled = [defaults objectForKey:WebPreferencesKey::ftpEnabledKey().createNSString().get()] && [defaults boolForKey:WebPreferencesKey::ftpEnabledKey().createNSString().get()];

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    parameters.storageAccessPromptQuirksData = StorageAccessPromptQuirkController::sharedSingleton().cachedListData();
#endif
}

void WebProcessPool::platformInvalidateContext()
{
    unregisterNotificationObservers();
}

#if PLATFORM(IOS_FAMILY)
void WebProcessPool::setJavaScriptConfigurationFileEnabledFromDefaults()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];

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
    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->enablePowerLogging();
}
#endif

#if PLATFORM(IOS_FAMILY)
void WebProcessPool::hardwareKeyboardAvailabilityChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    auto pool = extractWebProcessPool(observer);
    if (!pool)
        return;
    auto keyboardState = currentHardwareKeyboardState();
    if (keyboardState == pool->cachedHardwareKeyboardState())
        return;
    pool->setCachedHardwareKeyboardState(keyboardState);
    pool->hardwareKeyboardAvailabilityChanged();
}

void WebProcessPool::hardwareKeyboardAvailabilityChanged()
{
    for (Ref process : processes()) {
        auto pages = process->pages();
        for (auto& page : pages)
            page->hardwareKeyboardAvailabilityChanged(cachedHardwareKeyboardState());
    }
}

void WebProcessPool::initializeHardwareKeyboardAvailability()
{
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([weakThis = WeakPtr { *this }]() mutable {
        auto keyboardState = currentHardwareKeyboardState();
        callOnMainRunLoop([weakThis = WTFMove(weakThis), keyboardState] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->setCachedHardwareKeyboardState(keyboardState);
            protectedThis->hardwareKeyboardAvailabilityChanged();
        });
    }).get());
}
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(CFPREFS_DIRECT_MODE)
void WebProcessPool::startObservingPreferenceChanges()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
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

#if ENABLE(NOTIFY_BLOCKING)
#define WK_NOTIFICATION_COMMENT(...)
#define WK_NOTIFICATION(name) name ## _s,
    const Vector<ASCIILiteral> notificationMessages = {
#include "Resources/cocoa/NotificationAllowList/ForwardedNotifications.def"
#if PLATFORM(MAC)
#include "Resources/cocoa/NotificationAllowList/MacForwardedNotifications.def"
#else
#include "Resources/cocoa/NotificationAllowList/EmbeddedForwardedNotifications.def"
#endif
    };
#undef WK_NOTIFICATION
#undef WK_NOTIFICATION_COMMENT

    m_notifyTokens = WTF::compactMap(notificationMessages, [weakThis = WeakPtr { *this }](const ASCIILiteral& message) -> std::optional<int> {
        int notifyToken = 0;
        auto registerStatus = notify_register_dispatch(message, &notifyToken, globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [weakThis, message](int token) {
            uint64_t state = 0;
            auto status = notify_get_state(token, &state);
            callOnMainRunLoop([weakThis, message, state, status] {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->setNotifyState(message, status, state);
                String messageString(message);
                for (auto& process : protectedThis->m_processes) {
                    if (process->auditToken() && !WTF::hasEntitlement(process->auditToken().value(), "com.apple.developer.web-browser-engine.restrict.notifyd"_s))
                        continue;
                    process->send(Messages::WebProcess::PostNotification(messageString, (status == NOTIFY_STATUS_OK) ? std::optional<uint64_t>(state) : std::nullopt), 0);
                }
            });
        });
        if (registerStatus)
            return std::nullopt;

        if (RefPtr protectedThis = weakThis.get()) {
            uint64_t state;
            int stateStatus = notify_get_state(notifyToken, &state);
            protectedThis->setNotifyState(message, stateStatus, state);
        }

        return notifyToken;
    });

    const Vector<RetainPtr<NSString>> nsNotificationMessages = {
        NSProcessInfoPowerStateDidChangeNotification
    };
    m_notificationObservers = WTF::compactMap(nsNotificationMessages, [weakThis = WeakPtr { *this }](const RetainPtr<NSString>& message) -> RetainPtr<NSObject>  {
        RetainPtr observer = [[NSNotificationCenter defaultCenter] addObserverForName:message.get() object:nil queue:[NSOperationQueue currentQueue] usingBlock:[weakThis, message](NSNotification *notification) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (!protectedThis->m_processes.isEmpty()) {
                String messageString(message.get());
                for (auto& process : protectedThis->m_processes)
                    process->send(Messages::WebProcess::PostObserverNotification(messageString), 0);
            }
        }];
        return observer;
    });
#endif

#if !PLATFORM(IOS_FAMILY)
    m_powerObserver = makeUnique<WebCore::PowerObserver>([weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->sendToAllProcesses(Messages::WebProcess::SystemWillPowerOn());
    });
    m_systemSleepListener = PAL::SystemSleepListener::create(*this);
    // Listen for enhanced accessibility changes and propagate them to the WebProcess.
    m_enhancedAccessibilityObserver = [[NSNotificationCenter defaultCenter] addObserverForName:WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *note) {
        setEnhancedAccessibility([[retainPtr([note userInfo]) objectForKey:@"AXEnhancedUserInterface"] boolValue]);
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

    m_accessibilityDisplayOptionsNotificationObserver = [retainPtr([NSWorkspace.sharedWorkspace notificationCenter]) addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        screenPropertiesChanged();
    }];

    m_scrollerStyleNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSPreferredScrollerStyleDidChangeNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        auto scrollbarStyle = [NSScroller preferredScrollerStyle];
        sendToAllProcesses(Messages::WebProcess::ScrollerStylePreferenceChanged(scrollbarStyle));
    }];

    m_activationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidBecomeActiveNotification object:NSAppSingleton() queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
#if ENABLE(CFPREFS_DIRECT_MODE)
        startObservingPreferenceChanges();
#endif
        setApplicationIsActive(true);
    }];

    m_deactivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidResignActiveNotification object:NSAppSingleton() queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        setApplicationIsActive(false);
    }];

    m_didChangeScreenParametersNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidChangeScreenParametersNotification object:NSAppSingleton() queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        screenPropertiesChanged();
    }];
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    m_didBeginSuppressingHighDynamicRange = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationShouldBeginSuppressingHighDynamicRangeContentNotification object:NSAppSingleton() queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        suppressEDR(true);
    }];
    m_didEndSuppressingHighDynamicRange = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationShouldEndSuppressingHighDynamicRangeContentNotification object:NSAppSingleton() queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        suppressEDR(false);
    }];
#endif

    addCFNotificationObserver(colorPreferencesDidChangeCallback, RetainPtr { AppleColorPreferencesChangedNotification }.get(), CFNotificationCenterGetDistributedCenterSingleton());

    const char* messages[] = { kNotifyDSCacheInvalidation, kNotifyDSCacheInvalidationGroup, kNotifyDSCacheInvalidationHost, kNotifyDSCacheInvalidationService, kNotifyDSCacheInvalidationUser };
    m_openDirectoryNotifyTokens.reserveInitialCapacity(std::size(messages));
    for (auto* message : messages) {
        int notifyToken;
        notify_register_dispatch(message, &notifyToken, mainDispatchQueueSingleton(), ^(int token) {
            RELEASE_LOG(Notifications, "OpenDirectory invalidated cache");
#if ENABLE(GPU_PROCESS)
            auto handle = SandboxExtension::createHandleForMachLookup("com.apple.system.opendirectoryd.libinfo"_s, std::nullopt);
            if (!handle)
                return;
            if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
                gpuProcess->send(Messages::GPUProcess::OpenDirectoryCacheInvalidated(WTFMove(*handle)), 0);
#endif
            for (auto& process : m_processes) {
                if (!process->canSendMessage())
                    continue;
                auto handle = SandboxExtension::createHandleForMachLookup("com.apple.system.opendirectoryd.libinfo"_s, std::nullopt);
                if (!handle)
                    continue;
                auto bootstrapHandle = SandboxExtension::createHandleForMachBootstrapExtension();
                process->send(Messages::WebProcess::OpenDirectoryCacheInvalidated(WTFMove(*handle), WTFMove(bootstrapHandle)), 0);
            }
        });
        m_openDirectoryNotifyTokens.append(notifyToken);
    }
#elif !PLATFORM(MACCATALYST)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <https://webkit.org/b/255833> Adopt UIScreenBrightnessDidChangeNotification.
    addCFNotificationObserver(backlightLevelDidChangeCallback, (__bridge CFStringRef)UIBacklightLevelChangedNotification);
ALLOW_DEPRECATED_DECLARATIONS_END
#if PLATFORM(IOS) || PLATFORM(VISION)
#if ENABLE(REMOTE_INSPECTOR)
    addCFNotificationObserver(remoteWebInspectorEnabledCallback, CFSTR(WIRServiceEnabledNotification));
#endif
#endif // PLATFORM(IOS) || PLATFORM(VISION)
#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY)
    auto notificationName = adoptNS([[NSString alloc] initWithCString:kGSEventHardwareKeyboardAvailabilityChangedNotification encoding:NSUTF8StringEncoding]);
    addCFNotificationObserver(hardwareKeyboardAvailabilityChangedCallback, (__bridge CFStringRef)notificationName.get(), CFNotificationCenterGetDarwinNotifyCenterSingleton());

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
            if (PAL::updateCurrentUserInterfaceIdiom())
                sendToAllProcesses(Messages::WebProcess::UserInterfaceIdiomDidChange(PAL::currentUserInterfaceIdiom()));
        }];
    }
#endif

    m_finishedMobileAssetFontDownloadObserver = [[NSNotificationCenter defaultCenter] addObserverForName:@"FontActivateNotification" object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        RetainPtr fontFamily = dynamic_objc_cast<NSString>(notification.userInfo[@"FontActivateNotificationFontFamilyKey"]);
        if (fontFamily) {
            RetainPtr ctFont = adoptCF(CTFontCreateWithName(bridge_cast(fontFamily.get()), 0.0, nullptr));
            RetainPtr downloaded = adoptCF(static_cast<CFBooleanRef>(CTFontCopyAttribute(ctFont.get(), kCTFontDownloadedAttribute)));
            if (downloaded == kCFBooleanFalse)
                return;
            RetainPtr url = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(ctFont.get(), kCTFontURLAttribute)));
            for (Ref process : m_processes) {
                if (!process->canSendMessage())
                    continue;
                process->send(Messages::WebProcess::RegisterAdditionalFonts(AdditionalFonts::additionalFonts({ URL(url.get()) }, process->auditToken())), 0);
            }
        }
    }];

    m_powerSourceNotifier = WTF::makeUnique<WebCore::PowerSourceNotifier>([weakThis = WeakPtr { this }] (bool hasAC) {
        if (RefPtr webProcessPool = weakThis.get())
            webProcessPool->sendToAllProcesses(Messages::WebProcess::PowerSourceDidChange(hasAC));
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
        addCFNotificationObserver(accessibilityPreferencesChangedCallback, getkAXSReduceMotionAutoplayAnimatedImagesChangedNotificationSingleton());
#endif
#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    addCFNotificationObserver(accessibilityPreferencesChangedCallback, kAXSPrefersNonBlinkingCursorIndicatorDidChangeNotification);
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
#if ENABLE(NOTIFY_BLOCKING)
    for (auto token : m_notifyTokens)
        notify_cancel(token);
    for (auto observer : m_notificationObservers)
        [[NSNotificationCenter defaultCenter] removeObserver:observer.get()];
    m_notifyState.clear();
#endif
#if !PLATFORM(IOS_FAMILY)
    m_powerObserver = nullptr;
    m_systemSleepListener = nullptr;
    [[NSNotificationCenter defaultCenter] removeObserver:m_enhancedAccessibilityObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticTextReplacementNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticSpellingCorrectionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticQuoteSubstitutionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticDashSubstitutionNotificationObserver.get()];
    [retainPtr([NSWorkspace.sharedWorkspace notificationCenter]) removeObserver:m_accessibilityDisplayOptionsNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_scrollerStyleNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_deactivationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_didChangeScreenParametersNotificationObserver.get()];
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    [[NSNotificationCenter defaultCenter] removeObserver:m_didBeginSuppressingHighDynamicRange.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_didEndSuppressingHighDynamicRange.get()];
#endif
    removeCFNotificationObserver(RetainPtr { AppleColorPreferencesChangedNotification }.get(), CFNotificationCenterGetDistributedCenterSingleton());
    for (auto token : m_openDirectoryNotifyTokens)
        notify_cancel(token);
#elif !PLATFORM(MACCATALYST)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <https://webkit.org/b/255833> Adopt UIScreenBrightnessDidChangeNotification.
    removeCFNotificationObserver((__bridge CFStringRef)UIBacklightLevelChangedNotification);
ALLOW_DEPRECATED_DECLARATIONS_END
#if PLATFORM(IOS) || PLATFORM(VISION)
#if ENABLE(REMOTE_INSPECTOR)
    removeCFNotificationObserver(CFSTR(WIRServiceEnabledNotification));
#endif
#endif // PLATFORM(IOS) || PLATFORM(VISION)
#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:m_accessibilityEnabledObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_applicationLaunchObserver.get()];
    auto notificationName = adoptNS([[NSString alloc] initWithCString:kGSEventHardwareKeyboardAvailabilityChangedNotification encoding:NSUTF8StringEncoding]);
    removeCFNotificationObserver((__bridge CFStringRef)notificationName.get());
#endif

    [[NSNotificationCenter defaultCenter] removeObserver:m_activationObserver.get()];

    m_powerSourceNotifier = nullptr;

    [[NSNotificationCenter defaultCenter] removeObserver:m_finishedMobileAssetFontDownloadObserver.get()];

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

#if ENABLE(NOTIFY_BLOCKING)

void WebProcessPool::setNotifyState(const String& name, int status, uint64_t state)
{
    if (status == NOTIFY_STATUS_OK && state)
        m_notifyState.set(name, state);
    else
        m_notifyState.remove(name);
}

#endif

bool WebProcessPool::isURLKnownHSTSHost(const String& urlString) const
{
    RetainPtr<CFURLRef> url = URL { urlString }.createCFURL();

    return _CFNetworkIsKnownHSTSHostWithSession(url.get(), nullptr);
}

// FIXME: Deprecated. Left here until a final decision is made.
void WebProcessPool::setCookieStoragePartitioningEnabled(bool enabled)
{
    m_cookieStoragePartitioningEnabled = enabled;
}

void WebProcessPool::clearPermanentCredentialsForProtectionSpace(WebCore::ProtectionSpace&& protectionSpace)
{
    RetainPtr sharedStorage = [NSURLCredentialStorage sharedCredentialStorage];
    RetainPtr space = protectionSpace.nsSpace();
    RetainPtr credentials = [sharedStorage credentialsForProtectionSpace:space.get()];
    for (NSString* user in credentials.get()) {
        RetainPtr<NSURLCredential> credential = credentials.get()[user];
        if (credential.get().persistence == NSURLCredentialPersistencePermanent)
            [sharedStorage removeCredential:retainPtr(credentials.get()[user]).get() forProtectionSpace:space.get()];
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
    if (processHasContainer() && [_WKSystemPreferences isCaptivePortalModeIgnored:pathForProcessContainer().createNSString().get()])
        return false;
#endif
    
#if PLATFORM(MAC)
    if (!WTF::MacApplication::isSafari() && !WTF::MacApplication::isMiniBrowser())
        return false;
#endif
    
    return true;
}

void WebProcessPool::lockdownModeStateChanged()
{
    auto isNowEnabled = isLockdownModeEnabledBySystemIgnoringCaching();
    if (cachedLockdownModeEnabledGlobally() != isNowEnabled) {
        lockdownModeObservers().forEach([](Ref<LockdownModeObserver> observer) { observer->willChangeLockdownMode(); });
        cachedLockdownModeEnabledGlobally() = isNowEnabled;
        lockdownModeObservers().forEach([](Ref<LockdownModeObserver> observer) { observer->didChangeLockdownMode(); });
    }

    WEBPROCESSPOOL_RELEASE_LOG(Loading, "WebProcessPool::lockdownModeStateChanged() isNowEnabled=%d", isNowEnabled);

    for (Ref process : m_processes) {
        bool processHasLockdownModeEnabled = process->lockdownMode() == WebProcessProxy::LockdownMode::Enabled;
        if (processHasLockdownModeEnabled == isNowEnabled)
            continue;

        for (Ref page : process->pages()) {
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
    for (auto& process : m_processes) {
        process->protectedThrottler()->setAllowsActivities(!m_processesShouldSuspend);

#if ENABLE(WEBXR) && !USE(OPENXR)
        if (!m_processesShouldSuspend) {
            for (Ref page : process->pages())
                page->restartXRSessionActivityOnProcessResumeIfNeeded();
        }
#endif
    }
}

#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
void WebProcessPool::notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    for (Ref process : m_processes)
        process->notifyPreferencesChanged(domain, key, encodedValue);

    if (key == WKLockdownModeEnabledKey)
        lockdownModeStateChanged();
}
#endif // ENABLE(CFPREFS_DIRECT_MODE)

void WebProcessPool::screenPropertiesChanged()
{
    auto screenProperties = WebCore::collectScreenProperties();
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_suppressEDR) {
        for (auto& properties : screenProperties.screenDataMap.values()) {
            constexpr auto maxSuppressedHeadroom = 1.6f;
            auto suppressedHeadroom = std::min(maxSuppressedHeadroom, properties.currentEDRHeadroom);
            properties.currentEDRHeadroom = suppressedHeadroom;
            properties.suppressEDR = true;
        }
    }
#endif
    sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));

#if PLATFORM(MAC) && ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = this->gpuProcess())
        gpuProcess->setScreenProperties(screenProperties);
#endif
}

#if PLATFORM(MAC)
void WebProcessPool::displayPropertiesChanged(const WebCore::ScreenProperties& screenProperties, WebCore::PlatformDisplayID displayID, CGDisplayChangeSummaryFlags flags)
{
    sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));

    if (auto* displayLink = displayLinks().existingDisplayLinkForDisplay(displayID))
        displayLink->displayPropertiesChanged();

#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = this->gpuProcess())
        gpuProcess->setScreenProperties(screenProperties);
#endif
}

static void displayReconfigurationCallBack(CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    RunLoop::mainSingleton().dispatch([displayID, flags]() {
        auto screenProperties = WebCore::collectScreenProperties();
        for (auto& processPool : WebProcessPool::allProcessPools())
            processPool->displayPropertiesChanged(screenProperties, displayID, flags);
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

static void webProcessPoolHighDynamicRangeDidChangeCallback(CFNotificationCenterRef, void*, CFNotificationName, const void*, CFDictionaryRef)
{
    RunLoop::mainSingleton().dispatch([] {
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

        CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenterSingleton(), nullptr, webProcessPoolHighDynamicRangeDidChangeCallback, kMTSupportNotification_ShouldPlayHDRVideoChanged, MT_GetShouldPlayHDRVideoNotificationSingleton(), static_cast<CFNotificationSuspensionBehavior>(0));
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

#if PLATFORM(IOS) || PLATFORM(VISION)
void WebProcessPool::registerHighDynamicRangeChangeCallback()
{
    static NeverDestroyed<LowPowerModeNotifier> notifier { [](bool) {
        auto properties = WebCore::collectScreenProperties();
        for (auto& pool : WebProcessPool::allProcessPools())
            pool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(properties));
    } };
}
#endif // PLATFORM(IOS) || PLATFORM(VISION)

#if PLATFORM(IOS_FAMILY)
void WebProcessPool::didRefreshDisplay()
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    float headroom = currentEDRHeadroomForDisplay(primaryScreenDisplayID());
    if (m_currentEDRHeadroom != headroom) {
        m_currentEDRHeadroom = headroom;
        screenPropertiesChanged();
    }
#endif
}
#endif

void WebProcessPool::suppressEDR(bool suppressEDR)
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_suppressEDR == suppressEDR)
        return;

    m_suppressEDR = suppressEDR;
    screenPropertiesChanged();
#else
    UNUSED_PARAM(m_suppressEDR);
#endif
}

#if ENABLE(EXTENSION_CAPABILITIES)
ExtensionCapabilityGranter& WebProcessPool::extensionCapabilityGranter()
{
    if (!m_extensionCapabilityGranter)
        m_extensionCapabilityGranter = ExtensionCapabilityGranter::create();
    return *m_extensionCapabilityGranter;
}
#endif

#if PLATFORM(IOS_FAMILY)
HardwareKeyboardState WebProcessPool::cachedHardwareKeyboardState() const
{
    RELEASE_ASSERT(isMainRunLoop());
    return m_hardwareKeyboardState;
}

void WebProcessPool::setCachedHardwareKeyboardState(HardwareKeyboardState hardwareKeyboardState)
{
    RELEASE_ASSERT(isMainRunLoop());
    m_hardwareKeyboardState = hardwareKeyboardState;
}
#endif

#if ENABLE(CONTENT_EXTENSIONS)
static RefPtr<WebCompiledContentRuleList> createCompiledContentRuleList(WKContentRuleList* list)
{
    if (!list)
        return nullptr;

    auto data = list->_contentRuleList->compiledRuleList().data();
    return WebCompiledContentRuleList::create(WTFMove(data));
}

void WebProcessPool::platformLoadResourceMonitorRuleList(CompletionHandler<void(RefPtr<WebCompiledContentRuleList>)>&& completionHandler)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    RELEASE_LOG(ResourceMonitoring, "WebProcessPool::platformLoadResourceMonitorRuleList request to load rule list.");

    ResourceMonitorURLsController::singleton().prepare([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](WKContentRuleList *list, bool updated) mutable {
        RefPtr<WebCompiledContentRuleList> ruleList;

        if (RefPtr protectedThis = weakThis.get()) {
            if (list && (updated || !protectedThis->m_resourceMonitorRuleListCache)) {
                RELEASE_LOG(ResourceMonitoring, "WebProcessPool::platformLoadResourceMonitorRuleList rule list is loaded.");
                ruleList = createCompiledContentRuleList(list);
            } else
                RELEASE_LOG_ERROR(ResourceMonitoring, "WebProcessPool::platformLoadResourceMonitorRuleList failed to load rule list.");
        }
        completionHandler(WTFMove(ruleList));
    });
#else
    completionHandler(nullptr);
#endif
}

void WebProcessPool::platformCompileResourceMonitorRuleList(const String& rulesText, CompletionHandler<void(RefPtr<WebCompiledContentRuleList>)>&& completionHandler)
{
    StringView view { rulesText };
    RetainPtr source = view.createNSStringWithoutCopying();
    RetainPtr store = [WKContentRuleListStore defaultStore];

    [store compileContentRuleListForIdentifier:WebKitResourceMonitorURLsForTestingIdentifier encodedContentRuleList:source.get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](WKContentRuleList *list, NSError *error) mutable {
        if (error || !list)
            RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to compile test urls");

        completionHandler(createCompiledContentRuleList(list));
    }).get()];
}

String WebProcessPool::platformResourceMonitorRuleListSourceForTesting()
{
#if HAVE(RESOURCE_MONITOR_RULE_LIST_SOURCE_FOR_TESTING)
    return resourceMonitorRuleListSourceForTestingCocoa();
#else
    return emptyString();
#endif
}
#endif

template <typename Collection>
static Vector<SandboxExtension::Handle> sandboxExtensionsForFonts(const Collection& fontPathURLs, std::optional<audit_token_t> auditToken)
{
    Vector<SandboxExtension::Handle> handles;
    for (auto& fontPathURL : fontPathURLs) {
        std::optional<SandboxExtension::Handle> sandboxExtensionHandle;
        if (auditToken)
            sandboxExtensionHandle = SandboxExtension::createHandleForReadByAuditToken(fontPathURL.fileSystemPath(), *auditToken);
        else
            sandboxExtensionHandle = SandboxExtension::createHandle(fontPathURL.fileSystemPath(), SandboxExtension::Type::ReadOnly);
        if (sandboxExtensionHandle)
            handles.append(WTFMove(*sandboxExtensionHandle));
    }
    return handles;
}

#if PLATFORM(MAC)
void WebProcessPool::registerUserInstalledFonts(WebProcessProxy& process)
{
    if (m_userInstalledFontURLs) {
        process.send(Messages::WebProcess::RegisterFontMap(*m_userInstalledFontURLs, *m_userInstalledFontFamilyMap,  sandboxExtensionsForFonts(*m_sandboxExtensionURLs, process.auditToken())), 0);
        return;
    }

    HashMap<String, URL> fontURLs;
    HashMap<String, Vector<String>> fontFamilyMap;
    Vector<URL> sandboxExtensionURLs;

    RELEASE_LOG(Process, "WebProcessPool::registerUserInstalledFonts: start registering fonts");
    RetainPtr requestedProperties = [NSSet setWithArray:@[@"NSFontNameAttribute", @"NSFontFamilyAttribute", @"NSCTFontFileURLAttribute", @"NSCTFontUserInstalledAttribute"]];
    RetainPtr fontProperties = adoptCF(XTCopyPropertiesForAllFontsWithOptions(bridge_cast(requestedProperties.get()), kXTScopeGlobal, kXTOptionsDoNotSortResults));
    if (!fontProperties)
        return;
    for (CFIndex i = 0; i < CFArrayGetCount(fontProperties.get()); ++i) {
        RetainPtr fontDictionary = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(fontProperties.get(), i));
        if (!fontDictionary)
            continue;
        RetainPtr cfFontURL = checked_cf_cast<CFURLRef>(CFDictionaryGetValue(fontDictionary.get(), CFSTR("NSCTFontFileURLAttribute")));
        URL fontURL(cfFontURL.get());
        if (fontURL.string().startsWith("file:///System/Library/Fonts/"_s))
            continue;
        if (fontURL.string().startsWith("file:///System/Library/PrivateFrameworks/"_s))
            continue;
        RetainPtr fontNameAttribute = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(fontDictionary.get(), CFSTR("NSFontNameAttribute")));
        RetainPtr fontFamilyNameAttribute = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(fontDictionary.get(), CFSTR("NSFontFamilyAttribute")));
        String fontName(fontNameAttribute.get());
        String fontFamilyName(fontFamilyNameAttribute.get());
        auto fontNameLowerCase = fontName.convertToASCIILowercase();
        if (fontNameLowerCase.isEmpty())
            continue;
        fontURLs.add(fontNameLowerCase, fontURL);
        auto fontFamilyNameLowerCase = fontFamilyName.convertToASCIILowercase();
        if (fontFamilyNameLowerCase.isEmpty())
            continue;
        auto fontNames = fontFamilyMap.find(fontFamilyNameLowerCase);
        if (fontNames != fontFamilyMap.end())
            fontNames->value.append(fontNameLowerCase);
        else {
            Vector<String> fontNames { fontNameLowerCase };
            fontFamilyMap.add(fontFamilyNameLowerCase, WTFMove(fontNames));
        }
    }
    RELEASE_LOG(Process, "WebProcessPool::registerUserInstalledFonts: done registering fonts");

    RetainPtr assetFontURL7 = adoptNS([[NSURL alloc] initFileURLWithPath:@"/System/Library/AssetsV2/com_apple_MobileAsset_Font7" isDirectory:YES]);
    RetainPtr assetFontURL8 = adoptNS([[NSURL alloc] initFileURLWithPath:@"/System/Library/AssetsV2/com_apple_MobileAsset_Font8" isDirectory:YES]);
    sandboxExtensionURLs.append(URL(assetFontURL7.get()));
    sandboxExtensionURLs.append(URL(assetFontURL8.get()));

    process.send(Messages::WebProcess::RegisterFontMap(fontURLs, fontFamilyMap, sandboxExtensionsForFonts(sandboxExtensionURLs, process.auditToken())), 0);
    m_userInstalledFontURLs = WTFMove(fontURLs);
    m_userInstalledFontFamilyMap = WTFMove(fontFamilyMap);
    m_sandboxExtensionURLs = WTFMove(sandboxExtensionURLs);
}

void WebProcessPool::registerAdditionalFonts(NSArray *fontNames)
{
    if (!fontNames)
        return;

    if (!m_userInstalledFontURLs) {
        m_userInstalledFontURLs = HashMap<String, URL>();
        m_userInstalledFontFamilyMap = HashMap<String, Vector<String>>();
        m_sandboxExtensionURLs = Vector<URL>();
    }

    for (NSString *nsFontName : fontNames) {
        RetainPtr ctFont = adoptCF(CTFontCreateWithName(bridge_cast(nsFontName), 0.0, nullptr));
        RetainPtr downloaded = adoptCF(static_cast<CFBooleanRef>(CTFontCopyAttribute(ctFont.get(), kCTFontDownloadedAttribute)));
        if (downloaded == kCFBooleanFalse)
            return;
        RetainPtr url = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(ctFont.get(), kCTFontURLAttribute)));
        URL fontURL(url.get());
        String fontName(nsFontName);
        m_userInstalledFontURLs->add(fontName, fontURL);
        m_sandboxExtensionURLs->append(WTFMove(fontURL));
    }

    for (Ref process : m_processes) {
        if (!process->canSendMessage())
            continue;
        process->send(Messages::WebProcess::RegisterFontMap(*m_userInstalledFontURLs, *m_userInstalledFontFamilyMap, sandboxExtensionsForFonts(*m_sandboxExtensionURLs, process->auditToken())), 0);
    }
}
#endif // PLATFORM(MAC)

static URL fontURLFromName(ASCIILiteral fontName)
{
    RetainPtr cfFontName = fontName.createCFString();
    RetainPtr font = adoptCF(CTFontCreateWithName(cfFontName.get(), 0.0, nullptr));
    return URL(adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(font.get(), kCTFontURLAttribute))).get());
}

static RetainPtr<CTFontDescriptorRef> fontDescription(ASCIILiteral fontName)
{
    RetainPtr nsFontName = fontName.createNSString();
    RetainPtr attributes = @{ bridge_cast(kCTFontFamilyNameAttribute): nsFontName.get(), bridge_cast(kCTFontRegistrationScopeAttribute): @(kCTFontPriorityComputer) };
    return adoptCF(CTFontDescriptorCreateWithAttributes((__bridge CFDictionaryRef)attributes.get()));
}

void WebProcessPool::registerAssetFonts(WebProcessProxy& process)
{
    if (m_assetFontURLs) {
        process.send(Messages::WebProcess::RegisterAdditionalFonts(AdditionalFonts::additionalFonts({ *m_assetFontURLs }, process.auditToken())), 0);
        return;
    }

    Vector<ASCIILiteral> assetFonts = { "Canela Text"_s, "Proxima Nova"_s, "Publico Text"_s };

    RetainPtr<NSMutableArray> descriptions = [NSMutableArray array];
    for (auto& fontName : assetFonts)
        [descriptions addObject:(__bridge id)fontDescription(fontName).get()];

    auto blockPtr = makeBlockPtr([assetFonts = WTFMove(assetFonts), weakProcess = WeakPtr { process }, weakThis = WeakPtr { *this }](CTFontDescriptorMatchingState state, CFDictionaryRef progressParameter) mutable {
        if (state != kCTFontDescriptorMatchingDidFinish)
            return true;
        RELEASE_LOG(Process, "Font matching finished, progress parameter = %@", (__bridge id)progressParameter);
        RunLoop::mainSingleton().dispatch([assetFonts = WTFMove(assetFonts), weakProcess = WTFMove(weakProcess), weakThis = WTFMove(weakThis)] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (!protectedThis->m_assetFontURLs) {
                protectedThis->m_assetFontURLs = Vector<URL> { };
                for (auto& fontName : assetFonts) {
                    URL fontURL = fontURLFromName(fontName);
                    RELEASE_LOG(Process, "Registering font name %s with url %s", fontName.characters(), fontURL.string().utf8().data());
                    protectedThis->m_assetFontURLs->append(WTFMove(fontURL));
                }
            }
            if (weakProcess)
                weakProcess->send(Messages::WebProcess::RegisterAdditionalFonts(AdditionalFonts::additionalFonts({ *protectedThis->m_assetFontURLs }, weakProcess->auditToken())), 0);
        });
        return true;
    });

    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [descriptions = RetainPtr<NSArray>(descriptions), blockPtr] {
        CTFontDescriptorMatchFontDescriptorsWithProgressHandler((__bridge CFArrayRef)descriptions.get(), nullptr, blockPtr.get());
    });
}

} // namespace WebKit
