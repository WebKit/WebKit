/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SynchronousNetworkLoaderClient.h"

#if ENABLE(NETWORK_PROCESS)

#include "DataReference.h"
#include "NetworkResourceLoader.h"
#include "WebErrors.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SynchronousLoaderClient.h>

using namespace WebCore;

namespace WebKit {

SynchronousNetworkLoaderClient::SynchronousNetworkLoaderClient(const ResourceRequest& request, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> reply)
    : m_originalRequest(request)
    , m_delayedReply(reply)
{
    ASSERT(m_delayedReply);
}

SynchronousNetworkLoaderClient::~SynchronousNetworkLoaderClient()
{
    // By the time a SynchronousNetworkLoaderClient is being destroyed, it must always have sent its reply to the WebProcess.
    ASSERT(!m_delayedReply);
}

void SynchronousNetworkLoaderClient::willSendRequest(NetworkResourceLoader* loader, ResourceRequest& proposedRequest, const ResourceResponse& /* redirectResponse */)
{
    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    // This includes at least updating host records, and comparing the current request instead of the original request here.
    if (!protocolHostAndPortAreEqual(m_originalRequest.url(), proposedRequest.url())) {
        ASSERT(m_error.isNull());
        m_error = SynchronousLoaderClient::platformBadResponseError();
        proposedRequest = ResourceRequest();
    }
    
    m_currentRequest = proposedRequest;
    loader->continueWillSendRequest(m_currentRequest);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void SynchronousNetworkLoaderClient::canAuthenticateAgainstProtectionSpace(NetworkResourceLoader* loader, const ProtectionSpace&)
{
    // FIXME: We should ask the WebProcess like the asynchronous case below does.
    // This is currently impossible as the WebProcess is blocked waiting on this synchronous load.
    // It's possible that we can jump straight to the UI process to resolve this.
    loader->continueCanAuthenticateAgainstProtectionSpace(true);
}
#endif

void SynchronousNetworkLoaderClient::didReceiveResponse(NetworkResourceLoader*, const ResourceResponse& response)
{
    m_response = response;
}

void SynchronousNetworkLoaderClient::didReceiveBuffer(NetworkResourceLoader*, SharedBuffer*, int /* encodedDataLength */)
{
    // Nothing to do here. Data is buffered in NetworkResourceLoader and we will grab it from there
    // in sendDelayedReply().
}

void SynchronousNetworkLoaderClient::didFinishLoading(NetworkResourceLoader* loader, double /* finishTime */)
{
    sendDelayedReply(*loader);
}

void SynchronousNetworkLoaderClient::didFail(NetworkResourceLoader* loader, const ResourceError& error)
{
    m_error = error;
    sendDelayedReply(*loader);
}

void SynchronousNetworkLoaderClient::sendDelayedReply(NetworkResourceLoader& loader)
{
    ASSERT(m_delayedReply);

    if (m_response.isNull()) {
        ASSERT(!m_error.isNull());
        //platformSynthesizeErrorResponse();
    }

    Vector<char> responseData;
    SharedBuffer* buffer = loader.bufferedData();
    if (buffer && buffer->size())
        responseData.append(buffer->data(), buffer->size());

    m_delayedReply->send(m_error, m_response, responseData);
    m_delayedReply = nullptr;
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
