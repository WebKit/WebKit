/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PolicyChecker.h"

#include "BlobRegistry.h"
#include "BlobURL.h"
#include "ContentFilter.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventNames.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLPlugInElement.h"
#include "Logging.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/CompletionHandler.h>

#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

#define PAGE_ID (m_frame.loader().pageID().value_or(PageIdentifier()).toUInt64())
#define FRAME_ID (m_frame.loader().frameID().value_or(FrameIdentifier()).toUInt64())
#define POLICYCHECKER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Loading, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 "] PolicyChecker::" fmt, this, PAGE_ID, FRAME_ID, ##__VA_ARGS__)

namespace WebCore {

static bool isAllowedByContentSecurityPolicy(const URL& url, const Element* ownerElement, bool didReceiveRedirectResponse)
{
    if (!ownerElement)
        return true;
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (ownerElement->isInUserAgentShadowTree())
        return true;

    auto redirectResponseReceived = didReceiveRedirectResponse ? ContentSecurityPolicy::RedirectResponseReceived::Yes : ContentSecurityPolicy::RedirectResponseReceived::No;

    ASSERT(ownerElement->document().contentSecurityPolicy());
    if (is<HTMLPlugInElement>(ownerElement))
        return ownerElement->document().contentSecurityPolicy()->allowObjectFromSource(url, redirectResponseReceived);
    return ownerElement->document().contentSecurityPolicy()->allowChildFrameFromSource(url, redirectResponseReceived);
}

PolicyCheckIdentifier PolicyCheckIdentifier::create()
{
    static uint64_t identifier = 0;
    identifier++;
    return PolicyCheckIdentifier { Process::identifier(), identifier };
}

bool PolicyCheckIdentifier::isValidFor(PolicyCheckIdentifier expectedIdentifier)
{
    RELEASE_ASSERT_WITH_MESSAGE(m_policyCheck, "Received 0 as the policy check identifier");
    RELEASE_ASSERT_WITH_MESSAGE(m_process == expectedIdentifier.m_process, "Received a policy check response for a wrong process");
    RELEASE_ASSERT_WITH_MESSAGE(m_policyCheck <= expectedIdentifier.m_policyCheck, "Received a policy check response from the future");
    return m_policyCheck == expectedIdentifier.m_policyCheck;
}

FrameLoader::PolicyChecker::PolicyChecker(Frame& frame)
    : m_frame(frame)
    , m_delegateIsDecidingNavigationPolicy(false)
    , m_delegateIsHandlingUnimplementablePolicy(false)
    , m_loadType(FrameLoadType::Standard)
{
}

void FrameLoader::PolicyChecker::checkNavigationPolicy(ResourceRequest&& newRequest, const ResourceResponse& redirectResponse, NavigationPolicyDecisionFunction&& function)
{
    checkNavigationPolicy(WTFMove(newRequest), redirectResponse, m_frame.loader().activeDocumentLoader(), { }, WTFMove(function));
}

BlobURLHandle FrameLoader::PolicyChecker::extendBlobURLLifetimeIfNecessary(const ResourceRequest& request, PolicyDecisionMode mode) const
{
    if (mode != PolicyDecisionMode::Asynchronous || !request.url().protocolIsBlob())
        return { };

    return BlobURLHandle { request.url() };
}

void FrameLoader::PolicyChecker::checkNavigationPolicy(ResourceRequest&& request, const ResourceResponse& redirectResponse, DocumentLoader* loader, RefPtr<FormState>&& formState, NavigationPolicyDecisionFunction&& function, PolicyDecisionMode policyDecisionMode)
{
    NavigationAction action = loader->triggeringAction();
    if (action.isEmpty()) {
        action = NavigationAction { *m_frame.document(), request, InitiatedByMainFrame::Unknown, NavigationType::Other, loader->shouldOpenExternalURLsPolicyToPropagate() };
        loader->setTriggeringAction(NavigationAction { action });
    }

    if (m_frame.page() && m_frame.page()->openedByDOMWithOpener())
        action.setOpenedByDOMWithOpener();
    action.setHasOpenedFrames(m_frame.loader().hasOpenedFrames());

    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if (equalIgnoringHeaderFields(request, loader->lastCheckedRequest()) || (!request.isNull() && request.url().isEmpty())) {
        if (!request.isNull() && request.url().isEmpty())
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: continuing because the URL is empty");
        else
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: continuing because the URL is the same as the last request");
        function(ResourceRequest(request), { }, NavigationPolicyDecision::ContinueLoad);
        loader->setLastCheckedRequest(WTFMove(request));
        return;
    }

    // We are always willing to show alternate content for unreachable URLs;
    // treat it like a reload so it maintains the right state for b/f list.
    auto& substituteData = loader->substituteData();
    if (substituteData.isValid() && !substituteData.failingURL().isEmpty()) {
        bool shouldContinue = true;
#if ENABLE(CONTENT_FILTERING)
        if (auto loader = m_frame.loader().activeDocumentLoader())
            shouldContinue = ContentFilter::continueAfterSubstituteDataRequest(*loader, substituteData);
#endif
        if (isBackForwardLoadType(m_loadType))
            m_loadType = FrameLoadType::Reload;
        if (shouldContinue)
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: continuing because we have valid substitute data");
        else
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: not continuing with substitute data because the content filter told us not to");

        function(WTFMove(request), { }, shouldContinue ? NavigationPolicyDecision::ContinueLoad : NavigationPolicyDecision::IgnoreLoad);
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url(), m_frame.ownerElement(), !redirectResponse.isNull())) {
        if (m_frame.ownerElement()) {
            // Fire a load event (even though we were blocked by CSP) as timing attacks would otherwise
            // reveal that the frame was blocked. This way, it looks like any other cross-origin page load.
            m_frame.ownerElement()->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
        }
        POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: ignoring because disallowed by content security policy");
        function(WTFMove(request), { }, NavigationPolicyDecision::IgnoreLoad);
        return;
    }

    loader->setLastCheckedRequest(ResourceRequest(request));

#if USE(QUICK_LOOK)
    // Always allow QuickLook-generated URLs based on the protocol scheme.
    if (!request.isNull() && isQuickLookPreviewURL(request.url())) {
        POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: continuing because quicklook-generated URL");
        return function(WTFMove(request), makeWeakPtr(formState.get()), NavigationPolicyDecision::ContinueLoad);
    }
#endif

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilterUnblockHandler.canHandleRequest(request)) {
        RefPtr<Frame> frame { &m_frame };
        m_contentFilterUnblockHandler.requestUnblockAsync([frame](bool unblocked) {
            if (unblocked)
                frame->loader().reload();
        });
        POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: ignoring because ContentFilterUnblockHandler can handle the request");
        return function({ }, nullptr, NavigationPolicyDecision::IgnoreLoad);
    }
    m_contentFilterUnblockHandler = { };
#endif

    m_frame.loader().clearProvisionalLoadForPolicyCheck();

    auto blobURLLifetimeExtension = extendBlobURLLifetimeIfNecessary(request, policyDecisionMode);

    bool isInitialEmptyDocumentLoad = !m_frame.loader().stateMachine().committedFirstRealDocumentLoad() && request.url().protocolIsAbout() && !substituteData.isValid();
    auto requestIdentifier = PolicyCheckIdentifier::create();
    m_delegateIsDecidingNavigationPolicy = true;
    String suggestedFilename = action.downloadAttribute().isEmpty() ? nullAtom() : action.downloadAttribute();
    FramePolicyFunction decisionHandler = [this, function = WTFMove(function), request = ResourceRequest(request), formState = std::exchange(formState, nullptr), suggestedFilename = WTFMove(suggestedFilename),
         blobURLLifetimeExtension = WTFMove(blobURLLifetimeExtension), requestIdentifier, isInitialEmptyDocumentLoad] (PolicyAction policyAction, PolicyCheckIdentifier responseIdentifier) mutable {
        if (!responseIdentifier.isValidFor(requestIdentifier)) {
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: ignoring because response is not valid for request");
            return function({ }, nullptr, NavigationPolicyDecision::IgnoreLoad);
        }

        m_delegateIsDecidingNavigationPolicy = false;

        switch (policyAction) {
        case PolicyAction::Download:
            m_frame.loader().setOriginalURLForDownloadRequest(request);
            m_frame.loader().client().startDownload(request, suggestedFilename);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: ignoring because policyAction from dispatchDecidePolicyForNavigationAction is Ignore");
            return function({ }, nullptr, NavigationPolicyDecision::IgnoreLoad);
        case PolicyAction::StopAllLoads:
            POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: stopping because policyAction from dispatchDecidePolicyForNavigationAction is StopAllLoads");
            function({ }, nullptr, NavigationPolicyDecision::StopAllLoads);
            return;
        case PolicyAction::Use:
            if (!m_frame.loader().client().canHandleRequest(request)) {
                handleUnimplementablePolicy(m_frame.loader().client().cannotShowURLError(request));
                POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: ignoring because frame loader client can't handle the request");
                return function({ }, { }, NavigationPolicyDecision::IgnoreLoad);
            }
            if (isInitialEmptyDocumentLoad)
                POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: continuing because this is an initial empty document");
            else
                POLICYCHECKER_RELEASE_LOG("checkNavigationPolicy: continuing because this policyAction from dispatchDecidePolicyForNavigationAction is Use");
            return function(WTFMove(request), makeWeakPtr(formState.get()), NavigationPolicyDecision::ContinueLoad);
        }
        ASSERT_NOT_REACHED();
    };

    if (isInitialEmptyDocumentLoad) {
        // We ignore the response from the client for initial empty document loads and proceed with the load synchronously.
        m_frame.loader().client().dispatchDecidePolicyForNavigationAction(action, request, redirectResponse, formState.get(), policyDecisionMode, requestIdentifier, [](PolicyAction, PolicyCheckIdentifier) { });
        decisionHandler(PolicyAction::Use, requestIdentifier);
    } else
        m_frame.loader().client().dispatchDecidePolicyForNavigationAction(action, request, redirectResponse, formState.get(), policyDecisionMode, requestIdentifier, WTFMove(decisionHandler));
}

void FrameLoader::PolicyChecker::checkNewWindowPolicy(NavigationAction&& navigationAction, ResourceRequest&& request, RefPtr<FormState>&& formState, const String& frameName, NewWindowPolicyDecisionFunction&& function)
{
    if (m_frame.document() && m_frame.document()->isSandboxed(SandboxPopups))
        return function({ }, nullptr, { }, { }, ShouldContinuePolicyCheck::No);

    if (!DOMWindow::allowPopUp(m_frame))
        return function({ }, nullptr, { }, { }, ShouldContinuePolicyCheck::No);

    auto blobURLLifetimeExtension = extendBlobURLLifetimeIfNecessary(request);

    auto requestIdentifier = PolicyCheckIdentifier::create();
    m_frame.loader().client().dispatchDecidePolicyForNewWindowAction(navigationAction, request, formState.get(), frameName, requestIdentifier, [frame = makeRef(m_frame), request,
        formState = WTFMove(formState), frameName, navigationAction, function = WTFMove(function), blobURLLifetimeExtension = WTFMove(blobURLLifetimeExtension),
        requestIdentifier] (PolicyAction policyAction, PolicyCheckIdentifier responseIdentifier) mutable {

        if (!responseIdentifier.isValidFor(requestIdentifier))
            return function({ }, nullptr, { }, { }, ShouldContinuePolicyCheck::No);

        switch (policyAction) {
        case PolicyAction::Download:
            frame->loader().client().startDownload(request);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            function({ }, nullptr, { }, { }, ShouldContinuePolicyCheck::No);
            return;
        case PolicyAction::StopAllLoads:
            ASSERT_NOT_REACHED();
            function({ }, nullptr, { }, { }, ShouldContinuePolicyCheck::No);
            return;
        case PolicyAction::Use:
            function(request, makeWeakPtr(formState.get()), frameName, navigationAction, ShouldContinuePolicyCheck::Yes);
            return;
        }
        ASSERT_NOT_REACHED();
    });
}

void FrameLoader::PolicyChecker::stopCheck()
{
    m_frame.loader().client().cancelPolicyCheck();
}

void FrameLoader::PolicyChecker::cannotShowMIMEType(const ResourceResponse& response)
{
    handleUnimplementablePolicy(m_frame.loader().client().cannotShowMIMETypeError(response));
}

void FrameLoader::PolicyChecker::handleUnimplementablePolicy(const ResourceError& error)
{
    m_delegateIsHandlingUnimplementablePolicy = true;
    m_frame.loader().client().dispatchUnableToImplementPolicy(error);
    m_delegateIsHandlingUnimplementablePolicy = false;
}

} // namespace WebCore

#undef IS_ALLOWED
#undef PAGE_ID
#undef FRAME_ID
#undef POLICYCHECKER_RELEASE_LOG
