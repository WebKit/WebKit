/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AgentClusterRequest.h"

#include "WebPreferences.h"
#include <WebCore/OriginAgentClusterPolicy.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityOrigin.h>

namespace WebKit {

AgentClusterRequest AgentClusterRequest::forNavigationAction(const URL& url, const URL& topLevelCreationURL, IsSubstituteData isSubstituteData)
{
    return { isSubstituteData == IsSubstituteData::Yes ? Source::SubstituteData : Source::NavigationAction, WebCore::SecurityOrigin::create(url)->data(), topLevelCreationURL };
}

static WebCore::IsSecureContext isSecureContextForNavigationResponse(const WebCore::SecurityOrigin& origin, WebCore::IsSecureContext topLevelIsSecureContext)
{
    return origin.isPotentiallyTrustworthy() && topLevelIsSecureContext == WebCore::IsSecureContext::Yes ? WebCore::IsSecureContext::Yes : WebCore::IsSecureContext::No;
}

WebCore::DocumentIsolationPolicyValue documentIsolationPolicyForNavigationResponse(const URL& url, WebCore::DocumentIsolationPolicyValue headerPolicy, WebCore::IsSecureContext topLevelIsSecureContext, const WebPreferences& preferences)
{
    // The WebContent process only applies policies from response headers to http(s) and blob: documents.
    if (!url.protocolIsInHTTPFamilyOrBlob() || !preferences.documentIsolationPolicyEnabled() || !preferences.siteIsolationEnabled())
        return WebCore::DocumentIsolationPolicyValue::None;
    WebCore::DocumentIsolationPolicy policy;
    policy.value = headerPolicy;
    return WebCore::obtainDocumentIsolationPolicy(url, WTF::move(policy), isSecureContextForNavigationResponse(WebCore::SecurityOrigin::create(url), topLevelIsSecureContext)).value;
}

AgentClusterRequest AgentClusterRequest::forNavigationResponse(const WebCore::ResourceResponse& response, const URL& topLevelCreationURL, WebCore::IsSecureContext topLevelIsSecureContext, const WebPreferences& preferences)
{
    Ref origin = WebCore::SecurityOrigin::create(response.url());
    AgentClusterRequest request { Source::NavigationResponse, origin->data(), topLevelCreationURL };
    request.documentIsolationPolicy = documentIsolationPolicyForNavigationResponse(response.url(), WebCore::parseDocumentIsolationPolicy(response).value, topLevelIsSecureContext, preferences);
    if (preferences.originAgentClusterEnabled())
        request.requestedKeying = response.url().protocolIsInHTTPFamilyOrBlob() ? WebCore::obtainOriginAgentClusterPolicy(response, isSecureContextForNavigationResponse(origin, topLevelIsSecureContext), nullptr) : WebCore::OriginKeyed::No;
    return request;
}

} // namespace WebKit
