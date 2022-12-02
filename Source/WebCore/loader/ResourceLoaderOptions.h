/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginAccessControl.h"
#include "CrossOriginEmbedderPolicy.h"
#include "FetchIdentifier.h"
#include "FetchOptions.h"
#include "HTTPHeaderNames.h"
#include "ServiceWorkerTypes.h"
#include "StoredCredentialsPolicy.h"
#include <wtf/EnumTraits.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class SendCallbackPolicy : uint8_t {
    SendCallbacks,
    DoNotSendCallbacks
};
static constexpr unsigned bitWidthOfSendCallbackPolicy = 1;

// FIXME: These options are named poorly. We only implement force disabling content sniffing, not enabling it,
// and even that only on some platforms.
enum class ContentSniffingPolicy : bool {
    SniffContent,
    DoNotSniffContent
};
static constexpr unsigned bitWidthOfContentSniffingPolicy = 1;

enum class DataBufferingPolicy : uint8_t {
    BufferData,
    DoNotBufferData
};
static constexpr unsigned bitWidthOfDataBufferingPolicy = 1;

enum class SecurityCheckPolicy : uint8_t {
    SkipSecurityCheck,
    DoSecurityCheck
};
static constexpr unsigned bitWidthOfSecurityCheckPolicy = 1;

enum class CertificateInfoPolicy : uint8_t {
    IncludeCertificateInfo,
    DoNotIncludeCertificateInfo
};
static constexpr unsigned bitWidthOfCertificateInfoPolicy = 1;

enum class ContentSecurityPolicyImposition : uint8_t {
    SkipPolicyCheck,
    DoPolicyCheck
};
static constexpr unsigned bitWidthOfContentSecurityPolicyImposition = 1;

enum class DefersLoadingPolicy : uint8_t {
    AllowDefersLoading,
    DisallowDefersLoading
};
static constexpr unsigned bitWidthOfDefersLoadingPolicy = 1;

enum class CachingPolicy : uint8_t {
    AllowCaching,
    DisallowCaching
};
static constexpr unsigned bitWidthOfCachingPolicy = 1;

enum class ClientCredentialPolicy : bool {
    CannotAskClientForCredentials,
    MayAskClientForCredentials
};
static constexpr unsigned bitWidthOfClientCredentialPolicy = 1;

enum class SameOriginDataURLFlag : uint8_t {
    Set,
    Unset
};
static constexpr unsigned bitWidthOfSameOriginDataURLFlag = 1;

enum class InitiatorContext : uint8_t {
    Document,
    Worker,
};
static constexpr unsigned bitWidthOfInitiatorContext = 1;

enum class ServiceWorkersMode : uint8_t {
    All,
    None,
    Only // An error will happen if service worker is not handling the fetch. Used to bypass preflight safely.
};
static constexpr unsigned bitWidthOfServiceWorkersMode = 2;

enum class ApplicationCacheMode : uint8_t {
    Use,
    Bypass
};
static constexpr unsigned bitWidthOfApplicationCacheMode = 1;

enum class ContentEncodingSniffingPolicy : bool {
    Default,
    Disable
};
static constexpr unsigned bitWidthOfContentEncodingSniffingPolicy = 1;

enum class PreflightPolicy : uint8_t {
    Consider,
    Force,
    Prevent
};
static constexpr unsigned bitWidthOfPreflightPolicy = 2;

enum class LoadedFromOpaqueSource : uint8_t {
    Yes,
    No
};
static constexpr unsigned bitWidthOfLoadedFromOpaqueSource = 1;

enum class LoadedFromPluginElement : bool {
    No,
    Yes
};
static constexpr unsigned bitWidthOfLoadedFromPluginElement = 1;

struct ResourceLoaderOptions : public FetchOptions {
    ResourceLoaderOptions()
        : ResourceLoaderOptions(FetchOptions())
    {
    }

    ResourceLoaderOptions(FetchOptions options)
        : FetchOptions { WTFMove(options) }
        , sendLoadCallbacks(SendCallbackPolicy::DoNotSendCallbacks)
        , sniffContent(ContentSniffingPolicy::DoNotSniffContent)
        , contentEncodingSniffingPolicy(ContentEncodingSniffingPolicy::Default)
        , dataBufferingPolicy(DataBufferingPolicy::BufferData)
        , storedCredentialsPolicy(StoredCredentialsPolicy::DoNotUse)
        , securityCheck(SecurityCheckPolicy::DoSecurityCheck)
        , certificateInfoPolicy(CertificateInfoPolicy::DoNotIncludeCertificateInfo)
        , contentSecurityPolicyImposition(ContentSecurityPolicyImposition::DoPolicyCheck)
        , defersLoadingPolicy(DefersLoadingPolicy::AllowDefersLoading)
        , cachingPolicy(CachingPolicy::AllowCaching)
        , sameOriginDataURLFlag(SameOriginDataURLFlag::Unset)
        , initiatorContext(InitiatorContext::Document)
        , serviceWorkersMode(ServiceWorkersMode::All)
        , applicationCacheMode(ApplicationCacheMode::Use)
        , clientCredentialPolicy(ClientCredentialPolicy::CannotAskClientForCredentials)
        , preflightPolicy(PreflightPolicy::Consider)
        , loadedFromOpaqueSource(LoadedFromOpaqueSource::No)
        , loadedFromPluginElement(LoadedFromPluginElement::No)
    { }

    ResourceLoaderOptions(SendCallbackPolicy sendLoadCallbacks, ContentSniffingPolicy sniffContent, DataBufferingPolicy dataBufferingPolicy, StoredCredentialsPolicy storedCredentialsPolicy, ClientCredentialPolicy credentialPolicy, FetchOptions::Credentials credentials, SecurityCheckPolicy securityCheck, FetchOptions::Mode mode, CertificateInfoPolicy certificateInfoPolicy, ContentSecurityPolicyImposition contentSecurityPolicyImposition, DefersLoadingPolicy defersLoadingPolicy, CachingPolicy cachingPolicy)
        : sendLoadCallbacks(sendLoadCallbacks)
        , sniffContent(sniffContent)
        , contentEncodingSniffingPolicy(ContentEncodingSniffingPolicy::Default)
        , dataBufferingPolicy(dataBufferingPolicy)
        , storedCredentialsPolicy(storedCredentialsPolicy)
        , securityCheck(securityCheck)
        , certificateInfoPolicy(certificateInfoPolicy)
        , contentSecurityPolicyImposition(contentSecurityPolicyImposition)
        , defersLoadingPolicy(defersLoadingPolicy)
        , cachingPolicy(cachingPolicy)
        , sameOriginDataURLFlag(SameOriginDataURLFlag::Unset)
        , initiatorContext(InitiatorContext::Document)
        , serviceWorkersMode(ServiceWorkersMode::All)
        , applicationCacheMode(ApplicationCacheMode::Use)
        , clientCredentialPolicy(credentialPolicy)
        , preflightPolicy(PreflightPolicy::Consider)
        , loadedFromOpaqueSource(LoadedFromOpaqueSource::No)
        , loadedFromPluginElement(LoadedFromPluginElement::No)

    {
        this->credentials = credentials;
        this->mode = mode;
    }

#if ENABLE(SERVICE_WORKER)
    Markable<ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationIdentifier::MarkableTraits> serviceWorkerRegistrationIdentifier;
#endif
    Markable<ContentSecurityPolicyResponseHeaders, ContentSecurityPolicyResponseHeaders::MarkableTraits> cspResponseHeaders;
    std::optional<CrossOriginEmbedderPolicy> crossOriginEmbedderPolicy;
    OptionSet<HTTPHeadersToKeepFromCleaning> httpHeadersToKeep;
    uint8_t maxRedirectCount { 20 };
    FetchIdentifier navigationPreloadIdentifier;
    String nonce;

    SendCallbackPolicy sendLoadCallbacks : bitWidthOfSendCallbackPolicy;
    ContentSniffingPolicy sniffContent : bitWidthOfContentSniffingPolicy;
    ContentEncodingSniffingPolicy contentEncodingSniffingPolicy : bitWidthOfContentEncodingSniffingPolicy;
    DataBufferingPolicy dataBufferingPolicy : bitWidthOfDataBufferingPolicy;
    StoredCredentialsPolicy storedCredentialsPolicy : bitWidthOfStoredCredentialsPolicy;
    SecurityCheckPolicy securityCheck : bitWidthOfSecurityCheckPolicy;
    CertificateInfoPolicy certificateInfoPolicy : bitWidthOfCertificateInfoPolicy;
    ContentSecurityPolicyImposition contentSecurityPolicyImposition : bitWidthOfContentSecurityPolicyImposition;
    DefersLoadingPolicy defersLoadingPolicy : bitWidthOfDefersLoadingPolicy;
    CachingPolicy cachingPolicy : bitWidthOfCachingPolicy;
    SameOriginDataURLFlag sameOriginDataURLFlag : bitWidthOfSameOriginDataURLFlag;
    InitiatorContext initiatorContext : bitWidthOfInitiatorContext;
    ServiceWorkersMode serviceWorkersMode : bitWidthOfServiceWorkersMode;
    ApplicationCacheMode applicationCacheMode : bitWidthOfApplicationCacheMode;
    ClientCredentialPolicy clientCredentialPolicy : bitWidthOfClientCredentialPolicy;
    PreflightPolicy preflightPolicy : bitWidthOfPreflightPolicy;
    LoadedFromOpaqueSource loadedFromOpaqueSource : bitWidthOfLoadedFromOpaqueSource;
    LoadedFromPluginElement loadedFromPluginElement : bitWidthOfLoadedFromPluginElement;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PreflightPolicy> {
    using values = EnumValues<
        WebCore::PreflightPolicy,
        WebCore::PreflightPolicy::Consider,
        WebCore::PreflightPolicy::Force,
        WebCore::PreflightPolicy::Prevent
    >;
};

template<> struct EnumTraits<WebCore::ServiceWorkersMode> {
    using values = EnumValues<
        WebCore::ServiceWorkersMode,
        WebCore::ServiceWorkersMode::All,
        WebCore::ServiceWorkersMode::None,
        WebCore::ServiceWorkersMode::Only
    >;
};

} // namespace WTF
