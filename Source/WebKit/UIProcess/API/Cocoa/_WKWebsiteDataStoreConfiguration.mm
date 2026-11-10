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

#import "TimeBasedEvictionMode.h"
#import "UnifiedOriginStorageLevel.h"
#import <WebCore/SecurityOriginData.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/Borrow.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

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
        [NSException raise:NSInvalidArgumentException format:@"Identifier (%@) is invalid for data store", identifier.UUIDString];

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

    RetainPtr<NSString> path = directory.path;
    API::Object::constructInWrapper<WebKit::WebsiteDataStoreConfiguration>(self, StringImpl::create(path.get()).get(), StringImpl::create(path.get()).get());

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebsiteDataStoreConfiguration.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _configuration->~WebsiteDataStoreConfiguration();
    [super dealloc];
}

- (BOOL)isPersistent
{
    return _configuration->isPersistent();
}

- (NSURL *)_webStorageDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->localStorageDirectory()).get() isDirectory:YES];
}

- (void)_setWebStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _webStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];
    
    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _webStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setLocalStorageDirectory(StringImpl::create(url.path));
}

- (NSURL *)_indexedDBDatabaseDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->indexedDBDatabaseDirectory()).get() isDirectory:YES];
}

- (void)_setIndexedDBDatabaseDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _indexedDBDatabaseDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];
    
    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _indexedDBDatabaseDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setIndexedDBDatabaseDirectory(StringImpl::create(url.path));
}

- (NSURL *)networkCacheDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->networkCacheDirectory()).get() isDirectory:YES];
}

- (void)setNetworkCacheDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set networkCacheDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set networkCacheDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setNetworkCacheDirectory(StringImpl::create(url.path));
}

- (NSURL *)deviceIdHashSaltsStorageDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->deviceIdHashSaltsStorageDirectory()).get() isDirectory:YES];
}

- (void)setDeviceIdHashSaltsStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set deviceIdHashSaltsStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set deviceIdHashSaltsStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setDeviceIdHashSaltsStorageDirectory(StringImpl::create(url.path));
}

- (NSURL *)_webSQLDatabaseDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->webSQLDatabaseDirectory()).get() isDirectory:YES];
}

- (void)_setWebSQLDatabaseDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _webSQLDatabaseDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _webSQLDatabaseDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setWebSQLDatabaseDirectory(StringImpl::create(url.path));
}

- (NSURL *)httpProxy
{
    return _configuration->httpProxy().createNSURL().autorelease();
}

- (void)setHTTPProxy:(NSURL *)proxy
{
    protect(_configuration.get())->setHTTPProxy(proxy);
}

- (NSURL *)httpsProxy
{
    return _configuration->httpsProxy().createNSURL().autorelease();
}

- (void)setHTTPSProxy:(NSURL *)proxy
{
    protect(_configuration.get())->setHTTPSProxy(proxy);
}

- (NSURL *)_cookieStorageFile
{
    return [NSURL fileURLWithPath:createNSString(_configuration->cookieStorageFile()).get() isDirectory:NO];
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

    protect(_configuration.get())->setCookieStorageFile(StringImpl::create(url.path));
}

- (NSURL *)_resourceLoadStatisticsDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->resourceLoadStatisticsDirectory()).get() isDirectory:YES];
}

- (void)_setResourceLoadStatisticsDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _resourceLoadStatisticsDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _resourceLoadStatisticsDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setResourceLoadStatisticsDirectory(StringImpl::create(url.path));
}

- (NSURL *)_cacheStorageDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->cacheStorageDirectory()).get() isDirectory:YES];
}

- (void)_setCacheStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _cacheStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _cacheStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setCacheStorageDirectory(StringImpl::create(url.path));
}

- (NSURL *)_serviceWorkerRegistrationDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->serviceWorkerRegistrationDirectory()).get() isDirectory:YES];
}

- (void)_setServiceWorkerRegistrationDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _serviceWorkerRegistrationDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _serviceWorkerRegistrationDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setServiceWorkerRegistrationDirectory(StringImpl::create(url.path));
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
    protect(_configuration.get())->setSourceApplicationBundleIdentifier(StringImpl::create(identifier));
}

- (NSString *)sourceApplicationBundleIdentifier
{
    return createNSString(_configuration->sourceApplicationBundleIdentifier()).autorelease();
}

- (NSString *)sourceApplicationSecondaryIdentifier
{
    return createNSString(_configuration->sourceApplicationSecondaryIdentifier()).autorelease();
}

- (void)setSourceApplicationSecondaryIdentifier:(NSString *)identifier
{
    protect(_configuration.get())->setSourceApplicationSecondaryIdentifier(StringImpl::create(identifier));
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
    return [NSURL fileURLWithPath:createNSString(_configuration->mediaCacheDirectory()).get() isDirectory:YES];
}

- (void)setMediaCacheDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set mediaCacheDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set mediaCacheDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setMediaCacheDirectory(StringImpl::create(url.path));
}

- (NSURL *)mediaKeysStorageDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->mediaKeysStorageDirectory()).get() isDirectory:YES];
}

- (void)setMediaKeysStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set mediaKeysStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set mediaKeysStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setMediaKeysStorageDirectory(StringImpl::create(url.path));
}

- (NSURL *)hstsStorageDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->hstsStorageDirectory()).get() isDirectory:YES];
}

- (void)setHSTSStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set hstsStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set hstsStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setHSTSStorageDirectory(StringImpl::create(url.path));
}

- (NSURL *)alternativeServicesStorageDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->alternativeServicesDirectory()).get() isDirectory:YES];
}

- (void)setAlternativeServicesStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set alternativeServicesDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set alternativeServicesStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setAlternativeServicesDirectory(StringImpl::create(url.path));
}

- (NSURL *)generalStorageDirectory
{
    auto& directory = _configuration->generalStorageDirectory();
    if (!directory)
        return nil;
    return [NSURL fileURLWithPath:createNSString(directory).get() isDirectory:YES];
}

- (void)setGeneralStorageDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set generalStorageDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set generalStorageDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setGeneralStorageDirectory(StringImpl::create(url.path));
}

NODELETE static _WKUnifiedOriginStorageLevel toWKUnifiedOriginStorageLevel(WebKit::UnifiedOriginStorageLevel level)
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

NODELETE static WebKit::UnifiedOriginStorageLevel toUnifiedOriginStorageLevel(_WKUnifiedOriginStorageLevel wkLevel)
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
    return createNSString(_configuration->webPushPartitionString()).autorelease();
}

- (void)setWebPushPartitionString:(NSString *)string
{
    protect(_configuration.get())->setWebPushPartitionString(StringImpl::create(string));
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

- (_WKTimeBasedEvictionMode)timeBasedEvictionMode
{
    switch (_configuration->timeBasedEvictionMode()) {
    case WebKit::TimeBasedEvictionMode::Disabled:
        return _WKTimeBasedEvictionModeDisabled;
    case WebKit::TimeBasedEvictionMode::ServiceWorkerRegistrationsOnly:
        return _WKTimeBasedEvictionModeServiceWorkerRegistrationsOnly;
    case WebKit::TimeBasedEvictionMode::AllTypes:
        return _WKTimeBasedEvictionModeAllTypes;
    }
}

- (void)setTimeBasedEvictionMode:(_WKTimeBasedEvictionMode)mode
{
    switch (mode) {
    case _WKTimeBasedEvictionModeDisabled:
        _configuration->setTimeBasedEvictionMode(WebKit::TimeBasedEvictionMode::Disabled);
        break;
    case _WKTimeBasedEvictionModeServiceWorkerRegistrationsOnly:
        _configuration->setTimeBasedEvictionMode(WebKit::TimeBasedEvictionMode::ServiceWorkerRegistrationsOnly);
        break;
    case _WKTimeBasedEvictionModeAllTypes:
        _configuration->setTimeBasedEvictionMode(WebKit::TimeBasedEvictionMode::AllTypes);
        break;
    }
}

- (NSTimeInterval)timeBasedEvictionThreshold
{
    return _configuration->timeBasedEvictionThreshold().seconds();
}

- (void)setTimeBasedEvictionThreshold:(NSTimeInterval)seconds
{
    _configuration->setTimeBasedEvictionThreshold(Seconds(seconds));
}

- (NSNumber *)lastModificationTimeUpdateIntervalOverride
{
    auto interval = _configuration->lastModificationTimeUpdateIntervalOverride();
    if (!interval)
        return nil;

    return [NSNumber numberWithDouble:interval->seconds()];
}

- (void)setLastModificationTimeUpdateIntervalOverride:(NSNumber *)seconds
{
    if (seconds)
        _configuration->setLastModificationTimeUpdateIntervalOverride(Seconds([seconds doubleValue]));
    else
        _configuration->setLastModificationTimeUpdateIntervalOverride(std::nullopt);
}

- (NSNumber *)timeBasedEvictionIntervalOverride
{
    auto interval = _configuration->timeBasedEvictionIntervalOverride();
    if (!interval)
        return nil;

    return [NSNumber numberWithDouble:interval->seconds()];
}

- (void)setTimeBasedEvictionIntervalOverride:(NSNumber *)seconds
{
    if (seconds)
        _configuration->setTimeBasedEvictionIntervalOverride(Seconds([seconds doubleValue]));
    else
        _configuration->setTimeBasedEvictionIntervalOverride(std::nullopt);
}

- (NSArray<NSString *> *)mockPushSubscriptionOriginsForTesting
{
    auto& origins = _configuration->mockPushSubscriptionOriginsForTesting();
    RetainPtr result = adoptNS([[NSMutableArray alloc] initWithCapacity:origins.size()]);
    for (auto& origin : borrow(origins))
        [result addObject:createNSString(origin.toString()).get()];
    return result.autorelease();
}

- (void)setMockPushSubscriptionOriginsForTesting:(NSArray<NSString *> *)originStrings
{
    Vector<WebCore::SecurityOriginData> origins(ReserveCapacityTag { }, originStrings.count);
    // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
    for (SUPPRESS_UNRETAINED_LOCAL NSString *originString in originStrings) {
        auto origin = WebCore::SecurityOriginData::fromURL(URL { StringImpl::create(originString) });
        if (origin.isNull() || origin.isOpaque())
            continue;

        origins.append(WTF::move(origin));
    }
    SUPPRESS_UNCOUNTED_ARG _configuration->setMockPushSubscriptionOriginsForTesting(WTF::move(origins));
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
    std::optional<double> ratio;
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
    std::optional<double> ratio;
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
    std::optional<uint64_t> capacity;
    if (mockVolumeCapactiy)
        capacity = [mockVolumeCapactiy unsignedLongLongValue];

    _configuration->setVolumeCapacityOverride(capacity);
}

- (NSURL *)_resourceMonitorThrottlerDirectory
{
    return [NSURL fileURLWithPath:createNSString(_configuration->resourceMonitorThrottlerDirectory()).get() isDirectory:YES];
}

- (void)_setResourceMonitorThrottlerDirectory:(NSURL *)url
{
    if (!_configuration->isPersistent())
        [NSException raise:NSInvalidArgumentException format:@"Cannot set _resourceMonitorThrottlerDirectory on a non-persistent _WKWebsiteDataStoreConfiguration."];

    if (_configuration->identifier())
        [NSException raise:NSGenericException format:@"Cannot set _resourceMonitorThrottlerDirectory on a _WKWebsiteDataStoreConfiguration created with identifier"];

    checkURLArgument(url);
    protect(_configuration.get())->setResourceMonitorThrottlerDirectory(StringImpl::create(url.path));
}

- (NSURL *)webContentRestrictionsConfigurationURL
{
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    auto file = _configuration->webContentRestrictionsConfigurationFile();
    if (!isEmpty(file))
        return [NSURL fileURLWithPath:createNSString(file).get()];
#endif

    return nil;
}

- (void)setWebContentRestrictionsConfigurationURL:(NSURL *)url
{
    checkURLArgument(url);
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    _configuration->setWebContentRestrictionsConfigurationFile(StringImpl::create(url.path));
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
    return createNSString(_configuration->boundInterfaceIdentifier()).autorelease();
}

- (void)setBoundInterfaceIdentifier:(NSString *)identifier
{
    protect(_configuration.get())->setBoundInterfaceIdentifier(StringImpl::create(identifier));
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
    return createNSString(_configuration->dataConnectionServiceType()).autorelease();
}

- (void)setDataConnectionServiceType:(NSString *)type
{
    protect(_configuration.get())->setDataConnectionServiceType(StringImpl::create(type));
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
    Ref { *_configuration }->setProxyConfiguration((__bridge CFDictionaryRef)adoptNS([configuration copy]).get());
}

- (NSURL *)standaloneApplicationURL
{
    return _configuration->standaloneApplicationURL().createNSURL().autorelease();
}

- (void)setStandaloneApplicationURL:(NSURL *)url
{
    protect(_configuration.get())->setStandaloneApplicationURL(url);
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
    return createNSString(_configuration->pcmMachServiceName()).autorelease();
}

- (void)setPCMMachServiceName:(NSString *)name
{
    protect(_configuration.get())->setPCMMachServiceName(StringImpl::create(name));
}

- (NSString *)webPushMachServiceName
{
    return createNSString(_configuration->webPushMachServiceName()).autorelease();
}

- (void)setWebPushMachServiceName:(NSString *)name
{
    protect(_configuration.get())->setWebPushMachServiceName(StringImpl::create(name));
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

- (NSString *)additionalDomainsWithUserInteractionForTesting
{
    return createNSString(_configuration->additionalDomainsWithUserInteractionForTesting()).autorelease();
}

- (void)setAdditionalDomainsWithUserInteractionForTesting:(NSString *)domains
{
    protect(_configuration.get())->setAdditionalDomainsWithUserInteractionForTesting(StringImpl::create(domains));
}

- (NSUInteger)overridePersistentNotificationMinimumLifetimeForTesting
{
    return _configuration->overridePersistentNotificationMinimumLifetimeForTesting().value_or(0);
}

- (void)setOverridePersistentNotificationMinimumLifetimeForTesting:(NSUInteger)lifetime
{
    _configuration->setOverridePersistentNotificationMinimumLifetimeForTesting(lifetime);
}

- (BOOL)qualifiedServerTrustDebugEnabledForTesting
{
    return _configuration->qualifiedServerTrustDebugEnabledForTesting();
}

- (void)setQualifiedServerTrustDebugEnabledForTesting:(BOOL)enabled
{
    _configuration->setQualifiedServerTrustDebugEnabledForTesting(enabled);
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
