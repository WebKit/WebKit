/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "RequestManagerClientEfl.h"

#include "APICustomProtocolManagerClient.h"
#include "CustomProtocolManagerProxy.h"
#include "ewk_context_private.h"
#include "ewk_url_scheme_request_private.h"

namespace WebKit {

class CustomProtocolManagerClient final : public API::CustomProtocolManagerClient {
public:
    explicit CustomProtocolManagerClient(RequestManagerClientEfl* client)
        : m_client(client)
    {
    }

private:
    bool startLoading(CustomProtocolManagerProxy& manager, uint64_t customProtocolID, const WebCore::ResourceRequest& resourceRequest) override
    {
        auto urlRequest = API::URLRequest::create(resourceRequest);
        RefPtr<EwkUrlSchemeRequest> request = EwkUrlSchemeRequest::create(manager, urlRequest.ptr(), customProtocolID);
        String scheme(String::fromUTF8(request.get()->scheme()));
        RefPtr<WebKitURISchemeHandler> handler = (m_client->m_uriSchemeHandlers).get(scheme);
        ASSERT(handler.get());
        if (!handler->hasCallback())
            return true;

        (m_client->m_uriSchemeRequests).set(customProtocolID, request);
        handler->performCallback(request.get());
        return true;
    }

    void stopLoading(CustomProtocolManagerProxy&, uint64_t customProtocolID) override
    {
        (m_client->m_uriSchemeRequests).remove(customProtocolID);
    }

    void invalidate(CustomProtocolManagerProxy& manager) override
    {
        Vector<RefPtr<EwkUrlSchemeRequest>> requests;
        copyValuesToVector(m_client->m_uriSchemeRequests, requests);
        for (auto& request : requests) {
            if (request->manager() == &manager) {
                request->invalidate();
                stopLoading(manager, request->id());
            }
        }
    }

    RequestManagerClientEfl* m_client;
};

RequestManagerClientEfl::RequestManagerClientEfl(WKContextRef context)
{
    m_processPool = toImpl(context);
    m_processPool->setCustomProtocolManagerClient(std::make_unique<CustomProtocolManagerClient>(this));
}

void RequestManagerClientEfl::registerURLSchemeHandler(const String& scheme, Ewk_Url_Scheme_Request_Cb callback, void* userData)
{
    ASSERT(callback);

    RefPtr<WebKitURISchemeHandler> handler = adoptRef(new WebKitURISchemeHandler(callback, userData));
    auto addResult = m_uriSchemeHandlers.set(scheme, handler);
    if (addResult.isNewEntry)
        m_processPool->registerSchemeForCustomProtocol(scheme);
}

} // namespace WebKit
