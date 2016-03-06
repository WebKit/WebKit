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
#import "WebProcessPool.h"

#import "NetworkProcessCreationParameters.h"
#import "NetworkProcessProxy.h"
#import "PluginProcessManager.h"
#import "SandboxUtilities.h"
#import "TextChecker.h"
#import "VersionChecks.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebKitSystemInterface.h"
#import "WebPageGroup.h"
#import "WebPreferencesKeys.h"
#import "WebProcessCreationParameters.h"
#import "WebProcessMessages.h"
#import "WindowServerConnection.h"
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/Color.h>
#import <WebCore/FileSystem.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <sys/param.h>

#if PLATFORM(IOS)
#import "ArgumentCodersCF.h"
#import "WebMemoryPressureHandlerIOS.h"
#else
#import <QuartzCore/CARemoteLayerServer.h>
#endif

using namespace WebCore;

NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";
NSString *WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
NSString *WebStorageDirectoryDefaultsKey = @"WebKitLocalStorageDatabasePathPreferenceKey";
NSString *WebKitJSCJITEnabledDefaultsKey = @"WebKitJSCJITEnabledDefaultsKey";
NSString *WebKitJSCFTLJITEnabledDefaultsKey = @"WebKitJSCFTLJITEnabledDefaultsKey";
NSString *WebKitMediaKeysStorageDirectoryDefaultsKey = @"WebKitMediaKeysStorageDirectory";

#if !PLATFORM(IOS)
static NSString *WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification = @"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification";
#endif

// FIXME: <rdar://problem/9138817> - After this "backwards compatibility" radar is removed, this code should be removed to only return an empty String.
NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";

static NSString * const WebKit2HTTPProxyDefaultsKey = @"WebKit2HTTPProxy";
static NSString * const WebKit2HTTPSProxyDefaultsKey = @"WebKit2HTTPSProxy";

#if ENABLE(NETWORK_CACHE)
static NSString * const WebKitNetworkCacheEnabledDefaultsKey = @"WebKitNetworkCacheEnabled";
static NSString * const WebKitNetworkCacheEfficacyLoggingEnabledDefaultsKey = @"WebKitNetworkCacheEfficacyLoggingEnabled";
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
static NSString * const WebKitNetworkCacheSpeculativeRevalidationEnabledDefaultsKey = @"WebKitNetworkCacheResourceRevalidationEnabled";
#endif
#endif

static NSString * const WebKitSuppressMemoryPressureHandlerDefaultsKey = @"WebKitSuppressMemoryPressureHandler";

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
    
    [registrationDictionary setObject:[NSNumber numberWithBool:YES] forKey:WebKitJSCJITEnabledDefaultsKey];
    [registrationDictionary setObject:[NSNumber numberWithBool:YES] forKey:WebKitJSCFTLJITEnabledDefaultsKey];

#if ENABLE(NETWORK_CACHE)
    [registrationDictionary setObject:[NSNumber numberWithBool:YES] forKey:WebKitNetworkCacheEnabledDefaultsKey];
    [registrationDictionary setObject:[NSNumber numberWithBool:NO] forKey:WebKitNetworkCacheEfficacyLoggingEnabledDefaultsKey];
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    [registrationDictionary setObject:[NSNumber numberWithBool:YES] forKey:WebKitNetworkCacheSpeculativeRevalidationEnabledDefaultsKey];
#endif
#endif

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
    WebKit::WebMemoryPressureHandler::singleton();
#endif
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

void WebProcessPool::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    parameters.presenterApplicationPid = getpid();

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

#if PLATFORM(MAC)
    parameters.shouldRewriteConstAsVar = MacApplication::isIBooks();
#endif

#if HAVE(HOSTED_CORE_ANIMATION)
#if !PLATFORM(IOS)
    parameters.acceleratedCompositingPort = MachSendRight::create([CARemoteLayerServer sharedServer].serverPort);
#endif
#endif

    // FIXME: This should really be configurable; we shouldn't just blindly allow read access to the UI process bundle.
    parameters.uiProcessBundleResourcePath = [[NSBundle mainBundle] resourcePath];
    SandboxExtension::createHandle(parameters.uiProcessBundleResourcePath, SandboxExtension::ReadOnly, parameters.uiProcessBundleResourcePathExtensionHandle);

    parameters.uiProcessBundleIdentifier = String([[NSBundle mainBundle] bundleIdentifier]);

    parameters.fontWhitelist = m_fontWhitelist;

    if (m_bundleParameters) {
        auto data = adoptNS([[NSMutableData alloc] init]);
        auto keyedArchiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);

        [keyedArchiver setRequiresSecureCoding:YES];

        @try {
            [keyedArchiver encodeObject:m_bundleParameters.get() forKey:@"parameters"];
            [keyedArchiver finishEncoding];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to encode bundle parameters: %@", exception);
        }

        parameters.bundleParameterData = API::Data::createWithoutCopying((const unsigned char*)[data bytes], [data length], [] (unsigned char*, const void* data) {
            [(NSData *)data release];
        }, data.leakRef());
    }
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    RetainPtr<CFDataRef> cookieStorageData = adoptCF(CFHTTPCookieStorageCreateIdentifyingData(kCFAllocatorDefault, [[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]));
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    parameters.uiProcessCookieStorageIdentifier.append(CFDataGetBytePtr(cookieStorageData.get()), CFDataGetLength(cookieStorageData.get()));
#endif
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters& parameters)
{
    NSURLCache *urlCache = [NSURLCache sharedURLCache];
    parameters.nsURLCacheMemoryCapacity = [urlCache memoryCapacity];
    parameters.nsURLCacheDiskCapacity = [urlCache diskCapacity];

    parameters.parentProcessName = [[NSProcessInfo processInfo] processName];
    parameters.uiProcessBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    for (const auto& scheme : globalURLSchemesWithCustomProtocolHandlers())
        parameters.urlSchemesRegisteredForCustomProtocols.append(scheme);

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    parameters.httpProxy = [defaults stringForKey:WebKit2HTTPProxyDefaultsKey];
    parameters.httpsProxy = [defaults stringForKey:WebKit2HTTPSProxyDefaultsKey];
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());
#endif

#if ENABLE(NETWORK_CACHE)
    parameters.shouldEnableNetworkCache = isNetworkCacheEnabled();
    parameters.shouldEnableNetworkCacheEfficacyLogging = [defaults boolForKey:WebKitNetworkCacheEfficacyLoggingEnabledDefaultsKey];
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    parameters.shouldEnableNetworkCacheSpeculativeRevalidation = [defaults boolForKey:WebKitNetworkCacheSpeculativeRevalidationEnabledDefaultsKey];
#endif
#endif

    parameters.shouldSuppressMemoryPressureHandler = [defaults boolForKey:WebKitSuppressMemoryPressureHandlerDefaultsKey];

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    RetainPtr<CFDataRef> cookieStorageData = adoptCF(CFHTTPCookieStorageCreateIdentifyingData(kCFAllocatorDefault, [[NSHTTPCookieStorage sharedHTTPCookieStorage] _cookieStorage]));
    ASSERT(parameters.uiProcessCookieStorageIdentifier.isEmpty());
    parameters.uiProcessCookieStorageIdentifier.append(CFDataGetBytePtr(cookieStorageData.get()), CFDataGetLength(cookieStorageData.get()));
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
        NSLog(@"could not create \"%@\", error %@", nsPath, error);
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
        NSLog(@"could not create \"%@\", error %@", nsPath, error);
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
    return pathByAppendingComponent(legacyPlatformDefaultWebSQLDatabaseDirectory(), "___IndexedDB");
}

String WebProcessPool::legacyPlatformDefaultLocalStorageDirectory()
{
    registerUserDefaultsIfNeeded();

    NSString *localStorageDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebStorageDirectoryDefaultsKey];
    if (!localStorageDirectory || ![localStorageDirectory isKindOfClass:[NSString class]])
        localStorageDirectory = @"~/Library/WebKit/LocalStorage";
    return stringByResolvingSymlinksInPath([localStorageDirectory stringByStandardizingPath]);
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
    RetainPtr<NSString> cachePath = adoptNS((NSString *)WKCopyFoundationCacheDirectory());
    if (!cachePath)
        cachePath = @"~/Library/Caches/com.apple.WebKit.WebProcess";

#if ENABLE(NETWORK_CACHE)
    if (isNetworkCacheEnabled())
        cachePath = [cachePath stringByAppendingPathComponent:@"WebKitCache"];
#endif

    return stringByResolvingSymlinksInPath([cachePath stringByStandardizingPath]);
}

bool WebProcessPool::isNetworkCacheEnabled()
{
#if ENABLE(NETWORK_CACHE)
    registerUserDefaultsIfNeeded();

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    bool networkCacheEnabledByDefaults = [defaults boolForKey:WebKitNetworkCacheEnabledDefaultsKey];

    return networkCacheEnabledByDefaults && linkedOnOrAfter(LibraryVersion::FirstWithNetworkCache);
#else
    return false;
#endif
}

String WebProcessPool::platformDefaultIconDatabasePath() const
{
    // FIXME: <rdar://problem/9138817> - After this "backwards compatibility" radar is removed, this code should be removed to only return an empty String.
    NSString *databasesDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebIconDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory || ![databasesDirectory isKindOfClass:[NSString class]])
        databasesDirectory = @"~/Library/Icons/WebpageIcons.db";
    return stringByResolvingSymlinksInPath([databasesDirectory stringByStandardizingPath]);
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

        session = WKCreatePrivateStorageSession((CFStringRef)identifier);
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
#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    _CFNetworkResetHSTSHostsSinceDate(privateBrowsingSession(), (__bridge CFDateRef)startDate);
#endif
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
