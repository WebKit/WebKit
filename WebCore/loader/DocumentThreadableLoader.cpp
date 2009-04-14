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
#include "DocumentThreadableLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document* document, const ResourceRequest& request, ThreadableLoaderClient& client, StoredCredentials storedCredentials)
{
    bool sameOriginRequest = document->securityOrigin()->canRequest(request.url());

    Vector<char> data;
    ResourceError error;
    ResourceResponse response;
    unsigned long identifier = std::numeric_limits<unsigned long>::max();
    if (document->frame())
        identifier = document->frame()->loader()->loadResourceSynchronously(request, storedCredentials, error, response, data);

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (!error.isNull() && !request.url().isLocalFile() && response.httpStatusCode() <= 0) {
        client.didFail(error);
        return;
    }

    // FIXME: This check along with the one in willSendRequest is specific to xhr and
    // should be made more generic.
    if (sameOriginRequest && !document->securityOrigin()->canRequest(response.url())) {
        client.didFailRedirectCheck();
        return;
    }

    client.didReceiveResponse(response);

    const char* bytes = static_cast<const char*>(data.data());
    int len = static_cast<int>(data.size());
    client.didReceiveData(bytes, len);

    client.didFinishLoading(identifier);
}

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, LoadCallbacks callbacksSetting, ContentSniff contentSniff, StoredCredentials storedCredentials)
{
    ASSERT(document);
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, request, callbacksSetting, contentSniff, storedCredentials));
    if (!loader->m_loader)
        loader = 0;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, LoadCallbacks callbacksSetting, ContentSniff contentSniff, StoredCredentials storedCredentials)
    : m_client(client)
    , m_document(document)
    , m_allowStoredCredentials(storedCredentials == AllowStoredCredentials)
    , m_sameOriginRequest(document->securityOrigin()->canRequest(request.url()))
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

void DocumentThreadableLoader::willSendRequest(SubresourceLoader* loader, ResourceRequest& request, const ResourceResponse&)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (!m_document->securityOrigin()->canRequest(request.url())) {
        RefPtr<DocumentThreadableLoader> protect(this);
        m_client->didFailRedirectCheck();
        request = ResourceRequest();
    }
}

void DocumentThreadableLoader::didSendData(SubresourceLoader* loader, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    m_client->didReceiveResponse(response);
}

void DocumentThreadableLoader::didReceiveData(SubresourceLoader* loader, const char* data, int lengthReceived)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    m_client->didReceiveData(data, lengthReceived);
}

void DocumentThreadableLoader::didFinishLoading(SubresourceLoader* loader)
{
    ASSERT(loader == m_loader);
    ASSERT(m_client);
    m_client->didFinishLoading(loader->identifier());
}

void DocumentThreadableLoader::didFail(SubresourceLoader* loader, const ResourceError& error)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);

    m_client->didFail(error);
}

bool DocumentThreadableLoader::getShouldUseCredentialStorage(SubresourceLoader* loader, bool& shouldUseCredentialStorage)
{
    ASSERT_UNUSED(loader, loader == m_loader);

    if (!m_allowStoredCredentials) {
        shouldUseCredentialStorage = false;
        return true;
    }

    return false; // Only FrameLoaderClient can ultimately permit credential use.
}

void DocumentThreadableLoader::didReceiveAuthenticationChallenge(SubresourceLoader* loader, const AuthenticationChallenge&)
{
    ASSERT(loader == m_loader);
    // Users are not prompted for credentials for cross-origin requests.
    if (!m_sameOriginRequest) {
        RefPtr<DocumentThreadableLoader> protect(this);
        m_client->didFail(loader->blockedError());
        cancel();
    }
}

void DocumentThreadableLoader::receivedCancellation(SubresourceLoader* loader, const AuthenticationChallenge& challenge)
{
    ASSERT(m_client);
    ASSERT_UNUSED(loader, loader == m_loader);
    m_client->didReceiveAuthenticationCancellation(challenge.failureResponse());
}

} // namespace WebCore
