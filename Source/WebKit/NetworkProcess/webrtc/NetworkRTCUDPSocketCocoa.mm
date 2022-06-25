/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "NetworkRTCUDPSocketCocoa.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "DataReference.h"
#include "LibWebRTCNetworkMessages.h"
#include "Logging.h"
#include "NWSPI.h"
#include <WebCore/STUNMessageParsing.h>
#include <dispatch/dispatch.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <wtf/BlockPtr.h>
#include <wtf/SoftLinking.h>
#include <wtf/ThreadSafeRefCounted.h>

#if HAVE(NWPARAMETERS_TRACKER_API)
SOFT_LINK_LIBRARY_OPTIONAL(libnetwork)
SOFT_LINK_OPTIONAL(libnetwork, nw_parameters_allow_sharing_port_with_listener, void, __cdecl, (nw_parameters_t, nw_listener_t))
#endif

namespace WebKit {

using namespace WebCore;

class NetworkRTCUDPSocketCocoaConnections : public ThreadSafeRefCounted<NetworkRTCUDPSocketCocoaConnections> {
public:
    static Ref<NetworkRTCUDPSocketCocoaConnections> create(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& provider, const rtc::SocketAddress& address, Ref<IPC::Connection>&& connection, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain) { return adoptRef(*new NetworkRTCUDPSocketCocoaConnections(identifier, provider, address, WTFMove(connection), WTFMove(attributedBundleIdentifier), isFirstParty, isRelayDisabled, domain)); }

    void close();
    void setOption(int option, int value);
    void sendTo(const uint8_t*, size_t, const rtc::SocketAddress&, const rtc::PacketOptions&);
    void setListeningPort(int);

    class ConnectionStateTracker : public ThreadSafeRefCounted<ConnectionStateTracker> {
    public:
        static Ref<ConnectionStateTracker> create() { return adoptRef(*new ConnectionStateTracker()); }
        void markAsStopped() { m_isStopped = true; }
        bool isStopped() const { return m_isStopped; }

    private:
        bool m_isStopped { false };
    };

private:
    NetworkRTCUDPSocketCocoaConnections(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&, const rtc::SocketAddress&, Ref<IPC::Connection>&&, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&);

    std::pair<RetainPtr<nw_connection_t>, Ref<ConnectionStateTracker>> createNWConnection(const rtc::SocketAddress&);
    void setupNWConnection(nw_connection_t, ConnectionStateTracker&, const rtc::SocketAddress&);
    void configureParameters(nw_parameters_t, nw_ip_version_t);

    WebCore::LibWebRTCSocketIdentifier m_identifier;
    Ref<IPC::Connection> m_connection;
    bool m_isFirstParty { false };
    bool m_isKnownTracker { false };
    bool m_shouldBypassRelay { false };

    CString m_sourceApplicationBundleIdentifier;
    std::optional<audit_token_t> m_sourceApplicationAuditToken;
    String m_attributedBundleIdentifier;

    rtc::SocketAddress m_address;
    RetainPtr<nw_listener_t> m_nwListener;
    Lock m_nwConnectionsLock;
    bool m_isClosed WTF_GUARDED_BY_LOCK(m_nwConnectionsLock) { false };
    HashMap<rtc::SocketAddress, std::pair<RetainPtr<nw_connection_t>, RefPtr<ConnectionStateTracker>>> m_nwConnections WTF_GUARDED_BY_LOCK(m_nwConnectionsLock);
};

static dispatch_queue_t udpSocketQueue()
{
    static dispatch_queue_t queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("WebRTC UDP socket queue", DISPATCH_QUEUE_CONCURRENT);
    });
    return queue;
}

NetworkRTCUDPSocketCocoa::NetworkRTCUDPSocketCocoa(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& address, Ref<IPC::Connection>&& connection, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain)
    : m_rtcProvider(rtcProvider)
    , m_identifier(identifier)
    , m_connections(NetworkRTCUDPSocketCocoaConnections::create(identifier, rtcProvider, address, WTFMove(connection), WTFMove(attributedBundleIdentifier), isFirstParty, isRelayDisabled, domain))
{
}

NetworkRTCUDPSocketCocoa::~NetworkRTCUDPSocketCocoa()
{
}

void NetworkRTCUDPSocketCocoa::close()
{
    m_connections->close();
    m_rtcProvider.takeSocket(m_identifier);
}

void NetworkRTCUDPSocketCocoa::setListeningPort(int port)
{
    m_connections->setListeningPort(port);
}

void NetworkRTCUDPSocketCocoa::setOption(int option, int value)
{
    m_connections->setOption(option, value);
}

void NetworkRTCUDPSocketCocoa::sendTo(const uint8_t* data, size_t size, const rtc::SocketAddress& address, const rtc::PacketOptions& options)
{
    m_connections->sendTo(data, size, address, options);
}

static rtc::SocketAddress socketAddressFromIncomingConnection(nw_connection_t connection)
{
    auto endpoint = adoptNS(nw_connection_copy_endpoint(connection));
    auto type = nw_endpoint_get_type(endpoint.get());
    if (type == nw_endpoint_type_address) {
        auto* ipAddress = nw_endpoint_copy_address_string(endpoint.get());
        rtc::SocketAddress remoteAddress { ipAddress, nw_endpoint_get_port(endpoint.get()) };
        free(ipAddress);
        return remoteAddress;
    }
    return rtc::SocketAddress { nw_endpoint_get_hostname(endpoint.get()), nw_endpoint_get_port(endpoint.get()) };
}

static inline bool isNat64IPAddress(const rtc::IPAddress& ip)
{
    if (ip.family() != AF_INET)
        return false;

    struct ifaddrs* interfaces;
    if (auto error = getifaddrs(&interfaces))
        return true;
    std::unique_ptr<struct ifaddrs> toBeFreed(interfaces);

    for (auto* interface = interfaces; interface; interface = interface->ifa_next) {
        if (interface->ifa_addr->sa_family != AF_INET)
            continue;

        rtc::IPAddress interfaceAddress { reinterpret_cast<sockaddr_in*>(interface->ifa_addr)->sin_addr };
        if (ip != interfaceAddress)
            continue;

        return nw_nat64_does_interface_index_support_nat64(if_nametoindex(interface->ifa_name));
    }

    return false;
}

static std::string computeHostAddress(const rtc::SocketAddress& address)
{
    if (address.ipaddr().IsNil())
        return address.hostname();

    if (!isNat64IPAddress(address.ipaddr()))
        return address.ipaddr().ToString();

    return "0.0.0.0";
}

NetworkRTCUDPSocketCocoaConnections::NetworkRTCUDPSocketCocoaConnections(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& address, Ref<IPC::Connection>&& connection, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain)
    : m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_isFirstParty(isFirstParty)
    , m_isKnownTracker(isKnownTracker(domain))
    , m_shouldBypassRelay(isRelayDisabled)
    , m_sourceApplicationBundleIdentifier(rtcProvider.applicationBundleIdentifier())
    , m_sourceApplicationAuditToken(rtcProvider.sourceApplicationAuditToken())
    , m_attributedBundleIdentifier(WTFMove(attributedBundleIdentifier))
{
    auto parameters = adoptNS(nw_parameters_create_secure_udp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    {
        auto hostAddress = computeHostAddress(address);
        auto localEndpoint = adoptNS(nw_endpoint_create_host_with_numeric_port(hostAddress.c_str(), 0));
        m_address = { nw_endpoint_get_hostname(localEndpoint.get()), nw_endpoint_get_port(localEndpoint.get()) };
        nw_parameters_set_local_endpoint(parameters.get(), localEndpoint.get());
    }
    configureParameters(parameters.get(), address.family() == AF_INET ? nw_ip_version_4 : nw_ip_version_6);

    m_nwListener = adoptNS(nw_listener_create(parameters.get()));
    nw_listener_set_queue(m_nwListener.get(), udpSocketQueue());

    // The callback holds a reference to the nw_listener and we clear it when going in nw_listener_state_cancelled state, which is triggered when closing the socket.
    nw_listener_set_state_changed_handler(m_nwListener.get(), makeBlockPtr([nwListener = m_nwListener, connection = m_connection.copyRef(), protectedRTCProvider = Ref { rtcProvider }, identifier = m_identifier](nw_listener_state_t state, nw_error_t error) mutable {
        switch (state) {
        case nw_listener_state_invalid:
        case nw_listener_state_waiting:
            break;
        case nw_listener_state_ready:
            protectedRTCProvider->doSocketTaskOnRTCNetworkThread(identifier, [port = nw_listener_get_port(nwListener.get())](auto& socket) mutable {
                auto& udpSocket = static_cast<NetworkRTCUDPSocketCocoa&>(socket);
                udpSocket.setListeningPort(port);
            });
            break;
        case nw_listener_state_failed:
            RELEASE_LOG_ERROR(WebRTC, "NetworkRTCUDPSocketCocoaConnections failed with error %d", error ? nw_error_get_error_code(error) : 0);
            protectedRTCProvider->callOnRTCNetworkThread([protectedRTCProvider, identifier] {
                protectedRTCProvider->closeSocket(identifier);
            });
            connection->send(Messages::LibWebRTCNetwork::SignalClose(identifier, -1), 0);
            break;
        case nw_listener_state_cancelled:
            RELEASE_LOG(WebRTC, "NetworkRTCUDPSocketCocoaConnections cancelled listener %" PRIu64, identifier.toUInt64());
            nwListener.clear();
            break;
        }
    }).get());

    nw_listener_set_new_connection_handler(m_nwListener.get(), makeBlockPtr([protectedThis = Ref { *this }](nw_connection_t nwConnection) {
        Locker locker { protectedThis->m_nwConnectionsLock };
        if (protectedThis->m_isClosed)
            return;

        auto remoteAddress = socketAddressFromIncomingConnection(nwConnection);
        ASSERT(remoteAddress != HashTraits<rtc::SocketAddress>::emptyValue() && !HashTraits<rtc::SocketAddress>::isDeletedValue(remoteAddress));

        auto connectionStateTracker = ConnectionStateTracker::create();
        protectedThis->setupNWConnection(nwConnection, connectionStateTracker.get(), remoteAddress);

        protectedThis->m_nwConnections.set(remoteAddress, std::make_pair(nwConnection, WTFMove(connectionStateTracker)));
    }).get());

    nw_listener_start(m_nwListener.get());
}

void NetworkRTCUDPSocketCocoaConnections::setListeningPort(int port)
{
    m_address.SetPort(port);
    m_connection->send(Messages::LibWebRTCNetwork::SignalAddressReady(m_identifier, RTCNetwork::SocketAddress(m_address)), 0);
}

void NetworkRTCUDPSocketCocoaConnections::configureParameters(nw_parameters_t parameters, nw_ip_version_t version)
{
    auto protocolStack = adoptNS(nw_parameters_copy_default_protocol_stack(parameters));
    auto options = adoptNS(nw_protocol_stack_copy_internet_protocol(protocolStack.get()));
    nw_ip_options_set_version(options.get(), version);

    setNWParametersApplicationIdentifiers(parameters, m_sourceApplicationBundleIdentifier.data(), m_sourceApplicationAuditToken, m_attributedBundleIdentifier);
    setNWParametersTrackerOptions(parameters, m_shouldBypassRelay, m_isFirstParty, m_isKnownTracker);

    nw_parameters_set_reuse_local_address(parameters, true);
}

void NetworkRTCUDPSocketCocoaConnections::close()
{
    Locker locker { m_nwConnectionsLock };
    m_isClosed = true;

    for (auto& nwConnection : m_nwConnections.values()) {
        nwConnection.second->markAsStopped();
        nw_connection_cancel(nwConnection.first.get());
    }
    m_nwConnections.clear();

    nw_listener_cancel(m_nwListener.get());
    m_nwListener = nullptr;
}

void NetworkRTCUDPSocketCocoaConnections::setOption(int, int)
{
    // FIXME: Validate this is not needed.
}

static inline void processUDPData(RetainPtr<nw_connection_t>&& nwConnection, Ref<NetworkRTCUDPSocketCocoaConnections::ConnectionStateTracker> connectionStateTracker, int errorCode, Function<void(const uint8_t*, size_t)>&& processData)
{
    auto nwConnectionReference = nwConnection.get();
    nw_connection_receive(nwConnectionReference, 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([nwConnection = WTFMove(nwConnection), processData = WTFMove(processData), errorCode, connectionStateTracker = WTFMove(connectionStateTracker)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
        if (content) {
            dispatch_data_apply(content, makeBlockPtr([&](dispatch_data_t, size_t, const void* data, size_t size) {
                processData(static_cast<const uint8_t*>(data), size);
                return true;
            }).get());
        }
        if (connectionStateTracker->isStopped())
            return;

        if (error && errorCode != nw_error_get_error_code(error)) {
            errorCode = nw_error_get_error_code(error);
            RELEASE_LOG_ERROR(WebRTC, "NetworkRTCUDPSocketCocoaConnections failed processing UDP data with error %d", errorCode);
        }
        processUDPData(WTFMove(nwConnection), WTFMove(connectionStateTracker), errorCode, WTFMove(processData));
    }).get());
}

std::pair<RetainPtr<nw_connection_t>, Ref<NetworkRTCUDPSocketCocoaConnections::ConnectionStateTracker>> NetworkRTCUDPSocketCocoaConnections::createNWConnection(const rtc::SocketAddress& remoteAddress)
{
    auto parameters = adoptNS(nw_parameters_create_secure_udp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    {
        auto hostAddress = m_address.ipaddr().ToString();
        if (m_address.ipaddr().IsNil())
            hostAddress = m_address.hostname();

        // rdar://80176676: we workaround local loop port reuse by using 0 instead of m_address.port() when nw_parameters_allow_sharing_port_with_listener is not available.
        uint16_t port = 0;
#if HAVE(NWPARAMETERS_TRACKER_API)
        if (nw_parameters_allow_sharing_port_with_listenerPtr()) {
            nw_parameters_allow_sharing_port_with_listenerPtr()(parameters.get(), m_nwListener.get());
            port = m_address.port();
        }
#endif
        auto localEndpoint = adoptNS(nw_endpoint_create_host_with_numeric_port(hostAddress.c_str(), port));
        nw_parameters_set_local_endpoint(parameters.get(), localEndpoint.get());
    }
    configureParameters(parameters.get(), remoteAddress.family() == AF_INET ? nw_ip_version_4 : nw_ip_version_6);

    auto remoteHostAddress = remoteAddress.ipaddr().ToString();
    if (remoteAddress.ipaddr().IsNil())
        remoteHostAddress = remoteAddress.hostname();
    auto host = adoptNS(nw_endpoint_create_host(remoteHostAddress.c_str(), String::number(remoteAddress.port()).utf8().data()));
    auto nwConnection = adoptNS(nw_connection_create(host.get(), parameters.get()));

    auto connectionStateTracker = ConnectionStateTracker::create();

    setupNWConnection(nwConnection.get(), connectionStateTracker.get(), remoteAddress);
    return std::make_pair(WTFMove(nwConnection), WTFMove(connectionStateTracker));
}

void NetworkRTCUDPSocketCocoaConnections::setupNWConnection(nw_connection_t nwConnection, ConnectionStateTracker& connectionStateTracker, const rtc::SocketAddress& remoteAddress)
{
    nw_connection_set_queue(nwConnection, udpSocketQueue());

    nw_connection_set_state_changed_handler(nwConnection, makeBlockPtr([connectionStateTracker = Ref  { connectionStateTracker }](nw_connection_state_t state, _Nullable nw_error_t error) {
        RELEASE_LOG_ERROR_IF(state == nw_connection_state_failed, WebRTC, "NetworkRTCUDPSocketCocoaConnections connection failed with error %d", error ? nw_error_get_error_code(error) : 0);
        if (state == nw_connection_state_failed || state == nw_connection_state_cancelled)
            connectionStateTracker->markAsStopped();
    }).get());

    processUDPData(nwConnection, Ref  { connectionStateTracker }, 0, [identifier = m_identifier, connection = m_connection.copyRef(), ip = remoteAddress.ipaddr(), port = remoteAddress.port()](auto* message, auto size) mutable {
        IPC::DataReference data(message, size);
        connection->send(Messages::LibWebRTCNetwork::SignalReadPacket { identifier, data, RTCNetwork::IPAddress(ip), port, rtc::TimeMillis() * 1000 }, 0);
    });

    nw_connection_start(nwConnection);
}

void NetworkRTCUDPSocketCocoaConnections::sendTo(const uint8_t* data, size_t size, const rtc::SocketAddress& remoteAddress, const rtc::PacketOptions& options)
{
    bool isInCorrectValue = (remoteAddress == HashTraits<rtc::SocketAddress>::emptyValue()) || HashTraits<rtc::SocketAddress>::isDeletedValue(remoteAddress);
    ASSERT(!isInCorrectValue);
    if (isInCorrectValue)
        return;

    nw_connection_t nwConnection;
    {
        Locker locker { m_nwConnectionsLock };
        nwConnection = m_nwConnections.ensure(remoteAddress, [this, &remoteAddress] {
            return createNWConnection(remoteAddress);
        }).iterator->value.first.get();
    }

    auto value = adoptNS(dispatch_data_create(data, size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT));
    nw_connection_send(nwConnection, value.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([identifier = m_identifier, connection = m_connection.copyRef(), options](_Nullable nw_error_t error) {
        RELEASE_LOG_ERROR_IF(error, WebRTC, "NetworkRTCUDPSocketCocoaConnections::sendTo failed with error %d", error ? nw_error_get_error_code(error) : 0);
        connection->send(Messages::LibWebRTCNetwork::SignalSentPacket { identifier, options.packet_id, rtc::TimeMillis() }, 0);
    }).get());
}

} // namespace WebKit

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
