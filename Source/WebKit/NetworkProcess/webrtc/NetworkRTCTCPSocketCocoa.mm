/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "NetworkRTCTCPSocketCocoa.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "LibWebRTCNetworkMessages.h"
#include "Logging.h"
#include "NetworkRTCUtilitiesCocoa.h"
#include <WebCore/STUNMessageParsing.h>
#include <dispatch/dispatch.h>
#include <pal/spi/cocoa/NetworkSPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/cocoa/VectorCocoa.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/packet_socket_factory.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebKit {

using namespace WebCore;

static dispatch_queue_t tcpSocketQueue()
{
    static dispatch_queue_t queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("WebRTC TCP socket queue", DISPATCH_QUEUE_CONCURRENT);
    });
    return queue;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkRTCTCPSocketCocoa);

std::unique_ptr<NetworkRTCProvider::Socket> NetworkRTCTCPSocketCocoa::createClientTCPSocket(LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& remoteAddress, int tcpOptions, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain, Ref<IPC::Connection>&& connection)
{
    // FIXME: We should support ssltcp candidates, maybe support OPT_TLS_INSECURE as well.
    return makeUnique<NetworkRTCTCPSocketCocoa>(identifier, rtcProvider, remoteAddress, tcpOptions, attributedBundleIdentifier, isFirstParty, isRelayDisabled, domain, WTFMove(connection));
}

static inline void processIncomingData(RetainPtr<nw_connection_t>&& nwConnection, Function<Vector<uint8_t>(Vector<uint8_t>&&)>&& processData, Vector<uint8_t>&& buffer = { })
{
    auto nwConnectionReference = nwConnection.get();
    nw_connection_receive(nwConnectionReference, 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([nwConnection = WTFMove(nwConnection), processData = WTFMove(processData), buffer = WTFMove(buffer)](dispatch_data_t content, nw_content_context_t context, bool isComplete, nw_error_t error) mutable {
        if (content) {
            dispatch_data_apply_span(content, [&](std::span<const uint8_t> data) {
                buffer.append(data);
                return true;
            });
            buffer = processData(WTFMove(buffer));
        }
        if (isComplete && context && nw_content_context_get_is_final(context))
            return;
        if (error) {
            RELEASE_LOG_ERROR(WebRTC, "NetworkRTCTCPSocketCocoa processIncomingData failed with error %d", nw_error_get_error_code(error));
            return;
        }
        processIncomingData(WTFMove(nwConnection), WTFMove(processData), WTFMove(buffer));
    }).get());
}

static RetainPtr<nw_connection_t> createNWConnection(NetworkRTCProvider& rtcProvider, const char* hostName, const char* port, bool isTLS, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain)
{
    auto host = adoptNS(nw_endpoint_create_host(hostName, port));
    // FIXME: Handle TLS certificate validation like for other network code paths, using sec_protocol_options_set_verify_block
    auto tcpTLS = adoptNS(nw_parameters_create_secure_tcp(isTLS ? NW_PARAMETERS_DEFAULT_CONFIGURATION : NW_PARAMETERS_DISABLE_PROTOCOL, ^(nw_protocol_options_t tcp_options) {
        nw_tcp_options_set_no_delay(tcp_options, true);
    }));

    setNWParametersApplicationIdentifiers(tcpTLS.get(), rtcProvider.applicationBundleIdentifier(), rtcProvider.sourceApplicationAuditToken(), attributedBundleIdentifier);
    setNWParametersTrackerOptions(tcpTLS.get(), isRelayDisabled, isFirstParty, isKnownTracker(domain));

    return adoptNS(nw_connection_create(host.get(), tcpTLS.get()));
}

NetworkRTCTCPSocketCocoa::NetworkRTCTCPSocketCocoa(LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& remoteAddress, int options, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain, Ref<IPC::Connection>&& connection)
    : m_identifier(identifier)
    , m_rtcProvider(rtcProvider)
    , m_connection(WTFMove(connection))
    , m_isSTUN(options & rtc::PacketSocketFactory::OPT_STUN)
{
    auto hostName = remoteAddress.hostname();
    if (hostName.empty())
        hostName = remoteAddress.ipaddr().ToString();
    bool isTLS = options & rtc::PacketSocketFactory::OPT_TLS;
    m_nwConnection = createNWConnection(rtcProvider, hostName.c_str(), String::number(remoteAddress.port()).utf8().data(), isTLS, attributedBundleIdentifier, isFirstParty, isRelayDisabled, domain);

    nw_connection_set_queue(m_nwConnection.get(), tcpSocketQueue());
    nw_connection_set_state_changed_handler(m_nwConnection.get(), makeBlockPtr([weakNWConnection = WeakObjCPtr { m_nwConnection.get() }, identifier = m_identifier, rtcProvider = Ref { rtcProvider }, connection = m_connection.copyRef()](nw_connection_state_t state, _Nullable nw_error_t error) {
        switch (state) {
        case nw_connection_state_invalid:
        case nw_connection_state_waiting:
        case nw_connection_state_preparing:
            return;
        case nw_connection_state_ready:
            rtcProvider->callOnRTCNetworkThread([weakNWConnection, connection, identifier] {
                RetainPtr nwConnection = weakNWConnection.get();
                if (!nwConnection)
                    return;
                RetainPtr path = adoptNS(nw_connection_copy_current_path(nwConnection.get()));
                RetainPtr interface = adoptNS(nw_path_copy_interface(path.get()));
                if (auto name = String::fromUTF8(nw_interface_get_name(interface.get())); !name.isNull())
                    connection->send(Messages::LibWebRTCNetwork::SignalUsedInterface(identifier, WTFMove(name)), 0);
            });
            connection->send(Messages::LibWebRTCNetwork::SignalConnect(identifier), 0);
            return;
        case nw_connection_state_failed:
            rtcProvider->callOnRTCNetworkThread([rtcProvider, identifier] {
                rtcProvider->closeSocket(identifier);
            });
            connection->send(Messages::LibWebRTCNetwork::SignalClose(identifier, -1), 0);
            return;
        case nw_connection_state_cancelled:
            return;
        }
    }).get());

    processIncomingData(m_nwConnection.get(), [identifier = m_identifier, connection = m_connection.copyRef(), ip = remoteAddress.ipaddr(), port = remoteAddress.port(), isSTUN = m_isSTUN](Vector<uint8_t>&& buffer) mutable {
        return WebRTC::extractMessages(WTFMove(buffer), isSTUN ? WebRTC::MessageType::STUN : WebRTC::MessageType::Data, [&](auto data) {
            connection->send(Messages::LibWebRTCNetwork::SignalReadPacket { identifier, data, RTCNetwork::IPAddress(ip), port, rtc::TimeMicros(), RTC::Network::EcnMarking::kNotEct }, 0);
        });
    });

    nw_connection_start(m_nwConnection.get());
}

NetworkRTCTCPSocketCocoa::~NetworkRTCTCPSocketCocoa()
{
    ASSERT(m_isClosed);
}

void NetworkRTCTCPSocketCocoa::close()
{
#if ASSERT_ENABLED
    m_isClosed = true;
#endif
    if (m_nwConnection)
        nw_connection_cancel(m_nwConnection.get());
    Ref { m_rtcProvider.get() }->takeSocket(m_identifier);
}

void NetworkRTCTCPSocketCocoa::setOption(int option, int value)
{
    if (option != rtc::Socket::OPT_DSCP)
        return;

    auto trafficClass = trafficClassFromDSCP(static_cast<rtc::DiffServCodePoint>(value));
    if (!trafficClass) {
        RELEASE_LOG_ERROR(WebRTC, "NetworkRTCTCPSocketCocoa has an unexpected DSCP value %d", value);
        return;
    }

    nw_connection_reset_traffic_class(m_nwConnection.get(), *trafficClass);
}

Vector<uint8_t> NetworkRTCTCPSocketCocoa::createMessageBuffer(std::span<const uint8_t> data)
{
    if (data.size() >= std::numeric_limits<uint16_t>::max())
        return { };

    if (m_isSTUN) {
        auto messageLengths = WebRTC::getSTUNOrTURNMessageLengths(data);
        if (!messageLengths)
            return { };

        ASSERT(messageLengths->messageLength == data.size());
        ASSERT(messageLengths->messageLengthWithPadding >= data.size());
        if (messageLengths->messageLengthWithPadding < data.size())
            return { };

        Vector<uint8_t> buffer;
        buffer.reserveInitialCapacity(messageLengths->messageLengthWithPadding);
        buffer.append(data);
        for (size_t cptr = 0 ; cptr < messageLengths->messageLengthWithPadding - data.size(); ++cptr)
            buffer.append(0);
        return buffer;
    }

    // Prepend length.
    Vector<uint8_t> buffer;
    buffer.reserveInitialCapacity(data.size() + 2);
    buffer.appendList({ (data.size() >> 8) & 0xFF, data.size() & 0xFF });
    buffer.append(data);
    return buffer;
}

void NetworkRTCTCPSocketCocoa::sendTo(std::span<const uint8_t> data, const rtc::SocketAddress&, const rtc::PacketOptions& options)
{
    auto buffer = createMessageBuffer(data);
    if (buffer.isEmpty())
        return;

    nw_connection_send(m_nwConnection.get(), makeDispatchData(WTFMove(buffer)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([identifier = m_identifier, connection = m_connection.copyRef(), options](_Nullable nw_error_t) {
        connection->send(Messages::LibWebRTCNetwork::SignalSentPacket { identifier, options.packet_id, rtc::TimeMillis() }, 0);
    }).get());
}

void NetworkRTCTCPSocketCocoa::getInterfaceName(NetworkRTCProvider& rtcProvider, const URL& url, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain, CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(!isMainRunLoop());

    bool isHTTPS = url.protocolIs("https"_s);
    auto port = url.port().value_or(isHTTPS ? 443 : 80);
    auto nwConnection = createNWConnection(rtcProvider, url.host().toString().utf8().data(), String::number(port).utf8().data(), isHTTPS, attributedBundleIdentifier, isFirstParty, isRelayDisabled, domain);

    Function<void(String&&)> callback = [completionHandler = WTFMove(completionHandler), rtcProvider = Ref { rtcProvider }] (auto&& name) mutable {
        rtcProvider->callOnRTCNetworkThread([completionHandler = WTFMove(completionHandler), name = WTFMove(name).isolatedCopy()] () mutable {
            completionHandler(WTFMove(name));
        });
    };

    nw_connection_set_queue(nwConnection.get(), tcpSocketQueue());
    nw_connection_set_state_changed_handler(nwConnection.get(), makeBlockPtr([callback = WTFMove(callback), nwConnection](nw_connection_state_t state, _Nullable nw_error_t error) mutable {
        auto checkInterface = [&] {
            if (!nwConnection)
                return;

            auto path = adoptNS(nw_connection_copy_current_path(nwConnection.get()));
            auto interface = adoptNS(nw_path_copy_interface(path.get()));

            auto* name = nw_interface_get_name(interface.get());
            callback(name ? String::fromUTF8(name) : String { });
            nw_connection_cancel(nwConnection.get());
            nwConnection = { };
        };

        switch (state) {
        case nw_connection_state_preparing:
            checkInterface();
            return;
        case nw_connection_state_ready:
        case nw_connection_state_waiting:
        case nw_connection_state_invalid:
        case nw_connection_state_failed:
            if (!nwConnection)
                return;

            callback({ });
            nw_connection_cancel(nwConnection.get());
            nwConnection = { };
            return;
        case nw_connection_state_cancelled:
            if (!nwConnection)
                return;

            callback({ });
            nwConnection = { };
            return;
        }
    }).get());

    nw_connection_start(nwConnection.get());
}

} // namespace WebKit

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
