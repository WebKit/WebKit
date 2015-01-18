/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "CacheModel.h"
#import "DownloadClient.h"
#import "ProcessModel.h"
#import "SandboxUtilities.h"
#import "WKObject.h"
#import "WeakObjCPtr.h"
#import "WebCertificateInfo.h"
#import "WebCookieManagerProxy.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import "_WKDownloadDelegate.h"
#import "_WKProcessPoolConfiguration.h"
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/CertificateInfo.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThreadSystemInterface.h>
#import "WKGeolocationProviderIOS.h"
#endif

// FIXME: Move this from WKProcessPool to APIWebsiteDataStoreCocoa.
NSURL *websiteDataDirectoryURL(NSString *directoryName);

@implementation WKProcessPool {
    WebKit::WeakObjCPtr<id <_WKDownloadDelegate>> _downloadDelegate;

#if PLATFORM(IOS)
    RetainPtr<WKGeolocationProviderIOS> _geolocationProvider;
#endif // PLATFORM(IOS)
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
    return [NSString stringWithFormat:@"<%@: %p; configuration = %@>", NSStringFromClass(self.class), self, _configuration.get()];
}

- (_WKProcessPoolConfiguration *)_configuration
{
    return [[_configuration copy] autorelease];
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

NSURL *websiteDataDirectoryURL(NSString *directoryName)
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

    return url;
}

- (instancetype)_initWithConfiguration:(_WKProcessPoolConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    _configuration = adoptNS([configuration copy]);

#if PLATFORM(IOS)
    // FIXME: Remove once <rdar://problem/15256572> is fixed.
    InitWebCoreThreadSystemInterface();
#endif

    WebKit::WebProcessPoolConfiguration processPoolConfiguration;

    if (NSURL *bundleURL = [_configuration injectedBundleURL]) {
        if (!bundleURL.isFileURL)
            [NSException raise:NSInvalidArgumentException format:@"Injected Bundle URL must be a file URL"];

        processPoolConfiguration.injectedBundlePath = bundleURL.path;
    }

    processPoolConfiguration.localStorageDirectory = websiteDataDirectoryURL(@"LocalStorage").absoluteURL.path.fileSystemRepresentation;
    processPoolConfiguration.webSQLDatabaseDirectory = websiteDataDirectoryURL(@"WebSQL").absoluteURL.path.fileSystemRepresentation;
    processPoolConfiguration.indexedDBDatabaseDirectory = websiteDataDirectoryURL(@"IndexedDB").absoluteURL.path.fileSystemRepresentation;
    processPoolConfiguration.mediaKeysStorageDirectory = websiteDataDirectoryURL(@"MediaKeys").absoluteURL.path.fileSystemRepresentation;

    API::Object::constructInWrapper<WebKit::WebProcessPool>(self, WTF::move(processPoolConfiguration));
    _processPool->setUsesNetworkProcess(true);
    _processPool->setProcessModel(WebKit::ProcessModelMultipleSecondaryProcesses);
    _processPool->setMaximumNumberOfProcesses([_configuration maximumProcessCount]);

#if ENABLE(CACHE_PARTITIONING)
    for (NSString *urlScheme in [_configuration cachePartitionedURLSchemes])
        _processPool->registerURLSchemeAsCachePartitioned(urlScheme);
#endif

    // FIXME: Add a way to configure the cache model, see <rdar://problem/16206857>.
    _processPool->setCacheModel(WebKit::CacheModelPrimaryWebBrowser);

    return self;
}

- (void)_setAllowsSpecificHTTPSCertificate:(NSArray *)certificateChain forHost:(NSString *)host
{
    _processPool->allowSpecificHTTPSCertificateForHost(WebKit::WebCertificateInfo::create(WebCore::CertificateInfo((CFArrayRef)certificateChain)).get(), host);
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

- (id <_WKDownloadDelegate>)_downloadDelegate
{
    return _downloadDelegate.getAutoreleased();
}

- (void)_setDownloadDelegate:(id <_WKDownloadDelegate>)downloadDelegate
{
    _downloadDelegate = downloadDelegate;
    _processPool->setDownloadClient(std::make_unique<WebKit::DownloadClient>(downloadDelegate));
}

@end

#endif // WK_API_ENABLED
