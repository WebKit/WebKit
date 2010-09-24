/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "InjectedBundlePageLoaderClient.h"

#include "InjectedBundleScriptWorld.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

InjectedBundlePageLoaderClient::InjectedBundlePageLoaderClient()
{
    initialize(0);
}

void InjectedBundlePageLoaderClient::initialize(WKBundlePageLoaderClient* client)
{
    if (client && !client->version)
        m_client = *client;
    else 
        memset(&m_client, 0, sizeof(m_client));
}

void InjectedBundlePageLoaderClient::didStartProvisionalLoadForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didStartProvisionalLoadForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didStartProvisionalLoadForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didFailProvisionalLoadWithErrorForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didFailProvisionalLoadWithErrorForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didCommitLoadForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didCommitLoadForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didCommitLoadForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFinishDocumentLoadForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didFinishDocumentLoadForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didFinishDocumentLoadForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFinishLoadForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didFinishLoadForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didFinishLoadForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFailLoadWithErrorForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didFailLoadWithErrorForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didFailLoadWithErrorForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didReceiveTitleForFrame(WebPage* page, const String& title, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didReceiveTitleForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didReceiveTitleForFrame(toRef(page), toRef(title.impl()), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFirstLayoutForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didFirstLayoutForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didFirstLayoutForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFirstVisuallyNonEmptyLayoutForFrame(WebPage* page, WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didFirstVisuallyNonEmptyLayoutForFrame)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didFirstVisuallyNonEmptyLayoutForFrame(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didRemoveFrameFromHierarchy(WebPage* page , WebFrame* frame, RefPtr<APIObject>& userData)
{
    if (!m_client.didRemoveFrameFromHierarchy)
        return;

    WKTypeRef userDataToPass = 0;
    m_client.didRemoveFrameFromHierarchy(toRef(page), toRef(frame), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toWK(userDataToPass));
}

void InjectedBundlePageLoaderClient::didClearWindowObjectForFrame(WebPage* page, WebFrame* frame, DOMWrapperWorld* world)
{
    if (!m_client.didClearWindowObjectForFrame)
        return;

    m_client.didClearWindowObjectForFrame(toRef(page), toRef(frame), toRef(InjectedBundleScriptWorld::getOrCreate(world).get()), m_client.clientInfo);
}

void InjectedBundlePageLoaderClient::didCancelClientRedirectForFrame(WebPage* page, WebFrame* frame)
{
    if (!m_client.didCancelClientRedirectForFrame)
        return;

    m_client.didCancelClientRedirectForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageLoaderClient::willPerformClientRedirectForFrame(WebPage* page, WebFrame* frame, const String& url, double delay, double date)
{
    if (!m_client.willPerformClientRedirectForFrame)
        return;

    m_client.willPerformClientRedirectForFrame(toRef(page), toRef(frame), toURLRef(url.impl()), delay, date, m_client.clientInfo);
}

void InjectedBundlePageLoaderClient::didChangeLocationWithinPageForFrame(WebPage* page, WebFrame* frame)
{
    if (!m_client.didChangeLocationWithinPageForFrame)
        return;

    m_client.didChangeLocationWithinPageForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageLoaderClient::didHandleOnloadEventsForFrame(WebPage* page, WebFrame* frame)
{
    if (!m_client.didHandleOnloadEventsForFrame)
        return;

    m_client.didHandleOnloadEventsForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageLoaderClient::didDisplayInsecureContentForFrame(WebPage* page, WebFrame* frame)
{
    if (!m_client.didDisplayInsecureContentForFrame)
        return;

    m_client.didDisplayInsecureContentForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageLoaderClient::didRunInsecureContentForFrame(WebPage* page, WebFrame* frame)
{
    if (!m_client.didRunInsecureContentForFrame)
        return;

    m_client.didRunInsecureContentForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

} // namespace WebKit
