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
#import "HistoryClient.h"
#import "ProcessModel.h"
#import "WKObject.h"
#import "WKProcessPoolConfigurationPrivate.h"
#import "WebCertificateInfo.h"
#import "WebContext.h"
#import <WebCore/CertificateInfo.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThreadSystemInterface.h>
#endif

@implementation WKProcessPool

- (instancetype)init
{
    return [self initWithConfiguration:adoptNS([[WKProcessPoolConfiguration alloc] init]).get()];
}

- (instancetype)initWithConfiguration:(WKProcessPoolConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    _configuration = adoptNS([configuration copy]);

#if PLATFORM(IOS)
    // FIXME: Remove once <rdar://problem/15256572> is fixed.
    InitWebCoreThreadSystemInterface();
#endif

    String bundlePath;
    if (NSURL *bundleURL = [_configuration _injectedBundleURL]) {
        if (!bundleURL.isFileURL)
            [NSException raise:NSInvalidArgumentException format:@"Injected Bundle URL must be a file URL"];

        bundlePath = bundleURL.path;
    }

    API::Object::constructInWrapper<WebKit::WebContext>(self, bundlePath);
    _context->setHistoryClient(std::make_unique<WebKit::HistoryClient>());
    _context->setUsesNetworkProcess(true);
    _context->setProcessModel(WebKit::ProcessModelMultipleSecondaryProcesses);

    // FIXME: Add a way to configure the cache model, see <rdar://problem/16206857>.
    _context->setCacheModel(WebKit::CacheModelPrimaryWebBrowser);

    return self;
}

- (void)dealloc
{
    _context->~WebContext();

    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; configuration = %@>", NSStringFromClass(self.class), self, _configuration.get()];
}

- (WKProcessPoolConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (API::Object&)_apiObject
{
    return *_context;
}

@end

@implementation WKProcessPool (WKPrivate)

- (void)_setAllowsSpecificHTTPSCertificate:(NSArray *)certificateChain forHost:(NSString *)host
{
    _context->allowSpecificHTTPSCertificateForHost(WebKit::WebCertificateInfo::create(WebCore::CertificateInfo((CFArrayRef)certificateChain)).get(), host);
}

@end

#endif // WK_API_ENABLED
