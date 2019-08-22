/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#import "AccessibilitySupportSPI.h"
#import "CookieStorageUtilsCF.h"
#import "LegacyCustomProtocolManagerClient.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkProcessMessages.h"
#import "NetworkProcessProxy.h"
#import "PluginProcessManager.h"
#import "SandboxUtilities.h"
#import "TextChecker.h"
#import "VersionChecks.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebMemoryPressureHandler.h"
#import "WebPageGroup.h"
#import "WebPreferencesKeys.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessMessages.h"
#import "WindowServerConnection.h"
#import <WebCore/Color.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <sys/param.h>
#import <wtf/FileSystem.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/dyldSPI.h>

#if PLATFORM(MAC)
#import <QuartzCore/CARemoteLayerServer.h>
#else
#import "UIKitSPI.h"
#endif

NSString *WebServiceWorkerRegistrationDirectoryDefaultsKey = @"WebServiceWorkerRegistrationDirectory";
NSString *WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
NSString *WebKitJSCJITEnabledDefaultsKey = @"WebKitJSCJITEnabledDefaultsKey";
NSString *WebKitJSCFTLJITEnabledDefaultsKey = @"WebKitJSCFTLJITEnabledDefaultsKey";

#if !PLATFORM(IOS_FAMILY)
static NSString *WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification = @"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification";
#endif

static NSString * const WebKitSuppressMemoryPressureHandlerDefaultsKey = @"WebKitSuppressMemoryPressureHandler";

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
#endif

#if PLATFORM(IOS)
SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices, BKSDisplayBrightnessGetCurrent, float, (), ());
#endif

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

void WebProcessPool::updateProcessSuppressionState()
{
    if (m_networkProcess)
        m_networkProcess->setProcessSuppressionEnabled(processSuppressionEnabled());

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (!m_processSuppressionDisabledForPageCounter.value())
        m_pluginProcessManagerProcessSuppressionDisabledToken = nullptr;
    else if (!m_pluginProcessManagerProcessSuppressionDisabledToken)
        m_pluginProcessManagerProcessSuppressionDisabledToken = PluginProcessManager::singleton().processSuppressionDisabledToken();
#endif
}

NSMutableDictionary *WebProcessPool::ensureBundleParameters()
{
    if (!m_bundleParameters)
        m_bundleParameters = adoptNS([[NSMutableDictionary alloc] init]);

    return m_bundleParameters.get();
}

void WebProcessPool::platformInitialize()
{
    registerUserDefaultsIfNeeded();
    registerNotificationObservers();

    // FIXME: This should be able to share code with WebCore's MemoryPressureHandler (and be platform independent).
    // Right now it cannot because WebKit1 and WebKit2 need to be able to coexist in the UI process,
    // and you can only have one WebCore::MemoryPressureHandler.

    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitSuppressMemoryPressureHandler"])
        installMemoryPressureHandler();

    setLegacyCustomProtocolManagerClient(makeUnique<LegacyCustomProtocolManagerClient>());
}

#if PLATFORM(IOS_FAMILY)
String WebProcessPool::cookieStorageDirectory() const
{
    String path = pathForProcessContainer();
    if (path.isEmpty())
        path = NSHomeDirectory();

    path = path + "/Library/Cookies";
    path = stringByResolvingSymlinksInPath(path);
    return path;
}
#endif

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
    m_resolvedPaths.uiProcessBundleResourcePath = resolvePathForSandboxExtension([[NSBundle mainBundle] resourcePath]);

#if PLATFORM(IOS_FAMILY)
    m_resolvedPaths.cookieStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(cookieStorageDirectory());
    m_resolvedPaths.containerCachesDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(webContentCachesDirectory());
    m_resolvedPaths.containerTemporaryDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(containerTemporaryDirectory());
#endif
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
#if !PLATFORM(IOS_FAMILY)
    parameters.acceleratedCompositingPort = MachSendRight::create([CARemoteLayerServer sharedServer].serverPort);
#endif
#endif

    // FIXME: This should really be configurable; we shouldn't just blindly allow read access to the UI process bundle.
    parameters.uiProcessBundleResourcePath = m_resolvedPaths.uiProcessBundleResourcePath;
    SandboxExtension::createHandleWithoutResolvingPath(parameters.uiProcessBundleResourcePath, SandboxExtension::Type::ReadOnly, parameters.uiProcessBundleResourcePathExtensionHandle);

    parameters.uiProcessBundleIdentifier = String([[NSBundle mainBundle] bundleIdentifier]);
    parameters.uiProcessSDKVersion = dyld_get_program_sdk_version();

#if PLATFORM(IOS_FAMILY)
    if (!m_resolvedPaths.cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.cookieStorageDirectory, SandboxExtension::Type::ReadWrite, parameters.cookieStorageDirectoryExtensionHandle);

    if (!m_resolvedPaths.containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.containerCachesDirectory, SandboxExtension::Type::ReadWrite, parameters.containerCachesDirectoryExtensionHandle);

    if (!m_resolvedPaths.containerTemporaryDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.containerTemporaryDirectory, SandboxExtension::Type::ReadWrite, parameters.containerTemporaryDirectoryExtensionHandle);
#endif

    parameters.fontWhitelist = m_fontWhitelist;

    if (m_bundleParameters) {
        auto keyedArchiver = secureArchiver();

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
    bool webRTCEnabled = m_defaultPageGroup->preferences().peerConnectionEnabled();
    if ([defaults objectForKey:@"ExperimentalPeerConnectionEnabled"])
        webRTCEnabled = [defaults boolForKey:@"ExperimentalPeerConnectionEnabled"];

    bool isSafari = false;
#if PLATFORM(IOS_FAMILY)
    if (WebCore::IOSApplication::isMobileSafari())
        isSafari = true;
#elif PLATFORM(MAC)
    if (WebCore::MacApplication::isSafari())
        isSafari = true;
#endif

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    parameters.webCoreLoggingChannels = [[NSUserDefaults standardUserDefaults] stringForKey:@"WebCoreLogging"];
    parameters.webKitLoggingChannels = [[NSUserDefaults standardUserDefaults] stringForKey:@"WebKit2Logging"];
#endif

    // FIXME: Remove this and related parameter when <rdar://problem/29448368> is fixed.
    if (isSafari && !parameters.shouldCaptureAudioInUIProcess && mediaDevicesEnabled)
        SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone", parameters.audioCaptureExtensionHandle);
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    parameters.shouldLogUserInteraction = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif
    
#if PLATFORM(MAC)
    auto screenProperties = WebCore::collectScreenProperties();
    parameters.screenProperties = WTFMove(screenProperties);
    parameters.useOverlayScrollbars = ([NSScroller preferredScrollerStyle] == NSScrollerStyleOverlay);
#endif
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters& parameters)
{
    parameters.uiProcessBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
    parameters.uiProcessSDKVersion = dyld_get_program_sdk_version();

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    {
        bool isSafari = false;
#if PLATFORM(IOS_FAMILY)
        isSafari = WebCore::IOSApplication::isMobileSafari();
#elif PLATFORM(MAC)
        isSafari = WebCore::MacApplication::isSafari();
#endif
        if (isSafari) {
            parameters.defaultDataStoreParameters.networkSessionParameters.httpProxy = URL(URL(), [defaults stringForKey:(NSString *)WebKit2HTTPProxyDefaultsKey]);
            parameters.defaultDataStoreParameters.networkSessionParameters.httpsProxy = URL(URL(), [defaults stringForKey:(NSString *)WebKit2HTTPSProxyDefaultsKey]);
        }
    }

    parameters.defaultDataStoreParameters.networkSessionParameters.enableLegacyTLS = [defaults boolForKey:@"WebKitEnableLegacyTLS"];

    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

#if PLATFORM(IOS_FAMILY)
    parameters.ctDataConnectionServiceType = m_configuration->ctDataConnectionServiceType();
#endif

    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

#if PLATFORM(MAC)
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    parameters.uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage([[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]);
#endif

    parameters.storageAccessAPIEnabled = storageAccessAPIEnabled();
    parameters.suppressesConnectionTerminationOnSystemChange = m_configuration->suppressesConnectionTerminationOnSystemChange();

    parameters.shouldEnableITPDatabase = [defaults boolForKey:[NSString stringWithFormat:@"InternalDebug%@", WebPreferencesKey::isITPDatabaseEnabledKey().createCFString().get()]];
    parameters.downloadMonitorSpeedMultiplier = m_configuration->downloadMonitorSpeedMultiplier();

    parameters.enableAdClickAttributionDebugMode = [defaults boolForKey:[NSString stringWithFormat:@"Experimental%@", WebPreferencesKey::adClickAttributionDebugModeEnabledKey().createCFString().get()]];
}

void WebProcessPool::platformInvalidateContext()
{
    unregisterNotificationObservers();
}

#if PLATFORM(IOS_FAMILY)
String WebProcessPool::parentBundleDirectory() const
{
    return [[[NSBundle mainBundle] bundlePath] stringByStandardizingPath];
}

String WebProcessPool::networkingCachesDirectory() const
{
    String path = pathForProcessContainer();
    if (path.isEmpty())
        path = NSHomeDirectory();

    path = path + "/Library/Caches/com.apple.WebKit.Networking/";
    path = stringByResolvingSymlinksInPath(path);

    NSError *error = nil;
    NSString* nsPath = path;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:nsPath withIntermediateDirectories:YES attributes:nil error:&error]) {
        NSLog(@"could not create networking caches directory \"%@\", error %@", nsPath, error);
        return String();
    }

    return path;
}

String WebProcessPool::webContentCachesDirectory() const
{
    String path = pathForProcessContainer();
    if (path.isEmpty())
        path = NSHomeDirectory();

    path = path + "/Library/Caches/com.apple.WebKit.WebContent/";
    path = stringByResolvingSymlinksInPath(path);

    NSError *error = nil;
    NSString* nsPath = path;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:nsPath withIntermediateDirectories:YES attributes:nil error:&error]) {
        NSLog(@"could not create web content caches directory \"%@\", error %@", nsPath, error);
        return String();
    }

    return path;
}

String WebProcessPool::containerTemporaryDirectory() const
{
    String path = NSTemporaryDirectory();
    return stringByResolvingSymlinksInPath(path);
}
#endif

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

bool WebProcessPool::networkProcessHasEntitlementForTesting(const String& entitlement)
{
    return WTF::hasEntitlement(ensureNetworkProcess().connection()->xpcConnection(), entitlement.utf8().data());
}

#if PLATFORM(IOS)
float WebProcessPool::displayBrightness()
{
    return BKSDisplayBrightnessGetCurrent();
}
    
void WebProcessPool::backlightLevelDidChangeCallback(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    WebProcessPool* pool = reinterpret_cast<WebProcessPool*>(observer);
    pool->sendToAllProcesses(Messages::WebProcess::BacklightLevelDidChange(BKSDisplayBrightnessGetCurrent()));
}
#endif

void WebProcessPool::registerNotificationObservers()
{
#if !PLATFORM(IOS_FAMILY)
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

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    m_scrollerStyleNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSPreferredScrollerStyleDidChangeNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        auto scrollbarStyle = [NSScroller preferredScrollerStyle];
        sendToAllProcesses(Messages::WebProcess::ScrollerStylePreferenceChanged(scrollbarStyle));
    }];
#endif

    m_activationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidBecomeActiveNotification object:NSApp queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        setApplicationIsActive(true);
    }];

    m_deactivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidResignActiveNotification object:NSApp queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *notification) {
        setApplicationIsActive(false);
    }];
#elif PLATFORM(IOS)
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), this, backlightLevelDidChangeCallback, static_cast<CFStringRef>(UIBacklightLevelChangedNotification), nullptr, CFNotificationSuspensionBehaviorCoalesce);
    m_accessibilityEnabledObserver = [[NSNotificationCenter defaultCenter] addObserverForName:(__bridge id)kAXSApplicationAccessibilityEnabledNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *) {
        for (size_t i = 0; i < m_processes.size(); ++i)
            m_processes[i]->unblockAccessibilityServerIfNeeded();
    }];
#endif // !PLATFORM(IOS_FAMILY)
}

void WebProcessPool::unregisterNotificationObservers()
{
#if !PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:m_enhancedAccessibilityObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticTextReplacementNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticSpellingCorrectionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticQuoteSubstitutionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticDashSubstitutionNotificationObserver.get()];
    [[NSWorkspace.sharedWorkspace notificationCenter] removeObserver:m_accessibilityDisplayOptionsNotificationObserver.get()];
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    [[NSNotificationCenter defaultCenter] removeObserver:m_scrollerStyleNotificationObserver.get()];
#endif
    [[NSNotificationCenter defaultCenter] removeObserver:m_activationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_deactivationObserver.get()];
#elif PLATFORM(IOS)
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(), this, static_cast<CFStringRef>(UIBacklightLevelChangedNotification) , nullptr);
    [[NSNotificationCenter defaultCenter] removeObserver:m_accessibilityEnabledObserver.get()];
#endif // !PLATFORM(IOS_FAMILY)
}

static CFURLStorageSessionRef privateBrowsingSession()
{
    static CFURLStorageSessionRef session;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *identifier = [NSString stringWithFormat:@"%@.PrivateBrowsing", [[NSBundle mainBundle] bundleIdentifier]];
        session = createPrivateStorageSession((__bridge CFStringRef)identifier);
    });

    return session;
}

bool WebProcessPool::isURLKnownHSTSHost(const String& urlString, bool privateBrowsingEnabled) const
{
    RetainPtr<CFURLRef> url = URL(URL(), urlString).createCFURL();

    return _CFNetworkIsKnownHSTSHostWithSession(url.get(), privateBrowsingEnabled ? privateBrowsingSession() : nullptr);
}

void WebProcessPool::resetHSTSHosts()
{
    _CFNetworkResetHSTSHostsWithSession(nullptr);
    _CFNetworkResetHSTSHostsWithSession(privateBrowsingSession());
}

void WebProcessPool::resetHSTSHostsAddedAfterDate(double startDateIntervalSince1970)
{
    NSDate *startDate = [NSDate dateWithTimeIntervalSince1970:startDateIntervalSince1970];
    _CFNetworkResetHSTSHostsSinceDate(nullptr, (__bridge CFDateRef)startDate);
    _CFNetworkResetHSTSHostsSinceDate(privateBrowsingSession(), (__bridge CFDateRef)startDate);
}

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
void WebProcessPool::startDisplayLink(IPC::Connection& connection, unsigned observerID, uint32_t displayID)
{
    for (auto& displayLink : m_displayLinks) {
        if (displayLink->displayID() == displayID) {
            displayLink->addObserver(connection, observerID);
            return;
        }
    }
    auto displayLink = makeUnique<DisplayLink>(displayID);
    displayLink->addObserver(connection, observerID);
    m_displayLinks.append(WTFMove(displayLink));
}

void WebProcessPool::stopDisplayLink(IPC::Connection& connection, unsigned observerID, uint32_t displayID)
{
    for (auto& displayLink : m_displayLinks) {
        if (displayLink->displayID() == displayID) {
            displayLink->removeObserver(connection, observerID);
            return;
        }
    }
}

void WebProcessPool::stopDisplayLinks(IPC::Connection& connection)
{
    for (auto& displayLink : m_displayLinks)
        displayLink->removeObservers(connection);
}
#endif

// FIXME: Deprecated. Left here until a final decision is made.
void WebProcessPool::setCookieStoragePartitioningEnabled(bool enabled)
{
    m_cookieStoragePartitioningEnabled = enabled;
}

void WebProcessPool::setStorageAccessAPIEnabled(bool enabled)
{
    m_storageAccessAPIEnabled = enabled;
    sendToNetworkingProcess(Messages::NetworkProcess::SetStorageAccessAPIEnabled(enabled));
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

} // namespace WebKit
