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
#include "WebResourceLoadClient.h"

#include "WKAPICast.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"

using namespace WebCore;

namespace WebKit {

void WebResourceLoadClient::didInitiateLoadForResource(WebPageProxy* page, WebFrameProxy* frame, uint64_t resourceIdentifier, const ResourceRequest& resourceRequest, bool pageIsProvisionallyLoading)
{
    if (!m_client.didInitiateLoadForResource)
        return;

    RefPtr<WebURLRequest> request = WebURLRequest::create(resourceRequest);
    return m_client.didInitiateLoadForResource(toAPI(page), toAPI(frame), resourceIdentifier, toAPI(request.get()), pageIsProvisionallyLoading, m_client.clientInfo);
}

void WebResourceLoadClient::didSendRequestForResource(WebPageProxy* page, WebFrameProxy* frame, uint64_t resourceIdentifier, const ResourceRequest& resourceRequest, const ResourceResponse& redirectResourceResponse)
{
    if (!m_client.didSendRequestForResource)
        return;

    RefPtr<WebURLRequest> request = WebURLRequest::create(resourceRequest);
    RefPtr<WebURLResponse> response;
    if (!redirectResourceResponse.isNull())
        response = WebURLResponse::create(redirectResourceResponse);
    return m_client.didSendRequestForResource(toAPI(page), toAPI(frame), resourceIdentifier, toAPI(request.get()), toAPI(response.get()), m_client.clientInfo);
}

void WebResourceLoadClient::didReceiveResponseForResource(WebPageProxy* page, WebFrameProxy* frame, uint64_t resourceIdentifier, const ResourceResponse& resourceResponse)
{
    if (!m_client.didReceiveResponseForResource)
        return;

    RefPtr<WebURLResponse> response = WebURLResponse::create(resourceResponse);
    return m_client.didReceiveResponseForResource(toAPI(page), toAPI(frame), resourceIdentifier, toAPI(response.get()), m_client.clientInfo);
}

void WebResourceLoadClient::didReceiveContentLengthForResource(WebPageProxy* page, WebFrameProxy* frame, uint64_t resourceIdentifier, uint64_t contentLength)
{
    if (!m_client.didReceiveContentLengthForResource)
        return;

    return m_client.didReceiveContentLengthForResource(toAPI(page), toAPI(frame), resourceIdentifier, contentLength, m_client.clientInfo);
}

void WebResourceLoadClient::didFinishLoadForResource(WebPageProxy* page, WebFrameProxy* frame, uint64_t resourceIdentifier)
{
    if (!m_client.didFinishLoadForResource)
        return;

    return m_client.didFinishLoadForResource(toAPI(page), toAPI(frame), resourceIdentifier, m_client.clientInfo);
}

void WebResourceLoadClient::didFailLoadForResource(WebPageProxy* page, WebFrameProxy* frame, uint64_t resourceIdentifier, const ResourceError& error)
{
    if (!m_client.didFailLoadForResource)
        return;

    return m_client.didFailLoadForResource(toAPI(page), toAPI(frame), resourceIdentifier, toAPI(error), m_client.clientInfo);
}

} // namespace WebKit
