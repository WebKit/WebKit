/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentThreadableLoader.h"

#include "AuthenticationChallenge.h"
#include "Document.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"

namespace WebCore {

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, LoadCallbacks callbacksSetting, ContentSniff contentSniff)
{
    ASSERT(document);
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, request, callbacksSetting, contentSniff));
    if (!loader->m_loader)
        loader = 0;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, LoadCallbacks callbacksSetting, ContentSniff contentSniff)
    : m_client(client)
    , m_document(document)
{
    ASSERT(document);
    ASSERT(client);
    m_loader = SubresourceLoader::create(document->frame(), this, request, false, callbacksSetting == SendLoadCallbacks, contentSniff == SniffContent);
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
    if (m_loader)
        m_loader->clearClient();
}

void DocumentThreadableLoader::cancel()
{
    if (!m_loader)
        return;

    m_loader->cancel();
    m_loader->clearClient();
    m_loader = 0;
    m_client = 0;
}

void DocumentThreadableLoader::willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse&)
{
    ASSERT(m_client);

    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (!m_document->securityOrigin()->canRequest(request.url())) {
        RefPtr<DocumentThreadableLoader> protect(this);
        m_client->didFailRedirectCheck();
        cancel();
    }
}

void DocumentThreadableLoader::didSendData(SubresourceLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::didReceiveResponse(SubresourceLoader*, const ResourceResponse& response)
{
    ASSERT(m_client);
    m_client->didReceiveResponse(response);
}

void DocumentThreadableLoader::didReceiveData(SubresourceLoader*, const char* data, int lengthReceived)
{
    ASSERT(m_client);
    m_client->didReceiveData(data, lengthReceived);
}

void DocumentThreadableLoader::didFinishLoading(SubresourceLoader* loader)
{
    ASSERT(loader);
    ASSERT(m_client);
    m_client->didFinishLoading(loader->identifier());
}

void DocumentThreadableLoader::didFail(SubresourceLoader*, const ResourceError& error)
{
    ASSERT(m_client);
    m_client->didFail(error);
}

void DocumentThreadableLoader::receivedCancellation(SubresourceLoader*, const AuthenticationChallenge& challenge)
{
    ASSERT(m_client);
    m_client->didReceiveAuthenticationCancellation(challenge.failureResponse());
}

} // namespace WebCore
