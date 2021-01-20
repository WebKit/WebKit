/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "InjectedBundlePageResourceLoadClient.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include "WebPage.h"

namespace WebKit {
using namespace WebCore;

InjectedBundlePageResourceLoadClient::InjectedBundlePageResourceLoadClient(const WKBundlePageResourceLoadClientBase* client)
{
    initialize(client);
}

void InjectedBundlePageResourceLoadClient::didInitiateLoadForResource(WebPage& page, WebFrame& frame, uint64_t identifier, const ResourceRequest& request, bool pageIsProvisionallyLoading)
{
    if (!m_client.didInitiateLoadForResource)
        return;

    m_client.didInitiateLoadForResource(toAPI(&page), toAPI(&frame), identifier, toAPI(request), pageIsProvisionallyLoading, m_client.base.clientInfo);
}

void InjectedBundlePageResourceLoadClient::willSendRequestForFrame(WebPage& page, WebFrame& frame, uint64_t identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!m_client.willSendRequestForFrame)
        return;

    RefPtr<API::URLRequest> returnedRequest = adoptRef(toImpl(m_client.willSendRequestForFrame(toAPI(&page), toAPI(&frame), identifier, toAPI(request), toAPI(redirectResponse), m_client.base.clientInfo)));
    if (returnedRequest) {
        // If the client returned an HTTP body, we want to use that http body. This is needed to fix <rdar://problem/23763584>
        auto& returnedResourceRequest = returnedRequest->resourceRequest();
        RefPtr<FormData> returnedHTTPBody = returnedResourceRequest.httpBody();
        request.updateFromDelegatePreservingOldProperties(returnedResourceRequest);
        if (returnedHTTPBody)
            request.setHTTPBody(WTFMove(returnedHTTPBody));
    } else
        request = { };
}

void InjectedBundlePageResourceLoadClient::didReceiveResponseForResource(WebPage& page, WebFrame& frame, uint64_t identifier, const ResourceResponse& response)
{
    if (!m_client.didReceiveResponseForResource)
        return;

    m_client.didReceiveResponseForResource(toAPI(&page), toAPI(&frame), identifier, toAPI(response), m_client.base.clientInfo);
}

void InjectedBundlePageResourceLoadClient::didReceiveContentLengthForResource(WebPage& page, WebFrame& frame, uint64_t identifier, uint64_t contentLength)
{
    if (!m_client.didReceiveContentLengthForResource)
        return;

    m_client.didReceiveContentLengthForResource(toAPI(&page), toAPI(&frame), identifier, contentLength, m_client.base.clientInfo);
}

void InjectedBundlePageResourceLoadClient::didFinishLoadForResource(WebPage& page, WebFrame& frame, uint64_t identifier)
{
    if (!m_client.didFinishLoadForResource)
        return;

    m_client.didFinishLoadForResource(toAPI(&page), toAPI(&frame), identifier, m_client.base.clientInfo);
}

void InjectedBundlePageResourceLoadClient::didFailLoadForResource(WebPage& page, WebFrame& frame, uint64_t identifier, const ResourceError& error)
{
    if (!m_client.didFailLoadForResource)
        return;

    m_client.didFailLoadForResource(toAPI(&page), toAPI(&frame), identifier, toAPI(error), m_client.base.clientInfo);
}

bool InjectedBundlePageResourceLoadClient::shouldCacheResponse(WebPage& page, WebFrame& frame, uint64_t identifier)
{
    if (!m_client.shouldCacheResponse)
        return true;

    return m_client.shouldCacheResponse(toAPI(&page), toAPI(&frame), identifier, m_client.base.clientInfo);
}

bool InjectedBundlePageResourceLoadClient::shouldUseCredentialStorage(WebPage& page, WebFrame& frame, uint64_t identifier)
{
    if (!m_client.shouldUseCredentialStorage)
        return true;

    return m_client.shouldUseCredentialStorage(toAPI(&page), toAPI(&frame), identifier, m_client.base.clientInfo);
}

} // namespace WebKit
