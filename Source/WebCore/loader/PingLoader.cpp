/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Roopesh Chander (roop@roopc.net)
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "PingLoader.h"

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "NetworkLoadMetrics.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "ResourceLoadInfo.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "UserContentController.h"
#include <wtf/text/CString.h>

namespace WebCore {

#if !ENABLE(CONTENT_EXTENSIONS)

// Returns true if we should block the load.
static inline bool processContentExtensionRulesForLoad(const Frame&, ResourceRequest&, ResourceType)
{
    return false;
}

#else

// Returns true if we should block the load.
static bool processContentExtensionRulesForLoad(const Frame& frame, ResourceRequest& request, ResourceType resourceType)
{
    auto* documentLoader = frame.loader().documentLoader();
    if (!documentLoader)
        return false;
    auto* page = frame.page();
    if (!page)
        return false;
    auto status = page->userContentProvider().processContentExtensionRulesForLoad(request.url(), resourceType, *documentLoader);
    applyBlockedStatusToRequest(status, page, request);
    return status.blockedLoad;
}

#endif

void PingLoader::loadImage(Frame& frame, const URL& url)
{
    ASSERT(frame.document());
    auto& document = *frame.document();

    if (!document.securityOrigin().canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(&frame, url);
        return;
    }

    ResourceRequest request(url);
    if (processContentExtensionRulesForLoad(frame, request, ResourceType::Image))
        return;

    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);

    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, "max-age=0");

    HTTPHeaderMap originalRequestHeader = request.httpHeaderFields();

    String referrer = SecurityPolicy::generateReferrerHeader(document.referrerPolicy(), request.url(), frame.loader().outgoingReferrer());
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);
    frame.loader().addExtraFieldsToSubresourceRequest(request);

    startPingLoad(frame, request, WTFMove(originalRequestHeader), ShouldFollowRedirects::Yes, ContentSecurityPolicyImposition::DoPolicyCheck);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::sendPing(Frame& frame, const URL& pingURL, const URL& destinationURL)
{
    ASSERT(frame.document());

    if (!pingURL.protocolIsInHTTPFamily())
        return;

    ResourceRequest request(pingURL);
    if (processContentExtensionRulesForLoad(frame, request, ResourceType::Raw))
        return;

    auto& document = *frame.document();
    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);

    request.setHTTPMethod("POST");
    request.setHTTPContentType("text/ping");
    request.setHTTPBody(FormData::create("PING"));
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, "max-age=0");

    HTTPHeaderMap originalRequestHeader = request.httpHeaderFields();

    frame.loader().addExtraFieldsToSubresourceRequest(request);

    auto& sourceOrigin = document.securityOrigin();
    FrameLoader::addHTTPOriginIfNeeded(request, sourceOrigin.toString());
    request.setHTTPHeaderField(HTTPHeaderName::PingTo, destinationURL);
    if (!SecurityPolicy::shouldHideReferrer(pingURL, frame.loader().outgoingReferrer())) {
        request.setHTTPHeaderField(HTTPHeaderName::PingFrom, document.url());
        if (!sourceOrigin.isSameSchemeHostPort(SecurityOrigin::create(pingURL).get())) {
            String referrer = SecurityPolicy::generateReferrerHeader(document.referrerPolicy(), pingURL, frame.loader().outgoingReferrer());
            if (!referrer.isEmpty())
                request.setHTTPReferrer(referrer);
        }
    }

    startPingLoad(frame, request, WTFMove(originalRequestHeader), ShouldFollowRedirects::Yes, ContentSecurityPolicyImposition::DoPolicyCheck);
}

void PingLoader::sendViolationReport(Frame& frame, const URL& reportURL, Ref<FormData>&& report, ViolationReportType reportType)
{
    ASSERT(frame.document());

    ResourceRequest request(reportURL);
    if (processContentExtensionRulesForLoad(frame, request, ResourceType::Raw))
        return;

    auto& document = *frame.document();
    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);

    request.setHTTPMethod("POST"_s);
    request.setHTTPBody(WTFMove(report));
    switch (reportType) {
    case ViolationReportType::ContentSecurityPolicy:
        request.setHTTPContentType("application/csp-report"_s);
        break;
    case ViolationReportType::XSSAuditor:
        request.setHTTPContentType("application/json"_s);
        break;
    }

    bool removeCookies = true;
    if (document.securityOrigin().isSameSchemeHostPort(SecurityOrigin::create(reportURL).get()))
        removeCookies = false;
    if (removeCookies)
        request.setAllowCookies(false);

    HTTPHeaderMap originalRequestHeader = request.httpHeaderFields();

    frame.loader().addExtraFieldsToSubresourceRequest(request);

    String referrer = SecurityPolicy::generateReferrerHeader(document.referrerPolicy(), reportURL, frame.loader().outgoingReferrer());
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);

    startPingLoad(frame, request, WTFMove(originalRequestHeader), ShouldFollowRedirects::No, ContentSecurityPolicyImposition::SkipPolicyCheck);
}

void PingLoader::startPingLoad(Frame& frame, ResourceRequest& request, HTTPHeaderMap&& originalRequestHeaders, ShouldFollowRedirects shouldFollowRedirects, ContentSecurityPolicyImposition policyCheck)
{
    unsigned long identifier = frame.page()->progress().createUniqueIdentifier();
    // FIXME: Why activeDocumentLoader? I would have expected documentLoader().
    // It seems like the PingLoader should be associated with the current
    // Document in the Frame, but the activeDocumentLoader will be associated
    // with the provisional DocumentLoader if there is a provisional
    // DocumentLoader.
    bool shouldUseCredentialStorage = frame.loader().client().shouldUseCredentialStorage(frame.loader().activeDocumentLoader(), identifier);
    FetchOptions options;
    options.credentials = shouldUseCredentialStorage ? FetchOptions::Credentials::Include : FetchOptions::Credentials::Omit;
    options.redirect = shouldFollowRedirects == ShouldFollowRedirects::Yes ? FetchOptions::Redirect::Follow : FetchOptions::Redirect::Error;

    // FIXME: Move ping loads to normal subresource loading to get normal inspector request instrumentation hooks.
    InspectorInstrumentation::willSendRequestOfType(&frame, identifier, frame.loader().activeDocumentLoader(), request, InspectorInstrumentation::LoadType::Ping);

    platformStrategies()->loaderStrategy()->startPingLoad(frame, request, WTFMove(originalRequestHeaders), options, policyCheck, [protectedFrame = makeRef(frame), identifier] (const ResourceError& error, const ResourceResponse& response) {
        if (!response.isNull())
            InspectorInstrumentation::didReceiveResourceResponse(protectedFrame, identifier, protectedFrame->loader().activeDocumentLoader(), response, nullptr);
        if (error.isNull()) {
            NetworkLoadMetrics emptyMetrics;
            InspectorInstrumentation::didFinishLoading(protectedFrame.ptr(), protectedFrame->loader().activeDocumentLoader(), identifier, emptyMetrics, nullptr);
        } else
            InspectorInstrumentation::didFailLoading(protectedFrame.ptr(), protectedFrame->loader().activeDocumentLoader(), identifier, error);
    });
}

}
