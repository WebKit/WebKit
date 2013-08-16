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

void SynchronousNetworkLoaderClient::willSendRequest(NetworkResourceLoader* loader, ResourceRequest& proposedRequest, const ResourceResponse& redirectResponse)
{
    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    // This includes at least updating host records, and comparing the current request instead of the original request here.
    if (!protocolHostAndPortAreEqual(m_originalRequest.url(), proposedRequest.url())) {
        ASSERT(m_error.isNull());
        m_error = SynchronousLoaderClient::platformBadResponseError();
        proposedRequest = 0;
    }
    
    m_currentRequest = proposedRequest;
    loader->continueWillSendRequest(m_currentRequest);
}

void SynchronousNetworkLoaderClient::canAuthenticateAgainstProtectionSpace(NetworkResourceLoader* loader, const ProtectionSpace&)
{
    // FIXME: We should ask the WebProcess like the asynchronous case below does.
    // This is currently impossible as the WebProcess is blocked waiting on this synchronous load.
    // It's possible that we can jump straight to the UI process to resolve this.
    loader->continueCanAuthenticateAgainstProtectionSpace(true);
}

void SynchronousNetworkLoaderClient::didReceiveResponse(NetworkResourceLoader*, const ResourceResponse& response)
{
    m_response = response;
}

void SynchronousNetworkLoaderClient::didReceiveBuffer(NetworkResourceLoader*, SharedBuffer* buffer, int encodedDataLength)
{
    // FIXME: There's a potential performance improvement here by preallocating a SharedMemory region
    // of the expected content length to avoid a copy when we send it to the WebProcess on completion.
    // It's unclear if the potential complexities of that approach are worth it.
    
    if (!m_responseData)
        m_responseData = adoptPtr(new Vector<uint8_t>);

    m_responseData->append(buffer->data(), buffer->size());
}

void SynchronousNetworkLoaderClient::didFinishLoading(NetworkResourceLoader*, double /* finishTime */)
{
    sendDelayedReply();
}

void SynchronousNetworkLoaderClient::didFail(NetworkResourceLoader*, const ResourceError& error)
{
    m_error = error;
    sendDelayedReply();
}

void SynchronousNetworkLoaderClient::sendDelayedReply()
{
    ASSERT(m_delayedReply);

    uint8_t* bytes = m_responseData ? m_responseData->data() : 0;
    size_t size = m_responseData ? m_responseData->size() : 0;

    if (m_response.isNull()) {
        ASSERT(!m_error.isNull());
        //platformSynthesizeErrorResponse();
    }

    m_delayedReply->send(m_error, m_response, CoreIPC::DataReference(bytes, size));
    m_delayedReply = nullptr;
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
