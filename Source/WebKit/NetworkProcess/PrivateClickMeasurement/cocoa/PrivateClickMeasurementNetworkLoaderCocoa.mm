/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "PrivateClickMeasurementNetworkLoader.h"

#import "NetworkDataTaskCocoa.h"
#import <WebCore/HTTPHeaderValues.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/UserAgent.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Borrow.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/cocoa/SpanCocoa.h>

NODELETE static RetainPtr<SecTrustRef>& allowedLocalTestServerTrust()
{
    static NeverDestroyed<RetainPtr<SecTrustRef>> serverTrust;
    return serverTrust.get();
}

static bool trustsServerForLocalTests(NSURLAuthenticationChallenge *challenge)
{
    if (![challenge.protectionSpace.host isEqualToString:@"127.0.0.1"]
        || !allowedLocalTestServerTrust())
        return false;

    return WebCore::certificatesMatch(allowedLocalTestServerTrust().get(), RetainPtr { challenge.protectionSpace.serverTrust }.get());
}

@interface WKNetworkSessionDelegateAllowingOnlyNonRedirectedJSON : NSObject <NSURLSessionDataDelegate>
@end

@implementation WKNetworkSessionDelegateAllowingOnlyNonRedirectedJSON

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    completionHandler(nil);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    if (WebCore::MIMETypeRegistry::isSupportedJSONMIMEType(StringImpl::create(response.MIMEType).get()))
        return completionHandler(NSURLSessionResponseAllow);
    completionHandler(NSURLSessionResponseCancel);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]
        && trustsServerForLocalTests(challenge))
        return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust: RetainPtr { challenge.protectionSpace.serverTrust }.get()]);
    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}

@end

namespace WebKit::PCM {

enum class LoadTaskIdentifierType { };
using LoadTaskIdentifier = ObjectIdentifier<LoadTaskIdentifierType>;
NODELETE static HashMap<LoadTaskIdentifier, RetainPtr<NSURLSessionDataTask>>& taskMap()
{
    static NeverDestroyed<HashMap<LoadTaskIdentifier, RetainPtr<NSURLSessionDataTask>>> map;
    return map.get();
}

static NSURLSession *statelessSessionWithoutRedirectsSingleton(const ApplicationBundleIdentifiersOrAuditToken& applicationBundleIdentifier)
{
    static NeverDestroyed<RetainPtr<WKNetworkSessionDelegateAllowingOnlyNonRedirectedJSON>> delegate = adoptNS([WKNetworkSessionDelegateAllowingOnlyNonRedirectedJSON new]);
    static NeverDestroyed<RetainPtr<NSURLSession>> session = [&] {
        RetainPtr configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];

        configuration.get().HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
        configuration.get().URLCredentialStorage = nil;
        configuration.get().URLCache = nil;
        configuration.get().HTTPCookieStorage = nil;
        configuration.get()._shouldSkipPreferredClientCertificateLookup = YES;

        WTF::switchOn(applicationBundleIdentifier,
            [&] (const std::pair<RefPtr<StringImpl>, RefPtr<StringImpl>>& bundleIdentifiers) {
                configuration.get()._sourceApplicationBundleIdentifier = createNSString(bundleIdentifiers.first).get();
                configuration.get()._sourceApplicationSecondaryIdentifier = createNSString(bundleIdentifiers.second).get();
            }, [&] (const Vector<uint8_t>& auditToken) {
                Borrow borrowedAuditToken = auditToken;
                configuration.get()._sourceApplicationAuditTokenData = [NSData dataWithBytes:borrowedAuditToken->span().data() length:borrowedAuditToken->size()];
            }
        );

        return [NSURLSession sessionWithConfiguration:configuration.get() delegate:delegate.get().get() delegateQueue:[NSOperationQueue mainQueue]];
    }();
    return session.get().get();
}

void NetworkLoader::allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo& certificateInfo)
{
    allowedLocalTestServerTrust() = certificateInfo.trust();
}

void NetworkLoader::start(URL&& url, RefPtr<JSON::Object>&& jsonPayload, WebCore::PrivateClickMeasurement::PcmDataCarried pcmDataCarried, const ApplicationBundleIdentifiersOrAuditToken& applicationBundleIdentifier, Callback&& callback)
{
    // Prevent contacting non-local servers when a test certificate chain is used for 127.0.0.1.
    // FIXME: Use a proxy server to have tests cover the reports sent to the destination, too.
    if (allowedLocalTestServerTrust() && url.host() != "127.0.0.1"_s)
        return callback(nullptr, { });

    auto request = adoptNS([[NSMutableURLRequest alloc] initWithURL:url.createNSURL().get()]);
    [request setValue:createNSString(WebCore::HTTPHeaderValues::maxAge0()).get() forHTTPHeaderField:@"Cache-Control"];
    [request setValue:createNSString(WebCore::standardUserAgentWithApplicationName(nullptr)).get() forHTTPHeaderField:@"User-Agent"];
    RetainPtr crossSiteMainDocument = [NSURLComponents componentsWithURL:request.get().URL resolvingAgainstBaseURL:NO];
    crossSiteMainDocument.get().host = [NSString stringWithFormat:@"not-%@", crossSiteMainDocument.get().host];
    [request setMainDocumentURL:crossSiteMainDocument.get().URL];
    [request setAttribution:NSURLRequestAttributionUser];

    if (jsonPayload) {
        request.get().HTTPMethod = @"POST";
        [request setValue:createNSString(WebCore::HTTPHeaderValues::applicationJSONContentType()).get() forHTTPHeaderField:@"Content-Type"];
        auto body = utf8(jsonPayload->toJSONString());
        request.get().HTTPBody = toNSData(byteCast<uint8_t>(body.span())).get();
    }

    setPCMDataCarriedOnRequest(pcmDataCarried, request.get());

    auto identifier = LoadTaskIdentifier::generate();
    RetainPtr task = [statelessSessionWithoutRedirectsSingleton(applicationBundleIdentifier) dataTaskWithRequest:request.get() completionHandler:makeBlockPtr([callback = WTF::move(callback), identifier](NSData *data, NSURLResponse *response, NSError *error) mutable {
        taskMap().remove(identifier);
        if (error)
            return callback(StringImpl::create(error.localizedDescription), { });
        if (auto jsonValue = JSON::Value::parseJSON(StringImpl::fromUTF8(span(data))))
            return callback(nullptr, jsonValue->asObject());
        callback(nullptr, nullptr);
    }).get()];
    [task resume];
    taskMap().add(identifier, task.get());
}

} // namespace WebKit::PCM
