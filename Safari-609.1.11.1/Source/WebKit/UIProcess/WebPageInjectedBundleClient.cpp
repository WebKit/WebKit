/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebPageInjectedBundleClient.h"

#include "APIMessageListener.h"
#include "WKAPICast.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void WebPageInjectedBundleClient::didReceiveMessageFromInjectedBundle(WebPageProxy* page, const String& messageName, API::Object* messageBody)
{
    if (!m_client.didReceiveMessageFromInjectedBundle)
        return;

    m_client.didReceiveMessageFromInjectedBundle(toAPI(page), toAPI(messageName.impl()), toAPI(messageBody), m_client.base.clientInfo);
}

void WebPageInjectedBundleClient::didReceiveSynchronousMessageFromInjectedBundle(WebPageProxy* page, const String& messageName, API::Object* messageBody, CompletionHandler<void(RefPtr<API::Object>)>&& completionHandler)
{
    if (!m_client.didReceiveSynchronousMessageFromInjectedBundle
        && !m_client.didReceiveSynchronousMessageFromInjectedBundleWithListener)
        return completionHandler(nullptr);

    if (m_client.didReceiveSynchronousMessageFromInjectedBundle) {
        WKTypeRef returnDataRef = nullptr;
        m_client.didReceiveSynchronousMessageFromInjectedBundle(toAPI(page), toAPI(messageName.impl()), toAPI(messageBody), &returnDataRef, m_client.base.clientInfo);
        return completionHandler(adoptRef(toImpl(returnDataRef)));
    }

    m_client.didReceiveSynchronousMessageFromInjectedBundleWithListener(toAPI(page), toAPI(messageName.impl()), toAPI(messageBody), toAPI(API::MessageListener::create(WTFMove(completionHandler)).ptr()), m_client.base.clientInfo);
}

} // namespace WebKit
