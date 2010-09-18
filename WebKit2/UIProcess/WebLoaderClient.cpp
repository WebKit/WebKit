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

#include "WebLoaderClient.h"

#include "WKAPICast.h"
#include <string.h>

namespace WebKit {

WebLoaderClient::WebLoaderClient()
{
    initialize(0);
}

void WebLoaderClient::initialize(const WKPageLoaderClient* client)
{
    if (client && !client->version)
        m_pageLoaderClient = *client;
    else 
        memset(&m_pageLoaderClient, 0, sizeof(m_pageLoaderClient));
}

void WebLoaderClient::didStartProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didStartProvisionalLoadForFrame)
        m_pageLoaderClient.didStartProvisionalLoadForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didReceiveServerRedirectForProvisionalLoadForFrame)
        m_pageLoaderClient.didReceiveServerRedirectForProvisionalLoadForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didFailProvisionalLoadWithErrorForFrame)
        m_pageLoaderClient.didFailProvisionalLoadWithErrorForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didCommitLoadForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didCommitLoadForFrame)
        m_pageLoaderClient.didCommitLoadForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFinishDocumentLoadForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didFinishDocumentLoadForFrame)
        m_pageLoaderClient.didFinishDocumentLoadForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFinishLoadForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didFinishLoadForFrame)
        m_pageLoaderClient.didFinishLoadForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFailLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didFailLoadWithErrorForFrame)
        m_pageLoaderClient.didFailLoadWithErrorForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didReceiveTitleForFrame(WebPageProxy* page, StringImpl* title, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didReceiveTitleForFrame)
        m_pageLoaderClient.didReceiveTitleForFrame(toRef(page), toRef(title), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFirstLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didFirstLayoutForFrame)
        m_pageLoaderClient.didFirstLayoutForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame)
{
    if (m_pageLoaderClient.didFirstVisuallyNonEmptyLayoutForFrame)
        m_pageLoaderClient.didFirstVisuallyNonEmptyLayoutForFrame(toRef(page), toRef(frame), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didStartProgress(WebPageProxy* page)
{
    if (m_pageLoaderClient.didStartProgress)
        m_pageLoaderClient.didStartProgress(toRef(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didChangeProgress(WebPageProxy* page)
{
    if (m_pageLoaderClient.didChangeProgress)
        m_pageLoaderClient.didChangeProgress(toRef(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFinishProgress(WebPageProxy* page)
{
    if (m_pageLoaderClient.didFinishProgress)
        m_pageLoaderClient.didFinishProgress(toRef(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didBecomeUnresponsive(WebPageProxy* page)
{
    if (m_pageLoaderClient.didBecomeUnresponsive)
        m_pageLoaderClient.didBecomeUnresponsive(toRef(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didBecomeResponsive(WebPageProxy* page)
{
    if (m_pageLoaderClient.didBecomeResponsive)
        m_pageLoaderClient.didBecomeResponsive(toRef(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::processDidExit(WebPageProxy* page)
{
    if (m_pageLoaderClient.processDidExit)
        m_pageLoaderClient.processDidExit(toRef(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didChangeBackForwardList(WebPageProxy* page)
{
    if (m_pageLoaderClient.didChangeBackForwardList)
        m_pageLoaderClient.didChangeBackForwardList(toRef(page), m_pageLoaderClient.clientInfo);
}

} // namespace WebKit
