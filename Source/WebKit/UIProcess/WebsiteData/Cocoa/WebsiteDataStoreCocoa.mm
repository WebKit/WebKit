/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#import "WebsiteDataStore.h"

#import "CookieStorageUtilsCF.h"
#import "DefaultWebBrowserChecks.h"
#import "LegacyGlobalSettings.h"
#import "NetworkProcessMessages.h"
#import "NetworkProcessProxy.h"
#import "SandboxUtilities.h"
#import "UnifiedOriginStorageLevel.h"
#import "WebFramePolicyListenerProxy.h"
#import "WebPageProxy.h"
#import "WebPreferencesDefaultValues.h"
#import "WebPreferencesKeys.h"
#import "WebProcessProxy.h"
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataStoreParameters.h"
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <WebCore/SecurityOriginData.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/cf/StringConcatenateCF.h>

#if ENABLE(GPU_PROCESS)
#import "GPUProcessProxy.h"
#endif

#if ENABLE(SCREEN_TIME)
#import <pal/cocoa/ScreenTimeSoftLink.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIApplication.h>
#import <pal/ios/ManagedConfigurationSoftLink.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#endif

namespace WebKit {

static NSString* const WebKit2HTTPProxyDefaultsKey = @"WebKit2HTTPProxy";
static NSString* const WebKit2HTTPSProxyDefaultsKey = @"WebKit2HTTPSProxy";

static constexpr double defaultBrowserTotalQuotaRatio = 0.8;
static constexpr double defaultBrowserOriginQuotaRatio = 0.6;
static constexpr double defaultAppTotalQuotaRatio = 0.2;
static constexpr double defaultAppOriginQuotaRatio = 0.15;

#if ENABLE(APP_BOUND_DOMAINS)
static WorkQueue& appBoundDomainQueue()
{
    static auto& queue = WorkQueue::create("com.apple.WebKit.AppBoundDomains"_s).leakRef();
    return queue;
}
static std::atomic<bool> hasInitializedAppBoundDomains = false;
static std::atomic<bool> keyExists = false;
#endif

#if ENABLE(MANAGED_DOMAINS)
static WorkQueue& managedDomainQueueSingleton()
{
    static MainRunLoopNeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("com.apple.WebKit.ManagedDomains"_s);
    return queue.get();
}
static std::atomic<bool> hasInitializedManagedDomains = false;
static std::atomic<bool> managedKeyExists = false;
#endif

static std::optional<bool> optionalExperimentalFeatureEnabled(const String& key, std::optional<bool> defaultValue = false)
{
    auto defaultsKey = adoptNS([[NSString alloc] initWithFormat:@"WebKitExperimental%@", key.createNSString().get()]);
    if ([[NSUserDefaults standardUserDefaults] objectForKey:defaultsKey.get()] != nil)
        return [[NSUserDefaults standardUserDefaults] boolForKey:defaultsKey.get()];

    return defaultValue;
}

bool experimentalFeatureEnabled(const String& key, bool defaultValue)
{
    return *optionalExperimentalFeatureEnabled(key, defaultValue);
}

static RetainPtr<NSString> applicationOrProcessIdentifier()
{
    RetainPtr<NSString> identifier = [NSBundle mainBundle].bundleIdentifier;
    RetainPtr<NSString> processName = [NSProcessInfo processInfo].processName;
    // SafariForWebKitDevelopment has the same bundle identifier as Safari, but it does not have the privilege to
    // access Safari's paths.
    if ([identifier isEqualToString:@"com.apple.Safari"] && [processName isEqualToString:@"SafariForWebKitDevelopment"])
        identifier = WTFMove(processName);
    else if (!identifier)
        identifier = WTFMove(processName);
    return identifier;
}

WebCore::ThirdPartyCookieBlockingMode WebsiteDataStore::thirdPartyCookieBlockingMode() const
{
    if (!m_thirdPartyCookieBlockingMode) {
        if (experimentalFeatureEnabled(WebPreferencesKey::isThirdPartyCookieBlockingDisabledKey()))
            m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction;
        else
            m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
    }
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && (!defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) || !CFN_COOKIE_ACCEPTS_POLICY_PARTITION)
    RELEASE_ASSERT(m_thirdPartyCookieBlockingMode != WebCore::ThirdPartyCookieBlockingMode::AllExceptPartitioned);
#endif
    return *m_thirdPartyCookieBlockingMode;
}

void WebsiteDataStore::platformSetNetworkParameters(WebsiteDataStoreParameters& parameters)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    bool shouldLogCookieInformation = false;
    auto sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::No;
    auto firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource { };
    if (experimentalFeatureEnabled(WebPreferencesKey::isSameSiteStrictEnforcementEnabledKey()))
        sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::Yes;

    if (experimentalFeatureEnabled(WebPreferencesKey::isFirstPartyWebsiteDataRemovalDisabledKey()))
        firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::None;
    else {
        if ([defaults boolForKey:adoptNS([[NSString alloc] initWithFormat:@"InternalDebug%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalReproTestingEnabledKey().createCFString().get()]).get()])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout;
        else if ([defaults boolForKey:adoptNS([[NSString alloc] initWithFormat:@"InternalDebug%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalLiveOnTestingEnabledKey().createCFString().get()]).get()])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout;
        else
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    }

    if (RetainPtr manualPrevalentResource = [defaults stringForKey:@"ITPManualPrevalentResource"]) {
        URL url { { }, manualPrevalentResource.get() };
        if (!url.isValid())
            url = { { }, makeString("http://"_s, manualPrevalentResource.get()) };
        if (url.isValid())
            resourceLoadStatisticsManualPrevalentResource = WebCore::RegistrableDomain { url };
    }
#if !RELEASE_LOG_DISABLED
    static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
    shouldLogCookieInformation = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif

    URL httpProxy = m_configuration->httpProxy();
    URL httpsProxy = m_configuration->httpsProxy();
    
    bool isSafari = false;
    bool isMiniBrowser = false;
#if PLATFORM(IOS_FAMILY)
    isSafari = WTF::IOSApplication::isMobileSafari();
    isMiniBrowser = WTF::IOSApplication::isMiniBrowser();
#elif PLATFORM(MAC)
    isSafari = WTF::MacApplication::isSafari();
    isMiniBrowser = WTF::MacApplication::isMiniBrowser();
#endif
    // FIXME: Remove these once Safari adopts _WKWebsiteDataStoreConfiguration.httpProxy and .httpsProxy.
    if (!httpProxy.isValid() && (isSafari || isMiniBrowser))
        httpProxy = URL { [defaults stringForKey:WebKit2HTTPProxyDefaultsKey] };
    if (!httpsProxy.isValid() && (isSafari || isMiniBrowser))
        httpsProxy = URL { [defaults stringForKey:WebKit2HTTPSProxyDefaultsKey] };

    auto& directories = resolvedDirectories();
#if HAVE(ALTERNATIVE_SERVICE)
    SandboxExtension::Handle alternativeServiceStorageDirectoryExtensionHandle;
    String alternativeServiceStorageDirectory = directories.alternativeServicesDirectory;
    createHandleFromResolvedPathIfPossible(alternativeServiceStorageDirectory, alternativeServiceStorageDirectoryExtensionHandle);
#endif

    bool shouldIncludeLocalhostInResourceLoadStatistics = isSafari;
    
    parameters.networkSessionParameters.proxyConfiguration = configuration().proxyConfiguration();
    parameters.networkSessionParameters.sourceApplicationBundleIdentifier = configuration().sourceApplicationBundleIdentifier();
    parameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = configuration().sourceApplicationSecondaryIdentifier();
    parameters.networkSessionParameters.shouldLogCookieInformation = shouldLogCookieInformation;
    parameters.networkSessionParameters.httpProxy = WTFMove(httpProxy);
    parameters.networkSessionParameters.httpsProxy = WTFMove(httpsProxy);
#if HAVE(ALTERNATIVE_SERVICE)
    parameters.networkSessionParameters.alternativeServiceDirectory = WTFMove(alternativeServiceStorageDirectory);
    parameters.networkSessionParameters.alternativeServiceDirectoryExtensionHandle = WTFMove(alternativeServiceStorageDirectoryExtensionHandle);
#endif
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost = shouldIncludeLocalhostInResourceLoadStatistics;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled = sameSiteStrictEnforcementEnabled;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode = firstPartyWebsiteDataRemovalMode;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.standaloneApplicationDomain = WebCore::RegistrableDomain { m_configuration->standaloneApplicationURL() };
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.manualPrevalentResource = WTFMove(resourceLoadStatisticsManualPrevalentResource);

    auto cookieFile = directories.cookieStorageFile;
    createHandleFromResolvedPathIfPossible(FileSystem::parentPath(cookieFile), parameters.cookieStoragePathExtensionHandle);

    if (m_uiProcessCookieStorageIdentifier.isEmpty()) {
        auto utf8File = cookieFile.utf8();
        auto url = adoptCF(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)utf8File.data(), (CFIndex)utf8File.length(), true));
        RetainPtr cfCookieStorage = adoptCF(CFHTTPCookieStorageCreateFromFile(kCFAllocatorDefault, url.get(), nullptr));
        m_uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage(cfCookieStorage.get());
    }

    parameters.uiProcessCookieStorageIdentifier = m_uiProcessCookieStorageIdentifier;
    parameters.networkSessionParameters.enablePrivateClickMeasurementDebugMode = experimentalFeatureEnabled(WebPreferencesKey::privateClickMeasurementDebugModeEnabledKey());
}

std::optional<bool> WebsiteDataStore::useNetworkLoader()
{
#if !HAVE(NETWORK_LOADER)
    return false;
#else

    [[maybe_unused]] const auto isSafari =
#if PLATFORM(MAC)
        WTF::MacApplication::isSafari();
#elif PLATFORM(IOS_FAMILY)
        WTF::IOSApplication::isMobileSafari() || WTF::IOSApplication::isSafariViewService();
#else
        false;
#endif

    if (auto isEnabled = optionalExperimentalFeatureEnabled(WebPreferencesKey::cFNetworkNetworkLoaderEnabledKey(), std::nullopt))
        return isEnabled;
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::UseCFNetworkNetworkLoader))
        return std::nullopt;
#if HAVE(NWSETTINGS_UNIFIED_HTTP) && defined(NW_SETTINGS_HAS_UNIFIED_HTTP)
    if (isRunningTest(applicationBundleIdentifier()))
        return true;
    if (nw_settings_get_unified_http_enabled())
        return isSafari;
#endif
    return std::nullopt;

#endif // NETWORK_LOADER
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
bool WebsiteDataStore::isOptInCookiePartitioningEnabled() const
{
#if defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    return std::ranges::any_of(m_processes, [](auto& process) {
        return std::ranges::any_of(process.pages(), [](auto& page) {
            return page->preferences().optInPartitionedCookiesEnabled();
        });
    });
#else
    return false;
#endif
}
#endif

void WebsiteDataStore::platformInitialize()
{
#if ENABLE(APP_BOUND_DOMAINS)
    initializeAppBoundDomains();
#endif
#if ENABLE(MANAGED_DOMAINS)
    initializeManagedDomains();
#endif
}

void WebsiteDataStore::platformDestroy()
{
}

static String defaultWebsiteDataStoreRootDirectory()
{
    static NeverDestroyed<RetainPtr<NSURL>> websiteDataStoreDirectory = [] {
        RetainPtr libraryDirectory = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        RELEASE_ASSERT(libraryDirectory);
        RetainPtr webkitDirectory = [libraryDirectory URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
        if (!WebKit::processHasContainer())
            webkitDirectory = [webkitDirectory URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];

        return [webkitDirectory URLByAppendingPathComponent:@"WebsiteDataStore" isDirectory:YES];
    }();

    return websiteDataStoreDirectory.get().get().absoluteURL.path;
}

void WebsiteDataStore::fetchAllDataStoreIdentifiers(CompletionHandler<void(Vector<WTF::UUID>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    websiteDataStoreIOQueueSingleton().dispatch([completionHandler = WTFMove(completionHandler), directory = defaultWebsiteDataStoreRootDirectory().isolatedCopy()]() mutable {
        auto identifiers = WTF::compactMap(FileSystem::listDirectory(directory), [](auto&& identifierString) {
            return WTF::UUID::parse(identifierString);
        });
        RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), identifiers = crossThreadCopy(WTFMove(identifiers))]() mutable {
            completionHandler(WTFMove(identifiers));
        });
    });
}

void WebsiteDataStore::removeDataStoreWithIdentifierImpl(const WTF::UUID& identifier, CompletionHandler<void(const String&)>&& completionHandler)
{
    websiteDataStoreIOQueueSingleton().dispatch([completionHandler = WTFMove(completionHandler), identifier, directory = defaultWebsiteDataStoreDirectory(identifier).isolatedCopy()]() mutable {
        RetainPtr nsCredentialStorage = adoptNS([[NSURLCredentialStorage alloc] _initWithIdentifier:identifier.toString().createNSString().get() private:NO]);
        RetainPtr credentials = [nsCredentialStorage allCredentials];
        for (NSURLProtectionSpace *space in credentials.get()) {
            for (NSURLCredential *credential in [credentials.get()[space] allValues])
                [nsCredentialStorage removeCredential:credential forProtectionSpace:space];
        }

        bool deleted = FileSystem::deleteNonEmptyDirectory(directory);
        RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), deleted]() mutable {
            if (!deleted)
                return completionHandler("Failed to delete files on disk"_s);

            completionHandler({ });
        });
    });
}

void WebsiteDataStore::removeDataStoreWithIdentifier(const WTF::UUID& identifier, CompletionHandler<void(const String&)>&& callback)
{
    ASSERT(isMainRunLoop());

    auto completionHandler = [identifier, callback = WTFMove(callback)](const String& error) mutable {
        RELEASE_LOG(Storage, "WebsiteDataStore::removeDataStoreWithIdentifier: Removal completed for identifier %" PUBLIC_LOG_STRING " (error '%" PUBLIC_LOG_STRING "')", identifier.toString().utf8().data(), error.isEmpty() ? "null"_s : error.utf8().data());
        callback(error);
    };
    RELEASE_LOG(Storage, "WebsiteDataStore::removeDataStoreWithIdentifier: Removal started for identifier %" PUBLIC_LOG_STRING, identifier.toString().utf8().data());
    if (!identifier.isValid())
        return completionHandler("Identifier is invalid"_s);

    if (RefPtr existingDataStore = existingDataStoreForIdentifier(identifier)) {
        if (existingDataStore->hasActivePages())
            return completionHandler("Data store is in use"_s);

        // FIXME: Try removing session from network process instead of returning error.
        if (existingDataStore->networkProcessIfExists())
            return completionHandler("Data store is in use (by network process)"_s);
    }

    if (RefPtr networkProcess = NetworkProcessProxy::defaultNetworkProcess().get()) {
        networkProcess->sendWithAsyncReply(Messages::NetworkProcess::EnsureSessionWithDataStoreIdentifierRemoved { identifier }, [identifier, completionHandler = WTFMove(completionHandler)]() mutable {
            removeDataStoreWithIdentifierImpl(identifier, WTFMove(completionHandler));
        });
        return;
    }

    removeDataStoreWithIdentifierImpl(identifier, WTFMove(completionHandler));
}

String WebsiteDataStore::defaultWebsiteDataStoreDirectory(const WTF::UUID& identifier)
{
    return FileSystem::pathByAppendingComponent(defaultWebsiteDataStoreRootDirectory(), identifier.toString());
}

String WebsiteDataStore::defaultCookieStorageFile(const String& baseDirectory)
{
    if (baseDirectory.isEmpty())
        return { };

    return FileSystem::pathByAppendingComponents(baseDirectory, std::initializer_list<StringView>({ "Cookies"_s, "Cookies.binarycookies"_s }));
}

String WebsiteDataStore::defaultSearchFieldHistoryDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "SearchHistory"_s);

    return websiteDataDirectoryFileSystemRepresentation("SearchHistory"_s);
}

String WebsiteDataStore::defaultCacheStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "CacheStorage"_s);

    return cacheDirectoryFileSystemRepresentation("CacheStorage"_s);
}

String WebsiteDataStore::defaultGeneralStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "Origins"_s);

    auto directory = websiteDataDirectoryFileSystemRepresentation("Default"_s);
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // This is the old storage directory, and there might be files left here.
        RetainPtr oldDirectory = cacheDirectoryFileSystemRepresentation("Storage"_s, { }, ShouldCreateDirectory::No).createNSString();
        RetainPtr fileManager = [NSFileManager defaultManager];
        RetainPtr<NSArray> files = [fileManager contentsOfDirectoryAtPath:oldDirectory.get() error:0];
        if (files) {
            for (NSString *fileName in files.get()) {
                if (![fileName length])
                    continue;

                RetainPtr path = [directory.createNSString() stringByAppendingPathComponent:fileName];
                RetainPtr oldPath = [oldDirectory stringByAppendingPathComponent:fileName];
                [fileManager moveItemAtPath:oldPath.get() toPath:path.get() error:nil];
            }
        }
        [fileManager removeItemAtPath:oldDirectory.get() error:nil];
    });

    return directory;
}

String WebsiteDataStore::defaultNetworkCacheDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "NetworkCache"_s);

    return cacheDirectoryFileSystemRepresentation("NetworkCache"_s);
}

String WebsiteDataStore::defaultAlternativeServicesDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "AlternativeServices"_s);

    return cacheDirectoryFileSystemRepresentation("AlternativeServices"_s, { }, ShouldCreateDirectory::No);
}

String WebsiteDataStore::defaultHSTSStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "HSTS"_s);

    return cacheDirectoryFileSystemRepresentation("HSTS"_s);
}

String WebsiteDataStore::defaultMediaCacheDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "MediaCache"_s);

    return tempDirectoryFileSystemRepresentation("MediaCache"_s);
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "IndexedDB"_s);

    return websiteDataDirectoryFileSystemRepresentation("IndexedDB"_s);
}

String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "ServiceWorkers"_s);

    return cacheDirectoryFileSystemRepresentation("ServiceWorkers"_s);
}

String WebsiteDataStore::defaultLocalStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "LocalStorage"_s);

    return websiteDataDirectoryFileSystemRepresentation("LocalStorage"_s);
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "MediaKeys"_s);

    return websiteDataDirectoryFileSystemRepresentation("MediaKeys"_s);
}

String WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "DeviceIdHashSalts"_s);

    return websiteDataDirectoryFileSystemRepresentation("DeviceIdHashSalts"_s);
}

#if ENABLE(ENCRYPTED_MEDIA)
String WebsiteDataStore::defaultMediaKeysHashSaltsStorageDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "MediaKeysHashSalts"_s);

    return websiteDataDirectoryFileSystemRepresentation("MediaKeysHashSalts"_s);
}
#endif

String WebsiteDataStore::defaultWebSQLDatabaseDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "WebSQL"_s);

    return websiteDataDirectoryFileSystemRepresentation("WebSQL"_s, { }, ShouldCreateDirectory::No);
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "ResourceLoadStatistics"_s);

    return websiteDataDirectoryFileSystemRepresentation("ResourceLoadStatistics"_s);
}

String WebsiteDataStore::defaultJavaScriptConfigurationDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "JavaScriptCoreDebug"_s);

    return tempDirectoryFileSystemRepresentation("JavaScriptCoreDebug"_s, ShouldCreateDirectory::No);
}

#if ENABLE(ARKIT_INLINE_PREVIEW)
String WebsiteDataStore::defaultModelElementCacheDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "ModelElement"_s);

    return tempDirectoryFileSystemRepresentation("ModelElement"_s, ShouldCreateDirectory::No);
}
#endif

#if ENABLE(CONTENT_EXTENSIONS)
String WebsiteDataStore::defaultResourceMonitorThrottlerDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "ResourceMonitorThrottler"_s);

    return websiteDataDirectoryFileSystemRepresentation("ResourceMonitorThrottler"_s, { }, ShouldCreateDirectory::No);
}
#endif

String WebsiteDataStore::tempDirectoryFileSystemRepresentation(const String& directoryName, ShouldCreateDirectory shouldCreateDirectory)
{
    static NeverDestroyed<RetainPtr<NSURL>> tempURL = [] {
        RetainPtr url = [NSURL fileURLWithPath:RetainPtr { NSTemporaryDirectory() }.get() isDirectory:YES];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();
        
        if (!WebKit::processHasContainer())
            url = [url URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];
        
        return [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
    }();
    
    RetainPtr url = [tempURL.get() URLByAppendingPathComponent:directoryName.createNSString().get() isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && (![[NSFileManager defaultManager] createDirectoryAtURL:url.get() withIntermediateDirectories:YES attributes:nil error:nullptr]))
        LOG_ERROR("Failed to create directory %@", url.get());
    
    return url.get().absoluteURL.path;
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName, const String&, ShouldCreateDirectory shouldCreateDirectory)
{
    static NeverDestroyed<RetainPtr<NSURL>> cacheURL = [] {
        RetainPtr url = [[NSFileManager defaultManager] URLForDirectory:NSCachesDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();

        if (!WebKit::processHasContainer())
            url = [url URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];

        return [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
    }();

    RetainPtr url = [cacheURL.get() URLByAppendingPathComponent:directoryName.createNSString().get() isDirectory:YES];
    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && ![[NSFileManager defaultManager] createDirectoryAtURL:url.get() withIntermediateDirectories:YES attributes:nil error:nullptr])
        LOG_ERROR("Failed to create directory %@", url.get());

    return url.get().absoluteURL.path;
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName, const String&, ShouldCreateDirectory shouldCreateDirectory)
{
    static NeverDestroyed<RetainPtr<NSURL>> websiteDataURL = [] {
        RetainPtr url = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();

        url = [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
        if (!WebKit::processHasContainer())
            url = [url URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];

        return [url URLByAppendingPathComponent:@"WebsiteData" isDirectory:YES];
    }();

    RetainPtr url = [websiteDataURL.get() URLByAppendingPathComponent:directoryName.createNSString().get() isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes) {
        if (![[NSFileManager defaultManager] createDirectoryAtURL:url.get() withIntermediateDirectories:YES attributes:nil error:nullptr])
            LOG_ERROR("Failed to create directory %@", url.get());
    }

    return url.get().absoluteURL.path;
}

#if ENABLE(APP_BOUND_DOMAINS)
static HashSet<WebCore::RegistrableDomain>& appBoundDomains()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<WebCore::RegistrableDomain>> appBoundDomains;
    return appBoundDomains;
}

static HashSet<String>& appBoundSchemes()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<String>> appBoundSchemes;
    return appBoundSchemes;
}

void WebsiteDataStore::initializeAppBoundDomains(ForceReinitialization forceReinitialization)
{
    ASSERT(RunLoop::isMain());

    if (hasInitializedAppBoundDomains && forceReinitialization != ForceReinitialization::Yes)
        return;
    
    static const auto maxAppBoundDomainCount = 10;
    
    appBoundDomainQueue().dispatch([forceReinitialization] () mutable {
        if (hasInitializedAppBoundDomains && forceReinitialization != ForceReinitialization::Yes)
            return;
        
        NSArray<NSString *> *appBoundData = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"WKAppBoundDomains"];
        keyExists = !!appBoundData;
        
        RunLoop::mainSingleton().dispatch([forceReinitialization, appBoundData = retainPtr(appBoundData)] {
            if (hasInitializedAppBoundDomains && forceReinitialization != ForceReinitialization::Yes)
                return;

            if (forceReinitialization == ForceReinitialization::Yes)
                appBoundDomains().clear();

            for (NSString *data in appBoundData.get()) {
                if (appBoundDomains().size() + appBoundSchemes().size() >= maxAppBoundDomainCount)
                    break;
                if ([data hasSuffix:@":"]) {
                    auto appBoundScheme = String([data substringToIndex:[data length] - 1]);
                    if (!appBoundScheme.isEmpty()) {
                        appBoundSchemes().add(appBoundScheme);
                        continue;
                    }
                }

                URL url { data };
                if (url.protocol().isEmpty())
                    url.setProtocol("https"_s);
                if (!url.isValid())
                    continue;
                WebCore::RegistrableDomain appBoundDomain { url };
                if (appBoundDomain.isEmpty())
                    continue;
                appBoundDomains().add(appBoundDomain);
            }
            hasInitializedAppBoundDomains = true;
            if (isAppBoundITPRelaxationEnabled)
                forwardAppBoundDomainsToITPIfInitialized([] { });
        });
    });
}

void WebsiteDataStore::addTestDomains() const
{
    if (appBoundDomains().isEmpty()) {
        for (auto& domain : appBoundDomainsForTesting(applicationBundleIdentifier()))
            appBoundDomains().add(domain);
    }
}

void WebsiteDataStore::ensureAppBoundDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&, const HashSet<String>&)>&& completionHandler) const
{
    if (hasInitializedAppBoundDomains) {
        if (m_configuration->enableInAppBrowserPrivacyForTesting())
            addTestDomains();
        completionHandler(appBoundDomains(), appBoundSchemes());
        return;
    }

    // Hopping to the background thread then back to the main thread
    // ensures that initializeAppBoundDomains() has finished.
    appBoundDomainQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        RunLoop::mainSingleton().dispatch([this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(hasInitializedAppBoundDomains);
            if (m_configuration->enableInAppBrowserPrivacyForTesting())
                addTestDomains();
            completionHandler(appBoundDomains(), appBoundSchemes());
        });
    });
}

static NavigatingToAppBoundDomain schemeOrDomainIsAppBound(const String& host, const String& protocol, const HashSet<WebCore::RegistrableDomain>& domains, const HashSet<String>& schemes)
{
    auto schemeIsAppBound = !protocol.isNull() && schemes.contains(protocol);
    auto domainIsAppBound = domains.contains(WebCore::RegistrableDomain::uncheckedCreateFromHost(host));
    return schemeIsAppBound || domainIsAppBound ? NavigatingToAppBoundDomain::Yes : NavigatingToAppBoundDomain::No;
}

void WebsiteDataStore::beginAppBoundDomainCheck(const String& host, const String& protocol, WebFramePolicyListenerProxy& listener)
{
    ASSERT(RunLoop::isMain());

    ensureAppBoundDomains([&host, &protocol, listener = Ref { listener }] (auto& domains, auto& schemes) mutable {
        // Must check for both an empty app bound domains list and an empty key before returning nullopt
        // because test cases may have app bound domains but no key.
        bool hasAppBoundDomains = keyExists || !domains.isEmpty();
        if (!hasAppBoundDomains) {
            listener->didReceiveAppBoundDomainResult(std::nullopt);
            return;
        }
        listener->didReceiveAppBoundDomainResult(schemeOrDomainIsAppBound(host, protocol, domains, schemes));
    });
}

void WebsiteDataStore::getAppBoundDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());

    ensureAppBoundDomains([completionHandler = WTFMove(completionHandler)] (auto& domains, auto& schemes) mutable {
        completionHandler(domains);
    });
}

void WebsiteDataStore::getAppBoundSchemes(CompletionHandler<void(const HashSet<String>&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());

    ensureAppBoundDomains([completionHandler = WTFMove(completionHandler)] (auto& domains, auto& schemes) mutable {
        completionHandler(schemes);
    });
}

std::optional<HashSet<WebCore::RegistrableDomain>> WebsiteDataStore::appBoundDomainsIfInitialized()
{
    ASSERT(RunLoop::isMain());
    if (!hasInitializedAppBoundDomains)
        return std::nullopt;
    return appBoundDomains();
}

void WebsiteDataStore::setAppBoundDomainsForTesting(HashSet<WebCore::RegistrableDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{
    for (auto& domain : domains)
        RELEASE_ASSERT(domain == "localhost"_s || domain == "127.0.0.1"_s);

    appBoundDomains() = WTFMove(domains);
    hasInitializedAppBoundDomains = true;
    forwardAppBoundDomainsToITPIfInitialized(WTFMove(completionHandler));
}

void WebsiteDataStore::reinitializeAppBoundDomains()
{
    hasInitializedAppBoundDomains = false;
    initializeAppBoundDomains(ForceReinitialization::Yes);
}
#endif


#if ENABLE(MANAGED_DOMAINS)
static HashSet<WebCore::RegistrableDomain>& managedDomains()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<WebCore::RegistrableDomain>> managedDomains;
    return managedDomains;
}

#if PLATFORM(MAC)
static NSString *managedSitesIdentifierSingleton()
{
    static NSString *identifier = @"com.apple.mail-shared";
    return identifier;
}

static NSString *crossSiteTrackingPreventionRelaxedDomainsKeySingleton()
{
    static NSString *key = @"CrossSiteTrackingPreventionRelaxedDomains";
    return key;
}

static NSString *crossSiteTrackingPreventionRelaxedAppsKeySingleton()
{
    static NSString *key = @"CrossSiteTrackingPreventionRelaxedApps";
    return key;
}
#endif

void WebsiteDataStore::initializeManagedDomains(ForceReinitialization forceReinitialization)
{
    ASSERT(RunLoop::isMain());

    if (hasInitializedManagedDomains && forceReinitialization != ForceReinitialization::Yes)
        return;

    managedDomainQueueSingleton().dispatch([forceReinitialization] () mutable {
        if (hasInitializedManagedDomains && forceReinitialization != ForceReinitialization::Yes)
            return;
        static const auto maxManagedDomainCount = 10;
        RetainPtr<NSArray<NSString *>> crossSiteTrackingPreventionRelaxedDomains;
        RetainPtr<NSArray<NSString *>> crossSiteTrackingPreventionRelaxedApps;

        bool isSafari = false;
#if PLATFORM(MAC)
        isSafari = WTF::MacApplication::isSafari();
        RetainPtr path = [adoptNS([[NSString alloc] initWithFormat:@"/Library/Managed Preferences/%@/%@.plist", RetainPtr { NSUserName() }.get(), managedSitesIdentifierSingleton()]) stringByStandardizingPath];
        RetainPtr managedSitesPrefs = adoptNS([[NSDictionary alloc] initWithContentsOfFile:path.get()]);
        crossSiteTrackingPreventionRelaxedDomains = [managedSitesPrefs objectForKey:crossSiteTrackingPreventionRelaxedDomainsKeySingleton()];
        crossSiteTrackingPreventionRelaxedApps = [managedSitesPrefs objectForKey:crossSiteTrackingPreventionRelaxedAppsKeySingleton()];
#elif !PLATFORM(MACCATALYST)
        isSafari = WTF::IOSApplication::isMobileSafari();
        if ([PAL::getMCProfileConnectionClassSingleton() instancesRespondToSelector:@selector(crossSiteTrackingPreventionRelaxedDomains)])
            crossSiteTrackingPreventionRelaxedDomains = [(MCProfileConnection *)[PAL::getMCProfileConnectionClassSingleton() sharedConnection] crossSiteTrackingPreventionRelaxedDomains];
        else
            crossSiteTrackingPreventionRelaxedDomains = @[];

        auto relaxedAppsSelector = NSSelectorFromString(@"crossSiteTrackingPreventionRelaxedApps");
        if ([PAL::getMCProfileConnectionClassSingleton() instancesRespondToSelector:relaxedAppsSelector])
            crossSiteTrackingPreventionRelaxedApps = [[PAL::getMCProfileConnectionClassSingleton() sharedConnection] performSelector:relaxedAppsSelector];
        else
            crossSiteTrackingPreventionRelaxedApps = @[];
#endif
        managedKeyExists = !!crossSiteTrackingPreventionRelaxedDomains;
    
        RetainPtr<NSString> bundleID = [[NSBundle mainBundle] bundleIdentifier];
        bool shouldUseRelaxedDomainsIfAvailable = isSafari || isRunningTest(bundleID.get()) || [crossSiteTrackingPreventionRelaxedApps containsObject:bundleID.get()];
        if (!shouldUseRelaxedDomainsIfAvailable)
            return;

        RunLoop::mainSingleton().dispatch([forceReinitialization, crossSiteTrackingPreventionRelaxedDomains] {
            if (hasInitializedManagedDomains && forceReinitialization != ForceReinitialization::Yes)
                return;

            if (forceReinitialization == ForceReinitialization::Yes)
                managedDomains().clear();

            for (NSString *data in crossSiteTrackingPreventionRelaxedDomains.get()) {
                if (managedDomains().size() >= maxManagedDomainCount)
                    break;

                URL url { data };
                if (url.protocol().isEmpty())
                    url.setProtocol("https"_s);
                if (!url.isValid())
                    continue;
                WebCore::RegistrableDomain managedDomain { url };
                if (managedDomain.isEmpty())
                    continue;
                managedDomains().add(managedDomain);
            }
            hasInitializedManagedDomains = true;
            forwardManagedDomainsToITPIfInitialized([] { });
        });
    });
}

void WebsiteDataStore::ensureManagedDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&)>&& completionHandler) const
{
    if (hasInitializedManagedDomains) {
        completionHandler(managedDomains());
        return;
    }

    // Hopping to the background thread then back to the main thread
    // ensures that initializeManagedDomains() has finished.
    managedDomainQueueSingleton().dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        RunLoop::mainSingleton().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(hasInitializedManagedDomains);
            completionHandler(managedDomains());
        });
    });
}

void WebsiteDataStore::getManagedDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());

    ensureManagedDomains([completionHandler = WTFMove(completionHandler)] (auto& domains) mutable {
        completionHandler(domains);
    });
}

const HashSet<WebCore::RegistrableDomain>* WebsiteDataStore::managedDomainsIfInitialized()
{
    ASSERT(RunLoop::isMain());
    if (!hasInitializedManagedDomains)
        return nullptr;
    return &managedDomains();
}

void WebsiteDataStore::setManagedDomainsForTesting(HashSet<WebCore::RegistrableDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{
    for (auto& domain : domains)
        RELEASE_ASSERT(domain == "localhost"_s || domain == "127.0.0.1"_s);

    managedDomains() = WTFMove(domains);
    hasInitializedManagedDomains = true;
    forwardManagedDomainsToITPIfInitialized(WTFMove(completionHandler));
}

void WebsiteDataStore::reinitializeManagedDomains()
{
    hasInitializedManagedDomains = false;
    initializeManagedDomains(ForceReinitialization::Yes);
}
#endif

bool WebsiteDataStore::networkProcessHasEntitlementForTesting(const String& entitlement)
{
    return WTF::hasEntitlement(networkProcess().protectedConnection()->protectedXPCConnection().get(), entitlement);
}

std::optional<double> WebsiteDataStore::defaultOriginQuotaRatio()
{
    return isFullWebBrowserOrRunningTest() ? defaultBrowserOriginQuotaRatio : defaultAppOriginQuotaRatio;
}

std::optional<double> WebsiteDataStore::defaultTotalQuotaRatio()
{
    return isFullWebBrowserOrRunningTest() ? defaultBrowserTotalQuotaRatio : defaultAppTotalQuotaRatio;
}

UnifiedOriginStorageLevel WebsiteDataStore::defaultUnifiedOriginStorageLevel()
{
    return UnifiedOriginStorageLevel::Standard;
}

#if PLATFORM(IOS_FAMILY)

String WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory(const String& subpath)
{
    String path = pathForProcessContainer();
    if (path.isEmpty())
        path = NSHomeDirectory();

    return makeString(path, subpath);
}

String WebsiteDataStore::parentBundleDirectory() const
{
    if (!isPersistent())
        return emptyString();

    return [[[NSBundle mainBundle] bundlePath] stringByStandardizingPath];
}

String WebsiteDataStore::resolvedCookieStorageDirectory()
{
    if (m_resolvedCookieStorageDirectory.isNull()) {
        if (!isPersistent())
            m_resolvedCookieStorageDirectory = emptyString();
        else {
            auto directory = cacheDirectoryInContainerOrHomeDirectory("/Library/Cookies"_s);
            m_resolvedCookieStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directory);
        }
    }

    return m_resolvedCookieStorageDirectory;
}

String WebsiteDataStore::resolvedContainerCachesNetworkingDirectory()
{
    if (m_resolvedContainerCachesNetworkingDirectory.isNull()) {
        if (!isPersistent())
            m_resolvedContainerCachesNetworkingDirectory = emptyString();
        else {
            auto directory = cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.Networking/"_s);
            m_resolvedContainerCachesNetworkingDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directory);
        }
    }

    return m_resolvedContainerCachesNetworkingDirectory;
}

String WebsiteDataStore::resolvedContainerTemporaryDirectory()
{
    if (m_resolvedContainerTemporaryDirectory.isNull())
        m_resolvedContainerTemporaryDirectory = defaultResolvedContainerTemporaryDirectory();

    return m_resolvedContainerTemporaryDirectory;
}

String WebsiteDataStore::defaultResolvedContainerTemporaryDirectory()
{
    static NeverDestroyed<String> resolvedTemporaryDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(String(NSTemporaryDirectory()));
    return resolvedTemporaryDirectory;
}

void WebsiteDataStore::setBackupExclusionPeriodForTesting(Seconds period, CompletionHandler<void()>&& completionHandler)
{
    networkProcess().setBackupExclusionPeriodForTesting(m_sessionID, period, WTFMove(completionHandler));
}

#endif

void WebsiteDataStore::saveRecentSearches(const String& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    m_queue->dispatch([name = name.isolatedCopy(), searchItems = crossThreadCopy(searchItems), directory = resolvedDirectories().searchFieldHistoryDirectory.isolatedCopy()] {
        WebCore::saveRecentSearchesToFile(name, searchItems, directory);
    });
}

void WebsiteDataStore::loadRecentSearches(const String& name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    m_queue->dispatch([name = name.isolatedCopy(), completionHandler = WTFMove(completionHandler), directory = resolvedDirectories().searchFieldHistoryDirectory.isolatedCopy()]() mutable {
        auto result = WebCore::loadRecentSearchesFromFile(name, directory);
        RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), result = crossThreadCopy(result)]() mutable {
            completionHandler(WTFMove(result));
        });
    });
}

void WebsiteDataStore::removeRecentSearches(WallTime oldestTimeToRemove, CompletionHandler<void()>&& completionHandler)
{
    m_queue->dispatch([time = oldestTimeToRemove.isolatedCopy(), directory = resolvedDirectories().searchFieldHistoryDirectory.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        WebCore::removeRecentlyModifiedRecentSearchesFromFile(time, directory);
        RunLoop::mainSingleton().dispatch(WTFMove(completionHandler));
    });
}

HashSet<WebCore::RegistrableDomain> WebsiteDataStore::platformAdditionalDomainsWithUserInteraction() const
{
    RetainPtr<id> arrayOrCommaDelimitedString;
    if (!m_configuration->additionalDomainsWithUserInteractionForTesting().isEmpty())
        arrayOrCommaDelimitedString = m_configuration->additionalDomainsWithUserInteractionForTesting().createNSString();
    else
        arrayOrCommaDelimitedString = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitDebugAdditionalDomainsWithUserInteraction"];

    if (!arrayOrCommaDelimitedString)
        return { };

    HashSet<WebCore::RegistrableDomain> result { };
    auto addHost = [&result](String host) {
        WebCore::SecurityOriginData origin { "https"_s, host, std::nullopt };
        WebCore::RegistrableDomain domain { origin };
        if (!domain.isEmpty())
            result.add(domain);
    };

    if ([arrayOrCommaDelimitedString isKindOfClass:[NSArray class]]) {
        for (id host in arrayOrCommaDelimitedString.get()) {
            if ([host isKindOfClass:[NSString class]])
                addHost((NSString *)host);
        }
    } else if ([arrayOrCommaDelimitedString isKindOfClass:[NSString class]]) {
        String commaDelimitedString = (NSString *)arrayOrCommaDelimitedString;
        for (String host : commaDelimitedString.split(","_s))
            addHost(host.trim(isASCIIWhitespace<char16_t>));
    }

    return result;
}

}
