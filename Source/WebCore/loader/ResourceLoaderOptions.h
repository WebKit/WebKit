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
#include "FetchOptions.h"
#include "HTTPHeaderNames.h"
#include "ServiceWorkerTypes.h"
#include "StoredCredentialsPolicy.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class SendCallbackPolicy : uint8_t {
    SendCallbacks,
    DoNotSendCallbacks
};

// FIXME: These options are named poorly. We only implement force disabling content sniffing, not enabling it,
// and even that only on some platforms.
enum class ContentSniffingPolicy : uint8_t {
    SniffContent,
    DoNotSniffContent
};

enum class DataBufferingPolicy : uint8_t {
    BufferData,
    DoNotBufferData
};

enum class SecurityCheckPolicy : uint8_t {
    SkipSecurityCheck,
    DoSecurityCheck
};

enum class CertificateInfoPolicy : uint8_t {
    IncludeCertificateInfo,
    DoNotIncludeCertificateInfo
};

enum class ContentSecurityPolicyImposition : uint8_t {
    SkipPolicyCheck,
    DoPolicyCheck
};

enum class DefersLoadingPolicy : uint8_t {
    AllowDefersLoading,
    DisallowDefersLoading
};

enum class CachingPolicy : uint8_t {
    AllowCaching,
    DisallowCaching
};

enum class ClientCredentialPolicy : uint8_t {
    CannotAskClientForCredentials,
    MayAskClientForCredentials
};

enum class SameOriginDataURLFlag : uint8_t {
    Set,
    Unset
};

enum class InitiatorContext : uint8_t {
    Document,
    Worker,
};

enum class ServiceWorkersMode : uint8_t {
    All,
    None,
    Only // An error will happen if service worker is not handling the fetch. Used to bypass preflight safely.
};

enum class ApplicationCacheMode : uint8_t {
    Use,
    Bypass
};

// FIXME: These options are named poorly. We only implement force disabling content encoding sniffing, not enabling it,
// and even that only on some platforms.
enum class ContentEncodingSniffingPolicy : uint8_t {
    Sniff,
    DoNotSniff,
};

enum class PreflightPolicy : uint8_t {
    Consider,
    Force,
    Prevent
};

enum class LoadedFromOpaqueSource : uint8_t {
    Yes,
    No
};

struct ResourceLoaderOptions : public FetchOptions {
    ResourceLoaderOptions() { }

    ResourceLoaderOptions(FetchOptions options) : FetchOptions { WTFMove(options) } { }

    ResourceLoaderOptions(SendCallbackPolicy sendLoadCallbacks, ContentSniffingPolicy sniffContent, DataBufferingPolicy dataBufferingPolicy, StoredCredentialsPolicy storedCredentialsPolicy, ClientCredentialPolicy credentialPolicy, FetchOptions::Credentials credentials, SecurityCheckPolicy securityCheck, FetchOptions::Mode mode, CertificateInfoPolicy certificateInfoPolicy, ContentSecurityPolicyImposition contentSecurityPolicyImposition, DefersLoadingPolicy defersLoadingPolicy, CachingPolicy cachingPolicy)
        : sendLoadCallbacks(sendLoadCallbacks)
        , sniffContent(sniffContent)
        , dataBufferingPolicy(dataBufferingPolicy)
        , storedCredentialsPolicy(storedCredentialsPolicy)
        , securityCheck(securityCheck)
        , certificateInfoPolicy(certificateInfoPolicy)
        , contentSecurityPolicyImposition(contentSecurityPolicyImposition)
        , defersLoadingPolicy(defersLoadingPolicy)
        , cachingPolicy(cachingPolicy)
        , clientCredentialPolicy(credentialPolicy)
    {
        this->credentials = credentials;
        this->mode = mode;
    }

#if ENABLE(SERVICE_WORKER)
    std::optional<ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier;
#endif
    HashSet<HTTPHeaderName, WTF::IntHash<HTTPHeaderName>, WTF::StrongEnumHashTraits<HTTPHeaderName>> httpHeadersToKeep;
    Vector<String> derivedCachedDataTypesToRetrieve;
    std::optional<ContentSecurityPolicyResponseHeaders> cspResponseHeaders;
    unsigned maxRedirectCount { 20 };

    SendCallbackPolicy sendLoadCallbacks { SendCallbackPolicy::DoNotSendCallbacks };
    ContentSniffingPolicy sniffContent { ContentSniffingPolicy::DoNotSniffContent };
    ContentEncodingSniffingPolicy sniffContentEncoding { ContentEncodingSniffingPolicy::Sniff };
    DataBufferingPolicy dataBufferingPolicy { DataBufferingPolicy::BufferData };
    StoredCredentialsPolicy storedCredentialsPolicy { StoredCredentialsPolicy::DoNotUse };
    SecurityCheckPolicy securityCheck { SecurityCheckPolicy::DoSecurityCheck };
    CertificateInfoPolicy certificateInfoPolicy { CertificateInfoPolicy::DoNotIncludeCertificateInfo };
    ContentSecurityPolicyImposition contentSecurityPolicyImposition { ContentSecurityPolicyImposition::DoPolicyCheck };
    DefersLoadingPolicy defersLoadingPolicy { DefersLoadingPolicy::AllowDefersLoading };
    CachingPolicy cachingPolicy { CachingPolicy::AllowCaching };
    SameOriginDataURLFlag sameOriginDataURLFlag { SameOriginDataURLFlag::Unset };
    InitiatorContext initiatorContext { InitiatorContext::Document };
    ServiceWorkersMode serviceWorkersMode { ServiceWorkersMode::All };
    ApplicationCacheMode applicationCacheMode { ApplicationCacheMode::Use };
    ClientCredentialPolicy clientCredentialPolicy { ClientCredentialPolicy::CannotAskClientForCredentials };
    PreflightPolicy preflightPolicy { PreflightPolicy::Consider };
    LoadedFromOpaqueSource loadedFromOpaqueSource { LoadedFromOpaqueSource::No };
};

} // namespace WebCore
