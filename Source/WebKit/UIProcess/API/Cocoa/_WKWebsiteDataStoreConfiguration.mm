/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#import "_WKWebsiteDataStoreConfigurationInternal.h"

#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>

static void checkURLArgument(NSURL *url)
{
    if (url && ![url isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", url];
}

@implementation _WKWebsiteDataStoreConfiguration

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    API::Object::constructInWrapper<WebKit::WebsiteDataStoreConfiguration>(self, WebKit::IsPersistent::Yes);

    return self;
}

- (instancetype)initNonPersistentConfiguration
{
    self = [super init];
    if (!self)
        return nil;

    API::Object::constructInWrapper<WebKit::WebsiteDataStoreConfiguration>(self, WebKit::IsPersistent::No);

    return self;
}

- (instancetype)initWithIdentifier:(NSUUID *)identifier
{
    self = [super init];
    if (!self)
        return nil;

    if (!identifier)
        [NSException raise:NSInvalidArgumentException format:@"Identifier cannot be nil"];

    API::Object::constructInWrapper<WebKit::WebsiteDataStoreConfiguration>(self, UUID(identifier));

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebsiteDataStoreConfiguration.class, self))
        return;
    _configuration->~WebsiteDataStoreConfiguration();
    [super dealloc];
}

- (BOOL)isPersistent
{
    return _configuration->isPersistent();
}

- (NSURL *)_webStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->localStorageDirectory() isDirectory:YES];
}

- (void)_setWebStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _webStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];
    
    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _webStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setLocalStorageDirectory(url.path);
}

- (NSURL *)_indexedDBDatabaseDirectory
{
    return [NSURL fileURLWithPath:_configuration->indexedDBDatabaseDirectory() isDirectory:YES];
}

- (void)_setIndexedDBDatabaseDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _indexedDBDatabaseDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];
    
    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _indexedDBDatabaseDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setIndexedDBDatabaseDirectory(url.path);
}

- (NSURL *)networkCacheDirectory
{
    return [NSURL fileURLWithPath:_configuration->networkCacheDirectory() isDirectory:YES];
}

- (void)setNetworkCacheDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set networkCacheDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set networkCacheDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setNetworkCacheDirectory(url.path);
}

- (NSURL *)deviceIdHashSaltsStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->deviceIdHashSaltsStorageDirectory() isDirectory:YES];
}

- (void)setDeviceIdHashSaltsStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set deviceIdHashSaltsStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set deviceIdHashSaltsStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setDeviceIdHashSaltsStorageDirectory(url.path);
}

- (NSURL *)_webSQLDatabaseDirectory
{
    return [NSURL fileURLWithPath:_configuration->webSQLDatabaseDirectory() isDirectory:YES];
}

- (void)_setWebSQLDatabaseDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _webSQLDatabaseDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _webSQLDatabaseDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setWebSQLDatabaseDirectory(url.path);
}

- (NSURL *)httpProxy
{
    return _configuration->httpProxy();
}

- (void)setHTTPProxy:(NSURL *)proxy
{
    _configuration->setHTTPProxy(proxy);
}

- (NSURL *)httpsProxy
{
    return _configuration->httpsProxy();
}

- (void)setHTTPSProxy:(NSURL *)proxy
{
    _configuration->setHTTPSProxy(proxy);
}

- (NSURL *)_cookieStorageFile
{
    return [NSURL fileURLWithPath:_configuration->cookieStorageFile() isDirectory:NO];
}

- (void)_setCookieStorageFile:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _cookieStorageFile on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _cookieStorageFile on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    if ([url hasDirectoryPath])
        [NSException raise:NSInvalidArgumentException format:@"The cookie storage path must point to a file, not a directory."];

    _configuration->setCookieStorageFile(url.path);
}

- (NSURL *)_resourceLoadStatisticsDirectory
{
    return [NSURL fileURLWithPath:_configuration->resourceLoadStatisticsDirectory() isDirectory:YES];
}

- (void)_setResourceLoadStatisticsDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _resourceLoadStatisticsDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _resourceLoadStatisticsDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setResourceLoadStatisticsDirectory(url.path);
}

- (NSURL *)_cacheStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->cacheStorageDirectory() isDirectory:YES];
}

- (void)_setCacheStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _cacheStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _cacheStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setCacheStorageDirectory(url.path);
}

- (NSURL *)_serviceWorkerRegistrationDirectory
{
    return [NSURL fileURLWithPath:_configuration->serviceWorkerRegistrationDirectory() isDirectory:YES];
}

- (void)_setServiceWorkerRegistrationDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _serviceWorkerRegistrationDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _serviceWorkerRegistrationDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setServiceWorkerRegistrationDirectory(url.path);
}

- (BOOL)serviceWorkerProcessTerminationDelayEnabled
{
    return _configuration->serviceWorkerProcessTerminationDelayEnabled();
}

- (void)setServiceWorkerProcessTerminationDelayEnabled:(BOOL)enabled
{
    _configuration->setServiceWorkerProcessTerminationDelayEnabled(enabled);
}

- (void)setSourceApplicationBundleIdentifier:(NSString *)identifier
{
    _configuration->setSourceApplicationBundleIdentifier(identifier);
}

- (NSString *)sourceApplicationBundleIdentifier
{
    return _configuration->sourceApplicationBundleIdentifier();
}

- (NSString *)sourceApplicationSecondaryIdentifier
{
    return _configuration->sourceApplicationSecondaryIdentifier();
}

- (void)setSourceApplicationSecondaryIdentifier:(NSString *)identifier
{
    _configuration->setSourceApplicationSecondaryIdentifier(identifier);
}

- (NSURL *)applicationCacheDirectory
{
    return [NSURL fileURLWithPath:_configuration->applicationCacheDirectory() isDirectory:YES];
}

- (void)setApplicationCacheDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set applicationCacheDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set applicationCacheDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setApplicationCacheDirectory(url.path);
}

- (NSString *)applicationCacheFlatFileSubdirectoryName
{
    return _configuration->applicationCacheFlatFileSubdirectoryName();
}

- (void)setApplicationCacheFlatFileSubdirectoryName:(NSString *)name
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set applicationCacheFlatFileSubdirectoryName on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set applicationCacheFlatFileSubdirectoryName on a _WKWebsiteDataStoreConfiguration created with identifier"];

    _configuration->setApplicationCacheFlatFileSubdirectoryName(name);
}

- (NSURL *)mediaCacheDirectory
{
    return [NSURL fileURLWithPath:_configuration->mediaCacheDirectory() isDirectory:YES];
}

- (void)setMediaCacheDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set mediaCacheDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set mediaCacheDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setMediaCacheDirectory(url.path);
}

- (NSURL *)mediaKeysStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->mediaKeysStorageDirectory() isDirectory:YES];
}

- (void)setMediaKeysStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set mediaKeysStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set mediaKeysStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setMediaKeysStorageDirectory(url.path);
}

- (NSURL *)hstsStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->hstsStorageDirectory() isDirectory:YES];
}

- (void)setHSTSStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set hstsStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set hstsStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setHSTSStorageDirectory(url.path);
}

- (NSURL *)alternativeServicesStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->alternativeServicesDirectory() isDirectory:YES];
}

- (void)setAlternativeServicesStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set alternativeServicesDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set alternativeServicesStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setAlternativeServicesDirectory(url.path);
}

- (NSURL *)generalStorageDirectory
{
    auto& directory = _configuration->generalStorageDirectory();
    if (directory.isNull())
        return nil;
    return [NSURL fileURLWithPath:directory isDirectory:YES];
}

- (void)setGeneralStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set generalStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set generalStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setGeneralStorageDirectory(url.path);
}

- (BOOL)shouldUseCustomStoragePaths
{
    return _configuration->shouldUseCustomStoragePaths();
}

- (void)setShouldUseCustomStoragePaths:(BOOL)use
{
    _configuration->setShouldUseCustomStoragePaths(use);
}

- (BOOL)deviceManagementRestrictionsEnabled
{
    return _configuration->deviceManagementRestrictionsEnabled();
}

- (void)setDeviceManagementRestrictionsEnabled:(BOOL)enabled
{
    _configuration->setDeviceManagementRestrictionsEnabled(enabled);
}

- (BOOL)networkCacheSpeculativeValidationEnabled
{
    return _configuration->networkCacheSpeculativeValidationEnabled();
}

- (void)setNetworkCacheSpeculativeValidationEnabled:(BOOL)enabled
{
    _configuration->setNetworkCacheSpeculativeValidationEnabled(enabled);
}

- (BOOL)fastServerTrustEvaluationEnabled
{
    return _configuration->fastServerTrustEvaluationEnabled();
}

- (void)setFastServerTrustEvaluationEnabled:(BOOL)enabled
{
    return _configuration->setFastServerTrustEvaluationEnabled(enabled);
}

- (NSUInteger)perOriginStorageQuota
{
    return _configuration->perOriginStorageQuota();
}

- (void)setPerOriginStorageQuota:(NSUInteger)quota
{
    _configuration->setPerOriginStorageQuota(quota);
}

- (NSUInteger)testSpeedMultiplier
{
    return _configuration->testSpeedMultiplier();
}

- (void)setTestSpeedMultiplier:(NSUInteger)quota
{
    _configuration->setTestSpeedMultiplier(quota);
}

- (BOOL)suppressesConnectionTerminationOnSystemChange
{
    return _configuration->suppressesConnectionTerminationOnSystemChange();
}

- (void)setSuppressesConnectionTerminationOnSystemChange:(BOOL)suppresses
{
    _configuration->setSuppressesConnectionTerminationOnSystemChange(suppresses);
}

- (BOOL)allowsServerPreconnect
{
    return _configuration->allowsServerPreconnect();
}

- (void)setAllowsServerPreconnect:(BOOL)allows
{
    _configuration->setAllowsServerPreconnect(allows);
}

- (NSString *)boundInterfaceIdentifier
{
    return _configuration->boundInterfaceIdentifier();
}

- (void)setBoundInterfaceIdentifier:(NSString *)identifier
{
    _configuration->setBoundInterfaceIdentifier(identifier);
}

- (BOOL)allowsCellularAccess
{
    return _configuration->allowsCellularAccess();
}

- (void)setAllowsCellularAccess:(BOOL)allows
{
    _configuration->setAllowsCellularAccess(allows);
}

- (BOOL)legacyTLSEnabled
{
    return _configuration->legacyTLSEnabled();
}

- (void)setLegacyTLSEnabled:(BOOL)enable
{
    _configuration->setLegacyTLSEnabled(enable);
}

- (NSDictionary *)proxyConfiguration
{
    return (__bridge NSDictionary *)_configuration->proxyConfiguration();
}

- (NSString *)dataConnectionServiceType
{
    return _configuration->dataConnectionServiceType();
}

- (void)setDataConnectionServiceType:(NSString *)type
{
    _configuration->setDataConnectionServiceType(type);
}

- (BOOL)preventsSystemHTTPProxyAuthentication
{
    return _configuration->preventsSystemHTTPProxyAuthentication();
}

- (void)setPreventsSystemHTTPProxyAuthentication:(BOOL)prevents
{
    _configuration->setPreventsSystemHTTPProxyAuthentication(prevents);
}

- (BOOL)requiresSecureHTTPSProxyConnection
{
    return _configuration->requiresSecureHTTPSProxyConnection();
}

- (void)setRequiresSecureHTTPSProxyConnection:(BOOL)requiresSecureProxy
{
    _configuration->setRequiresSecureHTTPSProxyConnection(requiresSecureProxy);
}

- (BOOL)shouldRunServiceWorkersOnMainThreadForTesting
{
    return _configuration->shouldRunServiceWorkersOnMainThreadForTesting();
}

- (void)setShouldRunServiceWorkersOnMainThreadForTesting:(BOOL)shouldRunOnMainThread
{
    _configuration->setShouldRunServiceWorkersOnMainThreadForTesting(shouldRunOnMainThread);
}

- (NSUInteger)overrideServiceWorkerRegistrationCountTestingValue
{
    return _configuration->overrideServiceWorkerRegistrationCountTestingValue().value_or(0);
}

- (void)setOverrideServiceWorkerRegistrationCountTestingValue:(NSUInteger)count
{
    _configuration->setOverrideServiceWorkerRegistrationCountTestingValue(count);
}

- (BOOL)_shouldAcceptInsecureCertificatesForWebSockets
{
#if !HAVE(NSURLSESSION_WEBSOCKET)
    return _configuration->shouldAcceptInsecureCertificatesForWebSockets();
#else
    return false;
#endif
}

- (void)_setShouldAcceptInsecureCertificatesForWebSockets:(BOOL)accept
{
#if !HAVE(NSURLSESSION_WEBSOCKET)
    _configuration->setShouldAcceptInsecureCertificatesForWebSockets(accept);
#else
    UNUSED_PARAM(accept);
#endif
}

- (void)setProxyConfiguration:(NSDictionary *)configuration
{
    _configuration->setProxyConfiguration((__bridge CFDictionaryRef)[configuration copy]);
}

- (NSURL *)standaloneApplicationURL
{
    return _configuration->standaloneApplicationURL();
}

- (void)setStandaloneApplicationURL:(NSURL *)url
{
    _configuration->setStandaloneApplicationURL(url);
}

- (BOOL)enableInAppBrowserPrivacyForTesting
{
    return _configuration->enableInAppBrowserPrivacyForTesting();
}

- (void)setEnableInAppBrowserPrivacyForTesting:(BOOL)enable
{
    _configuration->setEnableInAppBrowserPrivacyForTesting(enable);
}

- (BOOL)allowsHSTSWithUntrustedRootCertificate
{
    return _configuration->allowsHSTSWithUntrustedRootCertificate();
}

- (void)setAllowsHSTSWithUntrustedRootCertificate:(BOOL)allows
{
    _configuration->setAllowsHSTSWithUntrustedRootCertificate(allows);
}

- (NSString *)pcmMachServiceName
{
    return _configuration->pcmMachServiceName();
}

- (void)setPCMMachServiceName:(NSString *)name
{
    _configuration->setPCMMachServiceName(name);
}

- (NSString *)webPushMachServiceName
{
    return _configuration->webPushMachServiceName();
}

- (void)setWebPushMachServiceName:(NSString *)name
{
    _configuration->setWebPushMachServiceName(name);
}

- (BOOL)allLoadsBlockedByDeviceManagementRestrictionsForTesting
{
    return _configuration->allLoadsBlockedByDeviceManagementRestrictionsForTesting();
}

- (void)setAllLoadsBlockedByDeviceManagementRestrictionsForTesting:(BOOL)blocked
{
    _configuration->setAllLoadsBlockedByDeviceManagementRestrictionsForTesting(blocked);
}

- (BOOL)webPushDaemonUsesMockBundlesForTesting
{
    return _configuration->webPushDaemonUsesMockBundlesForTesting();
}

- (void)setWebPushDaemonUsesMockBundlesForTesting:(BOOL)usesMockBundles
{
    _configuration->setWebPushDaemonUsesMockBundlesForTesting(usesMockBundles);
}

- (BOOL)resourceLoadStatisticsDebugModeEnabled
{
    return _configuration->resourceLoadStatisticsDebugModeEnabled();
}

- (void)setResourceLoadStatisticsDebugModeEnabled:(BOOL)enabled
{
    _configuration->setResourceLoadStatisticsDebugModeEnabled(enabled);
}

- (API::Object&)_apiObject
{
    return *_configuration;
}

@end
