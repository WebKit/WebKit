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
#import <wtf/NeverDestroyed.h>
#import <wtf/cocoa/SpanCocoa.h>

static RetainPtr<SecTrustRef>& allowedLocalTestServerTrust()
{
    static NeverDestroyed<RetainPtr<SecTrustRef>> serverTrust;
    return serverTrust.get();
}

static bool trustsServerForLocalTests(NSURLAuthenticationChallenge *challenge)
{
    if (![challenge.protectionSpace.host isEqualToString:@"127.0.0.1"]
        || !allowedLocalTestServerTrust())
        return false;

    return WebCore::certificatesMatch(allowedLocalTestServerTrust().get(), challenge.protectionSpace.serverTrust);
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
    if (WebCore::MIMETypeRegistry::isSupportedJSONMIMEType(response.MIMEType))
        return completionHandler(NSURLSessionResponseAllow);
    completionHandler(NSURLSessionResponseCancel);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]
        && trustsServerForLocalTests(challenge))
        return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}

@end

namespace WebKit::PCM {

enum class LoadTaskIdentifierType { };
using LoadTaskIdentifier = ObjectIdentifier<LoadTaskIdentifierType>;
static HashMap<LoadTaskIdentifier, RetainPtr<NSURLSessionDataTask>>& taskMap()
{
    static NeverDestroyed<HashMap<LoadTaskIdentifier, RetainPtr<NSURLSessionDataTask>>> map;
    return map.get();
}

static NSURLSession *statelessSessionWithoutRedirects()
{
    static NeverDestroyed<RetainPtr<WKNetworkSessionDelegateAllowingOnlyNonRedirectedJSON>> delegate = adoptNS([WKNetworkSessionDelegateAllowingOnlyNonRedirectedJSON new]);
    static NeverDestroyed<RetainPtr<NSURLSession>> session = [&] {
        NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
        configuration.URLCredentialStorage = nil;
        configuration.URLCache = nil;
        configuration.HTTPCookieStorage = nil;
        configuration._shouldSkipPreferredClientCertificateLookup = YES;
        return [NSURLSession sessionWithConfiguration:configuration delegate:delegate.get().get() delegateQueue:[NSOperationQueue mainQueue]];
    }();
    return session.get().get();
}

void NetworkLoader::allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo& certificateInfo)
{
    allowedLocalTestServerTrust() = certificateInfo.trust();
}

void NetworkLoader::start(URL&& url, RefPtr<JSON::Object>&& jsonPayload, WebCore::PrivateClickMeasurement::PcmDataCarried pcmDataCarried, Callback&& callback)
{
    // Prevent contacting non-local servers when a test certificate chain is used for 127.0.0.1.
    // FIXME: Use a proxy server to have tests cover the reports sent to the destination, too.
    if (allowedLocalTestServerTrust() && url.host() != "127.0.0.1"_s)
        return callback({ }, { });

    auto request = adoptNS([[NSMutableURLRequest alloc] initWithURL:url]);
    [request setValue:WebCore::HTTPHeaderValues::maxAge0() forHTTPHeaderField:@"Cache-Control"];
    [request setValue:WebCore::standardUserAgentWithApplicationName({ }) forHTTPHeaderField:@"User-Agent"];
    if (jsonPayload) {
        request.get().HTTPMethod = @"POST";
        [request setValue:WebCore::HTTPHeaderValues::applicationJSONContentType() forHTTPHeaderField:@"Content-Type"];
        auto body = jsonPayload->toJSONString().utf8();
        request.get().HTTPBody = toNSData(body.span()).get();
    }

    setPCMDataCarriedOnRequest(pcmDataCarried, request.get());

    auto identifier = LoadTaskIdentifier::generate();
    NSURLSessionDataTask *task = [statelessSessionWithoutRedirects() dataTaskWithRequest:request.get() completionHandler:makeBlockPtr([callback = WTFMove(callback), identifier](NSData *data, NSURLResponse *response, NSError *error) mutable {
        taskMap().remove(identifier);
        if (error)
            return callback(error.localizedDescription, { });
        if (auto jsonValue = JSON::Value::parseJSON(String::fromUTF8(span(data))))
            return callback({ }, jsonValue->asObject());
        callback({ }, nullptr);
    }).get()];
    [task resume];
    taskMap().add(identifier, task);
}

} // namespace WebKit::PCM
