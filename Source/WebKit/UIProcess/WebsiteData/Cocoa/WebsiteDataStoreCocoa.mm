/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataStoreParameters.h"
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

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIApplication.h>
#endif

namespace WebKit {

static id terminationObserver;

static HashSet<WebsiteDataStore*>& dataStores()
{
    static NeverDestroyed<HashSet<WebsiteDataStore*>> dataStores;

    return dataStores;
}

static NSString * const WebKitNetworkLoadThrottleLatencyMillisecondsDefaultsKey = @"WebKitNetworkLoadThrottleLatencyMilliseconds";

WebsiteDataStoreParameters WebsiteDataStore::parameters()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    resolveDirectoriesIfNecessary();

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    bool shouldLogCookieInformation = false;
    bool enableResourceLoadStatisticsDebugMode = false;
    bool enableResourceLoadStatisticsNSURLSessionSwitching = WebCore::RuntimeEnabledFeatures::sharedFeatures().isITPSessionSwitchingEnabled();
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource { };
    bool enableLegacyTLS = [defaults boolForKey:@"WebKitEnableLegacyTLS"];
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    enableResourceLoadStatisticsDebugMode = [defaults boolForKey:@"ITPDebugMode"];
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
#if PLATFORM(IOS_FAMILY)
    isSafari = WebCore::IOSApplication::isMobileSafari();
#elif PLATFORM(MAC)
    isSafari = WebCore::MacApplication::isSafari();
#endif
    // FIXME: Remove these once Safari adopts _WKWebsiteDataStoreConfiguration.httpProxy and .httpsProxy.
    if (!httpProxy.isValid() && isSafari)
        httpProxy = URL(URL(), [defaults stringForKey:(NSString *)WebKit2HTTPProxyDefaultsKey]);
    if (!httpsProxy.isValid() && isSafari)
        httpsProxy = URL(URL(), [defaults stringForKey:(NSString *)WebKit2HTTPSProxyDefaultsKey]);

    auto resourceLoadStatisticsDirectory = m_configuration->resourceLoadStatisticsDirectory();
    SandboxExtension::Handle resourceLoadStatisticsDirectoryHandle;
    if (!resourceLoadStatisticsDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(resourceLoadStatisticsDirectory, resourceLoadStatisticsDirectoryHandle);

    auto networkCacheDirectory = resolvedNetworkCacheDirectory();
    SandboxExtension::Handle networkCacheDirectoryExtensionHandle;
    if (!networkCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(networkCacheDirectory, networkCacheDirectoryExtensionHandle);

    bool shouldIncludeLocalhostInResourceLoadStatistics = isSafari;
    WebsiteDataStoreParameters parameters;
    parameters.networkSessionParameters = {
        m_sessionID,
        boundInterfaceIdentifier(),
        allowsCellularAccess(),
        proxyConfiguration(),
        sourceApplicationBundleIdentifier(),
        sourceApplicationSecondaryIdentifier(),
        shouldLogCookieInformation,
        Seconds { [defaults integerForKey:WebKitNetworkLoadThrottleLatencyMillisecondsDefaultsKey] / 1000. },
        WTFMove(httpProxy),
        WTFMove(httpsProxy),
        enableLegacyTLS,
        WTFMove(resourceLoadStatisticsDirectory),
        WTFMove(resourceLoadStatisticsDirectoryHandle),
        resourceLoadStatisticsEnabled(),
        hasStatisticsTestingCallback(),
        shouldIncludeLocalhostInResourceLoadStatistics,
        enableResourceLoadStatisticsDebugMode,
        enableResourceLoadStatisticsNSURLSessionSwitching,
        m_configuration->deviceManagementRestrictionsEnabled(),
        m_configuration->allLoadsBlockedByDeviceManagementRestrictionsForTesting(),
        WTFMove(resourceLoadStatisticsManualPrevalentResource),
        WTFMove(networkCacheDirectory),
        WTFMove(networkCacheDirectoryExtensionHandle),
        m_configuration->dataConnectionServiceType(),
        m_configuration->fastServerTrustEvaluationEnabled(),
        m_configuration->networkCacheSpeculativeValidationEnabled(),
        m_configuration->testingSessionEnabled(),
        m_configuration->testSpeedMultiplier(),
        m_configuration->suppressesConnectionTerminationOnSystemChange(),
    };
    networkingHasBegun();

    auto cookieFile = resolvedCookieStorageFile();

    if (m_uiProcessCookieStorageIdentifier.isEmpty()) {
        auto utf8File = cookieFile.utf8();
        auto url = adoptCF(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)utf8File.data(), (CFIndex)utf8File.length(), true));
        m_cfCookieStorage = adoptCF(CFHTTPCookieStorageCreateFromFile(kCFAllocatorDefault, url.get(), nullptr));
        m_uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage(m_cfCookieStorage.get());
    }

    parameters.uiProcessCookieStorageIdentifier = m_uiProcessCookieStorageIdentifier;
    parameters.networkSessionParameters.sourceApplicationBundleIdentifier = sourceApplicationBundleIdentifier();
    parameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = sourceApplicationSecondaryIdentifier();

    parameters.pendingCookies = copyToVector(m_pendingCookies);

    if (!cookieFile.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(FileSystem::directoryName(cookieFile), parameters.cookieStoragePathExtensionHandle);

#if ENABLE(INDEXED_DATABASE)
    parameters.indexedDatabaseDirectory = resolvedIndexedDatabaseDirectory();
    if (!parameters.indexedDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    parameters.serviceWorkerRegistrationDirectory = resolvedServiceWorkerRegistrationDirectory();
    if (!parameters.serviceWorkerRegistrationDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
    parameters.serviceWorkerProcessTerminationDelayEnabled = m_configuration->serviceWorkerProcessTerminationDelayEnabled();
    parameters.serviceWorkerRegisteredSchemes = m_configuration->serviceWorkerRegisteredSchemes();
#endif

    parameters.localStorageDirectory = resolvedLocalStorageDirectory();
    if (!parameters.localStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.localStorageDirectory, parameters.localStorageDirectoryExtensionHandle);

    parameters.perOriginStorageQuota = perOriginStorageQuota();
    parameters.perThirdPartyOriginStorageQuota = perThirdPartyOriginStorageQuota();

    return parameters;
}

void WebsiteDataStore::platformInitialize()
{
    if (!terminationObserver) {
        ASSERT(dataStores().isEmpty());

#if PLATFORM(MAC)
        NSString *notificationName = NSApplicationWillTerminateNotification;
#else
        NSString *notificationName = UIApplicationWillTerminateNotification;
#endif
        terminationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:notificationName object:nil queue:nil usingBlock:^(NSNotification *note) {
            for (auto& dataStore : dataStores()) {
#if ENABLE(RESOURCE_LOAD_STATISTICS)
                if (dataStore->m_resourceLoadStatistics)
                    dataStore->m_resourceLoadStatistics->applicationWillTerminate();
#endif
            }
        }];
    }

    ASSERT(!dataStores().contains(this));
    dataStores().add(this);
}

void WebsiteDataStore::platformDestroy()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->applicationWillTerminate();
#endif

    ASSERT(dataStores().contains(this));
    dataStores().remove(this);

    if (dataStores().isEmpty()) {
        [[NSNotificationCenter defaultCenter] removeObserver:terminationObserver];
        terminationObserver = nil;
    }
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
    // this application cache path, but that's not supported as of right now.
    if (WebCore::IOSApplication::isMobileSafari() || WebCore::IOSApplication::isWebApp() || WebCore::IOSApplication::isWebBookmarksD()) {
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

WTF::String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const WTF::String& directoryName)
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
    if (![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nullptr])
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

}
