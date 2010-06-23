/*
 *  InjectedBundlePageClient.cpp
 *  WebKit2
 *
 *  Created by Sam Weinig on 6/16/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include "InjectedBundlePageClient.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include <WebCore/PlatformString.h>

using namespace WebCore;

namespace WebKit {

InjectedBundlePageClient::InjectedBundlePageClient()
{
    initialize(0);
}

void InjectedBundlePageClient::initialize(WKBundlePageClient* client)
{
    if (client && !client->version)
        m_client = *client;
    else 
        memset(&m_client, 0, sizeof(m_client));
}

void InjectedBundlePageClient::didStartProvisionalLoadForFrame(WebPage* page, WebFrame* frame)
{
    if (m_client.didStartProvisionalLoadForFrame)
        m_client.didStartProvisionalLoadForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebPage* page, WebFrame* frame)
{
    if (m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
        m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didFailProvisionalLoadWithErrorForFrame(WebPage* page, WebFrame* frame)
{
    if (m_client.didFailProvisionalLoadWithErrorForFrame)
        m_client.didFailProvisionalLoadWithErrorForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didCommitLoadForFrame(WebPage* page, WebFrame* frame)
{
    if (m_client.didCommitLoadForFrame)
        m_client.didCommitLoadForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didFinishLoadForFrame(WebPage* page, WebFrame* frame)
{
    if (m_client.didFinishLoadForFrame)
        m_client.didFinishLoadForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didFailLoadWithErrorForFrame(WebPage* page, WebFrame* frame)
{
    if (m_client.didFailLoadWithErrorForFrame)
        m_client.didFailLoadWithErrorForFrame(toRef(page), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didReceiveTitleForFrame(WebPage* page, const String& title, WebFrame* frame)
{
    if (m_client.didReceiveTitleForFrame)
        m_client.didReceiveTitleForFrame(toRef(page), toRef(title.impl()), toRef(frame), m_client.clientInfo);
}

void InjectedBundlePageClient::didClearWindowObjectForFrame(WebPage* page, WebFrame* frame, JSContextRef ctx, JSObjectRef window)
{
    if (m_client.didClearWindowObjectForFrame)
        m_client.didClearWindowObjectForFrame(toRef(page), toRef(frame), ctx, window, m_client.clientInfo);
}

} // namespace WebKit
