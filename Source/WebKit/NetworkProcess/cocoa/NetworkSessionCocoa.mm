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
#import "NetworkSessionCocoa.h"

#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationManager.h"
#import "DataReference.h"
#import "Download.h"
#import "LegacyCustomProtocolManager.h"
#import "Logging.h"
#import "NetworkLoad.h"
#import "NetworkProcess.h"
#import "NetworkSessionCreationParameters.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/Credential.h>
#import <WebCore/FormDataStreamMac.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/WebCoreURLResponse.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/text/WTFString.h>

using namespace WebKit;

CFStringRef const WebKit2HTTPProxyDefaultsKey = static_cast<CFStringRef>(@"WebKit2HTTPProxy");
CFStringRef const WebKit2HTTPSProxyDefaultsKey = static_cast<CFStringRef>(@"WebKit2HTTPSProxy");

static NSURLSessionResponseDisposition toNSURLSessionResponseDisposition(WebCore::PolicyAction disposition)
{
    switch (disposition) {
    case WebCore::PolicyAction::Ignore:
        return NSURLSessionResponseCancel;
    case WebCore::PolicyAction::Use:
        return NSURLSessionResponseAllow;
    case WebCore::PolicyAction::Download:
        return NSURLSessionResponseBecomeDownload;
    }
}

static NSURLSessionAuthChallengeDisposition toNSURLSessionAuthChallengeDisposition(WebKit::AuthenticationChallengeDisposition disposition)
{
    switch (disposition) {
    case WebKit::AuthenticationChallengeDisposition::UseCredential:
        return NSURLSessionAuthChallengeUseCredential;
    case WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling:
        return NSURLSessionAuthChallengePerformDefaultHandling;
    case WebKit::AuthenticationChallengeDisposition::Cancel:
        return NSURLSessionAuthChallengeCancelAuthenticationChallenge;
    case WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue:
        return NSURLSessionAuthChallengeRejectProtectionSpace;
    }
}

static WebCore::NetworkLoadPriority toNetworkLoadPriority(float priority)
{
    if (priority <= NSURLSessionTaskPriorityLow)
        return WebCore::NetworkLoadPriority::Low;
    if (priority >= NSURLSessionTaskPriorityHigh)
        return WebCore::NetworkLoadPriority::High;
    return WebCore::NetworkLoadPriority::Medium;
}

#if HAVE(CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER)
static String stringForSSLProtocol(SSLProtocol protocol)
{
    switch (protocol) {
    case kDTLSProtocol1:
        return "DTLS 1.0"_s;
    case kSSLProtocol2:
        return "SSL 2.0"_s;
    case kSSLProtocol3:
        return "SSL 3.0"_s;
    case kSSLProtocol3Only:
        return "SSL 3.0 (Only)"_s;
    case kTLSProtocol1:
        return "TLS 1.0"_s;
    case kTLSProtocol1Only:
        return "TLS 1.0 (Only)"_s;
    case kTLSProtocol11:
        return "TLS 1.1"_s;
    case kTLSProtocol12:
        return "TLS 1.2"_s;
    case kTLSProtocol13:
        return "TLS 1.3"_s;
    case kSSLProtocolAll:
        return "All";
    case kSSLProtocolUnknown:
        return "Unknown";
    case kTLSProtocolMaxSupported:
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

static String stringForSSLCipher(SSLCipherSuite cipher)
{
#define STRINGIFY_CIPHER(cipher) \
    case cipher: \
        return "" #cipher ""_s

    switch (cipher) {
    STRINGIFY_CIPHER(SSL_RSA_EXPORT_WITH_RC4_40_MD5);
    STRINGIFY_CIPHER(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_IDEA_CBC_SHA);
    STRINGIFY_CIPHER(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_RSA_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_DSS_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_RSA_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_DSS_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DHE_RSA_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5);
    STRINGIFY_CIPHER(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA);
    STRINGIFY_CIPHER(SSL_DH_anon_WITH_DES_CBC_SHA);
    STRINGIFY_CIPHER(SSL_FORTEZZA_DMS_WITH_NULL_SHA);
    STRINGIFY_CIPHER(SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_NULL_WITH_NULL_NULL);
    STRINGIFY_CIPHER(TLS_NULL_WITH_NULL_NULL);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_NULL_MD5);
    STRINGIFY_CIPHER(TLS_RSA_WITH_NULL_MD5);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_NULL_SHA);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_RC4_128_MD5);
    STRINGIFY_CIPHER(TLS_RSA_WITH_RC4_128_MD5);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_RC4_128_SHA);
    // STRINGIFY_CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256);
    // STRINGIFY_CIPHER(SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA);
    // STRINGIFY_CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256);
    // STRINGIFY_CIPHER(SSL_DH_anon_WITH_RC4_128_MD5);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_RC4_128_MD5);
    // STRINGIFY_CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_RC4_128_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_128_CBC_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_256_CBC_SHA);
    STRINGIFY_CIPHER(TLS_PSK_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_NULL_SHA);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DH_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_DSS_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DH_DSS_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DH_anon_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_PSK_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_PSK_WITH_NULL_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_DHE_PSK_WITH_NULL_SHA384);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_NULL_SHA256);
    STRINGIFY_CIPHER(TLS_RSA_PSK_WITH_NULL_SHA384);
    STRINGIFY_CIPHER(TLS_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_CHACHA20_POLY1305_SHA256);
    STRINGIFY_CIPHER(TLS_AES_128_CCM_SHA256);
    STRINGIFY_CIPHER(TLS_AES_128_CCM_8_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256);
    STRINGIFY_CIPHER(TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384);
    STRINGIFY_CIPHER(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
    STRINGIFY_CIPHER(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
    STRINGIFY_CIPHER(TLS_EMPTY_RENEGOTIATION_INFO_SCSV);
    STRINGIFY_CIPHER(SSL_RSA_WITH_RC2_CBC_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_IDEA_CBC_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_DES_CBC_MD5);
    STRINGIFY_CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_MD5);
    STRINGIFY_CIPHER(SSL_NO_SUCH_CIPHERSUITE);
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }

#undef STRINGIFY_CIPHER
}
#endif

@interface WKNetworkSessionDelegate : NSObject <NSURLSessionDataDelegate> {
    RefPtr<WebKit::NetworkSessionCocoa> _session;
    bool _withCredentials;
}

- (id)initWithNetworkSession:(WebKit::NetworkSessionCocoa&)session withCredentials:(bool)withCredentials;
- (void)sessionInvalidated;

@end

@implementation WKNetworkSessionDelegate

- (id)initWithNetworkSession:(WebKit::NetworkSessionCocoa&)session withCredentials:(bool)withCredentials
{
    self = [super init];
    if (!self)
        return nil;

    _session = &session;
    _withCredentials = withCredentials;

    return self;
}

- (void)sessionInvalidated
{
    _session = nullptr;
}

- (NetworkDataTaskCocoa*)existingTask:(NSURLSessionTask *)task
{
    if (!_session)
        return nullptr;

    if (!task)
        return nullptr;

    auto storedCredentialsPolicy = _withCredentials ? WebCore::StoredCredentialsPolicy::Use : WebCore::StoredCredentialsPolicy::DoNotUse;
    return _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentialsPolicy);
}

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error
{
    ASSERT(!_session);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend
{
    if (auto* networkDataTask = [self existingTask:task])
        networkDataTask->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task needNewBodyStream:(void (^)(NSInputStream *bodyStream))completionHandler
{
    auto* networkDataTask = [self existingTask:task];
    if (!networkDataTask) {
        completionHandler(nil);
        return;
    }

    auto* body = networkDataTask->firstRequest().httpBody();
    if (!body) {
        completionHandler(nil);
        return;
    }

    completionHandler(WebCore::createHTTPBodyNSInputStream(*body).get());
}

#if HAVE(CFNETWORK_WITH_IGNORE_HSTS) && ENABLE(RESOURCE_LOAD_STATISTICS)
static NSURLRequest* downgradeRequest(NSURLRequest *request)
{
    NSMutableURLRequest *nsMutableRequest = [[request mutableCopy] autorelease];
    if ([nsMutableRequest.URL.scheme isEqualToString:@"https"]) {
        NSURLComponents *components = [NSURLComponents componentsWithURL:nsMutableRequest.URL resolvingAgainstBaseURL:NO];
        components.scheme = @"http";
        [nsMutableRequest setURL:components.URL];
        ASSERT([nsMutableRequest.URL.scheme isEqualToString:@"http"]);
        return nsMutableRequest;
    }

    ASSERT_NOT_REACHED();
    return request;
}

static bool schemeWasUpgradedDueToDynamicHSTS(NSURLRequest *request)
{
    return [request respondsToSelector:@selector(_schemeWasUpgradedDueToDynamicHSTS)]
        && [request _schemeWasUpgradedDueToDynamicHSTS];
}
#endif

#if HAVE(CFNETWORK_WITH_IGNORE_HSTS)
static void setIgnoreHSTS(NSMutableURLRequest *request, bool ignoreHSTS)
{
    if ([request respondsToSelector:@selector(_setIgnoreHSTS:)])
        [request _setIgnoreHSTS:ignoreHSTS];
}

static bool ignoreHSTS(NSURLRequest *request)
{
    return [request respondsToSelector:@selector(_ignoreHSTS)]
        && [request _ignoreHSTS];
}
#endif

static NSURLRequest* updateIgnoreStrictTransportSecuritySettingIfNecessary(NSURLRequest *request, bool shouldIgnoreHSTS)
{
#if HAVE(CFNETWORK_WITH_IGNORE_HSTS)
    if ([request.URL.scheme isEqualToString:@"https"] && shouldIgnoreHSTS && ignoreHSTS(request)) {
        // The request was upgraded for some other reason than HSTS.
        // Don't ignore HSTS to avoid the risk of another downgrade.
        NSMutableURLRequest *nsMutableRequest = [[request mutableCopy] autorelease];
        setIgnoreHSTS(nsMutableRequest, false);
        return nsMutableRequest;
    }
    
    if ([request.URL.scheme isEqualToString:@"http"] && ignoreHSTS(request) != shouldIgnoreHSTS) {
        NSMutableURLRequest *nsMutableRequest = [[request mutableCopy] autorelease];
        setIgnoreHSTS(nsMutableRequest, shouldIgnoreHSTS);
        return nsMutableRequest;
    }
#else
    UNUSED_PARAM(shouldIgnoreHSTS);
#endif

    return request;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu willPerformHTTPRedirection from %s to %s", taskIdentifier, response.URL.absoluteString.UTF8String, request.URL.absoluteString.UTF8String);

    if (auto* networkDataTask = [self existingTask:task]) {
        auto completionHandlerCopy = Block_copy(completionHandler);

        bool shouldIgnoreHSTS = false;
#if HAVE(CFNETWORK_WITH_IGNORE_HSTS) && ENABLE(RESOURCE_LOAD_STATISTICS)        
        shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request) && _session->networkProcess().storageSession(_session->sessionID())->shouldBlockCookies(request, networkDataTask->frameID(), networkDataTask->pageID());
        if (shouldIgnoreHSTS) {
            request = downgradeRequest(request);
            ASSERT([request.URL.scheme isEqualToString:@"http"]);
            LOG(NetworkSession, "%llu Downgraded %s from https to http", taskIdentifier, request.URL.absoluteString.UTF8String);
        }
#endif

        networkDataTask->willPerformHTTPRedirection(response, request, [completionHandlerCopy, taskIdentifier, shouldIgnoreHSTS](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%llu willPerformHTTPRedirection completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            auto nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
            nsRequest = updateIgnoreStrictTransportSecuritySettingIfNecessary(nsRequest, shouldIgnoreHSTS);
            completionHandlerCopy(nsRequest);
            Block_release(completionHandlerCopy);
        });
    } else {
        LOG(NetworkSession, "%llu willPerformHTTPRedirection completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask*)task _schemeUpgraded:(NSURLRequest*)request completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu _schemeUpgraded %s", taskIdentifier, request.URL.absoluteString.UTF8String);

    if (auto* networkDataTask = [self existingTask:task]) {
        bool shouldIgnoreHSTS = false;
#if HAVE(CFNETWORK_WITH_IGNORE_HSTS) && ENABLE(RESOURCE_LOAD_STATISTICS)
        shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request) && _session->networkProcess().storageSession(_session->sessionID())->shouldBlockCookies(request, networkDataTask->frameID(), networkDataTask->pageID());
        if (shouldIgnoreHSTS) {
            request = downgradeRequest(request);
            ASSERT([request.URL.scheme isEqualToString:@"http"]);
            LOG(NetworkSession, "%llu Downgraded %s from https to http", taskIdentifier, request.URL.absoluteString.UTF8String);
        }
#endif

        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->willPerformHTTPRedirection(WebCore::synthesizeRedirectResponseIfNecessary([task currentRequest], request, nil), request, [completionHandlerCopy, taskIdentifier, shouldIgnoreHSTS](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%llu _schemeUpgraded completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            auto nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
            nsRequest = updateIgnoreStrictTransportSecuritySettingIfNecessary(nsRequest, shouldIgnoreHSTS);
            completionHandlerCopy(nsRequest);
            Block_release(completionHandlerCopy);
        });
    } else {
        LOG(NetworkSession, "%llu _schemeUpgraded completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask willCacheResponse:(NSCachedURLResponse *)proposedResponse completionHandler:(void (^)(NSCachedURLResponse *cachedResponse))completionHandler
{
    if (!_session) {
        completionHandler(nil);
        return;
    }

    // FIXME: remove if <rdar://problem/20001985> is ever resolved.
    if ([proposedResponse.response respondsToSelector:@selector(allHeaderFields)]
        && [[(id)proposedResponse.response allHeaderFields] objectForKey:@"Content-Range"])
        completionHandler(nil);
    else
        completionHandler(proposedResponse);
}

#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
static bool canNSURLSessionTrustEvaluate()
{
    return [NSURLSession respondsToSelector:@selector(_strictTrustEvaluate: queue: completionHandler:)];
}

static inline void processServerTrustEvaluation(NetworkSessionCocoa *session, NSURLAuthenticationChallenge *challenge, OSStatus trustResult, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, NetworkDataTaskCocoa* networkDataTask, CompletionHandler<void(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential)>&& completionHandler)
{
    if (trustResult == noErr) {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    session->continueDidReceiveChallenge(challenge, taskIdentifier, networkDataTask, [completionHandler = WTFMove(completionHandler), secTrust = retainPtr(challenge.protectionSpace.serverTrust)] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        // FIXME: UIProcess should send us back non nil credentials but the credential IPC encoder currently only serializes ns credentials for username/password.
        if (disposition == WebKit::AuthenticationChallengeDisposition::UseCredential && !credential.nsCredential()) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust: secTrust.get()]);
            return;
        }
        completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
    });
}
#endif

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    if (!_session) {
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveChallenge", taskIdentifier);
    
    // Proxy authentication is handled by CFNetwork internally. We can get here if the user cancels
    // CFNetwork authentication dialog, and we shouldn't ask the client to display another one in that case.
    if (challenge.protectionSpace.isProxy) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
        return;
    }

    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        if (NetworkSessionCocoa::allowsSpecificHTTPSCertificateForHost(challenge))
            return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);

        // Handle server trust evaluation at platform-level if requested, for performance reasons and to use ATS defaults.
        if (!_session->networkProcess().canHandleHTTPSServerTrustEvaluation()) {
#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
            if (canNSURLSessionTrustEvaluate()) {
                auto* networkDataTask = [self existingTask:task];
                auto decisionHandler = makeBlockPtr([_session = _session.copyRef(), completionHandler = makeBlockPtr(completionHandler), taskIdentifier, networkDataTask = RefPtr<NetworkDataTaskCocoa>(networkDataTask)](NSURLAuthenticationChallenge *challenge, OSStatus trustResult) mutable {
                    processServerTrustEvaluation(_session.get(), challenge, trustResult, taskIdentifier, networkDataTask.get(), WTFMove(completionHandler));
                });
                [NSURLSession _strictTrustEvaluate:challenge queue:[NSOperationQueue mainQueue].underlyingQueue completionHandler:decisionHandler.get()];
                return;
            }
#endif
            return completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
        }
    }
    _session->continueDidReceiveChallenge(challenge, taskIdentifier, [self existingTask:task], [completionHandler = makeBlockPtr(completionHandler)] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
    });
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    if (!_session)
        return;

    LOG(NetworkSession, "%llu didCompleteWithError %@", task.taskIdentifier, error);
    if (auto* networkDataTask = [self existingTask:task])
        networkDataTask->didCompleteWithError(error, networkDataTask->networkLoadMetrics());
    else if (error) {
        auto downloadID = _session->takeDownloadID(task.taskIdentifier);
        if (downloadID.downloadID()) {
            if (auto* download = _session->networkProcess().downloadManager().download(downloadID)) {
                NSData *resumeData = nil;
                if (id userInfo = error.userInfo) {
                    if ([userInfo isKindOfClass:[NSDictionary class]])
                        resumeData = userInfo[@"NSURLSessionDownloadTaskResumeData"];
                }
                
                if (resumeData && [resumeData isKindOfClass:[NSData class]])
                    download->didFail(error, { static_cast<const uint8_t*>(resumeData.bytes), resumeData.length });
                else
                    download->didFail(error, { });
            }
        }
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)metrics
{
    LOG(NetworkSession, "%llu didFinishCollectingMetrics", task.taskIdentifier);
    if (auto* networkDataTask = [self existingTask:task]) {
        NSURLSessionTaskTransactionMetrics *m = metrics.transactionMetrics.lastObject;
        NSDate *fetchStartDate = m.fetchStartDate;
        NSTimeInterval domainLookupStartInterval = m.domainLookupStartDate ? [m.domainLookupStartDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval domainLookupEndInterval = m.domainLookupEndDate ? [m.domainLookupEndDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval connectStartInterval = m.connectStartDate ? [m.connectStartDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval secureConnectionStartInterval = m.secureConnectionStartDate ? [m.secureConnectionStartDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval connectEndInterval = m.connectEndDate ? [m.connectEndDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval requestStartInterval = [m.requestStartDate timeIntervalSinceDate:fetchStartDate];
        NSTimeInterval responseStartInterval = [m.responseStartDate timeIntervalSinceDate:fetchStartDate];
        NSTimeInterval responseEndInterval = [m.responseEndDate timeIntervalSinceDate:fetchStartDate];

        auto& networkLoadMetrics = networkDataTask->networkLoadMetrics();
        networkLoadMetrics.domainLookupStart = Seconds(domainLookupStartInterval);
        networkLoadMetrics.domainLookupEnd = Seconds(domainLookupEndInterval);
        networkLoadMetrics.connectStart = Seconds(connectStartInterval);
        networkLoadMetrics.secureConnectionStart = Seconds(secureConnectionStartInterval);
        networkLoadMetrics.connectEnd = Seconds(connectEndInterval);
        networkLoadMetrics.requestStart = Seconds(requestStartInterval);
        networkLoadMetrics.responseStart = Seconds(responseStartInterval);
        networkLoadMetrics.responseEnd = Seconds(responseEndInterval);
        networkLoadMetrics.markComplete();
        networkLoadMetrics.protocol = String(m.networkProtocolName);

        if (networkDataTask->shouldCaptureExtraNetworkLoadMetrics()) {
            networkLoadMetrics.priority = toNetworkLoadPriority(task.priority);

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
            networkLoadMetrics.remoteAddress = String(m._remoteAddressAndPort);
            networkLoadMetrics.connectionIdentifier = String([m._connectionIdentifier UUIDString]);
#endif

#if HAVE(CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER)
            networkLoadMetrics.tlsProtocol = stringForSSLProtocol(m._negotiatedTLSProtocol);
            networkLoadMetrics.tlsCipher = stringForSSLCipher(m._negotiatedTLSCipher);
#endif

            __block WebCore::HTTPHeaderMap requestHeaders;
            [m.request.allHTTPHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSString *value, BOOL *) {
                requestHeaders.set(String(name), String(value));
            }];
            networkLoadMetrics.requestHeaders = WTFMove(requestHeaders);

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
            uint64_t requestHeaderBytesSent = 0;
            uint64_t responseHeaderBytesReceived = 0;
            uint64_t responseBodyBytesReceived = 0;
            uint64_t responseBodyDecodedSize = 0;

            for (NSURLSessionTaskTransactionMetrics *transactionMetrics in metrics.transactionMetrics) {
                requestHeaderBytesSent += transactionMetrics._requestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics._responseHeaderBytesReceived;
                responseBodyBytesReceived += transactionMetrics._responseBodyBytesReceived;
                responseBodyDecodedSize += transactionMetrics._responseBodyBytesDecoded ? transactionMetrics._responseBodyBytesDecoded : transactionMetrics._responseBodyBytesReceived;
            }

            networkLoadMetrics.requestHeaderBytesSent = requestHeaderBytesSent;
            networkLoadMetrics.requestBodyBytesSent = task.countOfBytesSent;
            networkLoadMetrics.responseHeaderBytesReceived = responseHeaderBytesReceived;
            networkLoadMetrics.responseBodyBytesReceived = responseBodyBytesReceived;
            networkLoadMetrics.responseBodyDecodedSize = responseBodyDecodedSize;
#endif
        }
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    auto taskIdentifier = dataTask.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveResponse", taskIdentifier);
    if (auto* networkDataTask = [self existingTask:dataTask]) {
        ASSERT(RunLoop::isMain());
        
        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [response respondsToSelector:@selector(statusCode)] ? [(id)response statusCode] : 0;
        if (statusCode != 304) {
            bool isMainResourceLoad = networkDataTask->firstRequest().requester() == WebCore::ResourceRequest::Requester::Main;
            WebCore::adjustMIMETypeIfNecessary(response._CFURLResponse, isMainResourceLoad);
        }

        WebCore::ResourceResponse resourceResponse(response);
        // Lazy initialization is not helpful in the WebKit2 case because we always end up initializing
        // all the fields when sending the response to the WebContent process over IPC.
        resourceResponse.disableLazyInitialization();

        // FIXME: This cannot be eliminated until other code no longer relies on ResourceResponse's
        // NetworkLoadMetrics. For example, PerformanceTiming.
        copyTimingData([dataTask _timingData], resourceResponse.deprecatedNetworkLoadMetrics());

        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->didReceiveResponse(WTFMove(resourceResponse), [completionHandlerCopy, taskIdentifier](WebCore::PolicyAction policyAction) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%llu didReceiveResponse completionHandler (%d)", taskIdentifier, policyAction);
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            completionHandlerCopy(toNSURLSessionResponseDisposition(policyAction));
            Block_release(completionHandlerCopy);
        });
    } else {
        LOG(NetworkSession, "%llu didReceiveResponse completionHandler (cancel)", taskIdentifier);
        completionHandler(NSURLSessionResponseCancel);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    if (auto* networkDataTask = [self existingTask:dataTask])
        networkDataTask->didReceiveData(WebCore::SharedBuffer::create(data));
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL:(NSURL *)location
{
    if (!_session)
        return;

    auto downloadID = _session->takeDownloadID([downloadTask taskIdentifier]);
    if (auto* download = _session->networkProcess().downloadManager().download(downloadID))
        download->didFinish();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    if (!_session)
        return;

    ASSERT_WITH_MESSAGE(![self existingTask:downloadTask], "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");

    auto downloadID = _session->downloadID([downloadTask taskIdentifier]);
    if (auto* download = _session->networkProcess().downloadManager().download(downloadID))
        download->didReceiveData(bytesWritten);
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didResumeAtOffset:(int64_t)fileOffset expectedTotalBytes:(int64_t)expectedTotalBytes
{
    if (!_session)
        return;

    notImplemented();
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask
{
    if (auto* networkDataTask = [self existingTask:dataTask]) {
        Ref<NetworkDataTaskCocoa> protectedNetworkDataTask(*networkDataTask);
        auto downloadID = networkDataTask->pendingDownloadID();
        auto& downloadManager = _session->networkProcess().downloadManager();
        auto download = std::make_unique<WebKit::Download>(downloadManager, downloadID, downloadTask, _session->sessionID(), networkDataTask->suggestedFilename());
        networkDataTask->transferSandboxExtensionToDownload(*download);
        ASSERT(FileSystem::fileExists(networkDataTask->pendingDownloadLocation()));
        download->didCreateDestination(networkDataTask->pendingDownloadLocation());
        downloadManager.dataTaskBecameDownloadTask(downloadID, WTFMove(download));

        _session->addDownloadID([downloadTask taskIdentifier], downloadID);
    }
}

@end

namespace WebKit {

#if !ASSERT_DISABLED
static bool sessionsCreated = false;
#endif

static NSURLSessionConfiguration *configurationForSessionID(const PAL::SessionID& session)
{
    if (session.isEphemeral()) {
        NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration._shouldSkipPreferredClientCertificateLookup = YES;
        return configuration;
    }
    return [NSURLSessionConfiguration defaultSessionConfiguration];
}

#if PLATFORM(IOS_FAMILY)
static String& globalCTDataConnectionServiceType()
{
    static NeverDestroyed<String> ctDataConnectionServiceType;
    return ctDataConnectionServiceType.get();
}
#endif

#if PLATFORM(IOS_FAMILY)
void NetworkSessionCocoa::setCTDataConnectionServiceType(const String& type)
{
    ASSERT(!sessionsCreated);
    globalCTDataConnectionServiceType() = type;
}
#endif

Ref<NetworkSession> NetworkSessionCocoa::create(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
{
    return adoptRef(*new NetworkSessionCocoa(networkProcess, WTFMove(parameters)));
}

static NSDictionary *proxyDictionary(const URL& httpProxy, const URL& httpsProxy)
{
    if (!httpProxy.isValid() && !httpsProxy.isValid())
        return nil;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN

    NSMutableDictionary *dictionary = [[[NSMutableDictionary alloc] init] autorelease];
    if (httpProxy.isValid()) {
        [dictionary setObject:httpProxy.host().toString() forKey:(NSString *)kCFStreamPropertyHTTPProxyHost];
        if (auto port = httpProxy.port())
            [dictionary setObject:@(*port) forKey:(NSString *)kCFStreamPropertyHTTPProxyPort];
    }
    if (httpsProxy.isValid()) {
        [dictionary setObject:httpsProxy.host().toString() forKey:(NSString *)kCFStreamPropertyHTTPSProxyHost];
        if (auto port = httpsProxy.port())
            [dictionary setObject:@(*port) forKey:(NSString *)kCFStreamPropertyHTTPSProxyPort];
    }
    return dictionary;

    ALLOW_DEPRECATED_DECLARATIONS_END
}

NetworkSessionCocoa::NetworkSessionCocoa(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
    : NetworkSession(networkProcess, parameters.sessionID)
    , m_boundInterfaceIdentifier(parameters.boundInterfaceIdentifier)
    , m_proxyConfiguration(parameters.proxyConfiguration)
    , m_shouldLogCookieInformation(parameters.shouldLogCookieInformation)
    , m_loadThrottleLatency(parameters.loadThrottleLatency)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    relaxAdoptionRequirement();

#if !ASSERT_DISABLED
    sessionsCreated = true;
#endif

    NSURLSessionConfiguration *configuration = configurationForSessionID(m_sessionID);

#if USE(CFNETWORK_AUTO_ADDED_HTTP_HEADER_SUPPRESSION)
    // Without this, CFNetwork would sometimes add a Content-Type header to our requests (rdar://problem/34748470).
    configuration._suppressedAutoAddedHTTPHeaders = [NSSet setWithObject:@"Content-Type"];
#endif

    if (parameters.allowsCellularAccess == AllowsCellularAccess::No)
        configuration.allowsCellularAccess = NO;

    // The WebKit network cache was already queried.
    configuration.URLCache = nil;

    if (auto data = networkProcess.sourceApplicationAuditData())
        configuration._sourceApplicationAuditTokenData = (__bridge NSData *)data.get();

    if (!parameters.sourceApplicationBundleIdentifier.isEmpty()) {
        configuration._sourceApplicationBundleIdentifier = parameters.sourceApplicationBundleIdentifier;
        configuration._sourceApplicationAuditTokenData = nil;
    }

    if (!parameters.sourceApplicationSecondaryIdentifier.isEmpty())
        configuration._sourceApplicationSecondaryIdentifier = parameters.sourceApplicationSecondaryIdentifier;

    configuration.connectionProxyDictionary = proxyDictionary(parameters.httpProxy, parameters.httpsProxy);

#if PLATFORM(IOS_FAMILY)
    auto& ctDataConnectionServiceType = globalCTDataConnectionServiceType();
    if (!ctDataConnectionServiceType.isEmpty())
        configuration._CTDataConnectionServiceType = ctDataConnectionServiceType;
#endif

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    networkProcess.supplement<LegacyCustomProtocolManager>()->registerProtocolClass(configuration);
#endif

#if HAVE(TIMINGDATAOPTIONS)
    configuration._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;
#else
    setCollectsTimingData();
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
    // FIXME: Replace @"kCFStreamPropertyAutoErrorOnSystemChange" with a constant from the SDK once rdar://problem/40650244 is in a build.
    if (networkProcess.suppressesConnectionTerminationOnSystemChange())
        configuration._socketStreamProperties = @{ @"kCFStreamPropertyAutoErrorOnSystemChange" : @(NO) };
#endif

#if PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED >= 60000
    configuration._companionProxyPreference = NSURLSessionCompanionProxyPreferencePreferDirectToCloud;
#endif

    auto* storageSession = networkProcess.storageSession(parameters.sessionID);
    RELEASE_ASSERT(storageSession);

    NSHTTPCookieStorage* cookieStorage;
    if (CFHTTPCookieStorageRef storage = storageSession->cookieStorage().get()) {
        cookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
        configuration.HTTPCookieStorage = cookieStorage;
    } else
        cookieStorage = storageSession->nsCookieStorage();

#if HAVE(CFNETWORK_OVERRIDE_SESSION_COOKIE_ACCEPT_POLICY)
    // We still need to check the selector since CFNetwork updates and WebKit updates are separate
    // on older macOS.
    if ([cookieStorage respondsToSelector:@selector(_overrideSessionCookieAcceptPolicy)])
        cookieStorage._overrideSessionCookieAcceptPolicy = YES;
#endif

    m_sessionWithCredentialStorageDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this withCredentials:true]);
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionWithCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    LOG(NetworkSession, "Created NetworkSession with cookieAcceptPolicy %lu", configuration.HTTPCookieStorage.cookieAcceptPolicy);

    configuration.URLCredentialStorage = nil;
    configuration._shouldSkipPreferredClientCertificateLookup = YES;
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=177394
    // configuration.HTTPCookieStorage = nil;
    // configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;

    m_statelessSessionDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this withCredentials:false]);
    m_statelessSession = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_statelessSessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    m_resourceLoadStatisticsDirectory = parameters.resourceLoadStatisticsDirectory;
    setResourceLoadStatisticsEnabled(parameters.enableResourceLoadStatistics);
#endif
}

NetworkSessionCocoa::~NetworkSessionCocoa()
{
}

void NetworkSessionCocoa::invalidateAndCancel()
{
    NetworkSession::invalidateAndCancel();

    [m_sessionWithCredentialStorage invalidateAndCancel];
    [m_statelessSession invalidateAndCancel];
    [m_sessionWithCredentialStorageDelegate sessionInvalidated];
    [m_statelessSessionDelegate sessionInvalidated];
}

void NetworkSessionCocoa::clearCredentials()
{
#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    ASSERT(m_dataTaskMapWithCredentials.isEmpty());
    ASSERT(m_dataTaskMapWithoutState.isEmpty());
    ASSERT(m_downloadMap.isEmpty());
    // FIXME: Use resetWithCompletionHandler instead.
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:m_sessionWithCredentialStorage.get().configuration delegate:static_cast<id>(m_sessionWithCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    m_statelessSession = [NSURLSession sessionWithConfiguration:m_statelessSession.get().configuration delegate:static_cast<id>(m_statelessSessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
#endif
}

NetworkDataTaskCocoa* NetworkSessionCocoa::dataTaskForIdentifier(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
    ASSERT(RunLoop::isMain());
    if (storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use)
        return m_dataTaskMapWithCredentials.get(taskIdentifier);
    return m_dataTaskMapWithoutState.get(taskIdentifier);
}

NSURLSessionDownloadTask* NetworkSessionCocoa::downloadTaskWithResumeData(NSData* resumeData)
{
    return [m_sessionWithCredentialStorage downloadTaskWithResumeData:resumeData];
}

void NetworkSessionCocoa::addDownloadID(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, DownloadID downloadID)
{
#ifndef NDEBUG
    ASSERT(!m_downloadMap.contains(taskIdentifier));
    for (auto idInMap : m_downloadMap.values())
        ASSERT(idInMap != downloadID);
#endif
    m_downloadMap.add(taskIdentifier, downloadID);
}

DownloadID NetworkSessionCocoa::downloadID(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier)
{
    ASSERT(m_downloadMap.get(taskIdentifier).downloadID());
    return m_downloadMap.get(taskIdentifier);
}

DownloadID NetworkSessionCocoa::takeDownloadID(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier)
{
    auto downloadID = m_downloadMap.take(taskIdentifier);
    return downloadID;
}

static bool certificatesMatch(SecTrustRef trust1, SecTrustRef trust2)
{
    if (!trust1 || !trust2)
        return false;

    CFIndex count1 = SecTrustGetCertificateCount(trust1);
    CFIndex count2 = SecTrustGetCertificateCount(trust2);
    if (count1 != count2)
        return false;

    for (CFIndex i = 0; i < count1; i++) {
        auto cert1 = SecTrustGetCertificateAtIndex(trust1, i);
        auto cert2 = SecTrustGetCertificateAtIndex(trust2, i);
        RELEASE_ASSERT(cert1);
        RELEASE_ASSERT(cert2);
        if (!CFEqual(cert1, cert2))
            return false;
    }

    return true;
}

bool NetworkSessionCocoa::allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge& challenge)
{
    const String& host = challenge.protectionSpace().host();
    NSArray *certificates = [NSURLRequest allowsSpecificHTTPSCertificateForHost:host];
    if (!certificates)
        return false;

    bool requireServerCertificates = challenge.protectionSpace().authenticationScheme() == WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
    RetainPtr<SecPolicyRef> policy = adoptCF(SecPolicyCreateSSL(requireServerCertificates, host.createCFString().get()));

    SecTrustRef trustRef = nullptr;
    if (SecTrustCreateWithCertificates((CFArrayRef)certificates, policy.get(), &trustRef) != noErr)
        return false;
    RetainPtr<SecTrustRef> trust = adoptCF(trustRef);

    return certificatesMatch(trust.get(), challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust);
}

void NetworkSessionCocoa::continueDidReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, NetworkDataTaskCocoa* networkDataTask, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&& completionHandler)
{
    if (!networkDataTask) {
        auto downloadID = this->downloadID(taskIdentifier);
        if (downloadID.downloadID()) {
            if (auto* download = networkProcess().downloadManager().download(downloadID)) {
                WebCore::AuthenticationChallenge authenticationChallenge { challenge };
                // Received an authentication challenge for a download being resumed.
                download->didReceiveChallenge(authenticationChallenge, WTFMove(completionHandler));
                return;
            }
        }
        LOG(NetworkSession, "%llu didReceiveChallenge completionHandler (cancel)", taskIdentifier);
        completionHandler(AuthenticationChallengeDisposition::Cancel, { });
        return;
    }

    auto sessionID = this->sessionID();
    WebCore::AuthenticationChallenge authenticationChallenge { challenge };
    auto challengeCompletionHandler = [completionHandler = WTFMove(completionHandler), networkProcess = makeRef(networkProcess()), sessionID, authenticationChallenge, taskIdentifier, partition = networkDataTask->partition()](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
#if !LOG_DISABLED
        LOG(NetworkSession, "%llu didReceiveChallenge completionHandler %d", taskIdentifier, disposition);
#else
        UNUSED_PARAM(taskIdentifier);
#endif
#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
        UNUSED_PARAM(sessionID);
        UNUSED_PARAM(authenticationChallenge);
#else
        if (credential.persistence() == WebCore::CredentialPersistenceForSession && authenticationChallenge.protectionSpace().isPasswordBased()) {
            WebCore::Credential nonPersistentCredential(credential.user(), credential.password(), WebCore::CredentialPersistenceNone);
            URL urlToStore;
            if (authenticationChallenge.failureResponse().httpStatusCode() == 401)
                urlToStore = authenticationChallenge.failureResponse().url();
            if (auto storageSession = networkProcess->storageSession(sessionID))
                storageSession->credentialStorage().set(partition, nonPersistentCredential, authenticationChallenge.protectionSpace(), urlToStore);
            else
                ASSERT_NOT_REACHED();

            completionHandler(disposition, nonPersistentCredential);
            return;
        }
#endif
        completionHandler(disposition, credential);
    };
    networkDataTask->didReceiveChallenge(WTFMove(authenticationChallenge), WTFMove(challengeCompletionHandler));
}

}
