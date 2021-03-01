/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkRTCSocketCocoa.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "DataReference.h"
#include "LibWebRTCNetworkMessages.h"
#include <WebCore/STUNMessageParsing.h>
#include <dispatch/dispatch.h>
#include <wtf/BlockPtr.h>

namespace WebKit {

using namespace WebCore;

static dispatch_queue_t socketQueue()
{
    static dispatch_queue_t queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("WebRTC socket queue", DISPATCH_QUEUE_CONCURRENT);
    });
    return queue;
}

std::unique_ptr<NetworkRTCProvider::Socket> NetworkRTCSocketCocoa::createClientTCPSocket(LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& remoteAddress, int tcpOptions, Ref<IPC::Connection>&& connection)
{
    // FIXME: We should migrate ssltcp candidates, maybe support OPT_TLS_INSECURE as well.
    if ((tcpOptions & rtc::PacketSocketFactory::OPT_TLS_FAKE) || (tcpOptions & rtc::PacketSocketFactory::OPT_TLS_INSECURE))
        return nullptr;
    return makeUnique<NetworkRTCSocketCocoa>(identifier, rtcProvider, remoteAddress, tcpOptions, WTFMove(connection));
}

static inline void processIncomingData(RetainPtr<nw_connection_t>&& nwConnection, Function<Vector<uint8_t>(Vector<uint8_t>&&)>&& processData, Vector<uint8_t>&& buffer = { })
{
    auto nwConnectionReference = nwConnection.get();
    nw_connection_receive(nwConnectionReference, 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([nwConnection = WTFMove(nwConnection), processData = WTFMove(processData), buffer = WTFMove(buffer)](dispatch_data_t content, nw_content_context_t context, bool isComplete, nw_error_t error) mutable {
        if (content) {
            dispatch_data_apply(content, makeBlockPtr([&](dispatch_data_t, size_t, const void* data, size_t size) {
                // FIXME: Introduce uncheckedAppend version.
                buffer.append(static_cast<const uint8_t*>(data), size);
                return true;
            }).get());
            buffer = processData(WTFMove(buffer));
        }
        if (isComplete && context && nw_content_context_get_is_final(context))
            return;
        if (error)
            return;
        processIncomingData(WTFMove(nwConnection), WTFMove(processData), WTFMove(buffer));
    }).get());
}

NetworkRTCSocketCocoa::NetworkRTCSocketCocoa(LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& remoteAddress, int options, Ref<IPC::Connection>&& connection)
    : m_identifier(identifier)
    , m_rtcProvider(rtcProvider)
    , m_connection(WTFMove(connection))
    , m_isSTUN(options & rtc::PacketSocketFactory::OPT_STUN)
{
    auto hostName = remoteAddress.hostname();
    if (hostName.empty())
        hostName = remoteAddress.ipaddr().ToString();
    auto host = adoptNS(nw_endpoint_create_host(hostName.c_str(), String::number(remoteAddress.port()).utf8().data()));
    // FIXME: Handle TLS certificate validation like for other network code paths, using sec_protocol_options_set_verify_block
    bool isTLS = options & rtc::PacketSocketFactory::OPT_TLS;
    auto tcpTLS = adoptNS(nw_parameters_create_secure_tcp(isTLS ? NW_PARAMETERS_DEFAULT_CONFIGURATION : NW_PARAMETERS_DISABLE_PROTOCOL, ^(nw_protocol_options_t tcp_options) {
        nw_tcp_options_set_no_delay(tcp_options, true);
    }));

    m_nwConnection = adoptNS(nw_connection_create(host.get(), tcpTLS.get()));

    nw_connection_set_queue(m_nwConnection.get(), socketQueue());
    nw_connection_set_state_changed_handler(m_nwConnection.get(), makeBlockPtr([identifier = m_identifier, rtcProvider = makeRef(rtcProvider), connection = m_connection.copyRef()](nw_connection_state_t state, _Nullable nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        switch (state) {
        case nw_connection_state_invalid:
        case nw_connection_state_waiting:
        case nw_connection_state_preparing:
            return;
        case nw_connection_state_ready:
            connection->send(Messages::LibWebRTCNetwork::SignalConnect(identifier), 0);
            return;
        case nw_connection_state_failed:
            rtcProvider->callOnRTCNetworkThread([rtcProvider, identifier] {
                rtcProvider->takeSocket(identifier);
            });
            connection->send(Messages::LibWebRTCNetwork::SignalClose(identifier, -1), 0);
            return;
        case nw_connection_state_cancelled:
            return;
        }
    }).get());

    processIncomingData(m_nwConnection.get(), [identifier = m_identifier, connection = m_connection.copyRef(), ip = remoteAddress.ipaddr(), port = remoteAddress.port(), isSTUN = m_isSTUN](auto&& buffer) mutable {
        return WebRTC::extractMessages(WTFMove(buffer), isSTUN ? WebRTC::MessageType::STUN : WebRTC::MessageType::Data, [&](auto* message, auto size) {
            IPC::DataReference data(message, size);
            connection->send(Messages::LibWebRTCNetwork::SignalReadPacket { identifier, data, RTCNetwork::IPAddress(ip), port, rtc::TimeMillis() * 1000 }, 0);
        });
    });

    nw_connection_start(m_nwConnection.get());
}

void NetworkRTCSocketCocoa::close()
{
    if (!m_nwConnection)
        return;
    nw_connection_cancel(m_nwConnection.get());
    m_rtcProvider.takeSocket(m_identifier);
}

void NetworkRTCSocketCocoa::setOption(int, int)
{
    // FIXME: Validate this is not needed.
}

static RetainPtr<dispatch_data_t> dataFromVector(Vector<uint8_t>&& v)
{
    auto bufferSize = v.size();
    auto rawPointer = v.releaseBuffer().leakPtr();
    return adoptNS(dispatch_data_create(rawPointer, bufferSize, dispatch_get_main_queue(), ^{
        fastFree(rawPointer);
    }));
}

Vector<uint8_t> NetworkRTCSocketCocoa::createMessageBuffer(const uint8_t* data, size_t size)
{
    if (size >= std::numeric_limits<uint16_t>::max())
        return { };

    if (m_isSTUN) {
        auto messageLengths = WebRTC::getSTUNOrTURNMessageLengths(data, size);
        if (!messageLengths)
            return { };

        ASSERT(messageLengths->messageLength == size);
        ASSERT(messageLengths->messageLengthWithPadding >= size);
        if (messageLengths->messageLengthWithPadding < size)
            return { };

        Vector<uint8_t> buffer;
        buffer.reserveInitialCapacity(messageLengths->messageLengthWithPadding);
        buffer.append(data, size);
        for (size_t cptr = 0 ; cptr < messageLengths->messageLengthWithPadding - size; ++cptr)
            buffer.uncheckedAppend(0);
        return buffer;
    }

    // Prepend length.
    Vector<uint8_t> buffer;
    buffer.reserveInitialCapacity(size + 2);
    buffer.uncheckedAppend((size >> 8) & 0xFF);
    buffer.uncheckedAppend(size & 0xFF);
    buffer.append(data, size);
    return buffer;
}

void NetworkRTCSocketCocoa::sendTo(const uint8_t* data, size_t size, const rtc::SocketAddress&, const rtc::PacketOptions& options)
{
    auto buffer = createMessageBuffer(data, size);
    if (buffer.isEmpty())
        return;

    nw_connection_send(m_nwConnection.get(), dataFromVector(WTFMove(buffer)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([identifier = m_identifier, connection = m_connection.copyRef(), options](_Nullable nw_error_t) {
        connection->send(Messages::LibWebRTCNetwork::SignalSentPacket { identifier, options.packet_id, rtc::TimeMillis() }, 0);
    }).get());
}

} // namespace WebKit

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
