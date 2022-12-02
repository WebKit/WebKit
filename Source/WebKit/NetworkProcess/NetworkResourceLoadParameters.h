/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#include "NetworkLoadParameters.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "UserContentControllerIdentifier.h"
#include <WebCore/ContentSecurityPolicyResponseHeaders.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/NavigationRequester.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/SecurityContext.h>
#include <wtf/Seconds.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class NetworkResourceLoadParameters : public NetworkLoadParameters {
public:
    void encode(IPC::Encoder&) const;
    static std::optional<NetworkResourceLoadParameters> decode(IPC::Decoder&);

    RefPtr<WebCore::SecurityOrigin> parentOrigin() const;

    WebCore::ResourceLoaderIdentifier identifier;
    Vector<RefPtr<SandboxExtension>> requestBodySandboxExtensions; // Created automatically for the sender.
    RefPtr<SandboxExtension> resourceSandboxExtension; // Created automatically for the sender.
    Seconds maximumBufferingTime;
    WebCore::FetchOptions options;
    std::optional<WebCore::ContentSecurityPolicyResponseHeaders> cspResponseHeaders;
    URL parentFrameURL;
    URL frameURL;
    WebCore::CrossOriginEmbedderPolicy parentCrossOriginEmbedderPolicy;
    WebCore::CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    WebCore::HTTPHeaderMap originalRequestHeaders;
    bool shouldRestrictHTTPResponseAccess { false };
    WebCore::PreflightPolicy preflightPolicy { WebCore::PreflightPolicy::Consider };
    bool shouldEnableCrossOriginResourcePolicy { false };
    Vector<RefPtr<WebCore::SecurityOrigin>> frameAncestorOrigins;
    bool pageHasResourceLoadClient { false };
    std::optional<WebCore::FrameIdentifier> parentFrameID;
    bool crossOriginAccessControlCheckEnabled { true };
    URL documentURL;

    bool isCrossOriginOpenerPolicyEnabled { false };
    bool isClearSiteDataHeaderEnabled { false };
    bool isDisplayingInitialEmptyDocument { false };
    WebCore::SandboxFlags effectiveSandboxFlags { WebCore::SandboxNone };
    URL openerURL;
    WebCore::CrossOriginOpenerPolicy sourceCrossOriginOpenerPolicy;
    uint64_t navigationID { 0 };
    std::optional<WebCore::NavigationRequester> navigationRequester;

#if ENABLE(SERVICE_WORKER)
    WebCore::ServiceWorkersMode serviceWorkersMode { WebCore::ServiceWorkersMode::None };
    std::optional<WebCore::ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier;
    OptionSet<WebCore::HTTPHeadersToKeepFromCleaning> httpHeadersToKeep;
    std::optional<WebCore::FetchIdentifier> navigationPreloadIdentifier;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    URL mainDocumentURL;
    std::optional<UserContentControllerIdentifier> userContentControllerIdentifier;
#endif
    
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain { NavigatingToAppBoundDomain::No };
};

} // namespace WebKit
