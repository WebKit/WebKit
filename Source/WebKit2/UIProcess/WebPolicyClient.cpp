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
#include "WebPolicyClient.h"

#include "APIObject.h"
#include "APIURLRequest.h"
#include "WKAPICast.h"
#include "WebFramePolicyListenerProxy.h"

using namespace WebCore;

namespace WebKit {

WebPolicyClient::WebPolicyClient(const WKPagePolicyClientBase* client)
{
    initialize(client);
}

void WebPolicyClient::decidePolicyForNavigationAction(WebPageProxy* page, WebFrameProxy* frame, WebCore::NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, WebFrameProxy* originatingFrame, const WebCore::ResourceRequest& originalResourceRequest, const WebCore::ResourceRequest& resourceRequest, WebFramePolicyListenerProxy* listener, API::Object* userData)
{
    if (!m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0 && !m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1 && !m_client.decidePolicyForNavigationAction) {
        listener->use();
        return;
    }

    RefPtr<API::URLRequest> originalRequest = API::URLRequest::create(originalResourceRequest);
    RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

    if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0)
        m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0(toAPI(page), toAPI(frame), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
    else if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1)
        m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1(toAPI(page), toAPI(frame), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toAPI(originatingFrame), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
    else
        m_client.decidePolicyForNavigationAction(toAPI(page), toAPI(frame), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toAPI(originatingFrame), toAPI(originalRequest.get()), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
}

void WebPolicyClient::decidePolicyForNewWindowAction(WebPageProxy* page, WebFrameProxy* frame, NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, const ResourceRequest& resourceRequest, const String& frameName, WebFramePolicyListenerProxy* listener, API::Object* userData)
{
    if (!m_client.decidePolicyForNewWindowAction) {
        listener->use();
        return;
    }

    RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

    m_client.decidePolicyForNewWindowAction(toAPI(page), toAPI(frame), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toAPI(request.get()), toAPI(frameName.impl()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
}

void WebPolicyClient::decidePolicyForResponse(WebPageProxy* page, WebFrameProxy* frame, const ResourceResponse& resourceResponse, const ResourceRequest& resourceRequest, bool canShowMIMEType, WebFramePolicyListenerProxy* listener, API::Object* userData)
{
    if (m_client.decidePolicyForResponse_deprecatedForUseWithV0 && !m_client.decidePolicyForResponse) {
        listener->use();
        return;
    }

    RefPtr<API::URLResponse> response = API::URLResponse::create(resourceResponse);
    RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

    if (m_client.decidePolicyForResponse_deprecatedForUseWithV0)
        m_client.decidePolicyForResponse_deprecatedForUseWithV0(toAPI(page), toAPI(frame), toAPI(response.get()), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
    else
        m_client.decidePolicyForResponse(toAPI(page), toAPI(frame), toAPI(response.get()), toAPI(request.get()), canShowMIMEType, toAPI(listener), toAPI(userData), m_client.base.clientInfo);
}

void WebPolicyClient::unableToImplementPolicy(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, API::Object* userData)
{
    if (!m_client.unableToImplementPolicy)
        return;

    m_client.unableToImplementPolicy(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
}

} // namespace WebKit
