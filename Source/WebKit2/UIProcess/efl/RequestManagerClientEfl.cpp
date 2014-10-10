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

#include "ewk_context_private.h"
#include "ewk_url_scheme_request_private.h"

namespace WebKit {

static inline RequestManagerClientEfl* toRequestManagerClientEfl(const void* clientInfo)
{
    return static_cast<RequestManagerClientEfl*>(const_cast<void*>(clientInfo));
}

RequestManagerClientEfl::RequestManagerClientEfl(WKContextRef context)
{
    m_requestManager = toAPI(toImpl(context)->supplement<WebSoupCustomProtocolRequestManager>());
    ASSERT(m_requestManager);

    WKSoupCustomProtocolRequestManagerClientV0 wkRequestManagerClient;
    memset(&wkRequestManagerClient, 0, sizeof(WKSoupCustomProtocolRequestManagerClientV0));

    wkRequestManagerClient.base.version = 0;
    wkRequestManagerClient.base.clientInfo = this;
    wkRequestManagerClient.startLoading = startLoading;
    wkRequestManagerClient.stopLoading = stopLoading;

    WKSoupCustomProtocolRequestManagerSetClient(m_requestManager.get(), &wkRequestManagerClient.base);
}

void RequestManagerClientEfl::startLoading(WKSoupCustomProtocolRequestManagerRef manager, uint64_t customProtocolID, WKURLRequestRef requestRef, const void* clientInfo)
{
    RequestManagerClientEfl* client = toRequestManagerClientEfl(clientInfo);
    RefPtr<EwkUrlSchemeRequest> request = EwkUrlSchemeRequest::create(manager, toImpl(requestRef), customProtocolID);
    String scheme(String::fromUTF8(request.get()->scheme()));
    RefPtr<WebKitURISchemeHandler> handler = (client->m_uriSchemeHandlers).get(scheme);
    ASSERT(handler.get());
    if (!handler->hasCallback())
        return;

    (client->m_uriSchemeRequests).set(customProtocolID, request);
    handler->performCallback(request.get());
}

void RequestManagerClientEfl::stopLoading(WKSoupCustomProtocolRequestManagerRef manager, uint64_t customProtocolID, const void* clientInfo)
{
    UNUSED_PARAM(manager);

    RequestManagerClientEfl* client = toRequestManagerClientEfl(clientInfo);
    (client->m_uriSchemeRequests).remove(customProtocolID);
}

void RequestManagerClientEfl::registerURLSchemeHandler(const String& scheme, Ewk_Url_Scheme_Request_Cb callback, void* userData)
{
    ASSERT(callback);

    RefPtr<WebKitURISchemeHandler> handler = adoptRef(new WebKitURISchemeHandler(callback, userData));
    m_uriSchemeHandlers.set(scheme, handler);
    toImpl(m_requestManager.get())->registerSchemeForCustomProtocol(scheme);
}

} // namespace WebKit
