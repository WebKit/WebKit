/*
 *  InjectedBundlePageClient.cpp
 *  WebKit2
 *
 *  Created by Sam Weinig on 6/16/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include "InjectedBundlePageClient.h"

#include "WKBundleAPICast.h"

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

void InjectedBundlePageClient::didClearWindowObjectForFrame(WebPage* page, WebFrame* frame, JSContextRef ctx, JSObjectRef window)
{
    if (m_client.didClearWindowObjectForFrame)
        m_client.didClearWindowObjectForFrame(toRef(page), toRef(frame), ctx, window, m_client.clientInfo);
}

} // namespace WebKit
