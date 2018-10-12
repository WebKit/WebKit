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

void PolicyChecker::checkNavigationPolicy(ResourceRequest&& request, const ResourceResponse& redirectResponse, DocumentLoader* loader, RefPtr<FormState>&& formState, NavigationPolicyDecisionFunction&& function, PolicyDecisionMode policyDecisionMode, ShouldSkipSafeBrowsingCheck shouldSkipSafeBrowsingCheck)
{
    NavigationAction action = loader->triggeringAction();
    if (action.isEmpty()) {
        action = NavigationAction { *m_frame.document(), request, InitiatedByMainFrame::Unknown, NavigationType::Other, loader->shouldOpenExternalURLsPolicyToPropagate() };
        loader->setTriggeringAction(NavigationAction { action });
    }

    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if (equalIgnoringHeaderFields(request, loader->lastCheckedRequest()) || (!request.isNull() && request.url().isEmpty())) {
        function(ResourceRequest(request), { }, ShouldContinue::Yes);
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
        function(WTFMove(request), { }, shouldContinue ? ShouldContinue::Yes : ShouldContinue::No);
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url(), m_frame.ownerElement(), !redirectResponse.isNull())) {
        if (m_frame.ownerElement()) {
            // Fire a load event (even though we were blocked by CSP) as timing attacks would otherwise
            // reveal that the frame was blocked. This way, it looks like any other cross-origin page load.
            m_frame.ownerElement()->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
        }
        function(WTFMove(request), { }, ShouldContinue::No);
        return;
    }

    loader->setLastCheckedRequest(ResourceRequest(request));

    // Initial 'about:blank' load needs to happen synchronously so the policy check needs to be synchronous in this case.
    if (!m_frame.loader().stateMachine().committedFirstRealDocumentLoad() && request.url().protocolIsAbout() && !substituteData.isValid())
        policyDecisionMode = PolicyDecisionMode::Synchronous;

#if USE(QUICK_LOOK)
    // Always allow QuickLook-generated URLs based on the protocol scheme.
    if (!request.isNull() && isQuickLookPreviewURL(request.url()))
        return function(WTFMove(request), makeWeakPtr(formState.get()), ShouldContinue::Yes);
#endif

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilterUnblockHandler.canHandleRequest(request)) {
        RefPtr<Frame> frame { &m_frame };
        m_contentFilterUnblockHandler.requestUnblockAsync([frame](bool unblocked) {
            if (unblocked)
                frame->loader().reload();
        });
        return function({ }, nullptr, ShouldContinue::No);
    }
    m_contentFilterUnblockHandler = { };
#endif

    m_frame.loader().clearProvisionalLoadForPolicyCheck();

    auto blobURLLifetimeExtension = policyDecisionMode == PolicyDecisionMode::Asynchronous ? extendBlobURLLifetimeIfNecessary(request) : CompletionHandlerCallingScope { };

    m_delegateIsDecidingNavigationPolicy = true;
    String suggestedFilename = action.downloadAttribute().isEmpty() ? nullAtom() : action.downloadAttribute();
    m_frame.loader().client().dispatchDecidePolicyForNavigationAction(action, request, redirectResponse, formState.get(), policyDecisionMode, shouldSkipSafeBrowsingCheck, [this, function = WTFMove(function), request = ResourceRequest(request), formState = WTFMove(formState), suggestedFilename = WTFMove(suggestedFilename), blobURLLifetimeExtension = WTFMove(blobURLLifetimeExtension)](PolicyAction policyAction) mutable {
        m_delegateIsDecidingNavigationPolicy = false;

        switch (policyAction) {
        case PolicyAction::Download:
            m_frame.loader().setOriginalURLForDownloadRequest(request);
            m_frame.loader().client().startDownload(request, suggestedFilename);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            return function({ }, nullptr, ShouldContinue::No);
        case PolicyAction::Suspend:
            return function({ }, nullptr, ShouldContinue::ForSuspension);
        case PolicyAction::Use:
            if (!m_frame.loader().client().canHandleRequest(request)) {
                handleUnimplementablePolicy(m_frame.loader().client().cannotShowURLError(request));
                return function({ }, { }, ShouldContinue::No);
            }
            return function(WTFMove(request), makeWeakPtr(formState.get()), ShouldContinue::Yes);
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

    m_frame.loader().client().dispatchDecidePolicyForNewWindowAction(navigationAction, request, formState.get(), frameName, [frame = makeRef(m_frame), request, formState = WTFMove(formState), frameName, navigationAction, function = WTFMove(function), blobURLLifetimeExtension = WTFMove(blobURLLifetimeExtension)](PolicyAction policyAction) mutable {
        switch (policyAction) {
        case PolicyAction::Download:
            frame->loader().client().startDownload(request);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            function({ }, nullptr, { }, { }, ShouldContinue::No);
            return;
        case PolicyAction::Suspend:
            // It is invalid to get a "Suspend" policy for new windows, as the old document is not going away.
            RELEASE_ASSERT_NOT_REACHED();
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
