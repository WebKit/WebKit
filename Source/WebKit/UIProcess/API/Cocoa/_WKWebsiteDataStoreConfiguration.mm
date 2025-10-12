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

#import "UnifiedOriginStorageLevel.h"
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
        [NSException raise:NSInvalidArgumentException format:@"Identifier is nil"];

    auto uuid = WTF::UUID::fromNSUUID(identifier);
    if (!uuid || !uuid->isValid())
        [NSException raise:NSInvalidArgumentException format:@"Identifier (%s) is invalid for data store", String([identifier UUIDString]).utf8().data()];

    API::Object::constructInWrapper<WebKit::WebsiteDataStoreConfiguration>(self, *uuid);

    return self;
}

- (instancetype)initWithDirectory:(NSURL *)directory
{
    self = [super init];
    if (!self)
        return nil;

    if (!directory)
        [NSException raise:NSInvalidArgumentException format:@"Directory is nil"];

    NSString *path = directory.path;
    API::Object::constructInWrapper<WebKit::WebsiteDataStoreConfiguration>(self, path, path);

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
    return [NSURL fileURLWithPath:_configuration->localStorageDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->indexedDBDatabaseDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->networkCacheDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->deviceIdHashSaltsStorageDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->webSQLDatabaseDirectory().createNSString().get() isDirectory:YES];
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
    return _configuration->httpProxy().createNSURL().autorelease();
}

- (void)setHTTPProxy:(NSURL *)proxy
{
    _configuration->setHTTPProxy(proxy);
}

- (NSURL *)httpsProxy
{
    return _configuration->httpsProxy().createNSURL().autorelease();
}

- (void)setHTTPSProxy:(NSURL *)proxy
{
    _configuration->setHTTPSProxy(proxy);
}

- (NSURL *)_cookieStorageFile
{
    return [NSURL fileURLWithPath:_configuration->cookieStorageFile().createNSString().get() isDirectory:NO];
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
    return [NSURL fileURLWithPath:_configuration->resourceLoadStatisticsDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->cacheStorageDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->serviceWorkerRegistrationDirectory().createNSString().get() isDirectory:YES];
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
    return _configuration->sourceApplicationBundleIdentifier().createNSString().autorelease();
}

- (NSString *)sourceApplicationSecondaryIdentifier
{
    return _configuration->sourceApplicationSecondaryIdentifier().createNSString().autorelease();
}

- (void)setSourceApplicationSecondaryIdentifier:(NSString *)identifier
{
    _configuration->setSourceApplicationSecondaryIdentifier(identifier);
}

- (NSURL *)applicationCacheDirectory
{
    return nil;
}

- (void)setApplicationCacheDirectory:(NSURL *)url
{
}

- (NSString *)applicationCacheFlatFileSubdirectoryName
{
    return nil;
}

- (void)setApplicationCacheFlatFileSubdirectoryName:(NSString *)name
{
}

- (NSURL *)mediaCacheDirectory
{
    return [NSURL fileURLWithPath:_configuration->mediaCacheDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->mediaKeysStorageDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->hstsStorageDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:_configuration->alternativeServicesDirectory().createNSString().get() isDirectory:YES];
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
    return [NSURL fileURLWithPath:directory.createNSString().get() isDirectory:YES];
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

static _WKUnifiedOriginStorageLevel toWKUnifiedOriginStorageLevel(WebKit::UnifiedOriginStorageLevel level)
{
    switch (level) {
    case WebKit::UnifiedOriginStorageLevel::None:
        return _WKUnifiedOriginStorageLevelNone;
    case WebKit::UnifiedOriginStorageLevel::Basic:
        return _WKUnifiedOriginStorageLevelBasic;
    case WebKit::UnifiedOriginStorageLevel::Standard:
        return _WKUnifiedOriginStorageLevelStandard;
    }
}

static WebKit::UnifiedOriginStorageLevel toUnifiedOriginStorageLevel(_WKUnifiedOriginStorageLevel wkLevel)
{
    switch (wkLevel) {
    case _WKUnifiedOriginStorageLevelNone:
        return WebKit::UnifiedOriginStorageLevel::None;
    case _WKUnifiedOriginStorageLevelBasic:
        return WebKit::UnifiedOriginStorageLevel::Basic;
    case _WKUnifiedOriginStorageLevelStandard:
        return WebKit::UnifiedOriginStorageLevel::Standard;
    }
}

- (_WKUnifiedOriginStorageLevel)unifiedOriginStorageLevel
{
    return toWKUnifiedOriginStorageLevel(_configuration->unifiedOriginStorageLevel());
}

- (void)setUnifiedOriginStorageLevel:(_WKUnifiedOriginStorageLevel)level
{
    _configuration->setUnifiedOriginStorageLevel(toUnifiedOriginStorageLevel(level));
}

- (NSString *)webPushPartitionString
{
    return _configuration->webPushPartitionString().createNSString().autorelease();
}

- (void)setWebPushPartitionString:(NSString *)string
{
    _configuration->setWebPushPartitionString(string);
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

- (NSNumber *)originQuotaRatio
{
    auto ratio = _configuration->originQuotaRatio();
    if (!ratio)
        return nil;

    return [NSNumber numberWithDouble:*ratio];
}

- (void)setOriginQuotaRatio:(NSNumber *)originQuotaRatio
{
    std::optional<double> ratio = std::nullopt;
    if (originQuotaRatio) {
        ratio = [originQuotaRatio doubleValue];
        if (*ratio < 0.0 || *ratio > 1.0)
            [NSException raise:NSInvalidArgumentException format:@"OriginQuotaRatio must be in the range [0.0, 1]"];
    }

    _configuration->setOriginQuotaRatio(ratio);
}

- (NSNumber *)totalQuotaRatio
{
    auto ratio = _configuration->totalQuotaRatio();
    if (!ratio)
        return nil;

    return [NSNumber numberWithDouble:*ratio];
}

- (void)setTotalQuotaRatio:(NSNumber *)totalQuotaRatio
{
    std::optional<double> ratio = std::nullopt;
    if (totalQuotaRatio) {
        ratio = [totalQuotaRatio doubleValue];
        if (*ratio < 0.0 || *ratio > 1.0)
            [NSException raise:NSInvalidArgumentException format:@"TotalQuotaRatio must be in the range [0.0, 1]"];
    }

    _configuration->setTotalQuotaRatio(ratio);
}

- (NSNumber *)standardVolumeCapacity
{
    auto capacity = _configuration->standardVolumeCapacity();
    if (!capacity)
        return nil;

    return [NSNumber numberWithUnsignedLongLong:*capacity];
}

- (void)setStandardVolumeCapacity:(NSNumber *)standardVolumeCapacity
{
    std::optional<uint64_t> capacity;
    if (standardVolumeCapacity)
        capacity = [standardVolumeCapacity unsignedLongLongValue];

    _configuration->setStandardVolumeCapacity(capacity);
}

- (NSNumber *)volumeCapacityOverride
{
    auto capacity = _configuration->volumeCapacityOverride();
    if (!capacity)
        return nil;

    return [NSNumber numberWithUnsignedLongLong:*capacity];
}

- (void)setVolumeCapacityOverride:(NSNumber *)mockVolumeCapactiy
{
    std::optional<uint64_t> capacity = std::nullopt;
    if (mockVolumeCapactiy)
        capacity = [mockVolumeCapactiy unsignedLongLongValue];

    _configuration->setVolumeCapacityOverride(capacity);
}

- (NSURL *)_resourceMonitorThrottlerDirectory
{
    return [NSURL fileURLWithPath:_configuration->resourceMonitorThrottlerDirectory().createNSString().get() isDirectory:YES];
}

- (void)_setResourceMonitorThrottlerDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _resourceMonitorThrottlerDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _resourceMonitorThrottlerDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    _configuration->setResourceMonitorThrottlerDirectory(url.path);
}

- (NSURL *)webContentRestrictionsConfigurationURL
{
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    auto file = _configuration->webContentRestrictionsConfigurationFile();
    if (!file.isEmpty())
        return [NSURL fileURLWithPath:file.createNSString().get()];
#endif

    return nil;
}

- (void)setWebContentRestrictionsConfigurationURL:(NSURL *)url
{
    checkURLArgument(url);
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    _configuration->setWebContentRestrictionsConfigurationFile(url.path);
#endif
}

- (BOOL)isDeclarativeWebPushEnabled
{
#if ENABLE(DECLARATIVE_WEB_PUSH)
    return _configuration->isDeclarativeWebPushEnabled();
#else
    return NO;
#endif
}

- (void)setIsDeclarativeWebPushEnabled:(BOOL)enabled
{
    UNUSED_PARAM(enabled);
#if ENABLE(DECLARATIVE_WEB_PUSH)
    _configuration->setIsDeclarativeWebPushEnabled(enabled);
#endif
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
    return _configuration->boundInterfaceIdentifier().createNSString().autorelease();
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
    return _configuration->dataConnectionServiceType().createNSString().autorelease();
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
    return false;
}

- (void)_setShouldAcceptInsecureCertificatesForWebSockets:(BOOL)accept
{
    UNUSED_PARAM(accept);
}

- (void)setProxyConfiguration:(NSDictionary *)configuration
{
    _configuration->setProxyConfiguration((__bridge CFDictionaryRef)adoptNS([configuration copy]).get());
}

- (NSURL *)standaloneApplicationURL
{
    return _configuration->standaloneApplicationURL().createNSURL().autorelease();
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
    return _configuration->pcmMachServiceName().createNSString().autorelease();
}

- (void)setPCMMachServiceName:(NSString *)name
{
    _configuration->setPCMMachServiceName(name);
}

- (NSString *)webPushMachServiceName
{
    return _configuration->webPushMachServiceName().createNSString().autorelease();
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

- (BOOL)resourceLoadStatisticsDebugModeEnabled
{
    return _configuration->resourceLoadStatisticsDebugModeEnabled();
}

- (void)setResourceLoadStatisticsDebugModeEnabled:(BOOL)enabled
{
    _configuration->setResourceLoadStatisticsDebugModeEnabled(enabled);
}

- (NSNumber *)defaultTrackingPreventionEnabledOverride
{
    auto enabled = _configuration->defaultTrackingPreventionEnabledOverride();
    if (!enabled)
        return nil;

    return [NSNumber numberWithBool:*enabled];
}

- (void)setDefaultTrackingPreventionEnabledOverride:(NSNumber *)defaultTrackingPreventionEnabledOverride
{
    std::optional<bool> enabled;
    if (defaultTrackingPreventionEnabledOverride)
        enabled = [defaultTrackingPreventionEnabledOverride boolValue];

    _configuration->setDefaultTrackingPreventionEnabledOverride(enabled);
}

- (NSUUID *)identifier
{
    auto currentIdentifier = _configuration->identifier();
    if (!currentIdentifier)
        return nullptr;

    return currentIdentifier->createNSUUID().autorelease();
}

- (API::Object&)_apiObject
{
    return *_configuration;
}

@end
