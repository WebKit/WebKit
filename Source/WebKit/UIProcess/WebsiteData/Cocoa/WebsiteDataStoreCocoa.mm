/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#import <WebCore/RegistrableDomain.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/ThirdPartyCookieBlockingMode.h>
#import <WebCore/TrackingPreventionTypes.h>
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
#import <NetworkProcess/cocoa/NetworkSoftLink.h>

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
    static NeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("com.apple.WebKit.AppBoundDomains"_s);
    return queue.get();
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

static std::optional<bool> optionalExperimentalFeatureEnabled(StringImpl* key, std::optional<bool> defaultValue = false)
{
    auto defaultsKey = adoptNS([[NSString alloc] initWithFormat:@"WebKitExperimental%@", createNSString(key).get()]);
    if ([[NSUserDefaults standardUserDefaults] objectForKey:defaultsKey.get()] != nil)
        return [[NSUserDefaults standardUserDefaults] boolForKey:defaultsKey.get()];

    return defaultValue;
}

bool experimentalFeatureEnabled(StringImpl* key, bool defaultValue)
{
    return *optionalExperimentalFeatureEnabled(key, defaultValue);
}

static bool internalDebugFeatureEnabled(StringImpl* key)
{
    RetainPtr defaultsKey = adoptNS([[NSString alloc] initWithFormat:@"InternalDebug%@", createNSString(key).get()]);
    return [[NSUserDefaults standardUserDefaults] boolForKey:defaultsKey.get()];
}

static RetainPtr<NSString> applicationOrProcessIdentifier()
{
    RetainPtr<NSString> identifier = [NSBundle mainBundle].bundleIdentifier;
    RetainPtr<NSString> processName = [NSProcessInfo processInfo].processName;
    // SafariForWebKitDevelopment has the same bundle identifier as Safari, but it does not have the privilege to
    // access Safari's paths.
    if ([identifier isEqualToString:@"com.apple.Safari"] && [processName isEqualToString:@"SafariForWebKitDevelopment"])
        identifier = WTF::move(processName);
    else if (!identifier)
        identifier = WTF::move(processName);
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

#if PLATFORM(MAC)
static RefPtr<StringImpl> libraryRootDirectory()
{
    static NeverDestroyed<RetainPtr<NSURL>> libraryDirectory = [] {
        RetainPtr libraryDirectory = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        RELEASE_ASSERT(libraryDirectory);
        return libraryDirectory;
    }();

    return StringImpl::create(libraryDirectory.get().get().absoluteURL.path);
}
#endif // PLATFORM(MAC)

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
        if ([defaults boolForKey:adoptNS([[NSString alloc] initWithFormat:@"InternalDebug%@", createCFString(WebPreferencesKey::isFirstPartyWebsiteDataRemovalReproTestingEnabledKey()).get()]).get()])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout;
        else if ([defaults boolForKey:adoptNS([[NSString alloc] initWithFormat:@"InternalDebug%@", createCFString(WebPreferencesKey::isFirstPartyWebsiteDataRemovalLiveOnTestingEnabledKey()).get()]).get()])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout;
        else
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    }

    if (RetainPtr manualPrevalentResource = [defaults stringForKey:@"ITPManualPrevalentResource"]) {
        URL url { { }, StringImpl::create(manualPrevalentResource.get()).get() };
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
        httpProxy = URL { StringImpl::create([defaults stringForKey:WebKit2HTTPProxyDefaultsKey]) };
    if (!httpsProxy.isValid() && (isSafari || isMiniBrowser))
        httpsProxy = URL { StringImpl::create([defaults stringForKey:WebKit2HTTPSProxyDefaultsKey]) };

    auto& directories = resolvedDirectories();
#if HAVE(ALTERNATIVE_SERVICE)
    SandboxExtension::Handle alternativeServiceStorageDirectoryExtensionHandle;
    RefPtr<StringImpl> alternativeServiceStorageDirectory = directories.alternativeServicesDirectory;
    createHandleFromResolvedPathIfPossible(alternativeServiceStorageDirectory, alternativeServiceStorageDirectoryExtensionHandle);
#endif

    bool shouldIncludeLocalhostInResourceLoadStatistics = isSafari;
    
    parameters.networkSessionParameters.proxyConfiguration = configuration().proxyConfiguration();
    parameters.networkSessionParameters.sourceApplicationBundleIdentifier = configuration().sourceApplicationBundleIdentifier();
    parameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = configuration().sourceApplicationSecondaryIdentifier();
    parameters.networkSessionParameters.shouldLogCookieInformation = shouldLogCookieInformation;
    parameters.networkSessionParameters.httpProxy = WTF::move(httpProxy);
    parameters.networkSessionParameters.httpsProxy = WTF::move(httpsProxy);
#if HAVE(ALTERNATIVE_SERVICE)
    parameters.networkSessionParameters.alternativeServiceDirectory = WTF::move(alternativeServiceStorageDirectory);
    parameters.networkSessionParameters.alternativeServiceDirectoryExtensionHandle = WTF::move(alternativeServiceStorageDirectoryExtensionHandle);
#endif
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost = shouldIncludeLocalhostInResourceLoadStatistics;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled = sameSiteStrictEnforcementEnabled;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode = firstPartyWebsiteDataRemovalMode;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.standaloneApplicationDomain = WebCore::RegistrableDomain { m_configuration->standaloneApplicationURL() };
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.manualPrevalentResource = WTF::move(resourceLoadStatisticsManualPrevalentResource);
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.exemptAllowlistedCookiesFromITP = internalDebugFeatureEnabled(WebPreferencesKey::exemptAllowlistedCookiesFromItpKey());

    auto cookieFile = directories.cookieStorageFile;
#if PLATFORM(MAC)
    if (isEmpty(cookieFile))
        parameters.cookieStoragePath = FileSystem::parentPath(defaultCookieStorageFile(libraryRootDirectory()));
    else
        parameters.cookieStoragePath = FileSystem::parentPath(cookieFile);
#else
    parameters.cookieStoragePath = resolvedCookieStorageDirectory();
#endif

    createHandleFromResolvedPathIfPossible(FileSystem::parentPath(cookieFile), parameters.cookieStoragePathExtensionHandle);

    if (m_uiProcessCookieStorageIdentifier.isEmpty()) {
        auto utf8File = utf8(cookieFile);
        RetainPtr url = adoptCF(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, byteCast<UInt8>(utf8File.span()).data(), static_cast<CFIndex>(utf8File.length()), true));
        RetainPtr cfCookieStorage = adoptCF(CFHTTPCookieStorageCreateFromFile(kCFAllocatorDefault, url.get(), nullptr));
        m_uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage(cfCookieStorage.get());
    }

    parameters.uiProcessCookieStorageIdentifier = m_uiProcessCookieStorageIdentifier;
    parameters.networkSessionParameters.enablePrivateClickMeasurementDebugMode = experimentalFeatureEnabled(WebPreferencesKey::privateClickMeasurementDebugModeEnabledKey());
    parameters.networkSessionParameters.enablePrivateClickMeasurementPrivateRelayEnforcement = experimentalFeatureEnabled(WebPreferencesKey::privateClickMeasurementPrivateRelayEnforcementEnabledKey());
}

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

static RefPtr<StringImpl> defaultWebsiteDataStoreRootDirectory()
{
    static NeverDestroyed<RetainPtr<NSURL>> websiteDataStoreDirectory = [] {
        RetainPtr libraryDirectory = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        RELEASE_ASSERT(libraryDirectory);
        RetainPtr webkitDirectory = [libraryDirectory URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
        if (!WebKit::processHasContainer())
            webkitDirectory = [webkitDirectory URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];

        return [webkitDirectory URLByAppendingPathComponent:@"WebsiteDataStore" isDirectory:YES];
    }();

    return StringImpl::create(websiteDataStoreDirectory.get().get().absoluteURL.path);
}

void WebsiteDataStore::fetchAllDataStoreIdentifiers(CompletionHandler<void(Vector<WTF::UUID>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    websiteDataStoreIOQueueSingleton().dispatch([completionHandler = WTF::move(completionHandler), directory = isolatedCopy(defaultWebsiteDataStoreRootDirectory())]() mutable {
        auto identifiers = WTF::compactMap(FileSystem::listDirectory(directory), [](auto&& identifierString) {
            return WTF::UUID::parse(identifierString);
        });
        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), identifiers = crossThreadCopy(WTF::move(identifiers))]() mutable {
            completionHandler(WTF::move(identifiers));
        });
    });
}

void WebsiteDataStore::removeDataStoreWithIdentifierImpl(const WTF::UUID& identifier, CompletionHandler<void(StringImpl*)>&& completionHandler)
{
    websiteDataStoreIOQueueSingleton().dispatch([completionHandler = WTF::move(completionHandler), identifier, directory = isolatedCopy(defaultWebsiteDataStoreDirectory(identifier))]() mutable {
        RetainPtr nsCredentialStorage = adoptNS([[NSURLCredentialStorage alloc] _initWithIdentifier:createNSString(identifier.toString()).get() private:NO]);
        RetainPtr credentials = [nsCredentialStorage allCredentials];
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSURLProtectionSpace *space in credentials.get()) {
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSURLCredential *credential in [credentials.get()[space] allValues])
                [nsCredentialStorage removeCredential:credential forProtectionSpace:space];
        }

        bool deleted = FileSystem::deleteNonEmptyDirectory(directory);
        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), deleted]() mutable {
            if (!deleted)
                return completionHandler("Failed to delete files on disk"_si);

            completionHandler(nullptr);
        });
    });
}

void WebsiteDataStore::removeDataStoreWithIdentifier(const WTF::UUID& identifier, CompletionHandler<void(StringImpl*)>&& callback)
{
    ASSERT(isMainRunLoop());

    auto completionHandler = [identifier, callback = WTF::move(callback)](StringImpl* error) mutable {
        RELEASE_LOG(Storage, "WebsiteDataStore::removeDataStoreWithIdentifier: Removal completed for identifier %" PUBLIC_LOG_STRING " (error '%" PUBLIC_LOG_STRING "')", utf8(identifier.toString()), isEmpty(error) ? "null"_s : utf8(error));
        callback(error);
    };
    RELEASE_LOG(Storage, "WebsiteDataStore::removeDataStoreWithIdentifier: Removal started for identifier %" PUBLIC_LOG_STRING, utf8(identifier.toString()));
    if (!identifier.isValid())
        return completionHandler("Identifier is invalid"_si);

    if (RefPtr existingDataStore = existingDataStoreForIdentifier(identifier)) {
        if (existingDataStore->hasActivePages())
            return completionHandler("Data store is in use"_si);

        // FIXME: Try removing session from network process instead of returning error.
        if (existingDataStore->networkProcessIfExists())
            return completionHandler("Data store is in use (by network process)"_si);
    }

    if (RefPtr networkProcess = NetworkProcessProxy::defaultNetworkProcess().get()) {
        networkProcess->sendWithAsyncReply(Messages::NetworkProcess::EnsureSessionWithDataStoreIdentifierRemoved { identifier }, [identifier, completionHandler = WTF::move(completionHandler)]() mutable {
            removeDataStoreWithIdentifierImpl(identifier, WTF::move(completionHandler));
        });
        return;
    }

    removeDataStoreWithIdentifierImpl(identifier, WTF::move(completionHandler));
}

RefPtr<StringImpl> WebsiteDataStore::defaultWebsiteDataStoreDirectory(const WTF::UUID& identifier)
{
    return FileSystem::pathByAppendingComponent(defaultWebsiteDataStoreRootDirectory(), identifier.toString());
}

RefPtr<StringImpl> WebsiteDataStore::defaultCookieStorageFile(StringImpl* baseDirectory)
{
    if (isEmpty(baseDirectory))
        return { };

    return FileSystem::pathByAppendingComponents(baseDirectory, std::initializer_list<StringView>({ "Cookies"_s, "Cookies.binarycookies"_s }));
}

RefPtr<StringImpl> WebsiteDataStore::defaultSearchFieldHistoryDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "SearchHistory"_s);

    return websiteDataDirectoryFileSystemRepresentation("SearchHistory"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultCacheStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "CacheStorage"_s);

    return cacheDirectoryFileSystemRepresentation("CacheStorage"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultGeneralStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "Origins"_s);

    auto directory = websiteDataDirectoryFileSystemRepresentation("Default"_si);
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // This is the old storage directory, and there might be files left here.
        RetainPtr oldDirectory = createNSString(cacheDirectoryFileSystemRepresentation("Storage"_si, nullptr, ShouldCreateDirectory::No));
        RetainPtr fileManager = [NSFileManager defaultManager];
        RetainPtr<NSArray> files = [fileManager contentsOfDirectoryAtPath:oldDirectory.get() error:0];
        if (files) {
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *fileName in files.get()) {
                if (![fileName length])
                    continue;

                RetainPtr path = [createNSString(directory) stringByAppendingPathComponent:fileName];
                RetainPtr oldPath = [oldDirectory stringByAppendingPathComponent:fileName];
                [fileManager moveItemAtPath:oldPath.get() toPath:path.get() error:nil];
            }
        }
        [fileManager removeItemAtPath:oldDirectory.get() error:nil];
    });

    return directory;
}

RefPtr<StringImpl> WebsiteDataStore::defaultNetworkCacheDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "NetworkCache"_s);

    return cacheDirectoryFileSystemRepresentation("NetworkCache"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultAlternativeServicesDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "AlternativeServices"_s);

    return cacheDirectoryFileSystemRepresentation("AlternativeServices"_si, nullptr, ShouldCreateDirectory::No);
}

RefPtr<StringImpl> WebsiteDataStore::defaultHSTSStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "HSTS"_s);

    return cacheDirectoryFileSystemRepresentation("HSTS"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultMediaCacheDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "MediaCache"_s);

    return tempDirectoryFileSystemRepresentation("MediaCache"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultIndexedDBDatabaseDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "IndexedDB"_s);

    return websiteDataDirectoryFileSystemRepresentation("IndexedDB"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultIsolatedSitesDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "IsolatedSites"_s);

    return websiteDataDirectoryFileSystemRepresentation("IsolatedSites"_si, nullptr, ShouldCreateDirectory::No);
}

RefPtr<StringImpl> WebsiteDataStore::defaultServiceWorkerRegistrationDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "ServiceWorkers"_s);

    return cacheDirectoryFileSystemRepresentation("ServiceWorkers"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultLocalStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "LocalStorage"_s);

    return websiteDataDirectoryFileSystemRepresentation("LocalStorage"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultMediaKeysStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "MediaKeys"_s);

    return websiteDataDirectoryFileSystemRepresentation("MediaKeys"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "DeviceIdHashSalts"_s);

    return websiteDataDirectoryFileSystemRepresentation("DeviceIdHashSalts"_si);
}

#if ENABLE(ENCRYPTED_MEDIA)
RefPtr<StringImpl> WebsiteDataStore::defaultMediaKeysHashSaltsStorageDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "MediaKeysHashSalts"_s);

    return websiteDataDirectoryFileSystemRepresentation("MediaKeysHashSalts"_si);
}
#endif

RefPtr<StringImpl> WebsiteDataStore::defaultWebSQLDatabaseDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "WebSQL"_s);

    return websiteDataDirectoryFileSystemRepresentation("WebSQL"_si, nullptr, ShouldCreateDirectory::No);
}

RefPtr<StringImpl> WebsiteDataStore::defaultResourceLoadStatisticsDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "ResourceLoadStatistics"_s);

    return websiteDataDirectoryFileSystemRepresentation("ResourceLoadStatistics"_si);
}

RefPtr<StringImpl> WebsiteDataStore::defaultJavaScriptConfigurationDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "JavaScriptCoreDebug"_s);

    return tempDirectoryFileSystemRepresentation("JavaScriptCoreDebug"_si, ShouldCreateDirectory::No);
}


#if ENABLE(CONTENT_EXTENSIONS)
RefPtr<StringImpl> WebsiteDataStore::defaultResourceMonitorThrottlerDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "ResourceMonitorThrottler"_s);

    return websiteDataDirectoryFileSystemRepresentation("ResourceMonitorThrottler"_si, nullptr, ShouldCreateDirectory::No);
}
#endif

RefPtr<StringImpl> WebsiteDataStore::defaultEnhancedSecurityDirectory(StringImpl* baseDirectory)
{
    if (!isEmpty(baseDirectory))
        return FileSystem::pathByAppendingComponent(baseDirectory, "EnhancedSecurity"_s);

    return websiteDataDirectoryFileSystemRepresentation("EnhancedSecurity"_si, nullptr, ShouldCreateDirectory::No);
}

RefPtr<StringImpl> WebsiteDataStore::tempDirectoryFileSystemRepresentation(StringImpl* directoryName, ShouldCreateDirectory shouldCreateDirectory)
{
    static NeverDestroyed<RetainPtr<NSURL>> tempURL = [] {
        RetainPtr url = [NSURL fileURLWithPath:RetainPtr { NSTemporaryDirectory() }.get() isDirectory:YES];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();
        
        if (!WebKit::processHasContainer())
            url = [url URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];
        
        return [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
    }();
    
    RetainPtr url = [tempURL.get() URLByAppendingPathComponent:createNSString(directoryName).get() isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && (![[NSFileManager defaultManager] createDirectoryAtURL:url.get() withIntermediateDirectories:YES attributes:nil error:nullptr]))
        LOG_ERROR("Failed to create directory %@", url.get());
    
    return StringImpl::create(url.get().absoluteURL.path);
}

RefPtr<StringImpl> WebsiteDataStore::cacheDirectoryFileSystemRepresentation(StringImpl* directoryName, StringImpl*, ShouldCreateDirectory shouldCreateDirectory)
{
    static NeverDestroyed<RetainPtr<NSURL>> cacheURL = [] {
        RetainPtr url = [[NSFileManager defaultManager] URLForDirectory:NSCachesDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();

        if (!WebKit::processHasContainer())
            url = [url URLByAppendingPathComponent:applicationOrProcessIdentifier().get() isDirectory:YES];

        return [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
    }();

    RetainPtr url = [cacheURL.get() URLByAppendingPathComponent:createNSString(directoryName).get() isDirectory:YES];
    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && ![[NSFileManager defaultManager] createDirectoryAtURL:url.get() withIntermediateDirectories:YES attributes:nil error:nullptr])
        LOG_ERROR("Failed to create directory %@", url.get());

    return StringImpl::create(url.get().absoluteURL.path);
}

RefPtr<StringImpl> WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(StringImpl* directoryName, StringImpl*, ShouldCreateDirectory shouldCreateDirectory)
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

    RetainPtr url = [websiteDataURL.get() URLByAppendingPathComponent:createNSString(directoryName).get() isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes) {
        if (![[NSFileManager defaultManager] createDirectoryAtURL:url.get() withIntermediateDirectories:YES attributes:nil error:nullptr])
            LOG_ERROR("Failed to create directory %@", url.get());
    }

    return StringImpl::create(url.get().absoluteURL.path);
}

#if ENABLE(APP_BOUND_DOMAINS)
static HashSet<WebCore::RegistrableDomain>& appBoundDomains()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<WebCore::RegistrableDomain>> appBoundDomains;
    return appBoundDomains;
}

static HashSet<RefPtr<StringImpl>>& appBoundSchemes()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<RefPtr<StringImpl>>> appBoundSchemes;
    return appBoundSchemes;
}

void WebsiteDataStore::initializeAppBoundDomains(ForceReinitialization forceReinitialization)
{
    ASSERT(RunLoop::isMain());

    if (hasInitializedAppBoundDomains && forceReinitialization != ForceReinitialization::Yes)
        return;
    
    static const auto maxAppBoundDomainCount = 10;

    protect(appBoundDomainQueue())->dispatch([forceReinitialization] () mutable {
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
                    auto appBoundScheme = StringImpl::create([data substringToIndex:[data length] - 1]);
                    if (!isEmpty(appBoundScheme)) {
                        appBoundSchemes().add(appBoundScheme);
                        continue;
                    }
                }

                URL url { StringImpl::create(data) };
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

void WebsiteDataStore::ensureAppBoundDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&, const HashSet<RefPtr<StringImpl>>&)>&& completionHandler) const
{
    if (hasInitializedAppBoundDomains) {
        if (m_configuration->enableInAppBrowserPrivacyForTesting())
            addTestDomains();
        completionHandler(appBoundDomains(), appBoundSchemes());
        return;
    }

    // Hopping to the background thread then back to the main thread
    // ensures that initializeAppBoundDomains() has finished.
    protect(appBoundDomainQueue())->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] () mutable {
        RunLoop::mainSingleton().dispatch([this, protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)] () mutable {
            ASSERT(hasInitializedAppBoundDomains);
            if (m_configuration->enableInAppBrowserPrivacyForTesting())
                addTestDomains();
            completionHandler(appBoundDomains(), appBoundSchemes());
        });
    });
}

static NavigatingToAppBoundDomain schemeOrDomainIsAppBound(StringImpl* host, StringImpl* protocol, const HashSet<WebCore::RegistrableDomain>& domains, const HashSet<RefPtr<StringImpl>>& schemes)
{
    auto schemeIsAppBound = !!protocol && schemes.contains(protocol);
    auto domainIsAppBound = domains.contains(WebCore::RegistrableDomain::uncheckedCreateFromHost(host));
    return schemeIsAppBound || domainIsAppBound ? NavigatingToAppBoundDomain::Yes : NavigatingToAppBoundDomain::No;
}

void WebsiteDataStore::beginAppBoundDomainCheck(StringImpl* host, StringImpl* protocol, WebFramePolicyListenerProxy& listener)
{
    ASSERT(RunLoop::isMain());

    ensureAppBoundDomains([host = RefPtr { host }, protocol = RefPtr { protocol }, listener = Ref { listener }] (auto& domains, auto& schemes) mutable {
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

    ensureAppBoundDomains([completionHandler = WTF::move(completionHandler)] (auto& domains, auto& schemes) mutable {
        completionHandler(domains);
    });
}

void WebsiteDataStore::getAppBoundSchemes(CompletionHandler<void(const HashSet<RefPtr<StringImpl>>&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());

    ensureAppBoundDomains([completionHandler = WTF::move(completionHandler)] (auto& domains, auto& schemes) mutable {
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

    appBoundDomains() = WTF::move(domains);
    hasInitializedAppBoundDomains = true;
    forwardAppBoundDomainsToITPIfInitialized(WTF::move(completionHandler));
}

void WebsiteDataStore::reinitializeAppBoundDomains()
{
    hasInitializedAppBoundDomains = false;
    initializeAppBoundDomains(ForceReinitialization::Yes);
}
#endif


#if ENABLE(MANAGED_DOMAINS)
NODELETE static HashSet<WebCore::RegistrableDomain>& managedDomains()
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
        bool shouldUseRelaxedDomainsIfAvailable = isSafari || isRunningTest(StringImpl::create(bundleID.get()).get()) || [crossSiteTrackingPreventionRelaxedApps containsObject:bundleID.get()];
        if (!shouldUseRelaxedDomainsIfAvailable)
            return;

        RunLoop::mainSingleton().dispatch([forceReinitialization, crossSiteTrackingPreventionRelaxedDomains] {
            if (hasInitializedManagedDomains && forceReinitialization != ForceReinitialization::Yes)
                return;

            if (forceReinitialization == ForceReinitialization::Yes)
                managedDomains().clear();

            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *data in crossSiteTrackingPreventionRelaxedDomains.get()) {
                if (managedDomains().size() >= maxManagedDomainCount)
                    break;

                URL url { StringImpl::create(data) };
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
    managedDomainQueueSingleton().dispatch([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] () mutable {
        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)] () mutable {
            ASSERT(hasInitializedManagedDomains);
            completionHandler(managedDomains());
        });
    });
}

void WebsiteDataStore::getManagedDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());

    ensureManagedDomains([completionHandler = WTF::move(completionHandler)] (auto& domains) mutable {
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

    managedDomains() = WTF::move(domains);
    hasInitializedManagedDomains = true;
    forwardManagedDomainsToITPIfInitialized(WTF::move(completionHandler));
}

void WebsiteDataStore::reinitializeManagedDomains()
{
    hasInitializedManagedDomains = false;
    initializeManagedDomains(ForceReinitialization::Yes);
}
#endif

bool WebsiteDataStore::networkProcessHasEntitlementForTesting(StringImpl* entitlement)
{
    return WTF::hasEntitlement(protect(protect(networkProcess().connection())->xpcConnection()).get(), entitlement);
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

RefPtr<StringImpl> WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory(StringImpl* subpath)
{
    RefPtr<StringImpl> path = pathForProcessContainer();
    if (isEmpty(path))
        path = StringImpl::create(protect(NSHomeDirectory()).get());

    return makeString(path, subpath);
}

RefPtr<StringImpl> WebsiteDataStore::parentBundleDirectory() const
{
    if (!isPersistent())
        return emptyString();

    return StringImpl::create([[[NSBundle mainBundle] bundlePath] stringByStandardizingPath]);
}

RefPtr<StringImpl> WebsiteDataStore::resolvedCookieStorageDirectory()
{
    if (!m_resolvedCookieStorageDirectory) {
        if (!isPersistent())
            m_resolvedCookieStorageDirectory = emptyString();
        else {
            auto directory = cacheDirectoryInContainerOrHomeDirectory("/Library/Cookies"_si);
            m_resolvedCookieStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directory);
        }
    }

    return m_resolvedCookieStorageDirectory;
}

RefPtr<StringImpl> WebsiteDataStore::resolvedContainerCachesNetworkingDirectory()
{
    if (!m_resolvedContainerCachesNetworkingDirectory) {
        if (!isPersistent())
            m_resolvedContainerCachesNetworkingDirectory = emptyString();
        else {
            auto directory = cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.Networking/"_si);
            m_resolvedContainerCachesNetworkingDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directory);
        }
    }

    return m_resolvedContainerCachesNetworkingDirectory;
}

RefPtr<StringImpl> WebsiteDataStore::resolvedContainerTemporaryDirectory()
{
    if (!m_resolvedContainerTemporaryDirectory)
        m_resolvedContainerTemporaryDirectory = defaultResolvedContainerTemporaryDirectory();

    return m_resolvedContainerTemporaryDirectory;
}

RefPtr<StringImpl> WebsiteDataStore::defaultResolvedContainerTemporaryDirectory()
{
    static NeverDestroyed<RefPtr<StringImpl>> resolvedTemporaryDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(StringImpl::create(protect(NSTemporaryDirectory()).get()).get());
    return resolvedTemporaryDirectory;
}

void WebsiteDataStore::setBackupExclusionPeriodForTesting(Seconds period, CompletionHandler<void()>&& completionHandler)
{
    protect(networkProcess())->setBackupExclusionPeriodForTesting(m_sessionID, period, WTF::move(completionHandler));
}

#endif

void WebsiteDataStore::saveRecentSearches(StringImpl* name, const Vector<WebCore::RecentSearch>& searchItems)
{
    m_queue->dispatch([name = isolatedCopy(name), searchItems = crossThreadCopy(searchItems), directory = isolatedCopy(resolvedDirectories().searchFieldHistoryDirectory)] {
        WebCore::saveRecentSearchesToFile(name, searchItems, directory);
    });
}

void WebsiteDataStore::loadRecentSearches(StringImpl* name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    m_queue->dispatch([name = isolatedCopy(name), completionHandler = WTF::move(completionHandler), directory = isolatedCopy(resolvedDirectories().searchFieldHistoryDirectory)]() mutable {
        auto result = WebCore::loadRecentSearchesFromFile(name, directory);
        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), result = crossThreadCopy(result)]() mutable {
            completionHandler(WTF::move(result));
        });
    });
}

void WebsiteDataStore::removeRecentSearches(WallTime oldestTimeToRemove, CompletionHandler<void()>&& completionHandler)
{
    m_queue->dispatch([time = oldestTimeToRemove.isolatedCopy(), directory = isolatedCopy(resolvedDirectories().searchFieldHistoryDirectory), completionHandler = WTF::move(completionHandler)]() mutable {
        WebCore::removeRecentlyModifiedRecentSearchesFromFile(time, directory);
        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    });
}

HashSet<WebCore::RegistrableDomain> WebsiteDataStore::platformAdditionalDomainsWithUserInteraction() const
{
    RetainPtr<id> arrayOrCommaDelimitedString;
    if (!isEmpty(m_configuration->additionalDomainsWithUserInteractionForTesting()))
        arrayOrCommaDelimitedString = createNSString(m_configuration->additionalDomainsWithUserInteractionForTesting());
    else
        arrayOrCommaDelimitedString = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitDebugAdditionalDomainsWithUserInteraction"];

    if (!arrayOrCommaDelimitedString)
        return { };

    HashSet<WebCore::RegistrableDomain> result { };
    auto addHost = [&result](RefPtr<StringImpl> host) {
        WebCore::SecurityOriginData origin { "https"_si, host, std::nullopt };
        WebCore::RegistrableDomain domain { origin };
        if (!domain.isEmpty())
            result.add(domain);
    };

    if ([arrayOrCommaDelimitedString isKindOfClass:[NSArray class]]) {
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL id host in arrayOrCommaDelimitedString.get()) {
            if ([host isKindOfClass:[NSString class]])
                addHost(StringImpl::create((NSString *)host));
        }
    } else if ([arrayOrCommaDelimitedString isKindOfClass:[NSString class]]) {
        RefPtr<StringImpl> commaDelimitedString = StringImpl::create((NSString *)arrayOrCommaDelimitedString);
        for (RefPtr<StringImpl> host : split(commaDelimitedString, ","_s))
            addHost(trim(host, isASCIIWhitespace<char16_t>));
    }

    return result;
}

EnhancedSecuritySitesHolder& WebsiteDataStore::enhancedSecuritySitesHolder()
{
    ASSERT(isPersistent());

    if (!m_enhancedSecuritySites)
        lazyInitialize(m_enhancedSecuritySites, EnhancedSecuritySitesHolder::create(resolvedDirectories().enhancedSecurityDirectory));

    return *m_enhancedSecuritySites;
}

void WebsiteDataStore::trackEnhancedSecurityForDomain(WebCore::RegistrableDomain&& domain, EnhancedSecurity reason)
{
    if (!isPersistent())
        return;

    enhancedSecuritySitesHolder().trackEnhancedSecurityForDomain(WTF::move(domain), reason);
}

void WebsiteDataStore::fetchEnhancedSecurityOnlyDomains(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    if (!isPersistent())
        return completionHandler({ });

    enhancedSecuritySitesHolder().fetchEnhancedSecurityOnlyDomains(WTF::move(completionHandler));
}

void WebsiteDataStore::fetchAllEnhancedSecuritySites(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    if (!isPersistent())
        return completionHandler({ });

    enhancedSecuritySitesHolder().fetchAllEnhancedSecuritySites(WTF::move(completionHandler));
}

void WebsiteDataStore::removeEnhancedSecuritySites(const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    if (!isPersistent())
        return completionHandler();

    auto sites = origins.map([](auto& origin) {
        return WebCore::RegistrableDomain { origin };
    });

    enhancedSecuritySitesHolder().deleteSites(WTF::move(sites), WTF::move(completionHandler));
}

void WebsiteDataStore::removeAllEnhancedSecuritySites(CompletionHandler<void()>&& completionHandler)
{
    if (!isPersistent())
        return completionHandler();

    enhancedSecuritySitesHolder().deleteAllSites(WTF::move(completionHandler));
}

}
