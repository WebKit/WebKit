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
#import "WebProcessPool.h"

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
#import "WebPageGroup.h"
#import "WebPreferencesKeys.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessMessages.h"
#import "WindowServerConnection.h"
#import <WebCore/Color.h>
#import <WebCore/FileSystem.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <sys/param.h>

#if PLATFORM(IOS)
#import "ArgumentCodersCF.h"
#import "WebMemoryPressureHandlerIOS.h"
#else
#import <QuartzCore/CARemoteLayerServer.h>
#endif

using namespace WebCore;

NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";
NSString *WebServiceWorkerRegistrationDirectoryDefaultsKey = @"WebServiceWorkerRegistrationDirectory";
NSString *WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
NSString *WebStorageDirectoryDefaultsKey = @"WebKitLocalStorageDatabasePathPreferenceKey";
NSString *WebKitJSCJITEnabledDefaultsKey = @"WebKitJSCJITEnabledDefaultsKey";
NSString *WebKitJSCFTLJITEnabledDefaultsKey = @"WebKitJSCFTLJITEnabledDefaultsKey";
NSString *WebKitMediaKeysStorageDirectoryDefaultsKey = @"WebKitMediaKeysStorageDirectory";
NSString *WebKitMediaCacheDirectoryDefaultsKey = @"WebKitMediaCacheDirectory";

#if !PLATFORM(IOS)
static NSString *WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification = @"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification";
#endif

// FIXME: <rdar://problem/9138817> - After this "backwards compatibility" radar is removed, this code should be removed to only return an empty String.
NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";

static NSString * const WebKit2HTTPProxyDefaultsKey = @"WebKit2HTTPProxy";
static NSString * const WebKit2HTTPSProxyDefaultsKey = @"WebKit2HTTPSProxy";

static NSString * const WebKitNetworkCacheEnabledDefaultsKey = @"WebKitNetworkCacheEnabled";
static NSString * const WebKitNetworkCacheEfficacyLoggingEnabledDefaultsKey = @"WebKitNetworkCacheEfficacyLoggingEnabled";

static NSString * const WebKitSuppressMemoryPressureHandlerDefaultsKey = @"WebKitSuppressMemoryPressureHandler";
static NSString * const WebKitNetworkLoadThrottleLatencyMillisecondsDefaultsKey = @"WebKitNetworkLoadThrottleLatencyMilliseconds";

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
#endif

#if ENABLE(NETWORK_CAPTURE)
static NSString * const WebKitRecordReplayModeDefaultsKey = @"WebKitRecordReplayMode";
static NSString * const WebKitRecordReplayCacheLocationDefaultsKey = @"WebKitRecordReplayCacheLocation";
#endif

namespace WebKit {

NSString *SchemeForCustomProtocolRegisteredNotificationName = @"WebKitSchemeForCustomProtocolRegisteredNotification";
NSString *SchemeForCustomProtocolUnregisteredNotificationName = @"WebKitSchemeForCustomProtocolUnregisteredNotification";

static void registerUserDefaultsIfNeeded()
{
    static bool didRegister;
    if (didRegister)
        return;

    didRegister = true;
    NSMutableDictionary *registrationDictionary = [NSMutableDictionary dictionary];
    
    [registrationDictionary setObject:@YES forKey:WebKitJSCJITEnabledDefaultsKey];
    [registrationDictionary setObject:@YES forKey:WebKitJSCFTLJITEnabledDefaultsKey];

    [registrationDictionary setObject:@YES forKey:WebKitNetworkCacheEnabledDefaultsKey];
    [registrationDictionary setObject:@NO forKey:WebKitNetworkCacheEfficacyLoggingEnabledDefaultsKey];

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

#if PLATFORM(IOS)
    IPC::setAllowsDecodingSecKeyRef(true);
    installMemoryPressureHandler();
#endif

    setLegacyCustomProtocolManagerClient(std::make_unique<LegacyCustomProtocolManagerClient>());
}

#if PLATFORM(IOS)
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

#if PLATFORM(IOS)
    m_resolvedPaths.cookieStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(cookieStorageDirectory());
    m_resolvedPaths.containerCachesDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(webContentCachesDirectory());
    m_resolvedPaths.containerTemporaryDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(containerTemporaryDirectory());
#endif
}

void WebProcessPool::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
#if PLATFORM(MAC)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    parameters.accessibilityEnhancedUserInterfaceEnabled = [[NSApp accessibilityAttributeValue:@"AXEnhancedUserInterface"] boolValue];
#pragma clang diagnostic pop
#else
    parameters.accessibilityEnhancedUserInterfaceEnabled = false;
#endif

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    parameters.shouldEnableJIT = [defaults boolForKey:WebKitJSCJITEnabledDefaultsKey];
    parameters.shouldEnableFTLJIT = [defaults boolForKey:WebKitJSCFTLJITEnabledDefaultsKey];
    parameters.shouldEnableMemoryPressureReliefLogging = [defaults boolForKey:@"LogMemoryJetsamDetails"];
    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

#if HAVE(HOSTED_CORE_ANIMATION)
#if !PLATFORM(IOS)
    parameters.acceleratedCompositingPort = MachSendRight::create([CARemoteLayerServer sharedServer].serverPort);
#endif
#endif

    // FIXME: This should really be configurable; we shouldn't just blindly allow read access to the UI process bundle.
    parameters.uiProcessBundleResourcePath = m_resolvedPaths.uiProcessBundleResourcePath;
    SandboxExtension::createHandleWithoutResolvingPath(parameters.uiProcessBundleResourcePath, SandboxExtension::Type::ReadOnly, parameters.uiProcessBundleResourcePathExtensionHandle);

    parameters.uiProcessBundleIdentifier = String([[NSBundle mainBundle] bundleIdentifier]);

#if PLATFORM(IOS)
    if (!m_resolvedPaths.cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.cookieStorageDirectory, SandboxExtension::Type::ReadWrite, parameters.cookieStorageDirectoryExtensionHandle);

    if (!m_resolvedPaths.containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.containerCachesDirectory, SandboxExtension::Type::ReadWrite, parameters.containerCachesDirectoryExtensionHandle);

    if (!m_resolvedPaths.containerTemporaryDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.containerTemporaryDirectory, SandboxExtension::Type::ReadWrite, parameters.containerTemporaryDirectoryExtensionHandle);
#endif

    parameters.fontWhitelist = m_fontWhitelist;

    if (m_bundleParameters) {
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
        auto data = adoptNS([[NSMutableData alloc] init]);
        auto keyedArchiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);

        [keyedArchiver setRequiresSecureCoding:YES];
#else
        auto keyedArchiver = secureArchiver();
#endif

        @try {
            [keyedArchiver encodeObject:m_bundleParameters.get() forKey:@"parameters"];
            [keyedArchiver finishEncoding];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to encode bundle parameters: %@", exception);
        }

#if (!PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200)
        auto data = retainPtr(keyedArchiver.get().encodedData);
#endif

        parameters.bundleParameterData = API::Data::createWithoutCopying((const unsigned char*)[data bytes], [data length], [] (unsigned char*, const void* data) {
            [(NSData *)data release];
        }, data.leakRef());
    }
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

#if PLATFORM(MAC)
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    parameters.uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage([[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]);
#endif
#if ENABLE(MEDIA_STREAM)
    // Allow microphone access if either preference is set because WebRTC requires microphone access.
    bool mediaDevicesEnabled = m_defaultPageGroup->preferences().mediaDevicesEnabled();
    bool webRTCEnabled = m_defaultPageGroup->preferences().peerConnectionEnabled();
    if ([defaults objectForKey:@"ExperimentalPeerConnectionEnabled"])
        webRTCEnabled = [defaults boolForKey:@"ExperimentalPeerConnectionEnabled"];

    bool isSafari = false;
#if PLATFORM(IOS)
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
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters& parameters)
{
    NSURLCache *urlCache = [NSURLCache sharedURLCache];
    parameters.nsURLCacheMemoryCapacity = [urlCache memoryCapacity];
    parameters.nsURLCacheDiskCapacity = [urlCache diskCapacity];

    parameters.parentProcessName = [[NSProcessInfo processInfo] processName];
    parameters.uiProcessBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    parameters.httpProxy = [defaults stringForKey:WebKit2HTTPProxyDefaultsKey];
    parameters.httpsProxy = [defaults stringForKey:WebKit2HTTPSProxyDefaultsKey];
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());

    parameters.shouldEnableNetworkCache = isNetworkCacheEnabled();
    parameters.shouldEnableNetworkCacheEfficacyLogging = [defaults boolForKey:WebKitNetworkCacheEfficacyLoggingEnabledDefaultsKey];

    parameters.sourceApplicationBundleIdentifier = m_configuration->sourceApplicationBundleIdentifier();
    parameters.sourceApplicationSecondaryIdentifier = m_configuration->sourceApplicationSecondaryIdentifier();
#if PLATFORM(IOS)
    parameters.ctDataConnectionServiceType = m_configuration->ctDataConnectionServiceType();
#endif

    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];
    parameters.loadThrottleLatency = Seconds { [defaults integerForKey:WebKitNetworkLoadThrottleLatencyMillisecondsDefaultsKey] / 1000. };

#if PLATFORM(MAC)
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    parameters.uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage([[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]);
#endif

    parameters.cookieStoragePartitioningEnabled = cookieStoragePartitioningEnabled();
    parameters.storageAccessAPIEnabled = storageAccessAPIEnabled();

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    parameters.logCookieInformation = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif

#if ENABLE(NETWORK_CAPTURE)
    parameters.recordReplayMode = [defaults stringForKey:WebKitRecordReplayModeDefaultsKey];
    parameters.recordReplayCacheLocation = [defaults stringForKey:WebKitRecordReplayCacheLocationDefaultsKey];
    if (parameters.recordReplayCacheLocation.isEmpty())
        parameters.recordReplayCacheLocation = parameters.diskCacheDirectory;
#endif
}

void WebProcessPool::platformInvalidateContext()
{
    unregisterNotificationObservers();
}

#if PLATFORM(IOS)
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

String WebProcessPool::legacyPlatformDefaultWebSQLDatabaseDirectory()
{
    registerUserDefaultsIfNeeded();

    NSString *databasesDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory || ![databasesDirectory isKindOfClass:[NSString class]])
        databasesDirectory = @"~/Library/WebKit/Databases";
    return stringByResolvingSymlinksInPath([databasesDirectory stringByStandardizingPath]);
}

String WebProcessPool::legacyPlatformDefaultIndexedDBDatabaseDirectory()
{
    // Indexed databases exist in a subdirectory of the "database directory path."
    // Currently, the top level of that directory contains entities related to WebSQL databases.
    // We should fix this, and move WebSQL into a subdirectory (https://bugs.webkit.org/show_bug.cgi?id=124807)
    // In the meantime, an entity name prefixed with three underscores will not conflict with any WebSQL entities.
    return FileSystem::pathByAppendingComponent(legacyPlatformDefaultWebSQLDatabaseDirectory(), "___IndexedDB");
}

String WebProcessPool::legacyPlatformDefaultLocalStorageDirectory()
{
    registerUserDefaultsIfNeeded();

    NSString *localStorageDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebStorageDirectoryDefaultsKey];
    if (!localStorageDirectory || ![localStorageDirectory isKindOfClass:[NSString class]])
        localStorageDirectory = @"~/Library/WebKit/LocalStorage";
    return stringByResolvingSymlinksInPath([localStorageDirectory stringByStandardizingPath]);
}

String WebProcessPool::legacyPlatformDefaultMediaCacheDirectory()
{
    registerUserDefaultsIfNeeded();
    
    NSString *mediaKeysCacheDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebKitMediaCacheDirectoryDefaultsKey];
    if (!mediaKeysCacheDirectory || ![mediaKeysCacheDirectory isKindOfClass:[NSString class]]) {
        mediaKeysCacheDirectory = NSTemporaryDirectory();
        
        if (!WebKit::processHasContainer()) {
            NSString *bundleIdentifier = [NSBundle mainBundle].bundleIdentifier;
            if (!bundleIdentifier)
                bundleIdentifier = [NSProcessInfo processInfo].processName;
            mediaKeysCacheDirectory = [mediaKeysCacheDirectory stringByAppendingPathComponent:bundleIdentifier];
        }
        mediaKeysCacheDirectory = [mediaKeysCacheDirectory stringByAppendingPathComponent:@"WebKit/MediaCache"];
    }
    return stringByResolvingSymlinksInPath([mediaKeysCacheDirectory stringByStandardizingPath]);
}

String WebProcessPool::legacyPlatformDefaultMediaKeysStorageDirectory()
{
    registerUserDefaultsIfNeeded();

    NSString *mediaKeysStorageDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebKitMediaKeysStorageDirectoryDefaultsKey];
    if (!mediaKeysStorageDirectory || ![mediaKeysStorageDirectory isKindOfClass:[NSString class]])
        mediaKeysStorageDirectory = @"~/Library/WebKit/MediaKeys";
    return stringByResolvingSymlinksInPath([mediaKeysStorageDirectory stringByStandardizingPath]);
}

String WebProcessPool::legacyPlatformDefaultApplicationCacheDirectory()
{
    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (!appName)
        appName = [[NSProcessInfo processInfo] processName];
#if PLATFORM(IOS)
    // This quirk used to make these apps share application cache storage, but doesn't accomplish that any more.
    // Preserving it avoids the need to migrate data when upgrading.
    if (IOSApplication::isMobileSafari() || IOSApplication::isWebApp())
        appName = @"com.apple.WebAppCache";
#endif

    ASSERT(appName);

#if PLATFORM(IOS)
    NSString *cacheDir = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches"];
#else
    char cacheDirectory[MAXPATHLEN];
    size_t cacheDirectoryLen = confstr(_CS_DARWIN_USER_CACHE_DIR, cacheDirectory, MAXPATHLEN);
    if (!cacheDirectoryLen)
        return String();

    NSString *cacheDir = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:cacheDirectory length:cacheDirectoryLen - 1];
#endif
    NSString* cachePath = [cacheDir stringByAppendingPathComponent:appName];
    return stringByResolvingSymlinksInPath([cachePath stringByStandardizingPath]);
}

String WebProcessPool::legacyPlatformDefaultNetworkCacheDirectory()
{
    RetainPtr<NSString> cachePath = adoptNS((NSString *)_CFURLCacheCopyCacheDirectory([[NSURLCache sharedURLCache] _CFURLCache]));
    if (!cachePath)
        cachePath = @"~/Library/Caches/com.apple.WebKit.WebProcess";

    if (isNetworkCacheEnabled())
        cachePath = [cachePath stringByAppendingPathComponent:@"WebKitCache"];

    return stringByResolvingSymlinksInPath([cachePath stringByStandardizingPath]);
}

String WebProcessPool::legacyPlatformDefaultJavaScriptConfigurationDirectory()
{
#if PLATFORM(IOS)
    String path = pathForProcessContainer();
    if (path.isEmpty())
        path = NSHomeDirectory();
    
    path = path + "/Library/WebKit/JavaScriptCoreDebug";
    path = stringByResolvingSymlinksInPath(path);

    return path;
#else
    RetainPtr<NSString> javaScriptConfigPath = @"~/Library/WebKit/JavaScriptCoreDebug";
    
    return stringByResolvingSymlinksInPath([javaScriptConfigPath stringByStandardizingPath]);
#endif
}

#if PLATFORM(IOS)
void WebProcessPool::setJavaScriptConfigurationFileEnabledFromDefaults()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    setJavaScriptConfigurationFileEnabled([defaults boolForKey:@"WebKitJavaScriptCoreUseConfigFile"]);
}
#endif

bool WebProcessPool::isNetworkCacheEnabled()
{
    registerUserDefaultsIfNeeded();

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    bool networkCacheEnabledByDefaults = [defaults boolForKey:WebKitNetworkCacheEnabledDefaultsKey];

    return networkCacheEnabledByDefaults && linkedOnOrAfter(SDKVersion::FirstWithNetworkCache);
}

bool WebProcessPool::omitPDFSupport()
{
    // Since this is a "secret default" we don't bother registering it.
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
}

bool WebProcessPool::processSuppressionEnabled() const
{
    return !m_userObservablePageCounter.value() && !m_processSuppressionDisabledForPageCounter.value();
}

void WebProcessPool::registerNotificationObservers()
{
#if !PLATFORM(IOS)
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
#endif // !PLATFORM(IOS)
}

void WebProcessPool::unregisterNotificationObservers()
{
#if !PLATFORM(IOS)
    [[NSNotificationCenter defaultCenter] removeObserver:m_enhancedAccessibilityObserver.get()];    
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticTextReplacementNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticSpellingCorrectionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticQuoteSubstitutionNotificationObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_automaticDashSubstitutionNotificationObserver.get()];
#endif // !PLATFORM(IOS)
}

static CFURLStorageSessionRef privateBrowsingSession()
{
    static CFURLStorageSessionRef session;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *identifier = [NSString stringWithFormat:@"%@.PrivateBrowsing", [[NSBundle mainBundle] bundleIdentifier]];

        session = createPrivateStorageSession((CFStringRef)identifier);
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

void WebProcessPool::setCookieStoragePartitioningEnabled(bool enabled)
{
    m_cookieStoragePartitioningEnabled = enabled;
    sendToNetworkingProcess(Messages::NetworkProcess::SetCookieStoragePartitioningEnabled(enabled));
}

void WebProcessPool::setStorageAccessAPIEnabled(bool enabled)
{
    m_storageAccessAPIEnabled = enabled;
    sendToNetworkingProcess(Messages::NetworkProcess::SetStorageAccessAPIEnabled(enabled));
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
