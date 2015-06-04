/*
 * Copyright (C) 2015 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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
#include "ewk_page.h"

#include "WKBundle.h"
#include "WKBundleAPICast.h"
#include "WKBundleFrame.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "ewk_page_private.h"

using namespace WebKit;

static inline EwkPage* toEwkPage(const void* clientInfo)
{
    return const_cast<EwkPage*>(static_cast<const EwkPage*>(clientInfo));
}

EwkPage::EwkPage(WebPage* page)
    : m_page(page)
{
    WKBundlePageLoaderClientV7 loaderClient = {
        {
            7, // version
            this, // clientInfo
        },
        didStartProvisionalLoadForFrame,
        0, // didReceiveServerRedirectForProvisionalLoadForFrame,
        0, // didFailProvisionalLoadWithErrorForFrame
        0, // didCommitLoadForFrame
        didFinishDocumentLoadForFrame,
        0, // didFinishLoadForFrame
        0, // didFailLoadWithErrorForFrame
        0, // didSameDocumentNavigationForFrame,
        0, // didReceiveTitleForFrame
        0, // didFirstLayoutForFrame
        0, // didFirstVisuallyNonEmptyLayoutForFrame
        0, // didRemoveFrameFromHierarchy
        0, // didDisplayInsecureContentForFrame
        0, // didRunInsecureContentForFrame
        didClearWindowObjectForFrame,
        0, // didCancelClientRedirectForFrame
        0, // willPerformClientRedirectForFrame
        0, // didHandleOnloadEventsForFrame
        0, // didLayoutForFrame
        0, // didNewFirstVisuallyNonEmptyLayout
        0, // didDetectXSSForFrame
        0, // shouldGoToBackForwardListItem
        0, // globalObjectIsAvailableForFrame
        0, // willDisconnectDOMWindowExtensionFromGlobalObject
        0, // didReconnectDOMWindowExtensionToGlobalObject
        0, // willDestroyGlobalObjectForDOMWindowExtension
        0, // didFinishProgress
        0, // shouldForceUniversalAccessFromLocalURL
        0, // didReceiveIntentForFrame_unavailable
        0, // registerIntentServiceForFrame_unavailable
        0, // didLayout
        0, // featuresUsedInPage
        0, // willLoadURLRequest
        0, // willLoadDataRequest
        0 // willDestroyFrame
    };
    WKBundlePageSetPageLoaderClient(toAPI(page), &loaderClient.base);
}

void EwkPage::append(const Ewk_Page_Client* client)
{
    m_clients.append(client);
}

void EwkPage::remove(const Ewk_Page_Client* client)
{
    m_clients.remove(m_clients.find(client));
}

void EwkPage::didStartProvisionalLoadForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    EwkPage* self = toEwkPage(clientInfo);
    for (auto& it : self->m_clients) {
        if (it->load_started)
            it->load_started(self, it->data);
    }
}

void EwkPage::didClearWindowObjectForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKBundleScriptWorldRef, const void* clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    EwkPage* self = toEwkPage(clientInfo);
    for (auto& it : self->m_clients) {
        if (it->window_object_cleared)
            it->window_object_cleared(self, it->data);
    }
}

void EwkPage::didFinishDocumentLoadForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    EwkPage* self = toEwkPage(clientInfo);
    for (auto& it : self->m_clients) {
        if (it->load_finished)
            it->load_finished(self, it->data);
    }
}

JSGlobalContextRef ewk_page_js_global_context_get(const Ewk_Page* ewkPage)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkPage, nullptr);

    WebFrame* coreFrame = ewkPage->page()->mainWebFrame();
    if (!coreFrame)
        return nullptr;

    return coreFrame->jsContext();
}

void ewk_page_client_register(Ewk_Page* ewkPage, const Ewk_Page_Client* client)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkPage);
    EINA_SAFETY_ON_NULL_RETURN(client);

    ewkPage->append(client);
}

void ewk_page_client_unregister(Ewk_Page* ewkPage, const Ewk_Page_Client* client)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkPage);
    EINA_SAFETY_ON_NULL_RETURN(client);

    ewkPage->remove(client);
}
