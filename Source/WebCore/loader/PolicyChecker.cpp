/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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

void PolicyChecker::checkNavigationPolicy(ResourceRequest&& newRequest, bool didReceiveRedirectResponse, NavigationPolicyDecisionFunction&& function)
{
    checkNavigationPolicy(WTFMove(newRequest), didReceiveRedirectResponse, m_frame.loader().activeDocumentLoader(), nullptr, WTFMove(function));
}

void PolicyChecker::checkNavigationPolicy(ResourceRequest&& request, bool didReceiveRedirectResponse, DocumentLoader* loader, FormState* formState, NavigationPolicyDecisionFunction&& function)
{
    NavigationAction action = loader->triggeringAction();
    if (action.isEmpty()) {
        action = NavigationAction { *m_frame.document(), request, InitiatedByMainFrame::Unknown, NavigationType::Other, loader->shouldOpenExternalURLsPolicyToPropagate() };
        loader->setTriggeringAction(action);
    }

    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if (equalIgnoringHeaderFields(request, loader->lastCheckedRequest()) || (!request.isNull() && request.url().isEmpty())) {
        function(ResourceRequest(request), nullptr, true);
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
        function(WTFMove(request), nullptr, shouldContinue);
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url(), m_frame.ownerElement(), didReceiveRedirectResponse)) {
        if (m_frame.ownerElement()) {
            // Fire a load event (even though we were blocked by CSP) as timing attacks would otherwise
            // reveal that the frame was blocked. This way, it looks like any other cross-origin page load.
            m_frame.ownerElement()->dispatchEvent(Event::create(eventNames().loadEvent, false, false));
        }
        function(WTFMove(request), nullptr, false);
        return;
    }

    loader->setLastCheckedRequest(ResourceRequest(request));

    if (request.url() == blankURL())
        return function(WTFMove(request), formState, true);

#if USE(QUICK_LOOK)
    // Always allow QuickLook-generated URLs based on the protocol scheme.
    if (!request.isNull() && isQuickLookPreviewURL(request.url()))
        return function(WTFMove(request), formState, true);
#endif

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilterUnblockHandler.canHandleRequest(request)) {
        RefPtr<Frame> frame { &m_frame };
        m_contentFilterUnblockHandler.requestUnblockAsync([frame](bool unblocked) {
            if (unblocked)
                frame->loader().reload();
        });
        return function({ }, nullptr, false);
    }
    m_contentFilterUnblockHandler = { };
#endif

    m_delegateIsDecidingNavigationPolicy = true;
    String suggestedFilename = action.downloadAttribute().isEmpty() ? nullAtom() : action.downloadAttribute();
    ResourceRequest requestCopy = request;
    m_frame.loader().client().dispatchDecidePolicyForNavigationAction(action, request, didReceiveRedirectResponse, formState, [this, function = WTFMove(function), request = WTFMove(requestCopy), formState = makeRefPtr(formState), suggestedFilename = WTFMove(suggestedFilename)](PolicyAction policyAction) mutable {
        m_delegateIsDecidingNavigationPolicy = false;

        switch (policyAction) {
        case PolicyAction::Download:
            m_frame.loader().setOriginalURLForDownloadRequest(request);
            m_frame.loader().client().startDownload(request, suggestedFilename);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            return function({ }, nullptr, false);
        case PolicyAction::Use:
            if (!m_frame.loader().client().canHandleRequest(request)) {
                handleUnimplementablePolicy(m_frame.loader().client().cannotShowURLError(request));
                return function({ }, nullptr, false);
            }
            return function(WTFMove(request), formState.get(), true);
        }
        ASSERT_NOT_REACHED();
    });
}

void PolicyChecker::checkNewWindowPolicy(NavigationAction&& navigationAction, const ResourceRequest& request, FormState* formState, const String& frameName, NewWindowPolicyDecisionFunction&& function)
{
    if (m_frame.document() && m_frame.document()->isSandboxed(SandboxPopups))
        return function({ }, nullptr, { }, { }, false);

    if (!DOMWindow::allowPopUp(m_frame))
        return function({ }, nullptr, { }, { }, false);

    m_frame.loader().client().dispatchDecidePolicyForNewWindowAction(navigationAction, request, formState, frameName, [frame = makeRef(m_frame), request, formState = makeRefPtr(formState), frameName, navigationAction, function = WTFMove(function)](PolicyAction policyAction) mutable {
        switch (policyAction) {
        case PolicyAction::Download:
            frame->loader().client().startDownload(request);
            FALLTHROUGH;
        case PolicyAction::Ignore:
            function({ }, nullptr, { }, { }, false);
            return;
        case PolicyAction::Use:
            function(request, formState.get(), frameName, navigationAction, true);
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
