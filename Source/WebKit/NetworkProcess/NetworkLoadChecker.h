/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "UserContentControllerIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/ContentExtensionActions.h>
#include <WebCore/ContentSecurityPolicyResponseHeaders.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/ResourceError.h>
#include <pal/SessionID.h>
#include <variant>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class ContentSecurityPolicy;
struct ContentRuleListResults;
struct ContentSecurityPolicyClient;
enum class NetworkConnectionIntegrity : uint8_t;
class SecurityOrigin;
enum class PreflightPolicy : uint8_t;
enum class StoredCredentialsPolicy : uint8_t;
}

namespace WebKit {

class NetworkCORSPreflightChecker;
class NetworkProcess;
class NetworkResourceLoader;
class NetworkSchemeRegistry;

using DocumentURL = URL;

class NetworkLoadChecker : public CanMakeWeakPtr<NetworkLoadChecker> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class LoadType : bool { MainFrame, Other };

    NetworkLoadChecker(NetworkProcess&, NetworkResourceLoader*, NetworkSchemeRegistry*, WebCore::FetchOptions&&, PAL::SessionID, WebPageProxyIdentifier, WebCore::HTTPHeaderMap&&, URL&&, DocumentURL&&,  RefPtr<WebCore::SecurityOrigin>&&, RefPtr<WebCore::SecurityOrigin>&& topOrigin, RefPtr<WebCore::SecurityOrigin>&& parentOrigin, WebCore::PreflightPolicy, String&& referrer, bool allowPrivacyProxy, OptionSet<WebCore::NetworkConnectionIntegrity> networkConnectionIntegrityPolicy, bool shouldCaptureExtraNetworkLoadMetrics = false, LoadType requestLoadType = LoadType::Other);
    ~NetworkLoadChecker();

    struct RedirectionTriplet {
        WebCore::ResourceRequest request;
        WebCore::ResourceRequest redirectRequest;
        WebCore::ResourceResponse redirectResponse;
    };

    using RequestOrRedirectionTripletOrError = std::variant<WebCore::ResourceRequest, RedirectionTriplet, WebCore::ResourceError>;
    using ValidationHandler = CompletionHandler<void(RequestOrRedirectionTripletOrError&&)>;
    void check(WebCore::ResourceRequest&&, WebCore::ContentSecurityPolicyClient*, ValidationHandler&&);

    using RedirectionRequestOrError = Expected<RedirectionTriplet, WebCore::ResourceError>;
    using RedirectionValidationHandler = CompletionHandler<void(RedirectionRequestOrError&&)>;
    void checkRedirection(WebCore::ResourceRequest&& request, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse, WebCore::ContentSecurityPolicyClient*, RedirectionValidationHandler&&);

    WebCore::ResourceError validateResponse(const WebCore::ResourceRequest&, WebCore::ResourceResponse&);

    void setCSPResponseHeaders(WebCore::ContentSecurityPolicyResponseHeaders&& headers) { m_cspResponseHeaders = WTFMove(headers); }
    void setParentCrossOriginEmbedderPolicy(const WebCore::CrossOriginEmbedderPolicy& parentCrossOriginEmbedderPolicy) { m_parentCrossOriginEmbedderPolicy = parentCrossOriginEmbedderPolicy; }
    void setCrossOriginEmbedderPolicy(const WebCore::CrossOriginEmbedderPolicy& crossOriginEmbedderPolicy) { m_crossOriginEmbedderPolicy = crossOriginEmbedderPolicy; }
#if ENABLE(CONTENT_EXTENSIONS)
    void setContentExtensionController(URL&& mainDocumentURL, URL&& frameURL, std::optional<UserContentControllerIdentifier> identifier)
    {
        m_mainDocumentURL = WTFMove(mainDocumentURL);
        m_frameURL = WTFMove(frameURL);
        m_userContentControllerIdentifier = identifier;
    }
#endif

    NetworkProcess& networkProcess() { return m_networkProcess; }

    const URL& url() const { return m_url; }
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy() const { return m_storedCredentialsPolicy; }

    WebCore::NetworkLoadInformation takeNetworkLoadInformation() { return WTFMove(m_loadInformation); }
    void storeRedirectionIfNeeded(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    void enableContentExtensionsCheck() { m_checkContentExtensions = true; }

private:
    WebCore::ContentSecurityPolicy* contentSecurityPolicy();
    bool isChecking() const { return !!m_corsPreflightChecker; }
    bool isRedirected() const { return m_redirectCount; }

    void checkRequest(WebCore::ResourceRequest&&, WebCore::ContentSecurityPolicyClient*, ValidationHandler&&);

    bool isAllowedByContentSecurityPolicy(const WebCore::ResourceRequest&, WebCore::ContentSecurityPolicyClient*);

    void continueCheckingRequest(WebCore::ResourceRequest&&, ValidationHandler&&);
    void continueCheckingRequestOrDoSyntheticRedirect(WebCore::ResourceRequest&& originalRequest, WebCore::ResourceRequest&& currentRequest, ValidationHandler&&);

    bool doesNotNeedCORSCheck(const URL&) const;
    void checkCORSRequest(WebCore::ResourceRequest&&, ValidationHandler&&);
    void checkCORSRedirectedRequest(WebCore::ResourceRequest&&, ValidationHandler&&);
    void checkCORSRequestWithPreflight(WebCore::ResourceRequest&&, ValidationHandler&&);

    RequestOrRedirectionTripletOrError accessControlErrorForValidationHandler(String&&);

#if ENABLE(CONTENT_EXTENSIONS)
    struct ContentExtensionResult {
        WebCore::ResourceRequest request;
        const WebCore::ContentRuleListResults& results;
    };
    using ContentExtensionResultOrError = Expected<ContentExtensionResult, WebCore::ResourceError>;
    using ContentExtensionCallback = CompletionHandler<void(ContentExtensionResultOrError&&)>;
    void processContentRuleListsForLoad(WebCore::ResourceRequest&&, ContentExtensionCallback&&);
#endif

    WebCore::FetchOptions m_options;
    WebCore::StoredCredentialsPolicy m_storedCredentialsPolicy;
    bool m_allowPrivacyProxy;
    OptionSet<WebCore::NetworkConnectionIntegrity> m_networkConnectionIntegrityPolicy;
    PAL::SessionID m_sessionID;
    Ref<NetworkProcess> m_networkProcess;
    WebPageProxyIdentifier m_webPageProxyID;
    WebCore::HTTPHeaderMap m_originalRequestHeaders; // Needed for CORS checks.
    WebCore::HTTPHeaderMap m_firstRequestHeaders; // Needed for CORS checks.
    URL m_url;
    DocumentURL m_documentURL;
    RefPtr<WebCore::SecurityOrigin> m_origin;
    RefPtr<WebCore::SecurityOrigin> m_topOrigin;
    RefPtr<WebCore::SecurityOrigin> m_parentOrigin;
    std::optional<WebCore::ContentSecurityPolicyResponseHeaders> m_cspResponseHeaders;
    WebCore::CrossOriginEmbedderPolicy m_parentCrossOriginEmbedderPolicy;
    WebCore::CrossOriginEmbedderPolicy m_crossOriginEmbedderPolicy;
#if ENABLE(CONTENT_EXTENSIONS)
    URL m_mainDocumentURL;
    URL m_frameURL;
    std::optional<UserContentControllerIdentifier> m_userContentControllerIdentifier;
#endif

    std::unique_ptr<NetworkCORSPreflightChecker> m_corsPreflightChecker;
    bool m_isSameOriginRequest { true };
    bool m_isSimpleRequest { true };
    std::unique_ptr<WebCore::ContentSecurityPolicy> m_contentSecurityPolicy;
    size_t m_redirectCount { 0 };
    URL m_previousURL;
    WebCore::PreflightPolicy m_preflightPolicy;
    String m_referrer;
    bool m_checkContentExtensions { false };
    bool m_shouldCaptureExtraNetworkLoadMetrics { false };

    WebCore::NetworkLoadInformation m_loadInformation;

    LoadType m_requestLoadType;
    RefPtr<NetworkSchemeRegistry> m_schemeRegistry;
    WeakPtr<NetworkResourceLoader> m_networkResourceLoader;
};

}
