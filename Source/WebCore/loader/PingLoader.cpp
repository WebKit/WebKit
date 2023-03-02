/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Roopesh Chander (roop@roopc.net)
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContentRuleListResults.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTTPHeaderValues.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "NetworkLoadMetrics.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoadInfo.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "UserContentController.h"
#include "ViolationReportType.h"
#include <wtf/text/CString.h>

namespace WebCore {

#if ENABLE(CONTENT_EXTENSIONS)

// Returns true if we should block the load.
static bool processContentRuleListsForLoad(const Frame& frame, ResourceRequest& request, OptionSet<ContentExtensions::ResourceType> resourceType)
{
    auto* documentLoader = frame.loader().documentLoader();
    if (!documentLoader)
        return false;
    auto* page = frame.page();
    if (!page)
        return false;
    auto results = page->userContentProvider().processContentRuleListsForLoad(*page, request.url(), resourceType, *documentLoader);
    bool result = results.summary.blockedLoad;
    ContentExtensions::applyResultsToRequest(WTFMove(results), page, request);
    return result;
}

#endif

void PingLoader::loadImage(Frame& frame, const URL& url)
{
    ASSERT(frame.document());
    auto& document = *frame.document();

    if (!document.securityOrigin().canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(&frame, url.string());
        return;
    }

    if (!portAllowed(url)) {
        FrameLoader::reportBlockedLoadFailed(frame, url);
        return;
    }

    ResourceRequest request(url);
#if ENABLE(CONTENT_EXTENSIONS)
    if (processContentRuleListsForLoad(frame, request, ContentExtensions::ResourceType::Image))
        return;
#endif

    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);

    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, HTTPHeaderValues::maxAge0());

    HTTPHeaderMap originalRequestHeader = request.httpHeaderFields();

    String referrer = SecurityPolicy::generateReferrerHeader(document.referrerPolicy(), request.url(), frame.loader().outgoingReferrer());
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);
    frame.loader().updateRequestAndAddExtraFields(request, IsMainResource::No);

    startPingLoad(frame, request, WTFMove(originalRequestHeader), ShouldFollowRedirects::Yes, ContentSecurityPolicyImposition::DoPolicyCheck, ReferrerPolicy::EmptyString);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::sendPing(Frame& frame, const URL& pingURL, const URL& destinationURL)
{
    ASSERT(frame.document());

    if (!pingURL.protocolIsInHTTPFamily())
        return;

    ResourceRequest request(pingURL);
    request.setRequester(ResourceRequestRequester::Ping);

#if ENABLE(CONTENT_EXTENSIONS)
    if (processContentRuleListsForLoad(frame, request, ContentExtensions::ResourceType::Ping))
        return;
#endif

    auto& document = *frame.document();
    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);

    request.setHTTPMethod("POST"_s);
    request.setHTTPContentType("text/ping"_s);
    request.setHTTPBody(FormData::create("PING"));
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, HTTPHeaderValues::maxAge0());

    HTTPHeaderMap originalRequestHeader = request.httpHeaderFields();

    auto& sourceOrigin = document.securityOrigin();
    FrameLoader::addHTTPOriginIfNeeded(request, SecurityPolicy::generateOriginHeader(document.referrerPolicy(), request.url(), sourceOrigin));

    frame.loader().updateRequestAndAddExtraFields(request, IsMainResource::No);

    // https://html.spec.whatwg.org/multipage/links.html#hyperlink-auditing
    if (document.securityOrigin().isSameOriginAs(SecurityOrigin::create(pingURL).get())
        || !document.url().protocolIs("https"_s))
        request.setHTTPHeaderField(HTTPHeaderName::PingFrom, document.url().string());
    request.setHTTPHeaderField(HTTPHeaderName::PingTo, destinationURL.string());

    startPingLoad(frame, request, WTFMove(originalRequestHeader), ShouldFollowRedirects::Yes, ContentSecurityPolicyImposition::DoPolicyCheck, ReferrerPolicy::NoReferrer);
}

void PingLoader::sendViolationReport(Frame& frame, const URL& reportURL, Ref<FormData>&& report, ViolationReportType reportType)
{
    ASSERT(frame.document());

    // FIXME: Add the concept of browsing context group like in the specification instead of treating the whole process as a group.
    // https://bugs.webkit.org/show_bug.cgi?id=244945
    if (reportType == ViolationReportType::CrossOriginOpenerPolicy && Page::nonUtilityPageCount() <= 1)
        return;

    ResourceRequest request(reportURL);
#if ENABLE(CONTENT_EXTENSIONS)
    if (processContentRuleListsForLoad(frame, request, ContentExtensions::ResourceType::CSPReport))
        return;
#endif

    auto& document = *frame.document();
    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);

    request.setHTTPMethod("POST"_s);
    request.setHTTPBody(WTFMove(report));
    switch (reportType) {
    case ViolationReportType::ContentSecurityPolicy:
        request.setHTTPContentType("application/csp-report"_s);
        break;
    case ViolationReportType::COEPInheritenceViolation:
    case ViolationReportType::CORPViolation:
    case ViolationReportType::CrossOriginOpenerPolicy:
    case ViolationReportType::Deprecation:
    case ViolationReportType::StandardReportingAPIViolation:
    case ViolationReportType::Test:
        request.setHTTPContentType("application/reports+json"_s);
        break;
    }

    bool removeCookies = true;
    if (document.securityOrigin().isSameSchemeHostPort(SecurityOrigin::create(reportURL).get()))
        removeCookies = false;
    if (removeCookies)
        request.setAllowCookies(false);

    HTTPHeaderMap originalRequestHeader = request.httpHeaderFields();

    if (reportType == ViolationReportType::ContentSecurityPolicy)
        frame.loader().updateRequestAndAddExtraFields(request, IsMainResource::No);

    String referrer = SecurityPolicy::generateReferrerHeader(document.referrerPolicy(), reportURL, frame.loader().outgoingReferrer());
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);

    startPingLoad(frame, request, WTFMove(originalRequestHeader), ShouldFollowRedirects::No, ContentSecurityPolicyImposition::SkipPolicyCheck, ReferrerPolicy::EmptyString, reportType);
}

void PingLoader::startPingLoad(Frame& frame, ResourceRequest& request, HTTPHeaderMap&& originalRequestHeaders, ShouldFollowRedirects shouldFollowRedirects, ContentSecurityPolicyImposition policyCheck, ReferrerPolicy referrerPolicy, std::optional<ViolationReportType> violationReportType)
{
    auto identifier = ResourceLoaderIdentifier::generate();
    // FIXME: Why activeDocumentLoader? I would have expected documentLoader().
    // It seems like the PingLoader should be associated with the current
    // Document in the Frame, but the activeDocumentLoader will be associated
    // with the provisional DocumentLoader if there is a provisional
    // DocumentLoader.
    bool shouldUseCredentialStorage = frame.loader().client().shouldUseCredentialStorage(frame.loader().activeDocumentLoader(), identifier);
    ResourceLoaderOptions options;
    options.credentials = shouldUseCredentialStorage ? FetchOptions::Credentials::Include : FetchOptions::Credentials::Omit;
    options.redirect = shouldFollowRedirects == ShouldFollowRedirects::Yes ? FetchOptions::Redirect::Follow : FetchOptions::Redirect::Error;
    options.keepAlive = true;
    options.contentSecurityPolicyImposition = policyCheck;
    options.referrerPolicy = referrerPolicy;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.cache = FetchOptions::Cache::NoCache;

    // https://www.w3.org/TR/reporting/#try-delivery
    if (violationReportType && *violationReportType != ViolationReportType::ContentSecurityPolicy) {
        options.credentials = FetchOptions::Credentials::SameOrigin;
        options.mode = FetchOptions::Mode::Cors;
        options.serviceWorkersMode = ServiceWorkersMode::None;
        options.destination = FetchOptions::Destination::Report;
    }

    // FIXME: Deprecate the ping load code path.
    if (platformStrategies()->loaderStrategy()->usePingLoad()) {
        InspectorInstrumentation::willSendRequestOfType(&frame, identifier, frame.loader().activeDocumentLoader(), request, InspectorInstrumentation::LoadType::Ping);

        platformStrategies()->loaderStrategy()->startPingLoad(frame, request, WTFMove(originalRequestHeaders), options, policyCheck, [protectedFrame = Ref { frame }, identifier] (const ResourceError& error, const ResourceResponse& response) {
            if (!response.isNull())
                InspectorInstrumentation::didReceiveResourceResponse(protectedFrame, identifier, protectedFrame->loader().activeDocumentLoader(), response, nullptr);
            if (!error.isNull()) {
                InspectorInstrumentation::didFailLoading(protectedFrame.ptr(), protectedFrame->loader().activeDocumentLoader(), identifier, error);
                return;
            }
            InspectorInstrumentation::didFinishLoading(protectedFrame.ptr(), protectedFrame->loader().activeDocumentLoader(), identifier, { }, nullptr);
        });
        return;
    }

    CachedResourceRequest cachedResourceRequest { ResourceRequest { request }, options };
    frame.document()->cachedResourceLoader().requestPingResource(WTFMove(cachedResourceRequest));
}

// // https://html.spec.whatwg.org/multipage/origin.html#sanitize-url-report
String PingLoader::sanitizeURLForReport(const URL& url)
{
    URL sanitizedURL = url;
    sanitizedURL.removeCredentials();
    sanitizedURL.removeFragmentIdentifier();
    return sanitizedURL.string();
}

} // namespace WebCore
