/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#import "WKProcessPoolInternal.h"

#if WK_API_ENABLED

#import "AutomationClient.h"
#import "CacheModel.h"
#import "DownloadClient.h"
#import "Logging.h"
#import "PluginProcessManager.h"
#import "SandboxUtilities.h"
#import "UIGamepadProvider.h"
#import "WKObject.h"
#import "WebCertificateInfo.h"
#import "WebCookieManagerProxy.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import "_WKAutomationDelegate.h"
#import "_WKAutomationSessionInternal.h"
#import "_WKDownloadDelegate.h"
#import "_WKDownloadInternal.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/PluginData.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThreadSystemInterface.h>
#import "WKGeolocationProviderIOS.h"
#endif

static WKProcessPool *sharedProcessPool;

@implementation WKProcessPool {
    WeakObjCPtr<id <_WKAutomationDelegate>> _automationDelegate;
    WeakObjCPtr<id <_WKDownloadDelegate>> _downloadDelegate;

    RetainPtr<_WKAutomationSession> _automationSession;
#if PLATFORM(IOS)
    RetainPtr<WKGeolocationProviderIOS> _geolocationProvider;
    RetainPtr<id <_WKGeolocationCoreLocationProvider>> _coreLocationProvider;
#endif // PLATFORM(IOS)
}

- (instancetype)_initWithConfiguration:(_WKProcessPoolConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebProcessPool>(self, *configuration->_processPoolConfiguration);

    return self;
}

- (instancetype)init
{
    return [self _initWithConfiguration:adoptNS([[_WKProcessPoolConfiguration alloc] init]).get()];
}

- (void)dealloc
{
    _processPool->~WebProcessPool();

    [super dealloc];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    if (self == sharedProcessPool) {
        [coder encodeBool:YES forKey:@"isSharedProcessPool"];
        return;
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    if ([coder decodeBoolForKey:@"isSharedProcessPool"]) {
        [self release];

        return [[WKProcessPool _sharedProcessPool] retain];
    }

    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; configuration = %@>", NSStringFromClass(self.class), self, wrapper(_processPool->configuration())];
}

- (_WKProcessPoolConfiguration *)_configuration
{
    return wrapper(_processPool->configuration().copy());
}

- (API::Object&)_apiObject
{
    return *_processPool;
}

#if PLATFORM(IOS)
- (WKGeolocationProviderIOS *)_geolocationProvider
{
    if (!_geolocationProvider)
        _geolocationProvider = adoptNS([[WKGeolocationProviderIOS alloc] initWithProcessPool:*_processPool]);
    return _geolocationProvider.get();
}
#endif // PLATFORM(IOS)

@end

@implementation WKProcessPool (WKPrivate)

+ (WKProcessPool *)_sharedProcessPool
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedProcessPool = [[WKProcessPool alloc] init];
    });

    return sharedProcessPool;
}

+ (NSArray<WKProcessPool *> *)_allProcessPoolsForTesting
{
    auto& allPools = WebKit::WebProcessPool::allProcessPools();
    auto nsAllPools = adoptNS([[NSMutableArray alloc] initWithCapacity:allPools.size()]);
    for (auto* pool : allPools)
        [nsAllPools addObject:wrapper(*pool)];
    return nsAllPools.autorelease();
}

+ (NSURL *)_websiteDataURLForContainerWithURL:(NSURL *)containerURL
{
    return [WKProcessPool _websiteDataURLForContainerWithURL:containerURL bundleIdentifierIfNotInContainer:nil];
}

+ (NSURL *)_websiteDataURLForContainerWithURL:(NSURL *)containerURL bundleIdentifierIfNotInContainer:(NSString *)bundleIdentifier
{
    NSURL *url = [containerURL URLByAppendingPathComponent:@"Library" isDirectory:YES];
    url = [url URLByAppendingPathComponent:@"WebKit" isDirectory:YES];

    if (!WebKit::processHasContainer() && bundleIdentifier)
        url = [url URLByAppendingPathComponent:bundleIdentifier isDirectory:YES];

    return [url URLByAppendingPathComponent:@"WebsiteData" isDirectory:YES];
}

- (void)_setAllowsSpecificHTTPSCertificate:(NSArray *)certificateChain forHost:(NSString *)host
{
    _processPool->allowSpecificHTTPSCertificateForHost(WebKit::WebCertificateInfo::create(WebCore::CertificateInfo((__bridge CFArrayRef)certificateChain)).ptr(), host);
}

- (void)_registerURLSchemeServiceWorkersCanHandle:(NSString *)scheme
{
    _processPool->registerURLSchemeServiceWorkersCanHandle(scheme);
}

- (void)_registerURLSchemeAsCanDisplayOnlyIfCanRequest:(NSString *)scheme
{
    _processPool->registerURLSchemeAsCanDisplayOnlyIfCanRequest(scheme);
}

- (void)_setMaximumNumberOfProcesses:(NSUInteger)value
{
    _processPool->setMaximumNumberOfProcesses(value);
}

- (void)_setCanHandleHTTPSServerTrustEvaluation:(BOOL)value
{
    _processPool->setCanHandleHTTPSServerTrustEvaluation(value);
}

static WebKit::HTTPCookieAcceptPolicy toHTTPCookieAcceptPolicy(NSHTTPCookieAcceptPolicy policy)
{
    switch (static_cast<NSUInteger>(policy)) {
    case NSHTTPCookieAcceptPolicyAlways:
        return WebKit::HTTPCookieAcceptPolicyAlways;
    case NSHTTPCookieAcceptPolicyNever:
        return WebKit::HTTPCookieAcceptPolicyNever;
    case NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        return WebKit::HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    case NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain:
        return WebKit::HTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return WebKit::HTTPCookieAcceptPolicyAlways;
}

- (void)_setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy
{
    _processPool->supplement<WebKit::WebCookieManagerProxy>()->setHTTPCookieAcceptPolicy(PAL::SessionID::defaultSessionID(), toHTTPCookieAcceptPolicy(policy), [](WebKit::CallbackBase::Error){});
}

- (id)_objectForBundleParameter:(NSString *)parameter
{
    return [_processPool->bundleParameters() objectForKey:parameter];
}

- (void)_setObject:(id <NSCopying, NSSecureCoding>)object forBundleParameter:(NSString *)parameter
{
    auto copy = adoptNS([(NSObject *)object copy]);
    auto keyedArchiver = secureArchiver();

    @try {
        [keyedArchiver encodeObject:copy.get() forKey:@"parameter"];
        [keyedArchiver finishEncoding];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to encode bundle parameter: %@", exception);
    }

    if (copy)
        [_processPool->ensureBundleParameters() setObject:copy.get() forKey:parameter];
    else
        [_processPool->ensureBundleParameters() removeObjectForKey:parameter];

    auto data = keyedArchiver.get().encodedData;
    _processPool->sendToAllProcesses(Messages::WebProcess::SetInjectedBundleParameter(parameter, IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length])));
}

- (void)_setObjectsForBundleParametersWithDictionary:(NSDictionary *)dictionary
{
    auto copy = adoptNS([[NSDictionary alloc] initWithDictionary:dictionary copyItems:YES]);
    auto keyedArchiver = secureArchiver();

    @try {
        [keyedArchiver encodeObject:copy.get() forKey:@"parameters"];
        [keyedArchiver finishEncoding];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to encode bundle parameters: %@", exception);
    }

    [_processPool->ensureBundleParameters() setValuesForKeysWithDictionary:copy.get()];

    auto data = keyedArchiver.get().encodedData;
    _processPool->sendToAllProcesses(Messages::WebProcess::SetInjectedBundleParameters(IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length])));
}

#if !TARGET_OS_IPHONE

#if ENABLE(NETSCAPE_PLUGIN_API)

static bool isPluginLoadClientPolicyAcceptable(unsigned policy)
{
    return policy <= WebCore::PluginLoadClientPolicyMaximum;
}
static HashMap<String, HashMap<String, HashMap<String, uint8_t>>> toPluginLoadClientPoliciesHashMap(NSDictionary* dictionary)
{
    __block HashMap<String, HashMap<String, HashMap<String, uint8_t>>> pluginLoadClientPolicies;
    [dictionary enumerateKeysAndObjectsUsingBlock:^(id nsHost, id nsPoliciesForHost, BOOL *stop) {
        if (![nsHost isKindOfClass:[NSString class]]) {
            RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
            return;
        }
        if (![nsPoliciesForHost isKindOfClass:[NSDictionary class]]) {
            RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
            return;
        }

        String host = (NSString *)nsHost;
        __block HashMap<String, HashMap<String, uint8_t>> policiesForHost;
        [nsPoliciesForHost enumerateKeysAndObjectsUsingBlock:^(id nsIdentifier, id nsVersionsToPolicies, BOOL *stop) {
            if (![nsIdentifier isKindOfClass:[NSString class]]) {
                RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
                return;
            }
            if (![nsVersionsToPolicies isKindOfClass:[NSDictionary class]]) {
                RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
                return;
            }

            String bundleIdentifier = (NSString *)nsIdentifier;
            __block HashMap<String, uint8_t> versionsToPolicies;
            [nsVersionsToPolicies enumerateKeysAndObjectsUsingBlock:^(id nsVersion, id nsPolicy, BOOL *stop) {
                if (![nsVersion isKindOfClass:[NSString class]]) {
                    RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
                    return;
                }
                if (![nsPolicy isKindOfClass:[NSNumber class]]) {
                    RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
                    return;
                }
                unsigned policy = ((NSNumber *)nsPolicy).unsignedIntValue;
                if (!isPluginLoadClientPolicyAcceptable(policy)) {
                    RELEASE_LOG_ERROR(Plugins, "_resetPluginLoadClientPolicies was called with dictionary in wrong format");
                    return;
                }
                String version = (NSString *)nsVersion;
                versionsToPolicies.add(version, static_cast<uint8_t>(policy));
            }];
            if (!versionsToPolicies.isEmpty())
                policiesForHost.add(bundleIdentifier, WTFMove(versionsToPolicies));
        }];
        if (!policiesForHost.isEmpty())
            pluginLoadClientPolicies.add(host, WTFMove(policiesForHost));
    }];
    return pluginLoadClientPolicies;
}

static NSDictionary *policiesHashMapToDictionary(const HashMap<String, HashMap<String, HashMap<String, uint8_t>>>& map)
{
    auto policies = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
    for (auto& hostPair : map) {
        NSString *host = hostPair.key;
        policies.get()[host] = adoptNS([[NSMutableDictionary alloc] initWithCapacity:hostPair.value.size()]).get();
        for (auto& bundleIdentifierPair : hostPair.value) {
            NSString *bundlerIdentifier = bundleIdentifierPair.key;
            policies.get()[host][bundlerIdentifier] = adoptNS([[NSMutableDictionary alloc] initWithCapacity:bundleIdentifierPair.value.size()]).get();
            for (auto& versionPair : bundleIdentifierPair.value) {
                NSString *version = versionPair.key;
                policies.get()[host][bundlerIdentifier][version] = adoptNS([[NSNumber alloc] initWithUnsignedInt:versionPair.value]).get();
            }
        }
    }
    return policies.autorelease();
}

#endif

- (void)_resetPluginLoadClientPolicies:(NSDictionary *)policies
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    _processPool->resetPluginLoadClientPolicies(toPluginLoadClientPoliciesHashMap(policies));
#endif
}

-(NSDictionary *)_pluginLoadClientPolicies
{
    auto& map = _processPool->pluginLoadClientPolicies();
    return policiesHashMapToDictionary(map);
}
#endif


- (id <_WKDownloadDelegate>)_downloadDelegate
{
    return _downloadDelegate.getAutoreleased();
}

- (void)_setDownloadDelegate:(id <_WKDownloadDelegate>)downloadDelegate
{
    _downloadDelegate = downloadDelegate;
    _processPool->setDownloadClient(std::make_unique<WebKit::DownloadClient>(downloadDelegate));
}

- (id <_WKAutomationDelegate>)_automationDelegate
{
    return _automationDelegate.getAutoreleased();
}

- (void)_setAutomationDelegate:(id <_WKAutomationDelegate>)automationDelegate
{
    _automationDelegate = automationDelegate;
    _processPool->setAutomationClient(std::make_unique<WebKit::AutomationClient>(self, automationDelegate));
}

- (void)_warmInitialProcess
{
    _processPool->prewarmProcess();
}

- (void)_automationCapabilitiesDidChange
{
    _processPool->updateAutomationCapabilities();
}

- (void)_setAutomationSession:(_WKAutomationSession *)automationSession
{
    _automationSession = automationSession;
    _processPool->setAutomationSession(automationSession ? automationSession->_session.get() : nullptr);
}

- (void)_addSupportedPlugin:(NSString *) domain named:(NSString *) name withMimeTypes: (NSSet<NSString *> *) nsMimeTypes withExtensions: (NSSet<NSString *> *) nsExtensions
{
    HashSet<String> mimeTypes;
    for (NSString *mimeType in nsMimeTypes)
        mimeTypes.add(mimeType);
    HashSet<String> extensions;
    for (NSString *extension in nsExtensions)
        extensions.add(extension);

    _processPool->addSupportedPlugin(domain, name, WTFMove(mimeTypes), WTFMove(extensions));
}

- (void)_clearSupportedPlugins
{
    _processPool->clearSupportedPlugins();
}

- (void)_terminateStorageProcess
{
    _processPool->terminateStorageProcessForTesting();
}

- (void)_terminateNetworkProcess
{
    _processPool->terminateNetworkProcess();
}

- (void)_terminateServiceWorkerProcesses
{
    _processPool->terminateServiceWorkerProcesses();
}

- (void)_disableServiceWorkerProcessTerminationDelay
{
    _processPool->disableServiceWorkerProcessTerminationDelay();
}

- (pid_t)_networkProcessIdentifier
{
    return _processPool->networkProcessIdentifier();
}

- (pid_t)_storageProcessIdentifier
{
    return _processPool->storageProcessIdentifier();
}

- (void)_syncNetworkProcessCookies
{
    _processPool->syncNetworkProcessCookies();
}

- (size_t)_webProcessCount
{
    return _processPool->processes().size();
}

- (void)_makeNextWebProcessLaunchFailForTesting
{
    _processPool->setShouldMakeNextWebProcessLaunchFailForTesting(true);
}

- (void)_makeNextNetworkProcessLaunchFailForTesting
{
    _processPool->setShouldMakeNextNetworkProcessLaunchFailForTesting(true);
}

- (BOOL)_hasPrewarmedWebProcess
{
    for (auto& process : _processPool->processes()) {
        if (process->isPrewarmed())
            return YES;
    }
    return NO;
}

- (size_t)_webProcessCountIgnoringPrewarmed
{
    return [self _webProcessCount] - ([self _hasPrewarmedWebProcess] ? 1 : 0);
}

- (size_t)_webPageContentProcessCount
{
    auto allWebProcesses = _processPool->processes();
#if ENABLE(SERVICE_WORKER)
    auto& serviceWorkerProcesses = _processPool->serviceWorkerProxies();
    if (serviceWorkerProcesses.isEmpty())
        return allWebProcesses.size();

    return allWebProcesses.size() - serviceWorkerProcesses.size();
#else
    return allWebProcesses.size();
#endif
}

- (void)_preconnectToServer:(NSURL *)serverURL
{
    _processPool->preconnectToServer(serverURL);
}

- (size_t)_pluginProcessCount
{
#if !PLATFORM(IOS)
    return WebKit::PluginProcessManager::singleton().pluginProcesses().size();
#else
    return 0;
#endif
}

- (size_t)_serviceWorkerProcessCount
{
#if ENABLE(SERVICE_WORKER)
    return _processPool->serviceWorkerProxies().size();
#else
    return 0;
#endif
}

+ (void)_forceGameControllerFramework
{
#if ENABLE(GAMEPAD)
    WebKit::UIGamepadProvider::setUsesGameControllerFramework();
#endif
}

- (BOOL)_isCookieStoragePartitioningEnabled
{
    return _processPool->cookieStoragePartitioningEnabled();
}

- (void)_setCookieStoragePartitioningEnabled:(BOOL)enabled
{
    _processPool->setCookieStoragePartitioningEnabled(enabled);
}

- (BOOL)_isStorageAccessAPIEnabled
{
    return _processPool->storageAccessAPIEnabled();
}

- (void)_setStorageAccessAPIEnabled:(BOOL)enabled
{
    _processPool->setStorageAccessAPIEnabled(enabled);
}

- (void)_setAllowsAnySSLCertificateForServiceWorker:(BOOL) allows
{
#if ENABLE(SERVICE_WORKER)
    _processPool->setAllowsAnySSLCertificateForServiceWorker(allows);
#endif
}

#if PLATFORM(IOS)
- (id <_WKGeolocationCoreLocationProvider>)_coreLocationProvider
{
    return _coreLocationProvider.get();
}

- (void)_setCoreLocationProvider:(id<_WKGeolocationCoreLocationProvider>)coreLocationProvider
{
    if (_geolocationProvider)
        [NSException raise:NSGenericException format:@"Changing the location provider is not supported after a web view in the process pool has begun servicing geolocation requests."];

    _coreLocationProvider = coreLocationProvider;
}
#endif // PLATFORM(IOS)

- (_WKDownload *)_downloadURLRequest:(NSURLRequest *)request
{
    return (_WKDownload *)_processPool->download(nullptr, request)->wrapper();
}

- (_WKDownload *)_resumeDownloadFromData:(NSData *)resumeData path:(NSString *)path
{
    return wrapper(_processPool->resumeDownload(API::Data::createWithoutCopying(resumeData).ptr(), path));
}

@end

#endif // WK_API_ENABLED
