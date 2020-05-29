/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#import "WebSocketTask.h"
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
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/NakedRef.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/NetworkSessionCocoaAdditions.h>
#else
#define NETWORK_SESSION_COCOA_ADDITIONS_1
#define NETWORK_SESSION_COCOA_ADDITIONS_2 true
#endif

#import "DeviceManagementSoftLink.h"

using namespace WebKit;

CFStringRef const WebKit2HTTPProxyDefaultsKey = static_cast<CFStringRef>(@"WebKit2HTTPProxy");
CFStringRef const WebKit2HTTPSProxyDefaultsKey = static_cast<CFStringRef>(@"WebKit2HTTPSProxy");

constexpr unsigned maxNumberOfIsolatedSessions { 10 };

static NSURLSessionResponseDisposition toNSURLSessionResponseDisposition(WebCore::PolicyAction disposition)
{
    switch (disposition) {
    case WebCore::PolicyAction::StopAllLoads:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        FALLTHROUGH;
#endif
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
#if HAVE(CFNETWORK_METRICS_APIS_V4)
static String stringForTLSProtocolVersion(tls_protocol_version_t protocol)
{
    switch (protocol) {
    case tls_protocol_version_TLSv10:
        return "TLS 1.0"_s;
    case tls_protocol_version_TLSv11:
        return "TLS 1.1"_s;
    case tls_protocol_version_TLSv12:
        return "TLS 1.2"_s;
    case tls_protocol_version_TLSv13:
        return "TLS 1.3"_s;
    case tls_protocol_version_DTLSv10:
        return "DTLS 1.0"_s;
    case tls_protocol_version_DTLSv12:
        return "DTLS 1.2"_s;
    }
    return { };
}

static String stringForTLSCipherSuite(tls_ciphersuite_t suite)
{
#define STRINGIFY_CIPHER(cipher) \
    case tls_ciphersuite_##cipher: \
        return "" #cipher ""_s

    switch (suite) {
        STRINGIFY_CIPHER(RSA_WITH_3DES_EDE_CBC_SHA);
        STRINGIFY_CIPHER(RSA_WITH_AES_128_CBC_SHA);
        STRINGIFY_CIPHER(RSA_WITH_AES_256_CBC_SHA);
        STRINGIFY_CIPHER(RSA_WITH_AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(RSA_WITH_AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(RSA_WITH_AES_128_CBC_SHA256);
        STRINGIFY_CIPHER(RSA_WITH_AES_256_CBC_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_128_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_256_CBC_SHA);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_128_CBC_SHA256);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_256_CBC_SHA384);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
        STRINGIFY_CIPHER(ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
        STRINGIFY_CIPHER(AES_128_GCM_SHA256);
        STRINGIFY_CIPHER(AES_256_GCM_SHA384);
        STRINGIFY_CIPHER(CHACHA20_POLY1305_SHA256);
    }

    return { };

#undef STRINGIFY_CIPHER
}

#else // HAVE(CFNETWORK_METRICS_APIS_V4)

static String stringForSSLProtocol(SSLProtocol protocol)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
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
    ALLOW_DEPRECATED_DECLARATIONS_END
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
#endif // HAVE(CFNETWORK_METRICS_APIS_V4)
#endif // HAVE(CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER)

@interface WKNetworkSessionDelegate : NSObject <NSURLSessionDataDelegate
#if HAVE(NSURLSESSION_WEBSOCKET)
    , NSURLSessionWebSocketDelegate
#endif
> {
    WeakPtr<WebKit::NetworkSessionCocoa> _session;
    WeakPtr<WebKit::SessionWrapper> _sessionWrapper;
    bool _withCredentials;
}

- (id)initWithNetworkSession:(NakedRef<WebKit::NetworkSessionCocoa>)session wrapper:(WebKit::SessionWrapper&)sessionWrapper withCredentials:(bool)withCredentials;
- (void)sessionInvalidated;

@end

@implementation WKNetworkSessionDelegate

- (id)initWithNetworkSession:(NakedRef<WebKit::NetworkSessionCocoa>)session wrapper:(WebKit::SessionWrapper&)sessionWrapper withCredentials:(bool)withCredentials
{
    self = [super init];
    if (!self)
        return nil;

    _session = makeWeakPtr(session.get());
    _sessionWrapper = makeWeakPtr(sessionWrapper);
    _withCredentials = withCredentials;

    return self;
}

- (void)sessionInvalidated
{
    _sessionWrapper = nullptr;
}

- (NetworkDataTaskCocoa*)existingTask:(NSURLSessionTask *)task
{
    if (!_sessionWrapper)
        return nullptr;

    if (!task)
        return nullptr;

    return _sessionWrapper->dataTaskMap.get(task.taskIdentifier);
}

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error
{
    ASSERT(!_sessionWrapper);
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
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

static NSURLRequest* updateIgnoreStrictTransportSecuritySettingIfNecessary(NSURLRequest *request, bool shouldIgnoreHSTS)
{
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

    return request;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu willPerformHTTPRedirection from %s to %s", taskIdentifier, response.URL.absoluteString.UTF8String, request.URL.absoluteString.UTF8String);

    if (auto* networkDataTask = [self existingTask:task]) {
        auto completionHandlerCopy = Block_copy(completionHandler);

        if (auto* sessionCocoa = [self sessionFromTask:task])
            sessionCocoa->taskReceivedBytes(taskIdentifier);

        bool shouldIgnoreHSTS = false;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        if (auto* sessionCocoa = networkDataTask->networkSession()) {
            shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request) && sessionCocoa->networkProcess().storageSession(sessionCocoa->sessionID())->shouldBlockCookies(request, networkDataTask->frameID(), networkDataTask->pageID(), networkDataTask->shouldRelaxThirdPartyCookieBlocking());
            if (shouldIgnoreHSTS) {
                request = downgradeRequest(request);
                ASSERT([request.URL.scheme isEqualToString:@"http"]);
                LOG(NetworkSession, "%llu Downgraded %s from https to http", taskIdentifier, request.URL.absoluteString.UTF8String);
            }
        } else
            ASSERT_NOT_REACHED();
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
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        if (auto* sessionCocoa = networkDataTask->networkSession()) {
            shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request) && sessionCocoa->networkProcess().storageSession(sessionCocoa->sessionID())->shouldBlockCookies(request, networkDataTask->frameID(), networkDataTask->pageID(), networkDataTask->shouldRelaxThirdPartyCookieBlocking());
            if (shouldIgnoreHSTS) {
                request = downgradeRequest(request);
                ASSERT([request.URL.scheme isEqualToString:@"http"]);
                LOG(NetworkSession, "%llu Downgraded %s from https to http", taskIdentifier, request.URL.absoluteString.UTF8String);
            }
        } else
            ASSERT_NOT_REACHED();
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

#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
static inline void processServerTrustEvaluation(NetworkSessionCocoa& session, SessionWrapper& sessionWrapper, NSURLAuthenticationChallenge *challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, NetworkDataTaskCocoa* networkDataTask, CompletionHandler<void(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential)>&& completionHandler)
{
    session.continueDidReceiveChallenge(sessionWrapper, challenge, negotiatedLegacyTLS, taskIdentifier, networkDataTask, [completionHandler = WTFMove(completionHandler), secTrust = retainPtr(challenge.protectionSpace.serverTrust)] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        // FIXME: UIProcess should send us back non nil credentials but the credential IPC encoder currently only serializes ns credentials for username/password.
        if (disposition == WebKit::AuthenticationChallengeDisposition::UseCredential && !credential.nsCredential()) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust: secTrust.get()]);
            return;
        }
        completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
    });
}
#endif

- (NetworkSessionCocoa*)sessionFromTask:(NSURLSessionTask *)task {
    if (auto* networkDataTask = [self existingTask:task])
        return static_cast<NetworkSessionCocoa*>(networkDataTask->networkSession());

    if (!_sessionWrapper)
        return nullptr;

    if (auto downloadID = _sessionWrapper->downloadMap.get(task.taskIdentifier)) {
        if (auto download = _session->networkProcess().downloadManager().download(downloadID))
            return static_cast<NetworkSessionCocoa*>(_session->networkProcess().networkSession(download->sessionID()));
        return nullptr;
    }

#if HAVE(NSURLSESSION_WEBSOCKET)
    if (auto* webSocketTask = _sessionWrapper->webSocketDataTaskMap.get(task.taskIdentifier))
        return webSocketTask->networkSession();
#endif

    return nullptr;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    auto* sessionCocoa = [self sessionFromTask: task];
    if (!sessionCocoa || [task state] == NSURLSessionTaskStateCanceling) {
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveChallenge", taskIdentifier);
    
    // Proxy authentication is handled by CFNetwork internally. We can get here if the user cancels
    // CFNetwork authentication dialog, and we shouldn't ask the client to display another one in that case.
    if (challenge.protectionSpace.isProxy && !sessionCocoa->preventsSystemHTTPProxyAuthentication()) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
        return;
    }

    NegotiatedLegacyTLS negotiatedLegacyTLS = NegotiatedLegacyTLS::No;

    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        if (NetworkSessionCocoa::allowsSpecificHTTPSCertificateForHost(challenge))
            return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);

#if HAVE(TLS_PROTOCOL_VERSION_T)
        NSURLSessionTaskTransactionMetrics *metrics = task._incompleteTaskMetrics.transactionMetrics.lastObject;
        auto tlsVersion = (tls_protocol_version_t)metrics.negotiatedTLSProtocolVersion.unsignedShortValue;
        if (tlsVersion == tls_protocol_version_TLSv10 || tlsVersion == tls_protocol_version_TLSv11)
            negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
#endif
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (negotiatedLegacyTLS == NegotiatedLegacyTLS::No && [task respondsToSelector:@selector(_TLSNegotiatedProtocolVersion)]) {
            SSLProtocol tlsVersion = [task _TLSNegotiatedProtocolVersion];
            if (tlsVersion == kTLSProtocol11 || tlsVersion == kTLSProtocol1)
                negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
        }
        ALLOW_DEPRECATED_DECLARATIONS_END

        // Handle server trust evaluation at platform-level if requested, for performance reasons and to use ATS defaults.
        if (sessionCocoa->fastServerTrustEvaluationEnabled() && negotiatedLegacyTLS == NegotiatedLegacyTLS::No) {
            auto* networkDataTask = [self existingTask:task];
            if (networkDataTask)
                networkDataTask->didNegotiateModernTLS(challenge);
#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
            auto decisionHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKNetworkSessionDelegate>(self), sessionCocoa = makeWeakPtr(sessionCocoa), completionHandler = makeBlockPtr(completionHandler), taskIdentifier, networkDataTask = makeRefPtr(networkDataTask), negotiatedLegacyTLS](NSURLAuthenticationChallenge *challenge, OSStatus trustResult) mutable {
                auto strongSelf = weakSelf.get();
                if (!strongSelf)
                    return completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
                auto task = WTFMove(networkDataTask);
                auto* session = sessionCocoa.get();
                if (trustResult == noErr || !session) {
                    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
                    return;
                }
                processServerTrustEvaluation(*session, *strongSelf->_sessionWrapper, challenge, negotiatedLegacyTLS, taskIdentifier, task.get(), WTFMove(completionHandler));
            });
            [NSURLSession _strictTrustEvaluate:challenge queue:[NSOperationQueue mainQueue].underlyingQueue completionHandler:decisionHandler.get()];
            return;
#else
            return completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
#endif
        }
    }

    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodClientCertificate]) {
        HostAndPort key { challenge.protectionSpace.host, challenge.protectionSpace.port };
        if (auto* credential = sessionCocoa->successfulClientCertificateForHost(challenge.protectionSpace.host, challenge.protectionSpace.port))
            return completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
        sessionCocoa->continueDidReceiveChallenge(*_sessionWrapper, challenge, negotiatedLegacyTLS, taskIdentifier, [self existingTask:task], [completionHandler = makeBlockPtr(completionHandler), key, weakSessionCocoa = makeWeakPtr(sessionCocoa), taskIdentifier] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
            NSURLCredential *nsCredential = credential.nsCredential();
            if (disposition == WebKit::AuthenticationChallengeDisposition::UseCredential && nsCredential && weakSessionCocoa)
                weakSessionCocoa->clientCertificateSuggestedForHost(taskIdentifier, nsCredential, key.first, key.second);
            completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), nsCredential);
        });
        return;
    }

    sessionCocoa->continueDidReceiveChallenge(*_sessionWrapper, challenge, negotiatedLegacyTLS, taskIdentifier, [self existingTask:task], [completionHandler = makeBlockPtr(completionHandler)] (WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
    });
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    LOG(NetworkSession, "%llu didCompleteWithError %@", task.taskIdentifier, error);

    if (error) {
        NSDictionary *oldUserInfo = [error userInfo];
        NSMutableDictionary *newUserInfo = oldUserInfo ? [NSMutableDictionary dictionaryWithDictionary:oldUserInfo] : [NSMutableDictionary dictionary];
        newUserInfo[@"networkTaskDescription"] = [task description];
        error = [NSError errorWithDomain:[error domain] code:[error code] userInfo:newUserInfo];
    }

    if (auto* networkDataTask = [self existingTask:task]) {
        if (auto* sessionCocoa = [self sessionFromTask:task])
            sessionCocoa->taskFailed(task.taskIdentifier);
        networkDataTask->didCompleteWithError(error, networkDataTask->networkLoadMetrics());
    } else if (error) {
        if (!_sessionWrapper)
            return;
        auto downloadID = _sessionWrapper->downloadMap.take(task.taskIdentifier);
        if (!downloadID.downloadID())
            return;
        if (!_session)
            return;
        auto* download = _session->networkProcess().downloadManager().download(downloadID);
        if (!download)
            return;

        NSData *resumeData = nil;
        if (id userInfo = error.userInfo) {
            if ([userInfo isKindOfClass:[NSDictionary class]]) {
                resumeData = userInfo[@"NSURLSessionDownloadTaskResumeData"];
                if (resumeData && ![resumeData isKindOfClass:[NSData class]]) {
                    RELEASE_LOG(NetworkSession, "Download task %llu finished with resume data of wrong class: %s", (unsigned long long)task.taskIdentifier, NSStringFromClass([resumeData class]).UTF8String);
                    ASSERT_NOT_REACHED();
                    resumeData = nil;
                }
            }
        }

        auto resumeDataReference = resumeData ? IPC::DataReference { static_cast<const uint8_t*>(resumeData.bytes), resumeData.length } : IPC::DataReference { };

        if (download->wasCanceled())
            download->didCancel(resumeDataReference);
        else
            download->didFail(error, resumeDataReference);
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
        networkLoadMetrics.fetchStart = Seconds(fetchStartDate.timeIntervalSince1970);
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
#if HAVE(CFNETWORK_METRICS_CONNECTION_PROPERTIES)
        networkLoadMetrics.cellular = m.cellular;
        networkLoadMetrics.expensive = m.expensive;
        networkLoadMetrics.constrained = m.constrained;
        networkLoadMetrics.multipath = m.multipath;
#endif
        networkLoadMetrics.isReusedConnection = m.isReusedConnection;

        if (networkDataTask->shouldCaptureExtraNetworkLoadMetrics()) {
            networkLoadMetrics.priority = toNetworkLoadPriority(task.priority);

#if HAVE(CFNETWORK_METRICS_APIS_V4)
            if (auto port = [m.remotePort unsignedIntValue])
                networkLoadMetrics.remoteAddress = makeString(String(m.remoteAddress), ':', port);
            else
                networkLoadMetrics.remoteAddress = m.remoteAddress;
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            networkLoadMetrics.remoteAddress = String(m._remoteAddressAndPort);
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            networkLoadMetrics.connectionIdentifier = String([m._connectionIdentifier UUIDString]);

#if HAVE(CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER)
#if HAVE(CFNETWORK_METRICS_APIS_V4)
            networkLoadMetrics.tlsProtocol = stringForTLSProtocolVersion((tls_protocol_version_t)[m.negotiatedTLSProtocolVersion unsignedShortValue]);
            networkLoadMetrics.tlsCipher = stringForTLSCipherSuite((tls_ciphersuite_t)[m.negotiatedTLSCipherSuite unsignedShortValue]);
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            networkLoadMetrics.tlsProtocol = stringForSSLProtocol(m._negotiatedTLSProtocol);
            networkLoadMetrics.tlsCipher = stringForSSLCipher(m._negotiatedTLSCipher);
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif
#endif

            __block WebCore::HTTPHeaderMap requestHeaders;
            [m.request.allHTTPHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSString *value, BOOL *) {
                requestHeaders.set(String(name), String(value));
            }];
            networkLoadMetrics.requestHeaders = WTFMove(requestHeaders);

            uint64_t requestHeaderBytesSent = 0;
            uint64_t responseHeaderBytesReceived = 0;
            uint64_t responseBodyBytesReceived = 0;
            uint64_t responseBodyDecodedSize = 0;

            for (NSURLSessionTaskTransactionMetrics *transactionMetrics in metrics.transactionMetrics) {
#if HAVE(CFNETWORK_METRICS_APIS_V4)
                requestHeaderBytesSent += transactionMetrics.countOfRequestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics.countOfResponseHeaderBytesReceived;
                responseBodyBytesReceived += transactionMetrics.countOfResponseBodyBytesReceived;
                responseBodyDecodedSize += transactionMetrics.countOfResponseBodyBytesAfterDecoding ? transactionMetrics.countOfResponseBodyBytesAfterDecoding : transactionMetrics.countOfResponseBodyBytesReceived;
#else
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                requestHeaderBytesSent += transactionMetrics._requestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics._responseHeaderBytesReceived;
                responseBodyBytesReceived += transactionMetrics._responseBodyBytesReceived;
                responseBodyDecodedSize += transactionMetrics._responseBodyBytesDecoded ? transactionMetrics._responseBodyBytesDecoded : transactionMetrics._responseBodyBytesReceived;
                ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            }

            networkLoadMetrics.requestHeaderBytesSent = requestHeaderBytesSent;
            networkLoadMetrics.requestBodyBytesSent = task.countOfBytesSent;
            networkLoadMetrics.responseHeaderBytesReceived = responseHeaderBytesReceived;
            networkLoadMetrics.responseBodyBytesReceived = responseBodyBytesReceived;
            networkLoadMetrics.responseBodyDecodedSize = responseBodyDecodedSize;
        }
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    auto taskIdentifier = dataTask.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveResponse", taskIdentifier);
    if (auto* networkDataTask = [self existingTask:dataTask]) {
        ASSERT(RunLoop::isMain());

        if (auto* sessionCocoa = [self sessionFromTask:dataTask])
            sessionCocoa->taskReceivedBytes(taskIdentifier);

        NegotiatedLegacyTLS negotiatedLegacyTLS = NegotiatedLegacyTLS::No;
#if HAVE(TLS_PROTOCOL_VERSION_T)
        NSURLSessionTaskTransactionMetrics *metrics = dataTask._incompleteTaskMetrics.transactionMetrics.lastObject;
        auto tlsVersion = (tls_protocol_version_t)metrics.negotiatedTLSProtocolVersion.unsignedShortValue;
        if (tlsVersion == tls_protocol_version_TLSv10 || tlsVersion == tls_protocol_version_TLSv11)
            negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
        UNUSED_PARAM(metrics);
#else // We do not need to check _TLSNegotiatedProtocolVersion if we have metrics.negotiatedTLSProtocolVersion because it works at response time even before rdar://problem/56522601
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([dataTask respondsToSelector:@selector(_TLSNegotiatedProtocolVersion)]) {
            SSLProtocol tlsVersion = [dataTask _TLSNegotiatedProtocolVersion];
            if (tlsVersion == kTLSProtocol11 || tlsVersion == kTLSProtocol1)
                negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
        }
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        
        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [response isKindOfClass:NSHTTPURLResponse.class] ? [(NSHTTPURLResponse *)response statusCode] : 0;
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
        resourceResponse.setDeprecatedNetworkLoadMetrics(WebCore::copyTimingData([dataTask _timingData]));

        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->didReceiveResponse(WTFMove(resourceResponse), negotiatedLegacyTLS, [completionHandlerCopy, taskIdentifier](WebCore::PolicyAction policyAction) {
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
    if (!_sessionWrapper)
        return;
    auto downloadID = _sessionWrapper->downloadMap.take([downloadTask taskIdentifier]);
    if (!downloadID.downloadID())
        return;
    if (!_session)
        return;
    auto* download = _session->networkProcess().downloadManager().download(downloadID);
    if (!download)
        return;
    download->didFinish();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    ASSERT_WITH_MESSAGE(![self existingTask:downloadTask], "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");

    if (!_sessionWrapper)
        return;
    auto downloadID = _sessionWrapper->downloadMap.get([downloadTask taskIdentifier]);
    if (!downloadID.downloadID())
        return;
    if (!_session)
        return;
    auto* download = _session->networkProcess().downloadManager().download(downloadID);
    if (!download)
        return;
    download->didReceiveData(bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didResumeAtOffset:(int64_t)fileOffset expectedTotalBytes:(int64_t)expectedTotalBytes
{
#if PLATFORM(IOS_FAMILY)
    // This is to work around rdar://problem/63249830
    if ([downloadTask respondsToSelector:@selector(downloadFile)] && [downloadTask.downloadFile respondsToSelector:@selector(setSkipUnlink:)])
        downloadTask.downloadFile.skipUnlink = YES;
#endif
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask
{
    auto* networkDataTask = [self existingTask:dataTask];
    if (!networkDataTask)
        return;
    auto* sessionCocoa = networkDataTask->networkSession();
    if (!sessionCocoa)
        return;

    Ref<NetworkDataTaskCocoa> protectedNetworkDataTask(*networkDataTask);
    auto downloadID = networkDataTask->pendingDownloadID();
    auto& downloadManager = sessionCocoa->networkProcess().downloadManager();
    auto download = makeUnique<WebKit::Download>(downloadManager, downloadID, downloadTask, *sessionCocoa, networkDataTask->suggestedFilename());
    networkDataTask->transferSandboxExtensionToDownload(*download);
    ASSERT(FileSystem::fileExists(networkDataTask->pendingDownloadLocation()));
    download->didCreateDestination(networkDataTask->pendingDownloadLocation());
    downloadManager.dataTaskBecameDownloadTask(downloadID, WTFMove(download));

    RELEASE_ASSERT(!_sessionWrapper->downloadMap.contains(downloadTask.taskIdentifier));
    _sessionWrapper->downloadMap.add(downloadTask.taskIdentifier, downloadID);
}

#if HAVE(NSURLSESSION_WEBSOCKET)
- (WebSocketTask*)existingWebSocketTask:(NSURLSessionWebSocketTask *)task
{
    if (!_sessionWrapper)
        return nullptr;

    if (!task)
        return nullptr;

    return _sessionWrapper->webSocketDataTaskMap.get(task.taskIdentifier);
}


- (void)URLSession:(NSURLSession *)session webSocketTask:(NSURLSessionWebSocketTask *)task didOpenWithProtocol:(NSString *) protocol
{
    if (auto* webSocketTask = [self existingWebSocketTask:task])
        webSocketTask->didConnect(protocol);
}

- (void)URLSession:(NSURLSession *)session webSocketTask:(NSURLSessionWebSocketTask *)task didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode reason:(NSData *)reason
{
    if (auto* webSocketTask = [self existingWebSocketTask:task]) {
        auto reason = adoptNS([[NSString alloc] initWithData:[task closeReason] encoding:NSUTF8StringEncoding]);
        webSocketTask->didClose(closeCode, reason.get());
    }
}
#endif

@end

namespace WebKit {

#if ASSERT_ENABLED
static bool sessionsCreated = false;
#endif

static NSURLSessionConfiguration *configurationForSessionID(const PAL::SessionID& session)
{
    NSURLSessionConfiguration *configuration;
    if (session.isEphemeral()) {
        configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration._shouldSkipPreferredClientCertificateLookup = YES;
    } else
        configuration = [NSURLSessionConfiguration defaultSessionConfiguration];

#if HAVE(ALLOWS_SENSITIVE_LOGGING)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    configuration._allowsSensitiveLogging = NO;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    return configuration;
}

void NetworkSessionCocoa::clientCertificateSuggestedForHost(NetworkDataTaskCocoa::TaskIdentifier taskID, NSURLCredential *credential, const String& host, uint16_t port)
{
    m_suggestedClientCertificates.set(taskID, SuggestedClientCertificate { host, port, credential });
}

void NetworkSessionCocoa::taskReceivedBytes(NetworkDataTaskCocoa::TaskIdentifier identifier)
{
    if (LIKELY(m_suggestedClientCertificates.isEmpty()))
        return;

    auto suggestedClientCertificate = m_suggestedClientCertificates.take(identifier);
    HostAndPort key { suggestedClientCertificate.host, suggestedClientCertificate.port };
    if (suggestedClientCertificate.credential && decltype(m_successfulClientCertificates)::isValidKey(key))
        m_successfulClientCertificates.add(key, suggestedClientCertificate.credential);
}

void NetworkSessionCocoa::taskFailed(NetworkDataTaskCocoa::TaskIdentifier identifier)
{
    if (LIKELY(m_suggestedClientCertificates.isEmpty()))
        return;

    m_suggestedClientCertificates.take(identifier);
}

NSURLCredential *NetworkSessionCocoa::successfulClientCertificateForHost(const String& host, uint16_t port) const
{
    HostAndPort key { host, port };
    if (!decltype(m_successfulClientCertificates)::isValidKey(key))
        return nil;
    return m_successfulClientCertificates.get(key).get();
}

const String& NetworkSessionCocoa::boundInterfaceIdentifier() const
{
    return m_boundInterfaceIdentifier;
}

const String& NetworkSessionCocoa::sourceApplicationBundleIdentifier() const
{
    return m_sourceApplicationBundleIdentifier;
}

const String& NetworkSessionCocoa::sourceApplicationSecondaryIdentifier() const
{
    return m_sourceApplicationSecondaryIdentifier;
}

#if PLATFORM(IOS_FAMILY)
const String& NetworkSessionCocoa::dataConnectionServiceType() const
{
    return m_dataConnectionServiceType;
}
#endif

std::unique_ptr<NetworkSession> NetworkSessionCocoa::create(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
{
    return makeUnique<NetworkSessionCocoa>(networkProcess, WTFMove(parameters));
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

void SessionWrapper::initialize(NSURLSessionConfiguration *configuration, NetworkSessionCocoa& networkSession, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    UNUSED_PARAM(isNavigatingToAppBoundDomain);
#if PLATFORM(IOS_FAMILY)
    NETWORK_SESSION_COCOA_ADDITIONS_1
#endif
    delegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:networkSession wrapper:*this withCredentials:storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use]);
    session = [NSURLSession sessionWithConfiguration:configuration delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]];
}

NetworkSessionCocoa::NetworkSessionCocoa(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
    : NetworkSession(networkProcess, parameters)
    , m_boundInterfaceIdentifier(parameters.boundInterfaceIdentifier)
    , m_sourceApplicationBundleIdentifier(parameters.sourceApplicationBundleIdentifier)
    , m_sourceApplicationSecondaryIdentifier(parameters.sourceApplicationSecondaryIdentifier)
    , m_proxyConfiguration(parameters.proxyConfiguration)
    , m_shouldLogCookieInformation(parameters.shouldLogCookieInformation)
    , m_loadThrottleLatency(parameters.loadThrottleLatency)
    , m_fastServerTrustEvaluationEnabled(parameters.fastServerTrustEvaluationEnabled)
    , m_dataConnectionServiceType(parameters.dataConnectionServiceType)
    , m_isInAppBrowserPrivacyEnabled(parameters.isInAppBrowserPrivacyEnabled)
    , m_preventsSystemHTTPProxyAuthentication(parameters.preventsSystemHTTPProxyAuthentication)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

#if ASSERT_ENABLED
    sessionsCreated = true;
#endif

    NSURLSessionConfiguration *configuration = configurationForSessionID(m_sessionID);

#if HAVE(APP_SSO)
    configuration._preventsAppSSO = true;
#endif

    // Without this, CFNetwork would sometimes add a Content-Type header to our requests (rdar://problem/34748470).
    configuration._suppressedAutoAddedHTTPHeaders = [NSSet setWithObject:@"Content-Type"];

    if (parameters.allowsCellularAccess == AllowsCellularAccess::No)
        configuration.allowsCellularAccess = NO;

    // The WebKit network cache was already queried.
    configuration.URLCache = nil;

    if (auto data = networkProcess.sourceApplicationAuditData())
        configuration._sourceApplicationAuditTokenData = (__bridge NSData *)data.get();

    if (!m_sourceApplicationBundleIdentifier.isEmpty()) {
        configuration._sourceApplicationBundleIdentifier = m_sourceApplicationBundleIdentifier;
        configuration._sourceApplicationAuditTokenData = nil;
    }

    if (!m_sourceApplicationSecondaryIdentifier.isEmpty())
        configuration._sourceApplicationSecondaryIdentifier = m_sourceApplicationSecondaryIdentifier;

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if (!parameters.alternativeServiceDirectory.isEmpty()) {
        SandboxExtension::consumePermanently(parameters.alternativeServiceDirectoryExtensionHandle);
        configuration._alternativeServicesStorage = [[[_NSHTTPAlternativeServicesStorage alloc] initPersistentStoreWithURL:[[NSURL fileURLWithPath:parameters.alternativeServiceDirectory isDirectory:YES] URLByAppendingPathComponent:@"AlternativeService.sqlite"]] autorelease];
    } else
        ASSERT(m_sessionID.isEphemeral());
    configuration._allowsHTTP3 = parameters.http3Enabled;
#endif

    configuration._preventsSystemHTTPProxyAuthentication = parameters.preventsSystemHTTPProxyAuthentication;
    configuration._requiresSecureHTTPSProxyConnection = parameters.requiresSecureHTTPSProxyConnection;
    configuration.connectionProxyDictionary = (NSDictionary *)parameters.proxyConfiguration.get() ?: proxyDictionary(parameters.httpProxy, parameters.httpsProxy);

#if PLATFORM(IOS_FAMILY)
    if (!m_dataConnectionServiceType.isEmpty())
        configuration._CTDataConnectionServiceType = m_dataConnectionServiceType;
#endif

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    networkProcess.supplement<LegacyCustomProtocolManager>()->registerProtocolClass(configuration);
#endif

    configuration._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;

    // FIXME: Replace @"kCFStreamPropertyAutoErrorOnSystemChange" with a constant from the SDK once rdar://problem/40650244 is in a build.
    if (parameters.suppressesConnectionTerminationOnSystemChange)
        configuration._socketStreamProperties = @{ @"kCFStreamPropertyAutoErrorOnSystemChange" : @NO };

#if PLATFORM(WATCHOS)
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

    m_sessionWithCredentialStorage.initialize(configuration, *this, WebCore::StoredCredentialsPolicy::Use, NavigatingToAppBoundDomain::No);
    LOG(NetworkSession, "Created NetworkSession with cookieAcceptPolicy %lu", configuration.HTTPCookieStorage.cookieAcceptPolicy);

    configuration.URLCredentialStorage = nil;
    configuration._shouldSkipPreferredClientCertificateLookup = YES;
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=177394
    // configuration.HTTPCookieStorage = nil;
    // configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;

    m_sessionWithoutCredentialStorage.initialize(configuration, *this, WebCore::StoredCredentialsPolicy::DoNotUse, NavigatingToAppBoundDomain::No);

    m_deviceManagementRestrictionsEnabled = parameters.deviceManagementRestrictionsEnabled;
    m_allLoadsBlockedByDeviceManagementRestrictionsForTesting = parameters.allLoadsBlockedByDeviceManagementRestrictionsForTesting;

    if (m_resourceLoadStatistics && !parameters.resourceLoadStatisticsParameters.appBoundDomains.isEmpty())
        m_resourceLoadStatistics->setAppBoundDomains(WTFMove(parameters.resourceLoadStatisticsParameters.appBoundDomains), [] { });
#if HAVE(SESSION_CLEANUP)
    activateSessionCleanup(*this, parameters);
#endif
}

NetworkSessionCocoa::~NetworkSessionCocoa() = default;

void NetworkSessionCocoa::initializeEphemeralStatelessSession(NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    NSURLSessionConfiguration *existingConfiguration = m_sessionWithoutCredentialStorage.session.get().configuration;

    configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    configuration.URLCredentialStorage = nil;
    configuration.URLCache = nil;
    configuration.allowsCellularAccess = existingConfiguration.allowsCellularAccess;
    configuration.connectionProxyDictionary = existingConfiguration.connectionProxyDictionary;

    configuration._shouldSkipPreferredClientCertificateLookup = YES;
    configuration._sourceApplicationAuditTokenData = existingConfiguration._sourceApplicationAuditTokenData;
    configuration._sourceApplicationSecondaryIdentifier = existingConfiguration._sourceApplicationSecondaryIdentifier;
#if PLATFORM(IOS_FAMILY)
    configuration._CTDataConnectionServiceType = existingConfiguration._CTDataConnectionServiceType;
#endif

    m_ephemeralStatelessSession.initialize(configuration, *this, WebCore::StoredCredentialsPolicy::EphemeralStateless, isNavigatingToAppBoundDomain);
}

SessionWrapper& NetworkSessionCocoa::sessionWrapperForTask(const WebCore::ResourceRequest& request, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    auto shouldBeConsideredAppBound = isNavigatingToAppBoundDomain ? *isNavigatingToAppBoundDomain : NavigatingToAppBoundDomain::Yes;
    if (NETWORK_SESSION_COCOA_ADDITIONS_2)
        shouldBeConsideredAppBound = NavigatingToAppBoundDomain::No;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* storageSession = networkStorageSession()) {
        auto firstParty = WebCore::RegistrableDomain(request.firstPartyForCookies());
        if (storageSession->shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(firstParty))
            return isolatedSession(storedCredentialsPolicy, firstParty, shouldBeConsideredAppBound);
    } else
        ASSERT_NOT_REACHED();
#endif

#if PLATFORM(IOS_FAMILY)
    if (shouldBeConsideredAppBound == NavigatingToAppBoundDomain::Yes)
        return appBoundSession(storedCredentialsPolicy);
#endif

    switch (storedCredentialsPolicy) {
    case WebCore::StoredCredentialsPolicy::Use:
        return m_sessionWithCredentialStorage;
    case WebCore::StoredCredentialsPolicy::DoNotUse:
        return m_sessionWithoutCredentialStorage;
    case WebCore::StoredCredentialsPolicy::EphemeralStateless:
        if (!m_ephemeralStatelessSession.session)
            initializeEphemeralStatelessSession(NavigatingToAppBoundDomain::No);
        return m_ephemeralStatelessSession;
    }
}

SessionWrapper& NetworkSessionCocoa::appBoundSession(WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
    if (!m_appBoundSession) {
        m_appBoundSession = makeUnique<IsolatedSession>();
        m_appBoundSession->sessionWithCredentialStorage.initialize(m_sessionWithCredentialStorage.session.get().configuration, *this, WebCore::StoredCredentialsPolicy::Use, NavigatingToAppBoundDomain::Yes);
        m_appBoundSession->sessionWithoutCredentialStorage.initialize(m_sessionWithoutCredentialStorage.session.get().configuration, *this, WebCore::StoredCredentialsPolicy::DoNotUse, NavigatingToAppBoundDomain::Yes);
    }

    auto& sessionWrapper = [&] (auto storedCredentialsPolicy) -> SessionWrapper& {
        switch (storedCredentialsPolicy) {
        case WebCore::StoredCredentialsPolicy::Use:
            LOG(NetworkSession, "Using app-bound NSURLSession with credential storage.");
            return m_appBoundSession->sessionWithCredentialStorage;
        case WebCore::StoredCredentialsPolicy::DoNotUse:
            LOG(NetworkSession, "Using app-bound NSURLSession without credential storage.");
            return m_appBoundSession->sessionWithoutCredentialStorage;
        case WebCore::StoredCredentialsPolicy::EphemeralStateless:
            if (!m_ephemeralStatelessSession.session)
                initializeEphemeralStatelessSession(NavigatingToAppBoundDomain::Yes);
            return m_ephemeralStatelessSession;
        }
    } (storedCredentialsPolicy);

    return sessionWrapper;
}

SessionWrapper& NetworkSessionCocoa::isolatedSession(WebCore::StoredCredentialsPolicy storedCredentialsPolicy, const WebCore::RegistrableDomain firstPartyDomain, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    auto& entry = m_isolatedSessions.ensure(firstPartyDomain, [this, isNavigatingToAppBoundDomain] {
        auto newEntry = makeUnique<IsolatedSession>();
        newEntry->sessionWithCredentialStorage.initialize(m_sessionWithCredentialStorage.session.get().configuration, *this, WebCore::StoredCredentialsPolicy::Use, isNavigatingToAppBoundDomain);
        newEntry->sessionWithoutCredentialStorage.initialize(m_sessionWithoutCredentialStorage.session.get().configuration, *this, WebCore::StoredCredentialsPolicy::DoNotUse, isNavigatingToAppBoundDomain);
        return newEntry;
    }).iterator->value;

    entry->lastUsed = WallTime::now();

    auto& sessionWrapper = [&] (auto storedCredentialsPolicy) -> SessionWrapper& {
        switch (storedCredentialsPolicy) {
        case WebCore::StoredCredentialsPolicy::Use:
            LOG(NetworkSession, "Using isolated NSURLSession with credential storage.");
            return entry->sessionWithCredentialStorage;
        case WebCore::StoredCredentialsPolicy::DoNotUse:
            LOG(NetworkSession, "Using isolated NSURLSession without credential storage.");
            return entry->sessionWithoutCredentialStorage;
        case WebCore::StoredCredentialsPolicy::EphemeralStateless:
            if (!m_ephemeralStatelessSession.session)
                initializeEphemeralStatelessSession(isNavigatingToAppBoundDomain);
            return m_ephemeralStatelessSession;
        }
    } (storedCredentialsPolicy);
    
    if (m_isolatedSessions.size() > maxNumberOfIsolatedSessions) {
        WebCore::RegistrableDomain keyToRemove;
        auto oldestTimestamp = WallTime::now();
        for (auto& key : m_isolatedSessions.keys()) {
            auto timestamp = m_isolatedSessions.get(key)->lastUsed;
            if (timestamp < oldestTimestamp) {
                oldestTimestamp = timestamp;
                keyToRemove = key;
            }
        }
        LOG(NetworkSession, "About to remove isolated NSURLSession.");
        m_isolatedSessions.remove(keyToRemove);
    }

    RELEASE_ASSERT(m_isolatedSessions.size() <= maxNumberOfIsolatedSessions);

    return sessionWrapper;
}

bool NetworkSessionCocoa::hasIsolatedSession(const WebCore::RegistrableDomain domain) const
{
    return m_isolatedSessions.contains(domain);
}

void NetworkSessionCocoa::clearIsolatedSessions()
{
    m_isolatedSessions.clear();
}

void NetworkSessionCocoa::clearAppBoundSession()
{
    m_appBoundSession = nullptr;
}

void NetworkSessionCocoa::invalidateAndCancel()
{
    NetworkSession::invalidateAndCancel();

    [m_sessionWithCredentialStorage.session invalidateAndCancel];
    [m_sessionWithoutCredentialStorage.session invalidateAndCancel];
    [m_ephemeralStatelessSession.session invalidateAndCancel];
    [m_sessionWithCredentialStorage.delegate sessionInvalidated];
    [m_sessionWithoutCredentialStorage.delegate sessionInvalidated];
    [m_ephemeralStatelessSession.delegate sessionInvalidated];

    for (auto& session : m_isolatedSessions.values()) {
        [session->sessionWithCredentialStorage.session invalidateAndCancel];
        [session->sessionWithCredentialStorage.delegate sessionInvalidated];
        [session->sessionWithoutCredentialStorage.session invalidateAndCancel];
        [session->sessionWithoutCredentialStorage.delegate sessionInvalidated];
    }
    m_isolatedSessions.clear();

    if (m_appBoundSession) {
        [m_appBoundSession->sessionWithCredentialStorage.session invalidateAndCancel];
        [m_appBoundSession->sessionWithCredentialStorage.delegate sessionInvalidated];
        [m_appBoundSession->sessionWithoutCredentialStorage.session invalidateAndCancel];
        [m_appBoundSession->sessionWithoutCredentialStorage.delegate sessionInvalidated];
    }
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
    for (auto& entry : m_isolatedSessions.values())
        entry.session = [NSURLSession sessionWithConfiguration:entry.session.get().configuration delegate:static_cast<id>(entry.delegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    m_appBoundSession.session = [NSURLSession sessionWithConfiguration:m_appBoundSession.session.get().configuration delegate:static_cast<id>(m_appBoundSession.delegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
#endif
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

void NetworkSessionCocoa::continueDidReceiveChallenge(SessionWrapper& sessionWrapper, const WebCore::AuthenticationChallenge& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, NetworkDataTaskCocoa* networkDataTask, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&& completionHandler)
{
    if (!networkDataTask) {
#if HAVE(NSURLSESSION_WEBSOCKET)
        if (auto* webSocketTask = sessionWrapper.webSocketDataTaskMap.get(taskIdentifier)) {
            // FIXME: Handle challenges for web socket.
            completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, { });
            return;
        }
#endif
        auto downloadID = sessionWrapper.downloadMap.get(taskIdentifier);
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
    networkDataTask->didReceiveChallenge(WTFMove(authenticationChallenge), negotiatedLegacyTLS, WTFMove(challengeCompletionHandler));
}

DMFWebsitePolicyMonitor *NetworkSessionCocoa::deviceManagementPolicyMonitor()
{
#if HAVE(DEVICE_MANAGEMENT)
    ASSERT(m_deviceManagementRestrictionsEnabled);
    if (!m_deviceManagementPolicyMonitor)
        m_deviceManagementPolicyMonitor = adoptNS([allocDMFWebsitePolicyMonitorInstance() initWithPolicyChangeHandler:nil]);
    return m_deviceManagementPolicyMonitor.get();
#else
    RELEASE_ASSERT_NOT_REACHED();
    return nil;
#endif
}

#if HAVE(NSURLSESSION_WEBSOCKET)
std::unique_ptr<WebSocketTask> NetworkSessionCocoa::createWebSocketTask(NetworkSocketChannel& channel, const WebCore::ResourceRequest& request, const String& protocol)
{
    // FIXME: Use protocol.
    RetainPtr<NSURLSessionWebSocketTask> task = [m_sessionWithCredentialStorage.session webSocketTaskWithRequest:request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)];
    return makeUnique<WebSocketTask>(channel, WTFMove(task));
}

void NetworkSessionCocoa::addWebSocketTask(WebSocketTask& task)
{
    RELEASE_ASSERT(!m_sessionWithCredentialStorage.webSocketDataTaskMap.contains(task.identifier()));
    m_sessionWithCredentialStorage.webSocketDataTaskMap.add(task.identifier(), &task);
}

void NetworkSessionCocoa::removeWebSocketTask(WebSocketTask& task)
{
    RELEASE_ASSERT(m_sessionWithCredentialStorage.webSocketDataTaskMap.contains(task.identifier()));
    m_sessionWithCredentialStorage.webSocketDataTaskMap.remove(task.identifier());
}

#endif // HAVE(NSURLSESSION_WEBSOCKET)

Vector<WebCore::SecurityOriginData> NetworkSessionCocoa::hostNamesWithAlternativeServices() const
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    Vector<WebCore::SecurityOriginData> origins;
    _NSHTTPAlternativeServicesStorage* storage = m_sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    NSArray<_NSHTTPAlternativeServiceEntry *> *entries = [storage HTTPServiceEntriesWithFilter:_NSHTTPAlternativeServicesFilter.emptyFilter];

    for (_NSHTTPAlternativeServiceEntry* entry in entries) {
        WebCore::SecurityOriginData origin = { "https"_s, entry.host, entry.port };
        origins.append(origin);
    }
    return origins;
#else
    return { };
#endif
}

void NetworkSessionCocoa::deleteAlternativeServicesForHostNames(const Vector<String>& hosts)
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    _NSHTTPAlternativeServicesStorage* storage = m_sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    for (auto& host : hosts)
        [storage removeHTTPAlternativeServiceEntriesWithRegistrableDomain:host];
#else
    UNUSED_PARAM(hosts);
#endif
}

void NetworkSessionCocoa::clearAlternativeServices(WallTime modifiedSince)
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    _NSHTTPAlternativeServicesStorage* storage = m_sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    [storage removeHTTPAlternativeServiceEntriesCreatedAfterDate:date];
#else
    UNUSED_PARAM(modifiedSince);
#endif
}

} // namespace WebKit
