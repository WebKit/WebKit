/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RiceBackendProxy.h"

#if USE(LIBRICE)

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "RiceBackendMessages.h"

#include <WebCore/ExceptionOr.h>
#include <WebCore/RiceUtilities.h>

namespace WebKit {
using namespace WebCore;

Ref<RiceBackendProxy> RiceBackendProxy::create(WebPageProxyIdentifier webPageProxyID, WebCore::RiceBackendClient& client)
{
    Ref connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    auto sendResult = connection->sendSync(Messages::NetworkConnectionToWebProcess::InitializeRiceBackend(webPageProxyID), 0);
    auto [identifier] = sendResult.takeReply();
    return adoptRef(*new RiceBackendProxy(WTFMove(connection), webPageProxyID, *identifier, client));
}

RiceBackendProxy::RiceBackendProxy(Ref<IPC::Connection>&& connection, WebPageProxyIdentifier webPageProxyID, RiceBackendIdentifier identifier, WebCore::RiceBackendClient& client)
    : RiceBackend()
    , m_connection(WTFMove(connection))
    , m_webPageProxyID(webPageProxyID)
    , m_client(&client)
    , m_identifier(identifier)
{
    ASSERT(RunLoop::isMain());
    WebProcess::singleton().addRiceBackend(m_identifier, *this);
}

RiceBackendProxy::~RiceBackendProxy()
{
    WebProcess::singleton().removeRiceBackend(m_identifier);
    m_connection->send(Messages::NetworkConnectionToWebProcess::DestroyRiceBackend(m_identifier), 0);
}

IPC::Connection* RiceBackendProxy::messageSenderConnection() const
{
    return m_connection.ptr();
}

uint64_t RiceBackendProxy::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

void RiceBackendProxy::resolveAddress(const String& address, RiceBackend::ResolveAddressCallback&& callback)
{
    m_connection->sendWithAsyncReply(Messages::RiceBackend::ResolveAddress { address }, [callback = WTFMove(callback)](auto&& valueOrException) mutable {
        if (!valueOrException.has_value()) {
            callback(valueOrException.error().toException());
            return;
        }
        callback(WTFMove(*valueOrException));
    }, messageSenderDestinationID());
}

Vector<String> RiceBackendProxy::gatherSocketAddresses(unsigned streamId)
{
    Vector<String> addresses;
    callOnMainRunLoopAndWait([&] {
        auto sendResult = m_connection->sendSync(Messages::RiceBackend::GatherSocketAddresses { streamId }, messageSenderDestinationID());
        auto [reply] = sendResult.takeReply();
        addresses = reply;
    });
    return addresses;
}

void RiceBackendProxy::notifyIncomingData(unsigned streamId, RTCIceProtocol protocol, String&& from, String&& to, WebCore::SharedMemory::Handle&& data)
{
    m_client->notifyIncomingData(streamId, protocol, WTFMove(from), WTFMove(to), WTFMove(data));
}

void RiceBackendProxy::send(unsigned streamId, RTCIceProtocol protocol, String&& from, String&& to, WebCore::SharedMemory::Handle&& data)
{
    MessageSender::send(Messages::RiceBackend::SendData { streamId, protocol, WTFMove(from), WTFMove(to), WTFMove(data) });
}

void RiceBackendProxy::finalizeStream(unsigned streamId)
{
    MessageSender::send(Messages::RiceBackend::FinalizeStream { streamId });
}

} // namespace WebKit

#endif // USE(LIBRICE)
