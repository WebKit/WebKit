/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "NetworkContentRuleListManager.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>

namespace WebCore {
class ContentSecurityPolicy;
}

namespace WebKit {

class NetworkCORSPreflightChecker;

class NetworkLoadChecker : public RefCounted<NetworkLoadChecker> {
public:
    static Ref<NetworkLoadChecker> create(WebCore::FetchOptions&& options, PAL::SessionID sessionID, WebCore::HTTPHeaderMap&& originalHeaders, WebCore::URL&& url, RefPtr<WebCore::SecurityOrigin>&& sourceOrigin, WebCore::PreflightPolicy preflightPolicy, String&& referrer)
    {
        return adoptRef(*new NetworkLoadChecker { WTFMove(options), sessionID, WTFMove(originalHeaders), WTFMove(url), WTFMove(sourceOrigin), preflightPolicy, WTFMove(referrer) });
    }
    ~NetworkLoadChecker();

    using RequestOrError = Expected<WebCore::ResourceRequest, WebCore::ResourceError>;
    using ValidationHandler = CompletionHandler<void(RequestOrError&&)>;
    void check(WebCore::ResourceRequest&&, ValidationHandler&&);
    void checkRedirection(WebCore::ResourceResponse&, WebCore::ResourceRequest&&, ValidationHandler&&);
    void prepareRedirectedRequest(WebCore::ResourceRequest&);

    WebCore::ResourceError validateResponse(WebCore::ResourceResponse&);

    void setCSPResponseHeaders(WebCore::ContentSecurityPolicyResponseHeaders&& headers) { m_cspResponseHeaders = WTFMove(headers); }
#if ENABLE(CONTENT_EXTENSIONS)
    void setContentExtensionController(WebCore::URL&& mainDocumentURL, std::optional<UserContentControllerIdentifier> identifier)
    {
        m_mainDocumentURL = WTFMove(mainDocumentURL);
        m_userContentControllerIdentifier = identifier;
    }
#endif

    const WebCore::URL& url() const { return m_url; }
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy() const { return m_storedCredentialsPolicy; }

private:
    NetworkLoadChecker(WebCore::FetchOptions&&, PAL::SessionID, WebCore::HTTPHeaderMap&&, WebCore::URL&&, RefPtr<WebCore::SecurityOrigin>&&, WebCore::PreflightPolicy, String&& referrer);

    WebCore::ContentSecurityPolicy* contentSecurityPolicy();
    bool isChecking() const { return !!m_corsPreflightChecker; }
    bool isRedirected() const { return m_redirectCount; }

    void checkRequest(WebCore::ResourceRequest&&, ValidationHandler&&);

    void continueCheckingRequest(WebCore::ResourceRequest&&, ValidationHandler&&);

    bool doesNotNeedCORSCheck(const WebCore::URL&) const;
    void checkCORSRequest(WebCore::ResourceRequest&&, ValidationHandler&&);
    void checkCORSRedirectedRequest(WebCore::ResourceRequest&&, ValidationHandler&&);
    void checkCORSRequestWithPreflight(WebCore::ResourceRequest&&, ValidationHandler&&);

    RequestOrError accessControlErrorForValidationHandler(String&&);

#if ENABLE(CONTENT_EXTENSIONS)
    void processContentExtensionRulesForLoad(WebCore::ResourceRequest&&, CompletionHandler<void(WebCore::ResourceRequest&&, const WebCore::ContentExtensions::BlockedStatus&)>&&);
#endif

    WebCore::FetchOptions m_options;
    WebCore::StoredCredentialsPolicy m_storedCredentialsPolicy;
    PAL::SessionID m_sessionID;
    WebCore::HTTPHeaderMap m_originalRequestHeaders; // Needed for CORS checks.
    WebCore::HTTPHeaderMap m_firstRequestHeaders; // Needed for CORS checks.
    WebCore::URL m_url;
    RefPtr<WebCore::SecurityOrigin> m_origin;
    std::optional<WebCore::ContentSecurityPolicyResponseHeaders> m_cspResponseHeaders;
#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::URL m_mainDocumentURL;
    std::optional<UserContentControllerIdentifier> m_userContentControllerIdentifier;
#endif

    std::unique_ptr<NetworkCORSPreflightChecker> m_corsPreflightChecker;
    bool m_isSameOriginRequest { true };
    bool m_isSimpleRequest { true };
    std::unique_ptr<WebCore::ContentSecurityPolicy> m_contentSecurityPolicy;
    size_t m_redirectCount { 0 };
    WebCore::URL m_previousURL;
    WebCore::PreflightPolicy m_preflightPolicy;
    String m_dntHeaderValue;
    String m_referrer;
};

}
