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

#include "config.h"
#include "InjectedBundleClient.h"

#include "InjectedBundle.h"
#include "WKBundleAPICast.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"

namespace WebKit {

InjectedBundleClient::InjectedBundleClient(const WKBundleClientBase* client)
{
    initialize(client);
}

void InjectedBundleClient::didCreatePage(InjectedBundle& bundle, WebPage& page)
{
    if (!m_client.didCreatePage)
        return;

    m_client.didCreatePage(toAPI(&bundle), toAPI(&page), m_client.base.clientInfo);
}

void InjectedBundleClient::willDestroyPage(InjectedBundle& bundle, WebPage& page)
{
    if (!m_client.willDestroyPage)
        return;

    m_client.willDestroyPage(toAPI(&bundle), toAPI(&page), m_client.base.clientInfo);
}

void InjectedBundleClient::didReceiveMessage(InjectedBundle& bundle, const String& messageName, API::Object* messageBody)
{
    if (!m_client.didReceiveMessage)
        return;

    m_client.didReceiveMessage(toAPI(&bundle), toAPI(messageName.impl()), toAPI(messageBody), m_client.base.clientInfo);
}

void InjectedBundleClient::didReceiveMessageToPage(InjectedBundle& bundle, WebPage& page, const String& messageName, API::Object* messageBody)
{
    if (!m_client.didReceiveMessageToPage)
        return;

    m_client.didReceiveMessageToPage(toAPI(&bundle), toAPI(&page), toAPI(messageName.impl()), toAPI(messageBody), m_client.base.clientInfo);
}

} // namespace WebKit
