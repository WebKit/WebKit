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

#include "FetchOptions.h"
#include "HTTPHeaderNames.h"
#include "ServiceWorkerTypes.h"
#include "StoredCredentialsPolicy.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum SendCallbackPolicy {
    SendCallbacks,
    DoNotSendCallbacks
};

enum ContentSniffingPolicy {
    SniffContent,
    DoNotSniffContent
};

enum DataBufferingPolicy {
    BufferData,
    DoNotBufferData
};

enum SecurityCheckPolicy {
    SkipSecurityCheck,
    DoSecurityCheck
};

enum CertificateInfoPolicy {
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

enum class ClientCredentialPolicy {
    CannotAskClientForCredentials,
    MayAskClientForCredentials
};

enum class SameOriginDataURLFlag {
    Set,
    Unset
};

enum class InitiatorContext {
    Document,
    Worker,
};

enum class ServiceWorkersMode {
    All,
    None,
    Only // An error will happen if service worker is not handling the fetch. Used to bypass preflight safely.
};

enum class ContentEncodingSniffingPolicy {
    Sniff,
    DoNotSniff,
};

struct ResourceLoaderOptions : public FetchOptions {
    ResourceLoaderOptions() { }

    ResourceLoaderOptions(const FetchOptions& options) : FetchOptions(options) { }

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

    SendCallbackPolicy sendLoadCallbacks { DoNotSendCallbacks };
    ContentSniffingPolicy sniffContent { DoNotSniffContent };
    ContentEncodingSniffingPolicy sniffContentEncoding { ContentEncodingSniffingPolicy::Sniff };
    DataBufferingPolicy dataBufferingPolicy { BufferData };
    StoredCredentialsPolicy storedCredentialsPolicy { StoredCredentialsPolicy::DoNotUse };
    SecurityCheckPolicy securityCheck { DoSecurityCheck };
    CertificateInfoPolicy certificateInfoPolicy { DoNotIncludeCertificateInfo };
    ContentSecurityPolicyImposition contentSecurityPolicyImposition { ContentSecurityPolicyImposition::DoPolicyCheck };
    DefersLoadingPolicy defersLoadingPolicy { DefersLoadingPolicy::AllowDefersLoading };
    CachingPolicy cachingPolicy { CachingPolicy::AllowCaching };
    SameOriginDataURLFlag sameOriginDataURLFlag { SameOriginDataURLFlag::Unset };
    InitiatorContext initiatorContext { InitiatorContext::Document };
    ServiceWorkersMode serviceWorkersMode { ServiceWorkersMode::All };
#if ENABLE(SERVICE_WORKER)
    std::optional<ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier;
    // WebKit loading code is adding some HTTP headers between the application and the time service worker intercepts the fetch.
    // We keep a list of these headers so that we only remove the ones that are set by the loading code and not by the application.
    // FIXME: Remove this when service worker fetch interception happens before the setting of these headers in the loading code.
    HashSet<HTTPHeaderName, WTF::IntHash<HTTPHeaderName>, WTF::StrongEnumHashTraits<HTTPHeaderName>> httpHeadersToKeep;
#endif

    ClientCredentialPolicy clientCredentialPolicy { ClientCredentialPolicy::CannotAskClientForCredentials };
    unsigned maxRedirectCount { 20 };

    Vector<String> derivedCachedDataTypesToRetrieve;
};

} // namespace WebCore
