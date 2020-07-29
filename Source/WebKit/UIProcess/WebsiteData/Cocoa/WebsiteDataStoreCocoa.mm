/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#import "SandboxUtilities.h"
#import "StorageManager.h"
#import "WebFramePolicyListenerProxy.h"
#import "WebPreferencesKeys.h"
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataStoreParameters.h"
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/text/StringBuilder.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebsiteDataStoreAdditions.h>
#else
#define WEBSITE_DATA_STORE_ADDITIONS
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

static NSString * const WebKitNetworkLoadThrottleLatencyMillisecondsDefaultsKey = @"WebKitNetworkLoadThrottleLatencyMilliseconds";

static WorkQueue& appBoundDomainQueue()
{
    static auto& queue = WorkQueue::create("com.apple.WebKit.AppBoundDomains", WorkQueue::Type::Serial).leakRef();
    return queue;
}

static std::atomic<bool> hasInitializedAppBoundDomains = false;
static std::atomic<bool> keyExists = false;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
WebCore::ThirdPartyCookieBlockingMode WebsiteDataStore::thirdPartyCookieBlockingMode() const
{
    if (!m_thirdPartyCookieBlockingMode) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        if ([defaults boolForKey:[NSString stringWithFormat:@"Experimental%@", WebPreferencesKey::isThirdPartyCookieBlockingDisabledKey().createCFString().get()]])
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
    bool enableResourceLoadStatisticsDebugMode = false;
    auto sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::No;
    auto firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource { };
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    enableResourceLoadStatisticsDebugMode = [defaults boolForKey:@"ITPDebugMode"];

    if ([defaults boolForKey:[NSString stringWithFormat:@"Experimental%@", WebPreferencesKey::isSameSiteStrictEnforcementEnabledKey().createCFString().get()]])
        sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::Yes;

    if ([defaults boolForKey:[NSString stringWithFormat:@"Experimental%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalDisabledKey().createCFString().get()]])
        firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::None;
    else {
        if ([defaults boolForKey:[NSString stringWithFormat:@"InternalDebug%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalReproTestingEnabledKey().createCFString().get()]])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout;
        else if ([defaults boolForKey:[NSString stringWithFormat:@"InternalDebug%@", WebPreferencesKey::isFirstPartyWebsiteDataRemovalLiveOnTestingEnabledKey().createCFString().get()]])
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout;
        else
            firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    }
    m_isInAppBrowserPrivacyTestModeEnabled = [defaults boolForKey:[NSString stringWithFormat:@"WebKitDebug%@", WebPreferencesKey::isInAppBrowserPrivacyEnabledKey().createCFString().get()]];

    auto* manualPrevalentResource = [defaults stringForKey:@"ITPManualPrevalentResource"];
    if (manualPrevalentResource) {
        URL url { URL(), manualPrevalentResource };
        if (!url.isValid()) {
            StringBuilder builder;
            builder.appendLiteral("http://");
            builder.append(manualPrevalentResource);
            url = { URL(), builder.toString() };
        }
        if (url.isValid())
            resourceLoadStatisticsManualPrevalentResource = WebCore::RegistrableDomain { url };
    }
#if !RELEASE_LOG_DISABLED
    static NSString * const WebKitLogCookieInformationDefaultsKey = @"WebKitLogCookieInformation";
    shouldLogCookieInformation = [defaults boolForKey:WebKitLogCookieInformationDefaultsKey];
#endif
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

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
        httpProxy = URL(URL(), [defaults stringForKey:(NSString *)WebKit2HTTPProxyDefaultsKey]);
    if (!httpsProxy.isValid() && (isSafari || isMiniBrowser))
        httpsProxy = URL(URL(), [defaults stringForKey:(NSString *)WebKit2HTTPSProxyDefaultsKey]);

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    bool http3Enabled = WebsiteDataStore::http3Enabled();
    String alternativeServiceStorageDirectory;
    SandboxExtension::Handle alternativeServiceStorageDirectoryExtensionHandle;
    if (http3Enabled) {
        alternativeServiceStorageDirectory = resolvedAlternativeServicesStorageDirectory();
        if (!alternativeServiceStorageDirectory.isEmpty())
            SandboxExtension::createHandleForReadWriteDirectory(alternativeServiceStorageDirectory, alternativeServiceStorageDirectoryExtensionHandle);
    }
#endif

    bool shouldIncludeLocalhostInResourceLoadStatistics = isSafari;
    
    parameters.networkSessionParameters.proxyConfiguration = configuration().proxyConfiguration();
    parameters.networkSessionParameters.sourceApplicationBundleIdentifier = configuration().sourceApplicationBundleIdentifier();
    parameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = configuration().sourceApplicationSecondaryIdentifier();
    parameters.networkSessionParameters.shouldLogCookieInformation = shouldLogCookieInformation;
    parameters.networkSessionParameters.loadThrottleLatency = Seconds { [defaults integerForKey:WebKitNetworkLoadThrottleLatencyMillisecondsDefaultsKey] / 1000. };
    parameters.networkSessionParameters.httpProxy = WTFMove(httpProxy);
    parameters.networkSessionParameters.httpsProxy = WTFMove(httpsProxy);
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    parameters.networkSessionParameters.alternativeServiceDirectory = WTFMove(alternativeServiceStorageDirectory);
    parameters.networkSessionParameters.alternativeServiceDirectoryExtensionHandle = WTFMove(alternativeServiceStorageDirectoryExtensionHandle);
    parameters.networkSessionParameters.http3Enabled = WTFMove(http3Enabled);
#endif
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost = shouldIncludeLocalhostInResourceLoadStatistics;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.enableDebugMode = enableResourceLoadStatisticsDebugMode;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled = sameSiteStrictEnforcementEnabled;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode = firstPartyWebsiteDataRemovalMode;
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.standaloneApplicationDomain = WebCore::RegistrableDomain { m_configuration->standaloneApplicationURL() };
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.manualPrevalentResource = WTFMove(resourceLoadStatisticsManualPrevalentResource);

    auto cookieFile = resolvedCookieStorageFile();

    if (m_uiProcessCookieStorageIdentifier.isEmpty()) {
        auto utf8File = cookieFile.utf8();
        auto url = adoptCF(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)utf8File.data(), (CFIndex)utf8File.length(), true));
        m_cfCookieStorage = adoptCF(CFHTTPCookieStorageCreateFromFile(kCFAllocatorDefault, url.get(), nullptr));
        m_uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage(m_cfCookieStorage.get());
    }

    parameters.uiProcessCookieStorageIdentifier = m_uiProcessCookieStorageIdentifier;

    if (!cookieFile.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(FileSystem::directoryName(cookieFile), parameters.cookieStoragePathExtensionHandle);
}

bool WebsiteDataStore::http3Enabled()
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
#if PLATFORM(MAC)
    NSString *format = @"Experimental%@";
#else
    NSString *format = @"WebKitExperimental%@";
#endif
    return [[NSUserDefaults standardUserDefaults] boolForKey:[NSString stringWithFormat:format, (NSString *)WebPreferencesKey::http3EnabledKey()]];
#else
    return false;
#endif
}

void WebsiteDataStore::platformInitialize()
{
    ASSERT(!dataStores().contains(this));
    dataStores().add(this);

    initializeAppBoundDomains();
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

NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";
NSString *WebStorageDirectoryDefaultsKey = @"WebKitLocalStorageDatabasePathPreferenceKey";
NSString *WebKitMediaCacheDirectoryDefaultsKey = @"WebKitMediaCacheDirectory";
NSString *WebKitMediaKeysStorageDirectoryDefaultsKey = @"WebKitMediaKeysStorageDirectory";

WTF::String WebsiteDataStore::defaultApplicationCacheDirectory()
{
#if PLATFORM(IOS_FAMILY)
    // This quirk used to make these apps share application cache storage, but doesn't accomplish that any more.
    // Preserving it avoids the need to migrate data when upgrading.
    // FIXME: Ideally we should just have Safari, WebApp, and webbookmarksd create a data store with
    // this application cache path.
    if (WebCore::IOSApplication::isMobileSafari() || WebCore::IOSApplication::isWebBookmarksD()) {
        NSString *cachePath = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches/com.apple.WebAppCache"];

        return WebKit::stringByResolvingSymlinksInPath(cachePath.stringByStandardizingPath);
    }
#endif

    return cacheDirectoryFileSystemRepresentation("OfflineWebApplicationCache");
}

WTF::String WebsiteDataStore::defaultCacheStorageDirectory()
{
    return cacheDirectoryFileSystemRepresentation("CacheStorage");
}

WTF::String WebsiteDataStore::defaultNetworkCacheDirectory()
{
    return cacheDirectoryFileSystemRepresentation("NetworkCache");
}

WTF::String WebsiteDataStore::defaultAlternativeServicesDirectory()
{
    return cacheDirectoryFileSystemRepresentation("AlternativeServices", ShouldCreateDirectory::No);
}

WTF::String WebsiteDataStore::defaultMediaCacheDirectory()
{
    return tempDirectoryFileSystemRepresentation("MediaCache");
}

WTF::String WebsiteDataStore::defaultIndexedDBDatabaseDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation("IndexedDB");
}

WTF::String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory()
{
    return cacheDirectoryFileSystemRepresentation("ServiceWorkers");
}

WTF::String WebsiteDataStore::defaultLocalStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation("LocalStorage");
}

WTF::String WebsiteDataStore::defaultMediaKeysStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation("MediaKeys");
}

WTF::String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation("WebSQL");
}

WTF::String WebsiteDataStore::defaultResourceLoadStatisticsDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation("ResourceLoadStatistics");
}

WTF::String WebsiteDataStore::defaultJavaScriptConfigurationDirectory()
{
    return tempDirectoryFileSystemRepresentation("JavaScriptCoreDebug", ShouldCreateDirectory::No);
}

WTF::String WebsiteDataStore::tempDirectoryFileSystemRepresentation(const WTF::String& directoryName, ShouldCreateDirectory shouldCreateDirectory)
{
    static dispatch_once_t onceToken;
    static NSURL *tempURL;
    
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
        
        tempURL = [[url URLByAppendingPathComponent:@"WebKit" isDirectory:YES] retain];
    });
    
    NSURL *url = [tempURL URLByAppendingPathComponent:directoryName isDirectory:YES];

    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && (![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr]))
        LOG_ERROR("Failed to create directory %@", url);
    
    return url.absoluteURL.path.fileSystemRepresentation;
}

WTF::String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const WTF::String& directoryName, ShouldCreateDirectory shouldCreateDirectory)
{
    static dispatch_once_t onceToken;
    static NSURL *cacheURL;

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

        cacheURL = [[url URLByAppendingPathComponent:@"WebKit" isDirectory:YES] retain];
    });

    NSURL *url = [cacheURL URLByAppendingPathComponent:directoryName isDirectory:YES];
    if (shouldCreateDirectory == ShouldCreateDirectory::Yes
        && ![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr])
        LOG_ERROR("Failed to create directory %@", url);

    return url.absoluteURL.path.fileSystemRepresentation;
}

WTF::String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const WTF::String& directoryName)
{
    static dispatch_once_t onceToken;
    static NSURL *websiteDataURL;

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

        websiteDataURL = [[url URLByAppendingPathComponent:@"WebsiteData" isDirectory:YES] retain];
    });

    NSURL *url = [websiteDataURL URLByAppendingPathComponent:directoryName isDirectory:YES];
    if (![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr])
        LOG_ERROR("Failed to create directory %@", url);

    return url.absoluteURL.path.fileSystemRepresentation;
}

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

                URL url { URL(), data };
                if (url.protocol().isEmpty())
                    url.setProtocol("https");
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

void WebsiteDataStore::ensureAppBoundDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&, const HashSet<String>&)>&& completionHandler) const
{
    if (hasInitializedAppBoundDomains) {
        if (m_isInAppBrowserPrivacyTestModeEnabled) {
            WEBSITE_DATA_STORE_ADDITIONS;
        }
        completionHandler(appBoundDomains(), appBoundSchemes());
        return;
    }

    // Hopping to the background thread then back to the main thread
    // ensures that initializeAppBoundDomains() has finished.
    appBoundDomainQueue().dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] () mutable {
        RunLoop::main().dispatch([this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(hasInitializedAppBoundDomains);
            if (m_isInAppBrowserPrivacyTestModeEnabled) {
                WEBSITE_DATA_STORE_ADDITIONS;
            }
            completionHandler(appBoundDomains(), appBoundSchemes());
        });
    });
}

static NavigatingToAppBoundDomain schemeOrDomainIsAppBound(const URL& requestURL, const HashSet<WebCore::RegistrableDomain>& domains, const HashSet<String>& schemes)
{
    auto protocol = requestURL.protocol().toString();
    auto schemeIsAppBound = !protocol.isNull() && schemes.contains(protocol);
    auto domainIsAppBound = domains.contains(WebCore::RegistrableDomain(requestURL));
    return schemeIsAppBound || domainIsAppBound ? NavigatingToAppBoundDomain::Yes : NavigatingToAppBoundDomain::No;
}

void WebsiteDataStore::beginAppBoundDomainCheck(const URL& requestURL, WebFramePolicyListenerProxy& listener)
{
    ASSERT(RunLoop::isMain());

    ensureAppBoundDomains([&requestURL, listener = makeRef(listener)] (auto& domains, auto& schemes) mutable {
        // Must check for both an empty app bound domains list and an empty key before returning nullopt
        // because test cases may have app bound domains but no key.
        bool hasAppBoundDomains = keyExists || !domains.isEmpty();
        if (!hasAppBoundDomains) {
            listener->didReceiveAppBoundDomainResult(WTF::nullopt);
            return;
        }
        listener->didReceiveAppBoundDomainResult(schemeOrDomainIsAppBound(requestURL, domains, schemes));
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

Optional<HashSet<WebCore::RegistrableDomain>> WebsiteDataStore::appBoundDomainsIfInitialized()
{
    ASSERT(RunLoop::isMain());
    if (!hasInitializedAppBoundDomains)
        return WTF::nullopt;
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

}
