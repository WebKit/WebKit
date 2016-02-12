/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
#import "SandboxUtilities.h"
#import "WKObject.h"
#import "WeakObjCPtr.h"
#import "WebCertificateInfo.h"
#import "WebCookieManagerProxy.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import "_WKAutomationDelegate.h"
#import "_WKAutomationSessionInternal.h"
#import "_WKDownloadDelegate.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/CertificateInfo.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThreadSystemInterface.h>
#import "WKGeolocationProviderIOS.h"
#endif

@implementation WKProcessPool {
    WebKit::WeakObjCPtr<id <_WKAutomationDelegate>> _automationDelegate;
    WebKit::WeakObjCPtr<id <_WKDownloadDelegate>> _downloadDelegate;

    RetainPtr<_WKAutomationSession> _automationSession;
#if PLATFORM(IOS)
    RetainPtr<WKGeolocationProviderIOS> _geolocationProvider;
#endif // PLATFORM(IOS)
}

- (instancetype)_initWithConfiguration:(_WKProcessPoolConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

#if PLATFORM(IOS)
    // FIXME: Remove once <rdar://problem/15256572> is fixed.
    InitWebCoreThreadSystemInterface();
#endif

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

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; configuration = %@>", NSStringFromClass(self.class), self, wrapper(_processPool->configuration())];
}

- (_WKProcessPoolConfiguration *)_configuration
{
    return wrapper(_processPool->configuration().copy().leakRef());
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
    _processPool->allowSpecificHTTPSCertificateForHost(WebKit::WebCertificateInfo::create(WebCore::CertificateInfo((CFArrayRef)certificateChain)).ptr(), host);
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
    _processPool->supplement<WebKit::WebCookieManagerProxy>()->setHTTPCookieAcceptPolicy(toHTTPCookieAcceptPolicy(policy));
}

- (id)_objectForBundleParameter:(NSString *)parameter
{
    return [_processPool->bundleParameters() objectForKey:parameter];
}

- (void)_setObject:(id <NSCopying, NSSecureCoding>)object forBundleParameter:(NSString *)parameter
{
    auto copy = adoptNS([(NSObject *)object copy]);

    auto data = adoptNS([[NSMutableData alloc] init]);
    auto keyedArchiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [keyedArchiver setRequiresSecureCoding:YES];

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

    _processPool->sendToAllProcesses(Messages::WebProcess::SetInjectedBundleParameter(parameter, IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length])));
}

- (void)_setObjectsForBundleParametersWithDictionary:(NSDictionary *)dictionary
{
    auto copy = adoptNS([[NSDictionary alloc] initWithDictionary:dictionary copyItems:YES]);

    auto data = adoptNS([[NSMutableData alloc] init]);
    auto keyedArchiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [keyedArchiver setRequiresSecureCoding:YES];

    @try {
        [keyedArchiver encodeObject:copy.get() forKey:@"parameters"];
        [keyedArchiver finishEncoding];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to encode bundle parameters: %@", exception);
    }

    [_processPool->ensureBundleParameters() setValuesForKeysWithDictionary:copy.get()];

    _processPool->sendToAllProcesses(Messages::WebProcess::SetInjectedBundleParameters(IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length])));
}


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
    _processPool->warmInitialProcess();
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

@end

#endif // WK_API_ENABLED
