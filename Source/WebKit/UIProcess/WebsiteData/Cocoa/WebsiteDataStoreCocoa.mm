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
#import "NetworkProcessProxy.h"
#import "SandboxUtilities.h"
#import "WebFramePolicyListenerProxy.h"
#import "WebPreferencesDefaultValues.h"
#import "WebPreferencesKeys.h"
#import "WebProcessProxy.h"
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataStoreParameters.h"
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/cf/StringConcatenateCF.h>

#if ENABLE(GPU_PROCESS)
#import "GPUProcessProxy.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIApplication.h>
#import <pal/ios/ManagedConfigurationSoftLink.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#endif

namespace WebKit {

static HashSet<WebsiteDataStore*>& dataStores()
{
    static NeverDestroyed<HashSet<WebsiteDataStore*>> dataStores;

    return dataStores;
}

#if ENABLE(APP_BOUND_DOMAINS)
static WorkQueue& appBoundDomainQueue()
{
    static auto& queue = WorkQueue::create("com.apple.WebKit.AppBoundDomains").leakRef();
    return queue;
}
static std::atomic<bool> hasInitializedAppBoundDomains = false;
static std::atomic<bool> keyExists = false;
#endif

// FIXME: we should not read the values from NSUserDefaults; we should let clients who set the values to pass them via configuration.
static bool internalFeatureEnabled(const String& key, bool defaultValue = false)
{
    auto defaultsKey = adoptNS([[NSString alloc] initWithFormat:@"InternalDebug%@", static_cast<NSString *>(key)]);
    if ([[NSUserDefaults standardUserDefaults] objectForKey:defaultsKey.get()] != nil)
        return [[NSUserDefaults standardUserDefaults] boolForKey:defaultsKey.get()];

    return defaultValue;
}

static bool experimentalFeatureEnabled(const String& key, bool defaultValue = false)
{
    auto defaultsKey = adoptNS([[NSString alloc] initWithFormat:@"WebKitExperimental%@", static_cast<NSString *>(key)]);
    if ([[NSUserDefaults standardUserDefaults] objectForKey:defaultsKey.get()] != nil)
        return [[NSUserDefaults standardUserDefaults] boolForKey:defaultsKey.get()];

    return defaultValue;
}

#if ENABLE(TRACKING_PREVENTION)
WebCore::ThirdPartyCookieBlockingMode WebsiteDataStore::thirdPartyCookieBlockingMode() const
{
    if (!m_thirdPartyCookieBlockingMode) {
        if (experimentalFeatureEnabled(WebPreferencesKey::isThirdPartyCookieBlockingDisabledKey()))
            m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction;
        else
            m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
    }
    return *m_thirdPartyCookieBlockingMode;
}
#endif

void WebsiteDataStore::platformSetNetworkParameters(WebsiteDataStoreParameters& parameters)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    bool shouldLogCookieInformation = false;
    auto sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::No;
    auto firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource { };
#if ENABLE(TRACKING_PREVENTION)
    if (experimentalFeatureEnabled(WebPreferencesKey::isSameSiteStrictEnforcementEnabledKey()))
        sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::Yes;

    if (experimentalFeatureEnabled(WebPreferencesKey::isFirstPartyWebsiteDataRemovalDisabledKey()))
        firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::None;
    else {
        if ([defaults boolForKey:[NSString stringWithFormat:@"InternalDebug%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalReproTestingEnabledKey().createCFString().get()]])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout;
        else if ([defaults boolForKey:[NSString stringWithFormat:@"InternalDebug%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalLiveOnTestingEnabledKey().createCFString().get()]])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout;
        else
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    }

    if (auto manualPrevalentResource = [defaults stringForKey:@"ITPManualPrevalentResource"]) {
        URL url { { }, manualPrevalentResource };
        if (!url.isValid())
            url = { { }, makeString("http://", manualPrevalentResource) };
        if (url.isValid())
            resourceLoadStatisticsManualPrevalentResource = WebCore::RegistrableDomain { url };
    }
#if !RELEASE_LOG_DISABLED
    static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
    shouldLogCookieInformation = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif
#endif // ENABLE(TRACKING_PREVENTION)

    URL httpProxy = m_configuration->httpProxy();
    URL httpsProxy = m_configuration->httpsProxy();
    
    bool isSafari = false;
    bool isMiniBrowser = false;
#if PLATFORM(IOS_FAMILY)
    isSafari = WebCore::IOSApplication::isMobileSafari();
    isMiniBrowser = WebCore::IOSApplication::isMiniBrowser();
#elif PLATFORM(MAC)
    isSafari = WebCore::MacApplication::isSafari();
    isMiniBrowser = WebCore::MacApplication::isMiniBrowser();
#endif
    // FIXME: Remove these once Safari adopts _WKWebsiteDataStoreConfiguration.httpProxy and .httpsProxy.
    if (!httpProxy.isValid() && (isSafari || isMiniBrowser))
        httpProxy = URL { [defaults stringForKey:(NSString *)WebKit2HTTPProxyDefaultsKey] };
    if (!httpsProxy.isValid() && (isSafari || isMiniBrowser))
        httpsProxy = URL { [defaults stringForKey:(NSString *)WebKit2HTTPSProxyDefaultsKey] };

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    SandboxExtension::Handle alternativeServiceStorageDirectoryExtensionHandle;
    String alternativeServiceStorageDirectory = resolvedAlternativeServicesStorageDirectory();
    createHandleFromResolvedPathIfPossible(alternativeServiceStorageDirectory, alternativeServiceStorageDirectoryExtensionHandle);
#endif

    bool shouldIncludeLocalhostInResourceLoadStatistics = isSafari;
    
    parameters.networkSessionParameters.proxyConfiguration = configuration().proxyConfiguration();
    parameters.networkSessionParameters.sourceApplicationBundleIdentifier = configuration().sourceApplicationBundleIdentifier();
    parameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = configuration().sourceApplicationSecondaryIdentifier();
    parameters.networkSessionParameters.shouldLogCookieInformation = shouldLogCookieInformation;
    parameters.networkSessionParameters.httpProxy = WTFMove(httpProxy);
    parameters.networkSessionParameters.httpsProxy = WTFMove(httpsProxy);
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    parameters.networkSessionParameters.alternativeServiceDirectory = WTFMove(alternativeServiceStorageDirectory);
    parameters.networkSessionParameters.alternativeServiceDirectoryExtensionHandle = WTFMove(alternativeServiceStorageDirectoryExtensionHandle);
#endif
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost = shouldIncludeLocalhostInResourceLoadStatistics;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled = sameSiteStrictEnforcementEnabled;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode = firstPartyWebsiteDataRemovalMode;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.standaloneApplicationDomain = WebCore::RegistrableDomain { m_configuration->standaloneApplicationURL() };
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.manualPrevalentResource = WTFMove(resourceLoadStatisticsManualPrevalentResource);

    auto cookieFile = resolvedCookieStorageFile();
    createHandleFromResolvedPathIfPossible(FileSystem::parentPath(cookieFile), parameters.cookieStoragePathExtensionHandle);

    if (m_uiProcessCookieStorageIdentifier.isEmpty()) {
        auto utf8File = cookieFile.utf8();
        auto url = adoptCF(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)utf8File.data(), (CFIndex)utf8File.length(), true));
        m_cfCookieStorage = adoptCF(CFHTTPCookieStorageCreateFromFile(kCFAllocatorDefault, url.get(), nullptr));
        m_uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage(m_cfCookieStorage.get());
    }

    parameters.uiProcessCookieStorageIdentifier = m_uiProcessCookieStorageIdentifier;
    parameters.networkSessionParameters.enablePrivateClickMeasurementDebugMode = experimentalFeatureEnabled(WebPreferencesKey::privateClickMeasurementDebugModeEnabledKey());
}

bool WebsiteDataStore::useNetworkLoader()
{
#if HAVE(NETWORK_LOADER)
    return experimentalFeatureEnabled(WebPreferencesKey::cFNetworkNetworkLoaderEnabledKey());
#else
    return false;
#endif
}

void WebsiteDataStore::platformInitialize()
{
    ASSERT(!dataStores().contains(this));
    dataStores().add(this);
#if ENABLE(APP_BOUND_DOMAINS)
    initializeAppBoundDomains();
#endif
}

void WebsiteDataStore::platformDestroy()
{
    ASSERT(dataStores().contains(this));
    dataStores().remove(this);
}

void WebsiteDataStore::platformRemoveRecentSearches(WallTime oldestTimeToRemove)
{
    WebCore::removeRecentlyModifiedRecentSearches(oldestTimeToRemove);
}

static String defaultWebsiteDataStoreRootDirectory()
{
    static dispatch_once_t onceToken;
    static NeverDestroyed<RetainPtr<NSURL>> websiteDataStoreDirectory;
    dispatch_once(&onceToken, ^{
        NSURL *libraryDirectory = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        RELEASE_ASSERT(libraryDirectory);
        NSURL *webkitDirectory = [libraryDirectory URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
        if (!WebKit::processHasContainer()) {
            NSString *applicationIdentifier = [NSBundle mainBundle].bundleIdentifier;
            if (!applicationIdentifier)
                applicationIdentifier = [NSProcessInfo processInfo].processName;
            webkitDirectory = [webkitDirectory URLByAppendingPathComponent:applicationIdentifier isDirectory:YES];
        }

        websiteDataStoreDirectory.get() = [webkitDirectory URLByAppendingPathComponent:@"WebsiteDataStore" isDirectory:YES];
    });

    return websiteDataStoreDirectory.get().get().absoluteURL.path;
}

void WebsiteDataStore::fetchAllDataStoreIdentifiers(CompletionHandler<void(Vector<UUID>&&)>&& completionHandler)
{
    Vector<UUID> identifiers;
    for (auto identifierString : FileSystem::listDirectory(defaultWebsiteDataStoreRootDirectory())) {
        if (auto identifier = UUID::parse(identifierString))
            identifiers.append(*identifier);
    }

    completionHandler(WTFMove(identifiers));
}

void WebsiteDataStore::removeDataStoreWithIdentifier(const UUID& identifier, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!identifier)
        return completionHandler("Identifier is invalid"_s);

    if (!FileSystem::deleteNonEmptyDirectory(defaultWebsiteDataStoreDirectory(identifier)))
        return completionHandler("WebsiteDataStore with this identifier does not exist or deletion failed"_s);

    return completionHandler({ });
}

String WebsiteDataStore::defaultWebsiteDataStoreDirectory(const UUID& identifier)
{
    return FileSystem::pathByAppendingComponent(defaultWebsiteDataStoreRootDirectory(), identifier.toString());
}

String WebsiteDataStore::defaultCookieStorageFile(const String& baseDirectory)
{
    if (baseDirectory.isEmpty())
        return { };

    return FileSystem::pathByAppendingComponents(baseDirectory, { "Cookies"_s, "Cookies.binarycookies"_s });
}

String WebsiteDataStore::defaultApplicationCacheDirectory(const String& baseDirectory)
{
    if (!baseDirectory.isEmpty())
        return FileSystem::pathByAppendingComponent(baseDirectory, "ApplicationCache"_s);

#if PLATFORM(IOS_FAMILY)
    // This quirk used to make these apps share application cache storage, but doesn't accomplish that any more.
    // Preserving it avoids the need to migrate data when upgrading.
    // FIXME: Ideally we should just have Safari, WebApp, and webbookmarksd create a data store with
    // this application cache path.
    if (WebCore::IOSApplication::isMobileSafari() || WebCore::IOSApplication::isWebBookmarksD()) {
        NSString *cachePath = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches/com.apple.WebAppCache"];

        return WebKit::stringByResolvingSymlinksInPath(String { cachePath.stringByStandardizingPath });
    }
#endif

    return cacheDirectoryFileSystemRepresentation("OfflineWebApplicationCache"_s, { }, ShouldCreateDirectory::No);
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
        auto oldDirectory = cacheDirectoryFileSystemRepresentation("Storage"_s, { }, ShouldCreateDirectory::No);
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSArray *files = [fileManager contentsOfDirectoryAtPath:oldDirectory error:0];
        if (files) {
            for (NSString *fileName in files) {
                if (![fileName length])
                    continue;

                NSString *path = [directory stringByAppendingPathComponent:fileName];
                NSString *oldPath = [oldDirectory stringByAppendingPathComponent:fileName];
                [fileManager moveItemAtPath:oldPath toPath:path error:nil];
            }
        }
        [fileManager removeItemAtPath:oldDirectory error:nil];
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

String WebsiteDataStore::tempDirectoryFileSystemRepresentation(const String& directoryName, ShouldCreateDirectory shouldCreateDirectory)
{
    static dispatch_once_t onceToken;
    static NeverDestroyed<RetainPtr<NSURL>> tempURL;
    
    dispatch_once(&onceToken, ^{
        NSURL *url = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();
        
        if (!WebKit::processHasContainer()) {
            NSString *bundleIdentifier = [NSBundle mainBundle].bundleIdentifier;
            if (!bundleIdentifier)
                bundleIdentifier = [NSProcessInfo processInfo].processName;
            url = [url URLByAppendingPathComponent:bundleIdentifier isDirectory:YES];
        }
        
        tempURL.get() = [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
    });
    
    NSURL *url = [tempURL.get() URLByAppendingPathComponent:directoryName isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && (![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr]))
        LOG_ERROR("Failed to create directory %@", url);
    
    return url.absoluteURL.path;
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName, const String&, ShouldCreateDirectory shouldCreateDirectory)
{
    static dispatch_once_t onceToken;
    static NeverDestroyed<RetainPtr<NSURL>> cacheURL;

    dispatch_once(&onceToken, ^{
        NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSCachesDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();

        if (!WebKit::processHasContainer()) {
            NSString *bundleIdentifier = [NSBundle mainBundle].bundleIdentifier;
            if (!bundleIdentifier)
                bundleIdentifier = [NSProcessInfo processInfo].processName;
            url = [url URLByAppendingPathComponent:bundleIdentifier isDirectory:YES];
        }

        cacheURL.get() = [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];
    });

    NSURL *url = [cacheURL.get() URLByAppendingPathComponent:directoryName isDirectory:YES];
    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && ![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr])
        LOG_ERROR("Failed to create directory %@", url);

    return url.absoluteURL.path;
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName, const String&, ShouldCreateDirectory shouldCreateDirectory)
{
    static dispatch_once_t onceToken;
    static NeverDestroyed<RetainPtr<NSURL>> websiteDataURL;

    dispatch_once(&onceToken, ^{
        NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nullptr create:NO error:nullptr];
        if (!url)
            RELEASE_ASSERT_NOT_REACHED();

        url = [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];

        if (!WebKit::processHasContainer()) {
            NSString *bundleIdentifier = [NSBundle mainBundle].bundleIdentifier;
            if (!bundleIdentifier)
                bundleIdentifier = [NSProcessInfo processInfo].processName;
            url = [url URLByAppendingPathComponent:bundleIdentifier isDirectory:YES];
        }

        websiteDataURL.get() = [url URLByAppendingPathComponent:@"WebsiteData" isDirectory:YES];
    });

    NSURL *url = [websiteDataURL.get() URLByAppendingPathComponent:directoryName isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes) {
        if (![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr])
            LOG_ERROR("Failed to create directory %@", url);
    }

    return url.absoluteURL.path;
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
        keyExists = appBoundData ? true : false;
        
        RunLoop::main().dispatch([forceReinitialization, appBoundData = retainPtr(appBoundData)] {
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
        for (auto& domain : appBoundDomainsForTesting(WebCore::applicationBundleIdentifier()))
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
        RunLoop::main().dispatch([this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
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

bool WebsiteDataStore::networkProcessHasEntitlementForTesting(const String& entitlement)
{
    return WTF::hasEntitlement(networkProcess().connection()->xpcConnection(), entitlement);
}

bool WebsiteDataStore::defaultShouldUseCustomStoragePaths()
{
    return !internalFeatureEnabled(WebPreferencesKey::useGeneralDirectoryForStorageKey(), true);
}

#if PLATFORM(IOS_FAMILY)

String WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory(const String& subpath)
{
    String path = pathForProcessContainer();
    if (path.isEmpty())
        path = NSHomeDirectory();

    return path + subpath;
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

String WebsiteDataStore::resolvedContainerCachesWebContentDirectory()
{
    if (m_resolvedContainerCachesWebContentDirectory.isNull()) {
        if (!isPersistent())
            m_resolvedContainerCachesWebContentDirectory = emptyString();
        else {
            auto directory = cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.WebContent/"_s);
            m_resolvedContainerCachesWebContentDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directory);
        }
    }

    return m_resolvedContainerCachesWebContentDirectory;
}

String WebsiteDataStore::resolvedContainerTemporaryDirectory()
{
    if (m_resolvedContainerTemporaryDirectory.isNull())
        m_resolvedContainerTemporaryDirectory = defaultResolvedContainerTemporaryDirectory();

    return m_resolvedContainerTemporaryDirectory;
}

String WebsiteDataStore::defaultResolvedContainerTemporaryDirectory()
{
    static NeverDestroyed<String> resolvedTemporaryDirectory;
    static std::once_flag once;
    std::call_once(once, [] {
        resolvedTemporaryDirectory.get() = resolveAndCreateReadWriteDirectoryForSandboxExtension(String(NSTemporaryDirectory()));
    });
    return resolvedTemporaryDirectory;
}

void WebsiteDataStore::setBackupExclusionPeriodForTesting(Seconds period, CompletionHandler<void()>&& completionHandler)
{
    networkProcess().setBackupExclusionPeriodForTesting(m_sessionID, period, WTFMove(completionHandler));
}

#endif

}
