/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "PageLoadClientEfl.h"

#include "EwkViewImpl.h"
#include "PageClientBase.h"
#include "WKAPICast.h"
#include "WKFrame.h"
#include "WKPage.h"
#include "ewk_auth_request_private.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_error_private.h"
#include "ewk_intent_private.h"
#include "ewk_intent_service_private.h"
#include "ewk_view.h"

using namespace EwkViewCallbacks;

namespace WebKit {

static inline PageLoadClientEfl* toPageLoadClientEfl(const void* clientInfo)
{
    return static_cast<PageLoadClientEfl*>(const_cast<void*>(clientInfo));
}

void PageLoadClientEfl::didReceiveTitleForFrame(WKPageRef, WKStringRef title, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    viewImpl->smartCallback<TitleChange>().call(toImpl(title)->string());
}

#if ENABLE(WEB_INTENTS)
void PageLoadClientEfl::didReceiveIntentForFrame(WKPageRef, WKFrameRef, WKIntentDataRef intent, WKTypeRef, const void* clientInfo)
{
    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    RefPtr<EwkIntent> ewkIntent = EwkIntent::create(intent);
    viewImpl->smartCallback<IntentRequest>().call(ewkIntent.get());
}
#endif

#if ENABLE(WEB_INTENTS_TAG)
void PageLoadClientEfl::registerIntentServiceForFrame(WKPageRef, WKFrameRef, WKIntentServiceInfoRef serviceInfo, WKTypeRef, const void* clientInfo)
{
    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    RefPtr<EwkIntentService> ewkIntentService = EwkIntentService::create(serviceInfo);
    viewImpl->smartCallback<IntentServiceRegistration>().call(ewkIntentService.get());
}
#endif

void PageLoadClientEfl::didChangeProgress(WKPageRef page, const void* clientInfo)
{
    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    double progress = WKPageGetEstimatedProgress(page);
    viewImpl->smartCallback<LoadProgress>().call(&progress);
}

void PageLoadClientEfl::didFinishLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef /*userData*/, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    viewImpl->smartCallback<LoadFinished>().call();
}

void PageLoadClientEfl::didFailLoadWithErrorForFrame(WKPageRef, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    OwnPtr<EwkError> ewkError = EwkError::create(error);
    viewImpl->smartCallback<LoadError>().call(ewkError.get());
    viewImpl->smartCallback<LoadFinished>().call();
}

void PageLoadClientEfl::didStartProvisionalLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef /*userData*/, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    viewImpl->informURLChange();
    viewImpl->smartCallback<ProvisionalLoadStarted>().call();
}

void PageLoadClientEfl::didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef /*userData*/, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    viewImpl->informURLChange();
    viewImpl->smartCallback<ProvisionalLoadRedirect>().call();
}

void PageLoadClientEfl::didFailProvisionalLoadWithErrorForFrame(WKPageRef, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    OwnPtr<EwkError> ewkError = EwkError::create(error);
    viewImpl->smartCallback<ProvisionalLoadFailed>().call(ewkError.get());
}

#if USE(TILED_BACKING_STORE)
void PageLoadClientEfl::didCommitLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    viewImpl->pageClient()->didCommitLoad();
}
#endif

void PageLoadClientEfl::didChangeBackForwardList(WKPageRef, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void* clientInfo)
{
    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    ASSERT(viewImpl);

    Ewk_Back_Forward_List* list = ewk_view_back_forward_list_get(viewImpl->view());
    ASSERT(list);
    list->update(addedItem, removedItems);

    viewImpl->smartCallback<BackForwardListChange>().call();
}

void PageLoadClientEfl::didSameDocumentNavigationForFrame(WKPageRef, WKFrameRef frame, WKSameDocumentNavigationType, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();
    viewImpl->informURLChange();
}

void PageLoadClientEfl::didReceiveAuthenticationChallengeInFrame(WKPageRef, WKFrameRef, WKAuthenticationChallengeRef authenticationChallenge, const void* clientInfo)
{
    EwkViewImpl* viewImpl = toPageLoadClientEfl(clientInfo)->viewImpl();

    RefPtr<EwkAuthRequest> authenticationRequest = EwkAuthRequest::create(toImpl(authenticationChallenge));
    viewImpl->smartCallback<AuthenticationRequest>().call(authenticationRequest.get());
}

PageLoadClientEfl::PageLoadClientEfl(EwkViewImpl* viewImpl)
    : m_viewImpl(viewImpl)
{
    WKPageRef pageRef = m_viewImpl->wkPage();
    ASSERT(pageRef);

    WKPageLoaderClient loadClient;
    memset(&loadClient, 0, sizeof(WKPageLoaderClient));
    loadClient.version = kWKPageLoaderClientCurrentVersion;
    loadClient.clientInfo = this;
    loadClient.didReceiveTitleForFrame = didReceiveTitleForFrame;
#if ENABLE(WEB_INTENTS)
    loadClient.didReceiveIntentForFrame = didReceiveIntentForFrame;
#endif
#if ENABLE(WEB_INTENTS_TAG)
    loadClient.registerIntentServiceForFrame = registerIntentServiceForFrame;
#endif
    loadClient.didStartProgress = didChangeProgress;
    loadClient.didChangeProgress = didChangeProgress;
    loadClient.didFinishProgress = didChangeProgress;
    loadClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loadClient.didFailLoadWithErrorForFrame = didFailLoadWithErrorForFrame;
    loadClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loadClient.didReceiveServerRedirectForProvisionalLoadForFrame = didReceiveServerRedirectForProvisionalLoadForFrame;
    loadClient.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrame;
#if USE(TILED_BACKING_STORE)
    loadClient.didCommitLoadForFrame = didCommitLoadForFrame;
#endif
    loadClient.didChangeBackForwardList = didChangeBackForwardList;
    loadClient.didSameDocumentNavigationForFrame = didSameDocumentNavigationForFrame;
    loadClient.didReceiveAuthenticationChallengeInFrame = didReceiveAuthenticationChallengeInFrame;
    WKPageSetPageLoaderClient(pageRef, &loadClient);
}

} // namespace WebKit
