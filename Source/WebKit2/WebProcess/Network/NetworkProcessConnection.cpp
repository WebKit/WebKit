/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "NetworkProcessConnection.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebResourceBuffer.h"
#include <WebCore/ResourceBuffer.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

NetworkProcessConnection::NetworkProcessConnection(CoreIPC::Connection::Identifier connectionIdentifier)
{
    m_connection = CoreIPC::Connection::createClientConnection(connectionIdentifier, this, WebProcess::shared().runLoop());
    m_connection->open();
}

NetworkProcessConnection::~NetworkProcessConnection()
{
}

void NetworkProcessConnection::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (messageID.is<CoreIPC::MessageClassNetworkProcessConnection>()) {
        didReceiveNetworkProcessConnectionMessage(connection, messageID, decoder);
        return;
    }
    ASSERT_NOT_REACHED();
}

void NetworkProcessConnection::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, OwnPtr<CoreIPC::MessageEncoder>&)
{
    ASSERT_NOT_REACHED();
}

void NetworkProcessConnection::didClose(CoreIPC::Connection*)
{
    // The NetworkProcess probably crashed.
    WebProcess::shared().networkProcessConnectionClosed(this);
}

void NetworkProcessConnection::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
}

void NetworkProcessConnection::willSendRequest(uint64_t requestID, ResourceLoadIdentifier resourceLoadIdentifier, const ResourceRequest& proposedRequest, const ResourceResponse& redirectResponse)
{
    ResourceRequest newRequest = proposedRequest;
    WebProcess::shared().webResourceLoadScheduler().willSendRequest(resourceLoadIdentifier, newRequest, redirectResponse);

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::WillSendRequestHandled(requestID, newRequest), 0);
}

void NetworkProcessConnection::didReceiveResponse(ResourceLoadIdentifier resourceLoadIdentifier, const ResourceResponse& response)
{
    WebProcess::shared().webResourceLoadScheduler().didReceiveResponse(resourceLoadIdentifier, response);
}

void NetworkProcessConnection::didReceiveResource(ResourceLoadIdentifier resourceLoadIdentifier, const ShareableResource::Handle& handle, double finishTime)
{
    RefPtr<ResourceBuffer> resourceBuffer;

    RefPtr<ShareableResource> resource = ShareableResource::create(handle);
    if (resource)
        resourceBuffer = WebResourceBuffer::create(resource.release());
    else
        resourceBuffer = ResourceBuffer::create();
    
    WebProcess::shared().webResourceLoadScheduler().didReceiveResource(resourceLoadIdentifier, *resourceBuffer, finishTime);
}

void NetworkProcessConnection::didFailResourceLoad(ResourceLoadIdentifier resourceLoadIdentifier, const ResourceError& error)
{
    WebProcess::shared().webResourceLoadScheduler().didFailResourceLoad(resourceLoadIdentifier, error);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
