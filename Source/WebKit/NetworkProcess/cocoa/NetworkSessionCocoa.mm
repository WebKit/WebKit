/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#import "AppStoreDaemonSPI.h"
#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationManager.h"
#import "DataReference.h"
#import "DefaultWebBrowserChecks.h"
#import "Download.h"
#import "LegacyCustomProtocolManager.h"
#import "Logging.h"
#import "NetworkConnectionIntegrityHelpers.h"
#import "NetworkDataTaskCocoa.h"
#import "NetworkLoad.h"
#import "NetworkProcess.h"
#import "NetworkSessionCreationParameters.h"
#import "PrivateRelayed.h"
#import "WKURLSessionTaskDelegate.h"
#import "WebPageNetworkParameters.h"
#import "WebSocketTask.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/Credential.h>
#import <WebCore/FormDataStreamMac.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/NetworkConnectionIntegrity.h>
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
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/WeakLinking.h>
#import <wtf/text/WTFString.h>

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(APP_PRIVACY_REPORT) && HAVE(SYMPTOMS_FRAMEWORK)
#import <Symptoms/SymptomAnalytics.h>
#import <Symptoms/SymptomPresentationFeed.h>

SOFT_LINK_PRIVATE_FRAMEWORK_IN_UMBRELLA_OPTIONAL(Symptoms, SymptomAnalytics);
SOFT_LINK_PRIVATE_FRAMEWORK_IN_UMBRELLA_OPTIONAL(Symptoms, SymptomPresentationFeed);
SOFT_LINK_PRIVATE_FRAMEWORK_IN_UMBRELLA_OPTIONAL(Symptoms, SymptomPresentationLite);
SOFT_LINK_CLASS_OPTIONAL(SymptomAnalytics, AnalyticsWorkspace);
SOFT_LINK_CLASS_OPTIONAL(SymptomPresentationFeed, UsageFeed);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryBundleIDs, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryStartDate, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryEndDate, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationFeed, kSymptomAnalyticsServiceDomainTrackingClearHistoryKey, const NSString *);
SOFT_LINK_CONSTANT_MAY_FAIL(SymptomPresentationLite, kSymptomAnalyticsServiceEndpoint, NSString *)
#endif

#else
void WebKit::NetworkSessionCocoa::removeNetworkWebsiteData(std::optional<WallTime>, std::optional<HashSet<WebCore::RegistrableDomain>>&&, CompletionHandler<void()>&& completionHandler) { completionHandler(); }
#endif

#import "DeviceManagementSoftLink.h"

// FIXME: Remove this soft link once rdar://problem/50109631 is in a build and bots are updated.
SOFT_LINK_FRAMEWORK(CFNetwork)
SOFT_LINK_CLASS_OPTIONAL(CFNetwork, _NSHSTSStorage)

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

#if HAVE(NETWORK_CONNECTION_PRIVACY_STANCE)
static WebCore::PrivacyStance toPrivacyStance(nw_connection_privacy_stance_t stance)
{
    switch (stance) {
    case nw_connection_privacy_stance_unknown:
        return WebCore::PrivacyStance::Unknown;
    case nw_connection_privacy_stance_not_eligible:
        return WebCore::PrivacyStance::NotEligible;
    case nw_connection_privacy_stance_proxied:
        return WebCore::PrivacyStance::Proxied;
    case nw_connection_privacy_stance_failed:
        return WebCore::PrivacyStance::Failed;
    case nw_connection_privacy_stance_direct:
        return WebCore::PrivacyStance::Direct;
#if defined(NW_CONNECTION_HAS_PRIVACY_STANCE_FAILED_UNREACHABLE)
    case nw_connection_privacy_stance_failed_unreachable:
        return WebCore::PrivacyStance::FailedUnreachable;
#endif
    }
    ASSERT_NOT_REACHED();
    return WebCore::PrivacyStance::Unknown;
}

static NSString* privacyStanceToString(WebCore::PrivacyStance stance)
{
    switch (stance) {
    case WebCore::PrivacyStance::Unknown:
        return @"Unknown";
    case WebCore::PrivacyStance::NotEligible:
        return @"NotEligible";
    case WebCore::PrivacyStance::Proxied:
        return @"Proxied";
    case WebCore::PrivacyStance::Failed:
        return @"Failed";
    case WebCore::PrivacyStance::Direct:
        return @"Direct";
    case WebCore::PrivacyStance::FailedUnreachable:
        return @"FailedUnreachable";
    }
    ASSERT_NOT_REACHED();
    return @"Unknown";
}
#endif

#if HAVE(CFNETWORK_METRICS_APIS_V4)
static String stringForTLSProtocolVersion(tls_protocol_version_t protocol)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
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
ALLOW_DEPRECATED_DECLARATIONS_END
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
        return "All"_s;
    case kSSLProtocolUnknown:
        return "Unknown"_s;
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

    _session = session.get();
    _sessionWrapper = sessionWrapper;
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

#if ENABLE(TRACKING_PREVENTION)
static NSURLRequest* downgradeRequest(NSURLRequest *request)
{
    auto nsMutableRequest = adoptNS([request mutableCopy]);
    if ([[nsMutableRequest URL].scheme isEqualToString:@"https"]) {
        NSURLComponents *components = [NSURLComponents componentsWithURL:[nsMutableRequest URL] resolvingAgainstBaseURL:NO];
        components.scheme = @"http";
        [nsMutableRequest setURL:components.URL];
        ASSERT([[nsMutableRequest URL].scheme isEqualToString:@"http"]);
        return nsMutableRequest.autorelease();
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

static void setIgnoreHSTS(RetainPtr<NSURLRequest>& request, bool shouldIgnoreHSTS)
{
    auto mutableRequest = adoptNS([request mutableCopy]);
    setIgnoreHSTS(mutableRequest.get(), shouldIgnoreHSTS);
    request = mutableRequest;
}

static bool ignoreHSTS(NSURLRequest *request)
{
    return [request respondsToSelector:@selector(_ignoreHSTS)]
        && [request _ignoreHSTS];
}

static void updateIgnoreStrictTransportSecuritySetting(RetainPtr<NSURLRequest>& request, bool shouldIgnoreHSTS)
{
    auto scheme = request.get().URL.scheme;
    if ([scheme isEqualToString:@"https"]) {
        if (shouldIgnoreHSTS && ignoreHSTS(request.get())) {
            // The request was upgraded for some other reason than HSTS.
            // Don't ignore HSTS to avoid the risk of another downgrade.
            setIgnoreHSTS(request, false);
        }
    } else if ([scheme isEqualToString:@"http"]) {
        if (ignoreHSTS(request.get()) != shouldIgnoreHSTS)
            setIgnoreHSTS(request, shouldIgnoreHSTS);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu willPerformHTTPRedirection from %s to %s", taskIdentifier, response.URL.absoluteString.UTF8String, request.URL.absoluteString.UTF8String);

    if (auto* networkDataTask = [self existingTask:task]) {
        bool shouldIgnoreHSTS = false;
#if ENABLE(TRACKING_PREVENTION)
        if (auto* sessionCocoa = networkDataTask->networkSession()) {
            auto* storageSession = sessionCocoa->networkProcess().storageSession(sessionCocoa->sessionID());
            NSURL *firstPartyForCookies = networkDataTask->isTopLevelNavigation() ? request.URL : request.mainDocumentURL;
            shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request)
                && storageSession->shouldBlockCookies(firstPartyForCookies, request.URL, networkDataTask->frameID(), networkDataTask->pageID(), networkDataTask->shouldRelaxThirdPartyCookieBlocking());
            if (shouldIgnoreHSTS) {
                request = downgradeRequest(request);
                ASSERT([request.URL.scheme isEqualToString:@"http"]);
                LOG(NetworkSession, "%llu Downgraded %s from https to http", taskIdentifier, request.URL.absoluteString.UTF8String);
            }
        } else
            ASSERT_NOT_REACHED();
#endif

        WebCore::ResourceResponse resourceResponse(response);
        networkDataTask->checkTAO(resourceResponse);

        networkDataTask->willPerformHTTPRedirection(WTFMove(resourceResponse), request, [session = networkDataTask->networkSession(), completionHandler = makeBlockPtr(completionHandler), taskIdentifier, shouldIgnoreHSTS](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%llu willPerformHTTPRedirection completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            auto nsRequest = retainPtr(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody));
            updateIgnoreStrictTransportSecuritySetting(nsRequest, shouldIgnoreHSTS);
            completionHandler(nsRequest.get());
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
#if ENABLE(TRACKING_PREVENTION)
        if (auto* sessionCocoa = networkDataTask->networkSession()) {
            auto* storageSession = sessionCocoa->networkProcess().storageSession(sessionCocoa->sessionID());
            shouldIgnoreHSTS = schemeWasUpgradedDueToDynamicHSTS(request)
                && storageSession->shouldBlockCookies(request, networkDataTask->frameID(), networkDataTask->pageID(), networkDataTask->shouldRelaxThirdPartyCookieBlocking());
            if (shouldIgnoreHSTS) {
                request = downgradeRequest(request);
                ASSERT([request.URL.scheme isEqualToString:@"http"]);
                LOG(NetworkSession, "%llu Downgraded %s from https to http", taskIdentifier, request.URL.absoluteString.UTF8String);
            }
        } else
            ASSERT_NOT_REACHED();
#endif

        WebCore::ResourceResponse synthesizedResponse = WebCore::synthesizeRedirectResponseIfNecessary([task currentRequest], request, nil);
        NSString *origin = [request valueForHTTPHeaderField:@"Origin"] ?: @"*";
        synthesizedResponse.setHTTPHeaderField(WebCore::HTTPHeaderName::AccessControlAllowOrigin, origin);
        networkDataTask->willPerformHTTPRedirection(WTFMove(synthesizedResponse), request, [completionHandler = makeBlockPtr(completionHandler), taskIdentifier, shouldIgnoreHSTS](auto&& request) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%llu _schemeUpgraded completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            auto nsRequest = retainPtr(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody));
            updateIgnoreStrictTransportSecuritySetting(nsRequest, shouldIgnoreHSTS);
            completionHandler(nsRequest.get());
        });
    } else {
        LOG(NetworkSession, "%llu _schemeUpgraded completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

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

void NetworkSessionCocoa::setClientAuditToken(const WebCore::AuthenticationChallenge& challenge)
{
#if HAVE(SEC_TRUST_SET_CLIENT_AUDIT_TOKEN)
    if (auto auditData = networkProcess().sourceApplicationAuditData())
        SecTrustSetClientAuditToken(challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust, auditData.get());
#else
    UNUSED_PARAM(challenge);
#endif
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
        sessionCocoa->setClientAuditToken(challenge);
        if (NetworkSessionCocoa::allowsSpecificHTTPSCertificateForHost(challenge))
            return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);

        NSURLSessionTaskTransactionMetrics *metrics = task._incompleteTaskMetrics.transactionMetrics.lastObject;
        auto tlsVersion = (tls_protocol_version_t)metrics.negotiatedTLSProtocolVersion.unsignedShortValue;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (tlsVersion == tls_protocol_version_TLSv10 || tlsVersion == tls_protocol_version_TLSv11)
            negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
ALLOW_DEPRECATED_DECLARATIONS_END

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (negotiatedLegacyTLS == NegotiatedLegacyTLS::No && [task respondsToSelector:@selector(_TLSNegotiatedProtocolVersion)]) {
            SSLProtocol tlsVersion = [task _TLSNegotiatedProtocolVersion];
            if (tlsVersion == kTLSProtocol11 || tlsVersion == kTLSProtocol1)
                negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
        }
        ALLOW_DEPRECATED_DECLARATIONS_END

        if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes && task._preconnect)
            return completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);

        // Handle server trust evaluation at platform-level if requested, for performance reasons and to use ATS defaults.
        if (sessionCocoa->fastServerTrustEvaluationEnabled() && negotiatedLegacyTLS == NegotiatedLegacyTLS::No) {
            auto* networkDataTask = [self existingTask:task];
            if (networkDataTask) {
                NSURLProtectionSpace *protectionSpace = challenge.protectionSpace;
                networkDataTask->didNegotiateModernTLS(URL { makeString(String(protectionSpace.protocol), "://", String(protectionSpace.host), ':', protectionSpace.port) });
            }
            auto decisionHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKNetworkSessionDelegate>(self), sessionCocoa = WeakPtr { sessionCocoa }, completionHandler = makeBlockPtr(completionHandler), taskIdentifier, networkDataTask = RefPtr { networkDataTask }, negotiatedLegacyTLS](NSURLAuthenticationChallenge *challenge, OSStatus trustResult) mutable {
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
        }
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
#if HAVE(NETWORK_CONNECTION_PRIVACY_STANCE)
        if (auto* networkDataTask = [self existingTask:task])
            newUserInfo[@"networkTaskMetricsPrivacyStance"] = privacyStanceToString(networkDataTask->networkLoadMetrics().privacyStance);
#endif
        error = [NSError errorWithDomain:[error domain] code:[error code] userInfo:newUserInfo];
    }

    if (auto* networkDataTask = [self existingTask:task])
        networkDataTask->didCompleteWithError(error, networkDataTask->networkLoadMetrics());
    else if (error) {
        if (!_sessionWrapper)
            return;
        auto downloadID = _sessionWrapper->downloadMap.take(task.taskIdentifier);
        if (!downloadID)
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
        download->didFail(error, resumeDataReference);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)metrics
{
    LOG(NetworkSession, "%llu didFinishCollectingMetrics", task.taskIdentifier);
    if (auto* networkDataTask = [self existingTask:task]) {
        NSArray<NSURLSessionTaskTransactionMetrics *> *transactionMetrics = metrics.transactionMetrics;
        NSURLSessionTaskTransactionMetrics *m = transactionMetrics.lastObject;

        auto dateToMonotonicTime = [] (NSDate *date) {
            if (auto interval = date.timeIntervalSince1970)
                return WallTime::fromRawSeconds(interval).approximateMonotonicTime();
            return MonotonicTime { };
        };

        auto& networkLoadMetrics = networkDataTask->networkLoadMetrics();
        networkLoadMetrics.redirectStart = dateToMonotonicTime(transactionMetrics.firstObject.fetchStartDate);
        networkLoadMetrics.fetchStart = dateToMonotonicTime(m.fetchStartDate);
        networkLoadMetrics.domainLookupStart = dateToMonotonicTime(m.domainLookupStartDate);
        networkLoadMetrics.domainLookupEnd = dateToMonotonicTime(m.domainLookupEndDate);
        networkLoadMetrics.connectStart = dateToMonotonicTime(m.connectStartDate);
        if (m.reusedConnection && [m.response.URL.scheme isEqualToString:@"https"])
            networkLoadMetrics.secureConnectionStart = WebCore::reusedTLSConnectionSentinel;
        else
            networkLoadMetrics.secureConnectionStart = dateToMonotonicTime(m.secureConnectionStartDate);
        networkLoadMetrics.connectEnd = dateToMonotonicTime(m.connectEndDate);
        networkLoadMetrics.requestStart = dateToMonotonicTime(m.requestStartDate);
        // Sometimes, likely because of <rdar://90997689>, responseStart is before requestStart. If this happens, use the later of the two.
        networkLoadMetrics.responseStart = std::max(networkLoadMetrics.requestStart, dateToMonotonicTime(m.responseStartDate));
        networkLoadMetrics.responseEnd = dateToMonotonicTime(m.responseEndDate);
        networkLoadMetrics.markComplete();
        networkLoadMetrics.redirectCount = metrics.redirectCount;
        networkLoadMetrics.protocol = String(m.networkProtocolName);
        networkLoadMetrics.cellular = m.cellular;
        networkLoadMetrics.expensive = m.expensive;
        networkLoadMetrics.constrained = m.constrained;
        networkLoadMetrics.multipath = m.multipath;
        networkLoadMetrics.isReusedConnection = m.isReusedConnection;

#if HAVE(NETWORK_CONNECTION_PRIVACY_STANCE)
        networkLoadMetrics.privacyStance = toPrivacyStance(m._privacyStance);
#endif

        if (networkDataTask->shouldCaptureExtraNetworkLoadMetrics()) {
            auto additionalMetrics = WebCore::AdditionalNetworkLoadMetricsForWebInspector::create();
            additionalMetrics->priority = toNetworkLoadPriority(task.priority);

#if HAVE(CFNETWORK_METRICS_APIS_V4)
            if (auto port = [m.remotePort unsignedIntValue])
                additionalMetrics->remoteAddress = makeString(String(m.remoteAddress), ':', port);
            else
                additionalMetrics->remoteAddress = m.remoteAddress;
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            additionalMetrics->remoteAddress = String(m._remoteAddressAndPort);
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            additionalMetrics->connectionIdentifier = String([m._connectionIdentifier UUIDString]);

#if HAVE(CFNETWORK_METRICS_APIS_V4)
            additionalMetrics->tlsProtocol = stringForTLSProtocolVersion((tls_protocol_version_t)[m.negotiatedTLSProtocolVersion unsignedShortValue]);
            additionalMetrics->tlsCipher = stringForTLSCipherSuite((tls_ciphersuite_t)[m.negotiatedTLSCipherSuite unsignedShortValue]);
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            additionalMetrics->tlsProtocol = stringForSSLProtocol(m._negotiatedTLSProtocol);
            additionalMetrics->tlsCipher = stringForSSLCipher(m._negotiatedTLSCipher);
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif

            __block WebCore::HTTPHeaderMap requestHeaders;
            [m.request.allHTTPHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSString *value, BOOL *) {
                requestHeaders.set(String(name), String(value));
            }];
            additionalMetrics->requestHeaders = WTFMove(requestHeaders);

            uint64_t requestHeaderBytesSent = 0;
            uint64_t responseHeaderBytesReceived = 0;

            for (NSURLSessionTaskTransactionMetrics *transactionMetrics in metrics.transactionMetrics) {
#if HAVE(CFNETWORK_METRICS_APIS_V4)
                requestHeaderBytesSent += transactionMetrics.countOfRequestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics.countOfResponseHeaderBytesReceived;
#else
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                requestHeaderBytesSent += transactionMetrics._requestHeaderBytesSent;
                responseHeaderBytesReceived += transactionMetrics._responseHeaderBytesReceived;
                ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            }

            additionalMetrics->requestHeaderBytesSent = requestHeaderBytesSent;
            additionalMetrics->requestBodyBytesSent = task.countOfBytesSent;
            additionalMetrics->responseHeaderBytesReceived = responseHeaderBytesReceived;

            additionalMetrics->isProxyConnection = m.proxyConnection;

            networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector = WTFMove(additionalMetrics);
        }
#if HAVE(CFNETWORK_METRICS_APIS_V4)
        networkLoadMetrics.responseBodyBytesReceived = m.countOfResponseBodyBytesReceived;
        networkLoadMetrics.responseBodyDecodedSize = m.countOfResponseBodyBytesAfterDecoding;
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        networkLoadMetrics.responseBodyBytesReceived = m._responseBodyBytesReceived;
        networkLoadMetrics.responseBodyDecodedSize = m._responseBodyBytesDecoded;
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        // Sometimes the encoded body bytes received contains a few (3 or so) bytes from the header when there is no body.
        // When this happens, trim our metrics to make more sense.
        if (!networkLoadMetrics.responseBodyDecodedSize)
            networkLoadMetrics.responseBodyBytesReceived = 0;
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    auto taskIdentifier = dataTask.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveResponse", taskIdentifier);
    if (auto* networkDataTask = [self existingTask:dataTask]) {
        ASSERT(RunLoop::isMain());

        NegotiatedLegacyTLS negotiatedLegacyTLS = NegotiatedLegacyTLS::No;
        NSURLSessionTaskMetrics *taskMetrics = dataTask._incompleteTaskMetrics;

        NSURLSessionTaskTransactionMetrics *metrics = taskMetrics.transactionMetrics.lastObject;
#if HAVE(NETWORK_CONNECTION_PRIVACY_STANCE)
        auto privateRelayed = metrics._privacyStance == nw_connection_privacy_stance_direct
            || metrics._privacyStance == nw_connection_privacy_stance_not_eligible
            ? PrivateRelayed::No : PrivateRelayed::Yes;
#else
        auto privateRelayed = PrivateRelayed::No;
#endif
        auto tlsVersion = (tls_protocol_version_t)metrics.negotiatedTLSProtocolVersion.unsignedShortValue;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (tlsVersion == tls_protocol_version_TLSv10 || tlsVersion == tls_protocol_version_TLSv11)
            negotiatedLegacyTLS = NegotiatedLegacyTLS::Yes;
ALLOW_DEPRECATED_DECLARATIONS_END
        
        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [response isKindOfClass:NSHTTPURLResponse.class] ? [(NSHTTPURLResponse *)response statusCode] : 0;
        if (statusCode != 304) {
            bool isMainResourceLoad = networkDataTask->firstRequest().requester() == WebCore::ResourceRequestRequester::Main;
            WebCore::adjustMIMETypeIfNecessary(response._CFURLResponse, isMainResourceLoad);
        }

        WebCore::ResourceResponse resourceResponse(response);
        // Lazy initialization is not helpful in the WebKit2 case because we always end up initializing
        // all the fields when sending the response to the WebContent process over IPC.
        resourceResponse.disableLazyInitialization();

        networkDataTask->checkTAO(resourceResponse);

        resourceResponse.setDeprecatedNetworkLoadMetrics(WebCore::copyTimingData(taskMetrics, networkDataTask->networkLoadMetrics()));

        networkDataTask->didReceiveResponse(WTFMove(resourceResponse), negotiatedLegacyTLS, privateRelayed, [completionHandler = makeBlockPtr(completionHandler), taskIdentifier](WebCore::PolicyAction policyAction) {
#if !LOG_DISABLED
            LOG(NetworkSession, "%llu didReceiveResponse completionHandler (%d)", taskIdentifier, policyAction);
#else
            UNUSED_PARAM(taskIdentifier);
#endif
            completionHandler(toNSURLSessionResponseDisposition(policyAction));
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
    if (!downloadID)
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
    if (!downloadID)
        return;
    if (!_session)
        return;
    auto* download = _session->networkProcess().downloadManager().download(downloadID);
    if (!download)
        return;
    download->didReceiveData(bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
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
        // FIXME: We can re-enable ASSERT below once NSURLSession bug rdar://problem/72383646 is fixed.
        // ASSERT([reason isEqualToData:task.closeReason]);
        ASSERT(closeCode == [task closeCode]);
        auto closeReason = adoptNS([[NSString alloc] initWithData:reason encoding:NSUTF8StringEncoding]);
        webSocketTask->didClose(closeCode, closeReason.get());
    }
}
#endif

@end

namespace WebKit {

static RetainPtr<NSURLSession> createURLSession(NSURLSessionConfiguration *configuration, id<NSURLSessionDelegate> delegate)
{
    RetainPtr session = [NSURLSession sessionWithConfiguration:configuration delegate:delegate delegateQueue:NSOperationQueue.mainQueue];
#if ENABLE(NETWORK_CONNECTION_INTEGRITY)
    configureForNetworkConnectionIntegrity(session.get());
#endif
    return session;
}

#if ASSERT_ENABLED
static bool sessionsCreated = false;
#endif

static NSURLSessionConfiguration *configurationForSessionID(PAL::SessionID session, bool isFullWebBrowser)
{
#if HAVE(LOGGING_PRIVACY_LEVEL)
    auto loggingPrivacyLevel = nw_context_privacy_level_sensitive;
#endif

    NSURLSessionConfiguration *configuration;
    if (session.isEphemeral()) {
        configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
#if HAVE(LOGGING_PRIVACY_LEVEL) && defined(NW_CONTEXT_HAS_PRIVACY_LEVEL_SILENT)
        if (isFullWebBrowser)
            loggingPrivacyLevel = nw_context_privacy_level_silent;
#endif
    } else
        configuration = [NSURLSessionConfiguration defaultSessionConfiguration];

#if PLATFORM(MAC)
    bool preventCFNetworkClientCertificateLookup = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoClientCertificateLookup) || session.isEphemeral();
#else
    bool preventCFNetworkClientCertificateLookup = true;
#endif
    configuration._shouldSkipPreferredClientCertificateLookup = preventCFNetworkClientCertificateLookup;

#if HAVE(LOGGING_PRIVACY_LEVEL)
    auto setLoggingPrivacyLevel = NSSelectorFromString(@"set_loggingPrivacyLevel:");
    if ([configuration respondsToSelector:setLoggingPrivacyLevel]) {
        wtfObjCMsgSend<void>(configuration, setLoggingPrivacyLevel, loggingPrivacyLevel);
        RELEASE_LOG(NetworkSession, "Setting logging level for %{public}s session %" PRIu64 " to %{public}s", session.isEphemeral() ? "Ephemeral" : "Regular", session.toUInt64(), loggingPrivacyLevel == nw_context_privacy_level_silent ? "silent" : "sensitive");
    }
#elif HAVE(ALLOWS_SENSITIVE_LOGGING)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    configuration._allowsSensitiveLogging = NO;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

#if HAVE(CFNETWORK_NSURLSESSION_CONNECTION_CACHE_LIMITS)
    if (WebCore::ResourceRequest::resourcePrioritiesEnabled()) {
        configuration._connectionCacheNumPriorityLevels = WebCore::resourceLoadPriorityCount;
        configuration._connectionCacheMinimumFastLanePriority = toPlatformRequestPriority(WebCore::ResourceLoadPriority::Medium);
        configuration._connectionCacheNumFastLanes = 1;
    }
#endif

#if ENABLE(NETWORK_ISSUE_REPORTING)
    if ([configuration respondsToSelector:@selector(set_skipsStackTraceCapture:)])
        configuration._skipsStackTraceCapture = YES;
#endif

    return configuration;
}

_NSHSTSStorage *NetworkSessionCocoa::hstsStorage() const
{
#if HAVE(HSTS_STORAGE)
    NSURLSessionConfiguration *configuration = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration;
    // FIXME: Remove this respondsToSelector check once rdar://problem/50109631 is in a build and bots are updated.
    if ([configuration respondsToSelector:@selector(_hstsStorage)])
        return configuration._hstsStorage;
#endif
    return nil;
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

std::unique_ptr<NetworkSession> NetworkSessionCocoa::create(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
{
    return makeUnique<NetworkSessionCocoa>(networkProcess, parameters);
}

static RetainPtr<NSDictionary> proxyDictionary(const URL& httpProxy, const URL& httpsProxy)
{
    if (!httpProxy.isValid() && !httpsProxy.isValid())
        return nil;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN

    auto dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    if (httpProxy.isValid()) {
        [dictionary setObject:httpProxy.host().createNSString().get() forKey:(NSString *)kCFStreamPropertyHTTPProxyHost];
        if (auto port = httpProxy.port())
            [dictionary setObject:@(*port) forKey:(NSString *)kCFStreamPropertyHTTPProxyPort];
    }
    if (httpsProxy.isValid()) {
        [dictionary setObject:httpsProxy.host().createNSString().get() forKey:(NSString *)kCFStreamPropertyHTTPSProxyHost];
        if (auto port = httpsProxy.port())
            [dictionary setObject:@(*port) forKey:(NSString *)kCFStreamPropertyHTTPSProxyPort];
    }
    return dictionary;

    ALLOW_DEPRECATED_DECLARATIONS_END
}

void SessionWrapper::initialize(NSURLSessionConfiguration *configuration, NetworkSessionCocoa& networkSession, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    UNUSED_PARAM(isNavigatingToAppBoundDomain);

    auto isFullBrowser = isParentProcessAFullWebBrowser(networkSession.networkProcess());
#if PLATFORM(MAC)
    isFullBrowser = WebCore::MacApplication::isSafari();
#endif
    if (!configuration._sourceApplicationSecondaryIdentifier && isFullBrowser)
        configuration._sourceApplicationSecondaryIdentifier = @"com.apple.WebKit.InAppBrowser";

    delegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:networkSession wrapper:*this withCredentials:storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use]);
    session = createURLSession(configuration, delegate.get());
}

#if HAVE(SESSION_CLEANUP)
static void activateSessionCleanup(NetworkSessionCocoa& session, const NetworkSessionCreationParameters& parameters)
{
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000)
    // Don't override an explicitly set value.
    if (parameters.resourceLoadStatisticsParameters.isTrackingPreventionStateExplicitlySet)
        return;

#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    bool trackingPreventionEnabled = doesParentProcessHaveTrackingPreventionEnabled(session.networkProcess(), parameters.appHasRequestedCrossWebsiteTrackingPermission);
    bool passedEnabledState = session.isTrackingPreventionEnabled();

    // We do not need to log a discrepancy between states for WebKitTestRunner or TestWebKitAPI.
    if (trackingPreventionEnabled != passedEnabledState && !isRunningTest(WebCore::applicationBundleIdentifier()))
        WTFLogAlways("Passed ITP enabled state (%d) does not match TCC setting (%d)\n", passedEnabledState, trackingPreventionEnabled);
    session.setTrackingPreventionEnabled(passedEnabledState);
#endif
#endif
}
#endif

NetworkSessionCocoa::NetworkSessionCocoa(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    : NetworkSession(networkProcess, parameters)
    , m_defaultSessionSet(SessionSet::create())
    , m_boundInterfaceIdentifier(parameters.boundInterfaceIdentifier)
    , m_sourceApplicationBundleIdentifier(parameters.sourceApplicationBundleIdentifier)
    , m_sourceApplicationSecondaryIdentifier(parameters.sourceApplicationSecondaryIdentifier)
    , m_proxyConfiguration(parameters.proxyConfiguration)
    , m_shouldLogCookieInformation(parameters.shouldLogCookieInformation)
    , m_fastServerTrustEvaluationEnabled(parameters.fastServerTrustEvaluationEnabled)
    , m_dataConnectionServiceType(parameters.dataConnectionServiceType)
    , m_preventsSystemHTTPProxyAuthentication(parameters.preventsSystemHTTPProxyAuthentication)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

#if ASSERT_ENABLED
    sessionsCreated = true;
#endif

    NSURLSessionConfiguration *configuration = configurationForSessionID(m_sessionID, isParentProcessAFullWebBrowser(networkProcess));

    m_blobRegistry.setFileDirectory(FileSystem::createTemporaryDirectory(@"BlobRegistryFiles"));

#if HAVE(HSTS_STORAGE)
    if (!!parameters.hstsStorageDirectory && !m_sessionID.isEphemeral()) {
        SandboxExtension::consumePermanently(parameters.hstsStorageDirectoryExtensionHandle);
        // FIXME: Remove this respondsToSelector check once rdar://problem/50109631 is in a build and bots are updated.
        if ([configuration respondsToSelector:@selector(_hstsStorage)])
            configuration._hstsStorage = adoptNS([alloc_NSHSTSStorageInstance() initPersistentStoreWithURL:[NSURL fileURLWithPath:parameters.hstsStorageDirectory isDirectory:YES]]).get();
    }
#endif

#if HAVE(NETWORK_LOADER)
    RELEASE_LOG_IF(parameters.useNetworkLoader, NetworkSession, "Using experimental network loader.");
    configuration._usesNWLoader = parameters.useNetworkLoader;
#endif

    if (parameters.allowsHSTSWithUntrustedRootCertificate && [configuration respondsToSelector:@selector(_allowsHSTSWithUntrustedRootCertificate)])
        configuration._allowsHSTSWithUntrustedRootCertificate = YES;
    
#if HAVE(APP_SSO) || PLATFORM(MACCATALYST)
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
        configuration._alternativeServicesStorage = adoptNS([[_NSHTTPAlternativeServicesStorage alloc] initPersistentStoreWithURL:[[NSURL fileURLWithPath:parameters.alternativeServiceDirectory isDirectory:YES] URLByAppendingPathComponent:@"AlternativeService.sqlite" isDirectory:NO]]).get();
    }
#endif

    configuration._preventsSystemHTTPProxyAuthentication = parameters.preventsSystemHTTPProxyAuthentication;
    configuration._requiresSecureHTTPSProxyConnection = parameters.requiresSecureHTTPSProxyConnection;
    configuration.connectionProxyDictionary = (NSDictionary *)parameters.proxyConfiguration.get() ?: proxyDictionary(parameters.httpProxy, parameters.httpsProxy).get();

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

    RetainPtr<NSHTTPCookieStorage> cookieStorage;
    if (CFHTTPCookieStorageRef storage = storageSession->cookieStorage().get()) {
        cookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage]);
        configuration.HTTPCookieStorage = cookieStorage.get();
    } else
        cookieStorage = storageSession->nsCookieStorage();

#if HAVE(CFNETWORK_OVERRIDE_SESSION_COOKIE_ACCEPT_POLICY)
    // We still need to check the selector since CFNetwork updates and WebKit updates are separate
    // on older macOS.
    if ([cookieStorage respondsToSelector:@selector(_overrideSessionCookieAcceptPolicy)])
        cookieStorage.get()._overrideSessionCookieAcceptPolicy = YES;
#endif

    initializeNSURLSessionsInSet(m_defaultSessionSet.get(), configuration);

    m_deviceManagementRestrictionsEnabled = parameters.deviceManagementRestrictionsEnabled;
    m_allLoadsBlockedByDeviceManagementRestrictionsForTesting = parameters.allLoadsBlockedByDeviceManagementRestrictionsForTesting;

#if ENABLE(APP_BOUND_DOMAINS)
    if (m_resourceLoadStatistics && !parameters.resourceLoadStatisticsParameters.appBoundDomains.isEmpty())
        m_resourceLoadStatistics->setAppBoundDomains(HashSet<WebCore::RegistrableDomain> { parameters.resourceLoadStatisticsParameters.appBoundDomains }, [] { });
#endif

#if ENABLE(MANAGED_DOMAINS)
    if (m_resourceLoadStatistics && !parameters.resourceLoadStatisticsParameters.managedDomains.isEmpty())
        m_resourceLoadStatistics->setManagedDomains(HashSet<WebCore::RegistrableDomain> { parameters.resourceLoadStatisticsParameters.managedDomains }, [] { });
#endif

#if HAVE(SESSION_CLEANUP)
    activateSessionCleanup(*this, parameters);
#endif
}

NetworkSessionCocoa::~NetworkSessionCocoa() = default;

void NetworkSessionCocoa::initializeNSURLSessionsInSet(SessionSet& sessionSet, NSURLSessionConfiguration *configuration)
{
    sessionSet.sessionWithCredentialStorage.initialize(configuration, *this, WebCore::StoredCredentialsPolicy::Use, NavigatingToAppBoundDomain::No);
    auto cookieAcceptPolicy = configuration.HTTPCookieStorage.cookieAcceptPolicy;
    LOG(NetworkSession, "Created NetworkSession with cookieAcceptPolicy %lu", cookieAcceptPolicy);
    RELEASE_LOG_IF(cookieAcceptPolicy == NSHTTPCookieAcceptPolicyNever, NetworkSession, "Creating network session with ID %" PRIu64 " that will not accept cookies.", m_sessionID.toUInt64());
}

SessionSet& NetworkSessionCocoa::sessionSetForPage(WebPageProxyIdentifier webPageProxyID)
{
    SessionSet* sessionSet = webPageProxyID ? m_perPageSessionSets.get(webPageProxyID) : nullptr;
    return sessionSet ? *sessionSet : m_defaultSessionSet.get();
}

const SessionSet& NetworkSessionCocoa::sessionSetForPage(WebPageProxyIdentifier webPageProxyID) const
{
    SessionSet* sessionSet = webPageProxyID ? m_perPageSessionSets.get(webPageProxyID) : nullptr;
    return sessionSet ? *sessionSet : m_defaultSessionSet.get();
}

SessionWrapper& NetworkSessionCocoa::initializeEphemeralStatelessSessionIfNeeded(WebPageProxyIdentifier webPageProxyID, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    return sessionSetForPage(webPageProxyID).initializeEphemeralStatelessSessionIfNeeded(isNavigatingToAppBoundDomain, *this);
}

SessionWrapper& SessionSet::initializeEphemeralStatelessSessionIfNeeded(NavigatingToAppBoundDomain isNavigatingToAppBoundDomain, NetworkSessionCocoa& session)
{
    if (ephemeralStatelessSession.session)
        return ephemeralStatelessSession;

    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    NSURLSessionConfiguration *existingConfiguration = sessionWithCredentialStorage.session.get().configuration;

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

    ephemeralStatelessSession.initialize(configuration, session, WebCore::StoredCredentialsPolicy::EphemeralStateless, isNavigatingToAppBoundDomain);

    return ephemeralStatelessSession;
}

SessionWrapper& NetworkSessionCocoa::sessionWrapperForTask(WebPageProxyIdentifier webPageProxyID, const WebCore::ResourceRequest& request, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    auto shouldBeConsideredAppBound = isNavigatingToAppBoundDomain ? *isNavigatingToAppBoundDomain : NavigatingToAppBoundDomain::Yes;
    if (isParentProcessAFullWebBrowser(networkProcess()))
        shouldBeConsideredAppBound = NavigatingToAppBoundDomain::No;

#if ENABLE(TRACKING_PREVENTION)
    if (auto* storageSession = networkStorageSession()) {
        auto firstParty = WebCore::RegistrableDomain(request.firstPartyForCookies());
        if (storageSession->shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(firstParty))
            return sessionSetForPage(webPageProxyID).isolatedSession(storedCredentialsPolicy, firstParty, shouldBeConsideredAppBound, *this);
    } else
        ASSERT_NOT_REACHED();
#else
    UNUSED_PARAM(request);
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    if (shouldBeConsideredAppBound == NavigatingToAppBoundDomain::Yes)
        return appBoundSession(webPageProxyID, storedCredentialsPolicy);
#endif

    switch (storedCredentialsPolicy) {
    case WebCore::StoredCredentialsPolicy::Use:
    case WebCore::StoredCredentialsPolicy::DoNotUse:
        return sessionSetForPage(webPageProxyID).sessionWithCredentialStorage;
    case WebCore::StoredCredentialsPolicy::EphemeralStateless:
        return initializeEphemeralStatelessSessionIfNeeded(webPageProxyID, NavigatingToAppBoundDomain::No);
    }
}

#if ENABLE(APP_BOUND_DOMAINS)
SessionWrapper& NetworkSessionCocoa::appBoundSession(WebPageProxyIdentifier webPageProxyID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
    auto& sessionSet = sessionSetForPage(webPageProxyID);
    
    if (!sessionSet.appBoundSession) {
        sessionSet.appBoundSession = makeUnique<IsolatedSession>();
        sessionSet.appBoundSession->sessionWithCredentialStorage.initialize(sessionSet.sessionWithCredentialStorage.session.get().configuration, *this, WebCore::StoredCredentialsPolicy::Use, NavigatingToAppBoundDomain::Yes);
    }

    auto& sessionWrapper = [&] (auto storedCredentialsPolicy) -> SessionWrapper& {
        switch (storedCredentialsPolicy) {
        case WebCore::StoredCredentialsPolicy::Use:
        case WebCore::StoredCredentialsPolicy::DoNotUse:
            LOG(NetworkSession, "Using app-bound NSURLSession.");
            return sessionSet.appBoundSession->sessionWithCredentialStorage;
        case WebCore::StoredCredentialsPolicy::EphemeralStateless:
            return initializeEphemeralStatelessSessionIfNeeded(webPageProxyID, NavigatingToAppBoundDomain::Yes);
        }
    } (storedCredentialsPolicy);

    return sessionWrapper;
}

bool NetworkSessionCocoa::hasAppBoundSession() const
{
    if (!!m_defaultSessionSet->appBoundSession)
        return true;
    for (auto& sessionSet : m_perPageSessionSets.values()) {
        if (!!sessionSet->appBoundSession)
            return true;
    }
    
    return false;
}

void NetworkSessionCocoa::clearAppBoundSession()
{
    m_defaultSessionSet->appBoundSession = nullptr;
    for (auto& sessionSet : m_perPageSessionSets.values())
        sessionSet->appBoundSession = nullptr;
}
#endif

SessionWrapper& NetworkSessionCocoa::isolatedSession(WebPageProxyIdentifier webPageProxyID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, const WebCore::RegistrableDomain& firstPartyDomain, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain)
{
    return sessionSetForPage(webPageProxyID).isolatedSession(storedCredentialsPolicy, firstPartyDomain, isNavigatingToAppBoundDomain, *this);
}

SessionWrapper& SessionSet::isolatedSession(WebCore::StoredCredentialsPolicy storedCredentialsPolicy, const WebCore::RegistrableDomain& firstPartyDomain, NavigatingToAppBoundDomain isNavigatingToAppBoundDomain, NetworkSessionCocoa& session)
{
    auto& entry = isolatedSessions.ensure(firstPartyDomain, [this, &session, isNavigatingToAppBoundDomain] {
        auto newEntry = makeUnique<IsolatedSession>();
        newEntry->sessionWithCredentialStorage.initialize(sessionWithCredentialStorage.session.get().configuration, session, WebCore::StoredCredentialsPolicy::Use, isNavigatingToAppBoundDomain);
        return newEntry;
    }).iterator->value;

    entry->lastUsed = WallTime::now();

    auto& sessionWrapper = [&] (auto storedCredentialsPolicy) -> SessionWrapper& {
        switch (storedCredentialsPolicy) {
        case WebCore::StoredCredentialsPolicy::Use:
        case WebCore::StoredCredentialsPolicy::DoNotUse:
            LOG(NetworkSession, "Using isolated NSURLSession.");
            return entry->sessionWithCredentialStorage;
        case WebCore::StoredCredentialsPolicy::EphemeralStateless:
            return initializeEphemeralStatelessSessionIfNeeded(isNavigatingToAppBoundDomain, session);
        }
    } (storedCredentialsPolicy);

    if (isolatedSessions.size() > maxNumberOfIsolatedSessions) {
        WebCore::RegistrableDomain keyToRemove;
        auto oldestTimestamp = WallTime::now();
        for (auto& key : isolatedSessions.keys()) {
            auto timestamp = isolatedSessions.get(key)->lastUsed;
            if (timestamp < oldestTimestamp) {
                oldestTimestamp = timestamp;
                keyToRemove = key;
            }
        }
        LOG(NetworkSession, "About to remove isolated NSURLSession.");
        isolatedSessions.remove(keyToRemove);
    }

    RELEASE_ASSERT(isolatedSessions.size() <= maxNumberOfIsolatedSessions);

    return sessionWrapper;
}

bool NetworkSessionCocoa::hasIsolatedSession(const WebCore::RegistrableDomain& domain) const
{
    if (m_defaultSessionSet->isolatedSessions.contains(domain))
        return true;
    for (auto& sessionSet : m_perPageSessionSets.values()) {
        if (sessionSet->isolatedSessions.contains(domain))
            return true;
    }
    
    return false;
}

void NetworkSessionCocoa::clearIsolatedSessions()
{
    m_defaultSessionSet->isolatedSessions.clear();
    for (auto& sessionSet : m_perPageSessionSets.values())
        sessionSet->isolatedSessions.clear();
}

void NetworkSessionCocoa::invalidateAndCancelSessionSet(SessionSet& sessionSet)
{
    [sessionSet.sessionWithCredentialStorage.session invalidateAndCancel];
    [sessionSet.ephemeralStatelessSession.session invalidateAndCancel];
    [sessionSet.sessionWithCredentialStorage.delegate sessionInvalidated];
    [sessionSet.ephemeralStatelessSession.delegate sessionInvalidated];

    for (auto& session : sessionSet.isolatedSessions.values()) {
        [session->sessionWithCredentialStorage.session invalidateAndCancel];
        [session->sessionWithCredentialStorage.delegate sessionInvalidated];
    }
    sessionSet.isolatedSessions.clear();

    if (sessionSet.appBoundSession) {
        [sessionSet.appBoundSession->sessionWithCredentialStorage.session invalidateAndCancel];
        [sessionSet.appBoundSession->sessionWithCredentialStorage.delegate sessionInvalidated];
    }
}

void NetworkSessionCocoa::invalidateAndCancel()
{
    NetworkSession::invalidateAndCancel();

    invalidateAndCancelSessionSet(m_defaultSessionSet.get());
    for (auto& sessionSet : m_perPageSessionSets.values())
        invalidateAndCancelSessionSet(sessionSet.get());
}

void NetworkSessionCocoa::clearCredentials()
{
#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    ASSERT(m_dataTaskMapWithCredentials.isEmpty());
    ASSERT(m_dataTaskMapWithoutState.isEmpty());
    ASSERT(m_downloadMap.isEmpty());
    // FIXME: Use resetWithCompletionHandler instead.
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:m_sessionWithCredentialStorage.get().configuration delegate:static_cast<id>(m_sessionWithCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    m_statelessSession = createURLSession([m_statelessSession configuration], static_cast<id>(m_statelessSessionDelegate.get()));
    for (auto& entry : m_isolatedSessions.values())
        entry.session = createURLSession([entry.session configuration], static_cast<id>(entry.delegate.get()));
    m_appBoundSession.session = createURLSession([m_appBoundSession.session configuration], m_appBoundSession.delegate.get());
#endif
}

bool NetworkSessionCocoa::allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge& challenge)
{
    const String& host = challenge.protectionSpace().host();
    NSArray *certificates = [NSURLRequest allowsSpecificHTTPSCertificateForHost:host];
    if (!certificates)
        return false;

    bool requireServerCertificates = challenge.protectionSpace().authenticationScheme() == WebCore::ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested;
    RetainPtr<SecPolicyRef> policy = adoptCF(SecPolicyCreateSSL(requireServerCertificates, host.createCFString().get()));

    SecTrustRef trustRef = nullptr;
    if (SecTrustCreateWithCertificates((CFArrayRef)certificates, policy.get(), &trustRef) != noErr)
        return false;
    RetainPtr<SecTrustRef> trust = adoptCF(trustRef);

    return WebCore::certificatesMatch(trust.get(), challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust);
}

static CompletionHandler<void(WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)> createChallengeCompletionHandler(Ref<NetworkProcess>&& networkProcess, PAL::SessionID sessionID,  const WebCore::AuthenticationChallenge& challenge, const String& partition, uint64_t taskIdentifier, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&& completionHandler)
 {
    WebCore::AuthenticationChallenge authenticationChallenge { challenge };
    return [completionHandler = WTFMove(completionHandler), networkProcess = WTFMove(networkProcess), sessionID, authenticationChallenge, taskIdentifier, partition](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
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
}

void NetworkSessionCocoa::continueDidReceiveChallenge(SessionWrapper& sessionWrapper, const WebCore::AuthenticationChallenge& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, NetworkDataTaskCocoa* networkDataTask, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&& completionHandler)
{
    if (!networkDataTask) {
#if HAVE(NSURLSESSION_WEBSOCKET)
        if (auto* webSocketTask = sessionWrapper.webSocketDataTaskMap.get(taskIdentifier)) {
            auto challengeCompletionHandler = createChallengeCompletionHandler(networkProcess(), sessionID(), challenge, webSocketTask->partition(), 0, WTFMove(completionHandler));
            networkProcess().authenticationManager().didReceiveAuthenticationChallenge(sessionID(), webSocketTask->pageID(), !webSocketTask->topOrigin().isNull() ? &webSocketTask->topOrigin() : nullptr, challenge, negotiatedLegacyTLS, WTFMove(challengeCompletionHandler));
            return;
        }
#endif
        auto downloadID = sessionWrapper.downloadMap.get(taskIdentifier);
        if (downloadID) {
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

    auto challengeCompletionHandler = createChallengeCompletionHandler(networkProcess(), sessionID(), challenge, networkDataTask->partition(), taskIdentifier, WTFMove(completionHandler));
    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes
        && fastServerTrustEvaluationEnabled()
        && !networkDataTask->isTopLevelNavigation())
        return challengeCompletionHandler(AuthenticationChallengeDisposition::Cancel, { });

    networkDataTask->didReceiveChallenge(WebCore::AuthenticationChallenge { challenge }, negotiatedLegacyTLS, WTFMove(challengeCompletionHandler));
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
std::unique_ptr<WebSocketTask> NetworkSessionCocoa::createWebSocketTask(WebPageProxyIdentifier webPageProxyID, NetworkSocketChannel& channel, const WebCore::ResourceRequest& request, const String& protocol, const WebCore::ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<WebCore::NetworkConnectionIntegrity> networkConnectionIntegrityPolicy)
{
    ASSERT(!request.hasHTTPHeaderField(WebCore::HTTPHeaderName::SecWebSocketProtocol));
    RetainPtr nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    RetainPtr<NSMutableURLRequest> mutableRequest;

    auto ensureMutableRequest = [&] {
        if (!mutableRequest) {
            mutableRequest = adoptNS([nsRequest mutableCopy]);
            nsRequest = mutableRequest.get();
        }
        return mutableRequest.get();
    };

    if (!protocol.isNull())
        [ensureMutableRequest() addValue:StringView(protocol).createNSString().get() forHTTPHeaderField:@"Sec-WebSocket-Protocol"];

    // rdar://problem/68057031: explicitly disable sniffing for WebSocket handshakes.
    [nsRequest _setProperty:@NO forKey:(NSString *)_kCFURLConnectionPropertyShouldSniff];

#if ENABLE(APP_PRIVACY_REPORT)
    if (!request.isAppInitiated())
        ensureMutableRequest().attribution = NSURLRequestAttributionUser;

    appPrivacyReportTestingData().didLoadAppInitiatedRequest(nsRequest.get().attribution == NSURLRequestAttributionDeveloper);
#endif

#if HAVE(PROHIBIT_PRIVACY_PROXY)
    if (!allowPrivacyProxy)
        ensureMutableRequest()._prohibitPrivacyProxy = YES;
#endif

    if (hadMainFrameMainResourcePrivateRelayed || request.url().host() == clientOrigin.topOrigin.host) {
        if ([NSMutableURLRequest instancesRespondToSelector:@selector(_setPrivacyProxyFailClosedForUnreachableNonMainHosts:)])
            ensureMutableRequest()._privacyProxyFailClosedForUnreachableNonMainHosts = YES;
    }

    if (networkConnectionIntegrityPolicy.contains(WebCore::NetworkConnectionIntegrity::Enabled))
        enableNetworkConnectionIntegrity(ensureMutableRequest(), needsAdditionalNetworkConnectionIntegritySettings(request));

    auto& sessionSet = sessionSetForPage(webPageProxyID);
    RetainPtr task = [sessionSet.sessionWithCredentialStorage.session webSocketTaskWithRequest:nsRequest.get()];
    
    // Although the WebSocket protocol allows full 64-bit lengths, Chrome and Firefox limit the length to 2^63 - 1
    task.get().maximumMessageSize = 0x7FFFFFFFFFFFFFFFull;

    return makeUnique<WebSocketTask>(channel, webPageProxyID, sessionSet, request, clientOrigin, WTFMove(task));
}

void NetworkSessionCocoa::addWebSocketTask(WebPageProxyIdentifier webPageProxyID, WebSocketTask& task)
{
    auto& webSocketDataTaskMap = sessionSetForPage(webPageProxyID).sessionWithCredentialStorage.webSocketDataTaskMap;
    auto addResult = webSocketDataTaskMap.add(task.identifier(), &task);
    RELEASE_ASSERT(addResult.isNewEntry);
    RELEASE_LOG(NetworkSession, "NetworkSessionCocoa::addWebSocketTask, web socket count is %u", webSocketDataTaskMap.size());
}

void NetworkSessionCocoa::removeWebSocketTask(SessionSet& sessionSet, WebSocketTask& task)
{
    auto& webSocketDataTaskMap = sessionSet.sessionWithCredentialStorage.webSocketDataTaskMap;
    bool contained = webSocketDataTaskMap.remove(task.identifier());
    RELEASE_ASSERT(contained);
    RELEASE_LOG(NetworkSession, "NetworkSessionCocoa::removeWebSocketTask, web socket count is %u", webSocketDataTaskMap.size());
}

#endif // HAVE(NSURLSESSION_WEBSOCKET)

void NetworkSessionCocoa::addWebPageNetworkParameters(WebPageProxyIdentifier pageID, WebPageNetworkParameters&& parameters)
{
    auto addResult1 = m_perParametersSessionSets.add(parameters, nullptr);
    if (auto set = addResult1.iterator->value) {
        m_perPageSessionSets.add(pageID, *set);
        return;
    }

    auto addResult2 = m_perPageSessionSets.add(pageID, SessionSet::create());
    ASSERT(addResult2.isNewEntry);
    RetainPtr<NSURLSessionConfiguration> configuration = adoptNS([m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration copy]);
#if HAVE(CFNETWORK_NSURLSESSION_ATTRIBUTED_BUNDLE_IDENTIFIER) && USE(APPLE_INTERNAL_SDK)
    if ([configuration respondsToSelector:@selector(_attributedBundleIdentifier)])
        configuration.get()._attributedBundleIdentifier = parameters.attributedBundleIdentifier();
#endif
    initializeNSURLSessionsInSet(addResult2.iterator->value.get(), configuration.get());
    addResult1.iterator->value = addResult2.iterator->value.get();

    m_attributedBundleIdentifierFromPageIdentifiers.add(pageID, parameters.attributedBundleIdentifier());
}

void NetworkSessionCocoa::dataTaskWithRequest(WebPageProxyIdentifier pageID, WebCore::ResourceRequest&& request, CompletionHandler<void(DataTaskIdentifier)>&& completionHandler)
{
    auto identifier = DataTaskIdentifier::generate();
    auto nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    auto session = sessionWrapperForTask(pageID, request, WebCore::StoredCredentialsPolicy::Use, std::nullopt).session;
    auto task = [session dataTaskWithRequest:nsRequest];
    auto delegate = adoptNS([[WKURLSessionTaskDelegate alloc] initWithIdentifier:identifier session:*this]);
#if HAVE(NSURLSESSION_TASK_DELEGATE)
    task.delegate = delegate.get();
#endif
    auto addResult = m_dataTasksForAPI.add(identifier, task);
    RELEASE_ASSERT(addResult.isNewEntry);
    [task resume];
    completionHandler(identifier);
}

void NetworkSessionCocoa::cancelDataTask(DataTaskIdentifier identifier)
{
    [m_dataTasksForAPI.take(identifier) cancel];
}

void NetworkSessionCocoa::removeDataTask(DataTaskIdentifier identifier)
{
    m_dataTasksForAPI.remove(identifier);
}

void NetworkSessionCocoa::removeWebPageNetworkParameters(WebPageProxyIdentifier pageID)
{
    m_perPageSessionSets.remove(pageID);
    m_attributedBundleIdentifierFromPageIdentifiers.remove(pageID);
}

size_t NetworkSessionCocoa::countNonDefaultSessionSets() const
{
    HashSet<Ref<SessionSet>> uniqueSets;
    for (auto& set : m_perPageSessionSets.values())
        uniqueSets.add(set);
    return uniqueSets.size();
}

Vector<WebCore::SecurityOriginData> NetworkSessionCocoa::hostNamesWithAlternativeServices() const
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    Vector<WebCore::SecurityOriginData> origins;
    _NSHTTPAlternativeServicesStorage* storage = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
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

void NetworkSessionCocoa::donateToSKAdNetwork(WebCore::PrivateClickMeasurement&& pcm)
{
#if HAVE(SKADNETWORK_v4)
    auto config = adoptNS([ASDInstallWebAttributionParamsConfig new]);
    config.get().appAdamId = @(*pcm.adamID());
    config.get().adNetworkRegistrableDomain = pcm.destinationSite().registrableDomain.string();
    config.get().impressionId = pcm.ephemeralSourceNonce()->nonce;
    config.get().sourceWebRegistrableDomain = pcm.sourceSite().registrableDomain.string();
    config.get().version = @"4.0";
    config.get().attributionContext = AttributionTypeDefault;
    [[ASDInstallAttribution sharedInstance] addInstallWebAttributionParamsWithConfig:config.get() completionHandler:^(NSError *) { }];
#endif

    if (!m_privateClickMeasurementDebugModeEnabled)
        return;

    StringBuilder debugString;
    debugString.append("Submitting potential install attribution for AdamId: ");
    debugString.append(makeString(*pcm.adamID()));
    debugString.append(", adNetworkRegistrableDomain: ");
    debugString.append(pcm.destinationSite().registrableDomain.string());
    debugString.append(", impressionId: ");
    debugString.append(pcm.ephemeralSourceNonce()->nonce);
    debugString.append(", sourceWebRegistrableDomain: ");
    debugString.append(pcm.sourceSite().registrableDomain.string());
    debugString.append(", version: 3");
    networkProcess().broadcastConsoleMessage(sessionID(), MessageSource::PrivateClickMeasurement, MessageLevel::Debug, debugString.toString());
}

void NetworkSessionCocoa::deleteAlternativeServicesForHostNames(const Vector<String>& hosts)
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    _NSHTTPAlternativeServicesStorage* storage = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    for (auto& host : hosts)
        [storage removeHTTPAlternativeServiceEntriesWithRegistrableDomain:host];
#else
    UNUSED_PARAM(hosts);
#endif
}

void NetworkSessionCocoa::clearAlternativeServices(WallTime modifiedSince)
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    _NSHTTPAlternativeServicesStorage* storage = m_defaultSessionSet->sessionWithCredentialStorage.session.get().configuration._alternativeServicesStorage;
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    [storage removeHTTPAlternativeServiceEntriesCreatedAfterDate:date];
#else
    UNUSED_PARAM(modifiedSince);
#endif
}

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(APP_PRIVACY_REPORT) && HAVE(SYMPTOMS_FRAMEWORK)
static bool isActingOnBehalfOfAFullWebBrowser(const String& bundleID)
{
    return bundleID == "com.apple.webbookmarksd"_s;
}
#endif

void NetworkSessionCocoa::removeNetworkWebsiteData(std::optional<WallTime> modifiedSince, std::optional<HashSet<WebCore::RegistrableDomain>>&& domains, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(APP_PRIVACY_REPORT) && HAVE(SYMPTOMS_FRAMEWORK)
    // FIXME: Add automated test for this once rdar://81420753 is resolved.
    if (sessionID().isEphemeral()) {
        completionHandler();
        return;
    }

    auto bundleID = WebCore::applicationBundleIdentifier();
    if (!isParentProcessAFullWebBrowser(networkProcess()) && !isActingOnBehalfOfAFullWebBrowser(bundleID))
        return completionHandler();

    if (!SymptomAnalyticsLibrary()
        || !SymptomPresentationFeedLibrary()
        || !SymptomPresentationLiteLibrary()
        || !getAnalyticsWorkspaceClass()
        || !getUsageFeedClass()
        || !canLoadkSymptomAnalyticsServiceEndpoint()) {
        completionHandler();
        return;
    }

    RetainPtr<AnalyticsWorkspace> workspace = adoptNS([allocAnalyticsWorkspaceInstance() initWorkspaceWithService:getkSymptomAnalyticsServiceEndpoint()]);
    RetainPtr<UsageFeed> usageFeed = adoptNS([allocUsageFeedInstance() initWithWorkspace:workspace.get()]);

    if (![usageFeed.get() respondsToSelector:@selector(performNetworkDomainsActionWithOptions:reply:)]
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryKey()
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryBundleIDs()
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryStartDate()
        || !canLoadkSymptomAnalyticsServiceDomainTrackingClearHistoryEndDate()) {
        completionHandler();
        return;
    }

    auto *startDate = [NSDate distantPast];
    if (modifiedSince) {
        NSTimeInterval timeInterval = modifiedSince.value().secondsSinceEpoch().seconds();
        startDate = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    }

    auto contextArray = adoptNS([[NSMutableArray alloc] init]);
    if (domains) {
        contextArray = createNSArray(*domains, [&] (auto domain) {
            return [NSString stringWithUTF8String:domain.string().utf8().data()];
        });
    }

    if (isActingOnBehalfOfAFullWebBrowser(bundleID))
        bundleID = "com.apple.mobilesafari"_s;

    NSDictionary *options = @{
        (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryKey(): @{
            (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryBundleIDs(): @{
                bundleID : contextArray.get(),
            },
            (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryStartDate(): startDate,
            (id)getkSymptomAnalyticsServiceDomainTrackingClearHistoryEndDate(): [NSDate distantFuture]
        }
    };

    bool result = [usageFeed performNetworkDomainsActionWithOptions:options reply:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSDictionary *reply, NSError *error) mutable {
        if (error)
            LOG(NetworkSession, "Error deleting network domain data %" PUBLIC_LOG_STRING, error);

        completionHandler();
    }).get()];

    if (!result)
        LOG(NetworkSession, "Error deleting network domain data: invalid parameter or failure to contact the service");
#else
    UNUSED_PARAM(modifiedSince);
    completionHandler();
#endif
}

#endif // USE(APPLE_INTERNAL_SDK)

} // namespace WebKit
