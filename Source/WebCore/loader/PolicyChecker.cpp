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
#include <wtf/CompletionHandler.h>

#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

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

PolicyChecker::PolicyChecker(Frame& frame)
    : m_frame(frame)
    , m_delegateIsDecidingNavigationPolicy(false)
    , m_delegateIsHandlingUnimplementablePolicy(false)
    , m_loadType(FrameLoadType::Standard)
{
}

void PolicyChecker::checkNavigationPolicy(ResourceRequest&& newRequest, const ResourceResponse& redirectResponse, NavigationPolicyDecisionFunction&& function)
{
    checkNavigationPolicy(WTFMove(newRequest), redirectResponse, m_frame.loader().activeDocumentLoader(), { }, WTFMove(function));
}

CompletionHandlerCallingScope PolicyChecker::extendBlobURLLifetimeIfNecessary(ResourceRequest& request) const
{
    if (!request.url().protocolIsBlob())
        return { };

    // Create a new temporary blobURL in case this one gets revoked during the asynchronous navigation policy decision.
    URL temporaryBlobURL = BlobURL::createPublicURL(&m_frame.document()->securityOrigin());
    blobRegistry().registerBlobURL(temporaryBlobURL, request.url());
    request.setURL(temporaryBlobURL);
    return CompletionHandler<void()>([temporaryBlobURL = WTFMove(temporaryBlobURL)] {
        blobRegistry().unregisterBlobURL(temporaryBlobURL);
    });
}

void PolicyChecker::checkNavigationPolicy(ResourceRequest&& request, const ResourceResponse& redirectResponse, DocumentLoader* loader, RefPtr<FormState>&& formState, NavigationPolicyDecisionFunction&& function, PolicyDecisionMode policyDecisionMode)
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
        shouldContinue = ContentFilter::continueAfterSubstituteDataRequest(*m_frame.loader().activeDocumentLoader(), substituteData);
#endif
        if (isBackForwardLoadType(m_loadType))
            m_loadType = FrameLoadType::Reload;
        function(WTFMove(request), { }, shouldContinue ? NavigationPolicyDecision::ContinueLoad : NavigationPolicyDecision::IgnoreLoad);
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url(), m_frame.ownerElement(), !redirectResponse.isNull())) {
        if (m_frame.ownerElement()) {
            // Fire a load event (even though we were blocked by CSP) as timing attacks would otherwise
            // reveal that the frame was blocked. This way, it looks like any other cross-origin page load.
            m_frame.ownerElement()->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
        }
        function(WTFMove(request), { }, NavigationPolicyDecision::IgnoreLoad);
        return;
    }

    loader->setLastCheckedRequest(ResourceRequest(request));

    // Initial 'about:blank' load needs to happen synchronously so the policy check needs to be synchronous in this case.
    if (!m_frame.loader().stateMachine().committedFirstRealDocumentLoad() && request.url().protocolIsAbout() && !substituteData.isValid())
        policyDecisionMode = PolicyDecisionMode::Synchronous;

#if USE(QUICK_LOOK)
    // Always allow QuickLook-generated URLs based on the protocol scheme.
    if (!request.isNull() && isQuickLookPreviewURL(request.url()))
        return function(WTFMove(request), makeWeakPtr(formState.get()), NavigationPolicyDecision::ContinueLoad);
#endif

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilterUnblockHandler.canHandleRequest(request)) {
        RefPtr<Frame> frame { &m_frame };
        m_contentFilterUnblockHandler.requestUnblockAsync([frame](bool unblocked) {
            if (unblocked)
                frame->loader().reload();
        });
        return function({ }, nullptr, NavigationPolicyDecision::IgnoreLoad);
    }
    m_contentFilterUnblockHandler = { };
#endif

    m_frame.loader().clearProvisionalLoadForPolicyCheck();

    auto blobURLLifetimeExtension = policyDecisionMode == PolicyDecisionMode::Asynchronous ? extendBlobURLLifetimeIfNecessary(request) : CompletionHandlerCallingScope { };

    auto requestIdentifier = PolicyCheckIdentifier::create();
    m_delegateIsDecidingNavigationPolicy = true;
    String suggestedFilename = action.downloadAttribute().isEmpty() ? nullAtom() : action.downloadAttribute();
    m_frame.loader().client().dispatchDecidePolicyForNavigationAction(action, request, redirectResponse, formState.get(), policyDecisionMode, requestIdentifier,
        [this, function = WTFMove(function), request = ResourceRequest(request), formState = WTFMove(formState), suggestedFilename = WTFMove(suggestedFilename),
         blobURLLifetimeExtension = WTFMove(blobURLLifetimeExtension), requestIdentifier] (PolicyAction policyAction, PolicyCheckIdentifier responseIdentifier) mutable {

        if (!responseIdentifier.isValidFor(requestIdentifier))
            return;

        m_delegateIsDecidingNavigationPolicy = false;

        switch (policyAction) {
        case PolicyAction::Download:
            m_frame.loader().setOriginalURLForDownloadRequest(request);
            m_frame.loader().client().startDownload(request, suggestedFilename);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            return function({ }, nullptr, NavigationPolicyDecision::IgnoreLoad);
        case PolicyAction::StopAllLoads:
            function({ }, nullptr, NavigationPolicyDecision::StopAllLoads);
            return;
        case PolicyAction::Use:
            if (!m_frame.loader().client().canHandleRequest(request)) {
                handleUnimplementablePolicy(m_frame.loader().client().cannotShowURLError(request));
                return function({ }, { }, NavigationPolicyDecision::IgnoreLoad);
            }
            return function(WTFMove(request), makeWeakPtr(formState.get()), NavigationPolicyDecision::ContinueLoad);
        }
        ASSERT_NOT_REACHED();
    });
}

void PolicyChecker::checkNewWindowPolicy(NavigationAction&& navigationAction, ResourceRequest&& request, RefPtr<FormState>&& formState, const String& frameName, NewWindowPolicyDecisionFunction&& function)
{
    if (m_frame.document() && m_frame.document()->isSandboxed(SandboxPopups))
        return function({ }, nullptr, { }, { }, ShouldContinue::No);

    if (!DOMWindow::allowPopUp(m_frame))
        return function({ }, nullptr, { }, { }, ShouldContinue::No);

    auto blobURLLifetimeExtension = extendBlobURLLifetimeIfNecessary(request);

    auto requestIdentifier = PolicyCheckIdentifier::create();
    m_frame.loader().client().dispatchDecidePolicyForNewWindowAction(navigationAction, request, formState.get(), frameName, requestIdentifier, [frame = makeRef(m_frame), request,
        formState = WTFMove(formState), frameName, navigationAction, function = WTFMove(function), blobURLLifetimeExtension = WTFMove(blobURLLifetimeExtension),
        requestIdentifier] (PolicyAction policyAction, PolicyCheckIdentifier responseIdentifier) mutable {

        if (!responseIdentifier.isValidFor(requestIdentifier))
            return;

        switch (policyAction) {
        case PolicyAction::Download:
            frame->loader().client().startDownload(request);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            function({ }, nullptr, { }, { }, ShouldContinue::No);
            return;
        case PolicyAction::StopAllLoads:
            ASSERT_NOT_REACHED();
            function({ }, nullptr, { }, { }, ShouldContinue::No);
            return;
        case PolicyAction::Use:
            function(request, makeWeakPtr(formState.get()), frameName, navigationAction, ShouldContinue::Yes);
            return;
        }
        ASSERT_NOT_REACHED();
    });
}

void PolicyChecker::stopCheck()
{
    m_frame.loader().client().cancelPolicyCheck();
}

void PolicyChecker::cannotShowMIMEType(const ResourceResponse& response)
{
    handleUnimplementablePolicy(m_frame.loader().client().cannotShowMIMETypeError(response));
}

void PolicyChecker::handleUnimplementablePolicy(const ResourceError& error)
{
    m_delegateIsHandlingUnimplementablePolicy = true;
    m_frame.loader().client().dispatchUnableToImplementPolicy(error);
    m_delegateIsHandlingUnimplementablePolicy = false;
}

} // namespace WebCore
