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

#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "RiceBackendMessages.h"

#include <WebCore/ExceptionOr.h>
#include <WebCore/RiceUtilities.h>

namespace WebKit {
using namespace WebCore;

RefPtr<RiceBackendProxy> RiceBackendProxy::create(WebPageProxyIdentifier webPageProxyID, WebCore::RiceBackendClient& client)
{
    Ref connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    auto sendResult = connection->sendSync(Messages::NetworkConnectionToWebProcess::InitializeRiceBackend(webPageProxyID), 0);
    if (!sendResult.succeeded())
        return nullptr;

    auto [identifier] = sendResult.takeReply();
    if (!identifier)
        return nullptr;

    return adoptRef(*new RiceBackendProxy(WTF::move(connection), webPageProxyID, *identifier, client));
}

RiceBackendProxy::RiceBackendProxy(Ref<IPC::Connection>&& connection, WebPageProxyIdentifier webPageProxyID, RiceBackendIdentifier identifier, WebCore::RiceBackendClient& client)
    : RiceBackend()
    , m_connection(WTF::move(connection))
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
    m_connection->sendWithAsyncReply(Messages::RiceBackend::ResolveAddress { address }, [callback = WTF::move(callback)](auto&& valueOrException) mutable {
        if (!valueOrException.has_value()) {
            callback(valueOrException.error().toException());
            return;
        }
        callback(WTF::move(*valueOrException));
    }, messageSenderDestinationID());
}

WebCore::ExceptionOr<String> RiceBackendProxy::resolveAddressSync(const String& address)
{
    std::optional<WebCore::ExceptionOr<String>> result;
    callOnMainRunLoopAndWait([&] mutable {
        auto sendResult = m_connection->sendSync(Messages::RiceBackend::ResolveAddressSync { address }, messageSenderDestinationID(), 3_s);
        if (!sendResult.succeeded()) {
            result = WebCore::Exception { ExceptionCode::NetworkError, makeString("Unable to resolve address for "_s, address, ": "_s, sendResult.error()) };
            return;
        }
        auto [reply] = sendResult.takeReply();
        if (!reply) {
            result = reply.error().toException();
            return;
        }
        result = reply.value().isolatedCopy();
    });
    return *result;
}

WebCore::RiceGatherResult RiceBackendProxy::gatherSocketAddresses(WebCore::ScriptExecutionContextIdentifier identifier, unsigned streamId, unsigned minRtpPort, unsigned maxRtpPort)
{
    WebCore::RiceGatherResult result;
    callOnMainRunLoopAndWait([&] {
        auto sendResult = m_connection->sendSync(Messages::RiceBackend::GatherSocketAddresses { identifier, streamId, minRtpPort, maxRtpPort }, messageSenderDestinationID(), 3_s);
        if (!sendResult.succeeded())
            return;

        auto [reply] = sendResult.takeReply();
        result = reply;
    });
    return result;
}

void RiceBackendProxy::notifyIncomingData(unsigned streamId, RTCIceProtocol protocol, String&& from, String&& to, WebCore::SharedMemory::Handle&& data)
{
    m_client->notifyIncomingData(streamId, protocol, WTF::move(from), WTF::move(to), WTF::move(data));
}

void RiceBackendProxy::send(unsigned streamId, RTCIceProtocol protocol, String&& from, String&& to, WebCore::SharedMemory::Handle&& data)
{
    MessageSender::send(Messages::RiceBackend::SendData { streamId, protocol, WTF::move(from), WTF::move(to), WTF::move(data) });
}

void RiceBackendProxy::finalizeStream(unsigned streamId)
{
    MessageSender::send(Messages::RiceBackend::FinalizeStream { streamId });
}

void RiceBackendProxy::setSocketTypeOfService(unsigned streamId, unsigned value)
{
    MessageSender::send(Messages::RiceBackend::SetSocketTypeOfService { streamId, value });
}

void RiceBackendProxy::allocateSocket(unsigned streamId, unsigned componentId, WebCore::RTCIceProtocol protocol, String&& from, String&& to)
{
    MessageSender::send(Messages::RiceBackend::AllocateSocket { streamId, componentId, protocol, WTF::move(from), WTF::move(to) });
}

void RiceBackendProxy::allocatedSocket(unsigned streamId, unsigned componentId, WebCore::RTCIceProtocol protocol, String&& from, String&& to, String&& socket)
{
    m_client->allocatedSocket(streamId, componentId, protocol, WTF::move(from), WTF::move(to), WTF::move(socket));
}

void RiceBackendProxy::removeSocket(unsigned streamId, unsigned componentId, WebCore::RTCIceProtocol protocol, String&& from, String&& to)
{
    MessageSender::send(Messages::RiceBackend::RemoveSocket { streamId, componentId, protocol, WTF::move(from), WTF::move(to) });
}

} // namespace WebKit

#endif // USE(LIBRICE)
