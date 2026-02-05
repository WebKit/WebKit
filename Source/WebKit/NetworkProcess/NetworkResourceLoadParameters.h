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
#include <WebCore/FetchingWorkerIdentifier.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/NavigationRequester.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/SecurityContext.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/SharedWorkerIdentifier.h>
#include <wtf/Seconds.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct NetworkResourceLoadParameters {
    void createSandboxExtensionHandlesIfNecessary();

    RefPtr<WebCore::SecurityOrigin> parentOrigin() const;
    NetworkLoadParameters networkLoadParameters() const;

    WebPageProxyIdentifier webPageProxyID;
    WebCore::PageIdentifier webPageID;
    WebCore::FrameIdentifier webFrameID;
    WebCore::ResourceRequest request;
    RefPtr<WebCore::SecurityOrigin> topOrigin { };
    RefPtr<WebCore::SecurityOrigin> sourceOrigin { };
    WTF::ProcessID parentPID { 0 };
    WebCore::ContentSniffingPolicy contentSniffingPolicy { WebCore::ContentSniffingPolicy::SniffContent };
    WebCore::ContentEncodingSniffingPolicy contentEncodingSniffingPolicy { WebCore::ContentEncodingSniffingPolicy::Default };
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy { WebCore::StoredCredentialsPolicy::DoNotUse };
    WebCore::ClientCredentialPolicy clientCredentialPolicy { WebCore::ClientCredentialPolicy::CannotAskClientForCredentials };
    bool shouldClearReferrerOnHTTPSToHTTPRedirect { true };
    bool needsCertificateInfo { false };
    bool isMainFrameNavigation { false };
    std::optional<NavigationActionData> mainResourceNavigationDataForAnyFrame { };
    PreconnectOnly shouldPreconnectOnly { PreconnectOnly::No };
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain { NavigatingToAppBoundDomain::No };
    bool hadMainFrameMainResourcePrivateRelayed { false };
    bool allowPrivacyProxy { true };
    OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections { };

    RefPtr<WebCore::SecurityOrigin> protectedSourceOrigin() const { return sourceOrigin; }
    uint64_t requiredCookiesVersion { 0 };

    Markable<WebCore::ResourceLoaderIdentifier> identifier { };
    Vector<SandboxExtensionHandle> requestBodySandboxExtensions { };
    std::optional<SandboxExtensionHandle> resourceSandboxExtension { };
    Seconds maximumBufferingTime { };
    WebCore::FetchOptions options { };
    std::optional<WebCore::ContentSecurityPolicyResponseHeaders> cspResponseHeaders { };
    URL parentFrameURL { };
    URL frameURL { };
    WebCore::CrossOriginEmbedderPolicy parentCrossOriginEmbedderPolicy { };
    WebCore::CrossOriginEmbedderPolicy crossOriginEmbedderPolicy { };
    WebCore::HTTPHeaderMap originalRequestHeaders { };
    bool shouldRestrictHTTPResponseAccess { false };
    WebCore::PreflightPolicy preflightPolicy { WebCore::PreflightPolicy::Consider };
    bool shouldEnableCrossOriginResourcePolicy { false };
    Vector<Ref<WebCore::SecurityOrigin>> frameAncestorOrigins { };
    bool pageHasResourceLoadClient { false };
    std::optional<WebCore::FrameIdentifier> parentFrameID { };
    bool crossOriginAccessControlCheckEnabled { true };
    URL documentURL { };

    bool isCrossOriginOpenerPolicyEnabled { false };
    bool isClearSiteDataHeaderEnabled { false };
    bool isClearSiteDataExecutionContextEnabled { false };
    bool isDisplayingInitialEmptyDocument { false };
    WebCore::SandboxFlags effectiveSandboxFlags { };
    URL openerURL { };
    WebCore::CrossOriginOpenerPolicy sourceCrossOriginOpenerPolicy { };
    std::optional<WebCore::NavigationIdentifier> navigationID { };
    std::optional<WebCore::NavigationRequester> navigationRequester { };

    WebCore::ServiceWorkersMode serviceWorkersMode { WebCore::ServiceWorkersMode::None };
    std::optional<WebCore::ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier { };
    OptionSet<WebCore::HTTPHeadersToKeepFromCleaning> httpHeadersToKeep { };
    std::optional<WebCore::FetchIdentifier> navigationPreloadIdentifier { };
    WebCore::FetchingWorkerIdentifier workerIdentifier { };

#if ENABLE(CONTENT_EXTENSIONS) || (ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTRESTRICTIONS))
    URL mainDocumentURL { };
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    std::optional<UserContentControllerIdentifier> userContentControllerIdentifier { };
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
    bool pageHasLoadedWebExtensions { false };
#endif

    bool linkPreconnectEarlyHintsEnabled { false };
    bool shouldRecordFrameLoadForStorageAccess { false };

    bool isInitiatorPrefetch { false };
    bool isInitiatedByDedicatedWorker { false };
};

} // namespace WebKit
