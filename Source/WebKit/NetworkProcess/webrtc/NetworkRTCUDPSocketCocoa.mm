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
#include "NWParametersSPI.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/STUNMessageParsing.h>
#include <dispatch/dispatch.h>
#include <wtf/BlockPtr.h>
#include <wtf/SoftLinking.h>
#include <wtf/ThreadSafeRefCounted.h>

#if HAVE(NWPARAMETERS_TRACKER_API)
SOFT_LINK_LIBRARY(libnetworkextension)
SOFT_LINK_CLASS(libnetworkextension, NEHelperTrackerDisposition_t)
SOFT_LINK_CLASS(libnetworkextension, NEHelperTrackerAppInfoRef)
SOFT_LINK_CLASS(libnetworkextension, NEHelperTrackerDomainContextRef)
SOFT_LINK(libnetworkextension, NEHelperTrackerGetDisposition, NEHelperTrackerDisposition_t*, (NEHelperTrackerAppInfoRef *app_info_ref, CFArrayRef domains, NEHelperTrackerDomainContextRef *trackerDomainContextRef, CFIndex *trackerDomainIndex), (app_info_ref, domains, trackerDomainContextRef, trackerDomainIndex))

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

private:
    NetworkRTCUDPSocketCocoaConnections(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&, const rtc::SocketAddress&, Ref<IPC::Connection>&&, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&);

    RetainPtr<nw_connection_t> createNWConnection(const rtc::SocketAddress&);
    void setupNWConnection(nw_connection_t, const rtc::SocketAddress&);
    void configureParameters(nw_parameters_t, nw_ip_version_t);

    WebCore::LibWebRTCSocketIdentifier m_identifier;
    Ref<IPC::Connection> m_connection;
#if HAVE(NWPARAMETERS_TRACKER_API)
    bool m_isFirstParty { false };
    bool m_isKnownTracker { false };
#endif
    bool m_shouldBypassRelay { false };

    std::optional<audit_token_t> m_sourceApplicationAuditToken;
    String m_attributedBundleIdentifier;

    rtc::SocketAddress m_address;
    RetainPtr<nw_listener_t> m_nwListener;
    Lock m_nwConnectionsLock;
    bool m_isClosed WTF_GUARDED_BY_LOCK(m_nwConnectionsLock) { false };
    HashMap<rtc::SocketAddress, RetainPtr<nw_connection_t>> m_nwConnections WTF_GUARDED_BY_LOCK(m_nwConnectionsLock);
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

std::unique_ptr<NetworkRTCProvider::Socket> NetworkRTCUDPSocketCocoa::createUDPSocket(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort, Ref<IPC::Connection>&& connection, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain)
{
    return makeUnique<NetworkRTCUDPSocketCocoa>(identifier, rtcProvider, address, WTFMove(connection), WTFMove(attributedBundleIdentifier), isFirstParty, isRelayDisabled, domain);
}

NetworkRTCUDPSocketCocoa::NetworkRTCUDPSocketCocoa(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& address, Ref<IPC::Connection>&& connection, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain)
    : m_rtcProvider(rtcProvider)
    , m_identifier(identifier)
    , m_nwConnections(NetworkRTCUDPSocketCocoaConnections::create(identifier, rtcProvider, address, WTFMove(connection), WTFMove(attributedBundleIdentifier), isFirstParty, isRelayDisabled, domain))
{
}

NetworkRTCUDPSocketCocoa::~NetworkRTCUDPSocketCocoa()
{
}

void NetworkRTCUDPSocketCocoa::close()
{
    m_nwConnections->close();
    m_rtcProvider.takeSocket(m_identifier);
}

void NetworkRTCUDPSocketCocoa::setListeningPort(int port)
{
    m_nwConnections->setListeningPort(port);
}

void NetworkRTCUDPSocketCocoa::setOption(int option, int value)
{
    m_nwConnections->setOption(option, value);
}

void NetworkRTCUDPSocketCocoa::sendTo(const uint8_t* data, size_t size, const rtc::SocketAddress& address, const rtc::PacketOptions& options)
{
    m_nwConnections->sendTo(data, size, address, options);
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

#if HAVE(NWPARAMETERS_TRACKER_API)
static bool isKnownTracker(const WebCore::RegistrableDomain& domain)
{
    NSArray<NSString *> *domains = @[domain.string()];
    NEHelperTrackerDomainContextRef *context = nil;
    CFIndex index = 0;
    return !!NEHelperTrackerGetDisposition(nullptr, (CFArrayRef)domains, context, &index);
}
#endif

NetworkRTCUDPSocketCocoaConnections::NetworkRTCUDPSocketCocoaConnections(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, const rtc::SocketAddress& address, Ref<IPC::Connection>&& connection, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain& domain)
    : m_identifier(identifier)
    , m_connection(WTFMove(connection))
#if HAVE(NWPARAMETERS_TRACKER_API)
    , m_isFirstParty(isFirstParty)
    , m_isKnownTracker(isKnownTracker(domain))
#endif
    , m_shouldBypassRelay(isRelayDisabled)
    , m_sourceApplicationAuditToken(rtcProvider.sourceApplicationAuditToken())
    , m_attributedBundleIdentifier(WTFMove(attributedBundleIdentifier))
{
    auto parameters = adoptNS(nw_parameters_create_secure_udp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    {
        auto hostAddress = address.ipaddr().ToString();
        if (address.ipaddr().IsNil())
            hostAddress = address.hostname();
        auto localEndpoint = adoptNS(nw_endpoint_create_host_with_numeric_port(hostAddress.c_str(), 0));
        m_address = { nw_endpoint_get_hostname(localEndpoint.get()), nw_endpoint_get_port(localEndpoint.get()) };
        nw_parameters_set_local_endpoint(parameters.get(), localEndpoint.get());
    }
    configureParameters(parameters.get(), address.family() == AF_INET ? nw_ip_version_4 : nw_ip_version_6);

    m_nwListener = adoptNS(nw_listener_create(parameters.get()));
    nw_listener_set_queue(m_nwListener.get(), udpSocketQueue());

    // The callback holds a reference to the nw_listener and we clear it when going in nw_listener_state_cancelled state, which is triggered when closing the socket.
    nw_listener_set_state_changed_handler(m_nwListener.get(), makeBlockPtr([nwListener = m_nwListener, connection = m_connection.copyRef(), protectedRTCProvider = makeRef(rtcProvider), identifier = m_identifier](nw_listener_state_t state, nw_error_t error) mutable {
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
            nwListener.clear();
            break;
        }
    }).get());

    nw_listener_set_new_connection_handler(m_nwListener.get(), makeBlockPtr([protectedThis = makeRef(*this)](nw_connection_t connection) {
        Locker locker { protectedThis->m_nwConnectionsLock };
        if (protectedThis->m_isClosed)
            return;

        auto remoteAddress = socketAddressFromIncomingConnection(connection);
        ASSERT(remoteAddress != HashTraits<rtc::SocketAddress>::emptyValue() && !HashTraits<rtc::SocketAddress>::isDeletedValue(remoteAddress));

        protectedThis->m_nwConnections.set(remoteAddress, connection);
        protectedThis->setupNWConnection(connection, remoteAddress);
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

    if (m_shouldBypassRelay)
        nw_parameters_set_account_id(parameters, "com.apple.safari.peertopeer");
#if HAVE(NWPARAMETERS_TRACKER_API)
    nw_parameters_set_is_third_party_web_content(parameters, !m_isFirstParty);
    nw_parameters_set_is_known_tracker(parameters, m_isKnownTracker);
#endif

    if (m_sourceApplicationAuditToken)
        nw_parameters_set_source_application(parameters, *m_sourceApplicationAuditToken);
    if (!m_attributedBundleIdentifier.isEmpty())
        nw_parameters_set_source_application_by_bundle_id(parameters, m_attributedBundleIdentifier.utf8().data());

    nw_parameters_set_reuse_local_address(parameters, true);
}

void NetworkRTCUDPSocketCocoaConnections::close()
{
    Locker locker { m_nwConnectionsLock };
    m_isClosed = true;

    nw_listener_cancel(m_nwListener.get());
    m_nwListener = nullptr;

    for (auto& nwConnection : m_nwConnections.values())
        nw_connection_cancel(nwConnection.get());
    m_nwConnections.clear();
}

void NetworkRTCUDPSocketCocoaConnections::setOption(int, int)
{
    // FIXME: Validate this is not needed.
}

static inline void processUDPData(RetainPtr<nw_connection_t>&& nwConnection, Function<void(const uint8_t*, size_t)>&& processData)
{
    auto nwConnectionReference = nwConnection.get();
    nw_connection_receive(nwConnectionReference, 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([nwConnection = WTFMove(nwConnection), processData = WTFMove(processData)](dispatch_data_t content, nw_content_context_t context, bool isComplete, nw_error_t error) mutable {
        if (content) {
            dispatch_data_apply(content, makeBlockPtr([&](dispatch_data_t, size_t, const void* data, size_t size) {
                processData(static_cast<const uint8_t*>(data), size);
                return true;
            }).get());
        }
        if (isComplete && context && nw_content_context_get_is_final(context))
            return;
        if (error) {
            RELEASE_LOG_ERROR(WebRTC, "NetworkRTCUDPSocketCocoaConnections failed processing UDP data with error %d", nw_error_get_error_code(error));
            return;
        }
        processUDPData(WTFMove(nwConnection), WTFMove(processData));
    }).get());
}

RetainPtr<nw_connection_t> NetworkRTCUDPSocketCocoaConnections::createNWConnection(const rtc::SocketAddress& remoteAddress)
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

    setupNWConnection(nwConnection.get(), remoteAddress);
    return nwConnection;
}

void NetworkRTCUDPSocketCocoaConnections::setupNWConnection(nw_connection_t nwConnection, const rtc::SocketAddress& remoteAddress)
{
    nw_connection_set_queue(nwConnection, udpSocketQueue());

    processUDPData(nwConnection, [identifier = m_identifier, connection = m_connection.copyRef(), ip = remoteAddress.ipaddr(), port = remoteAddress.port()](auto* message, auto size) mutable {
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
        }).iterator->value.get();
    }

    auto value = adoptNS(dispatch_data_create(data, size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT));
    nw_connection_send(nwConnection, value.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([identifier = m_identifier, connection = m_connection.copyRef(), options](_Nullable nw_error_t error) {
        RELEASE_LOG_ERROR_IF(error, WebRTC, "NetworkRTCUDPSocketCocoaConnections::sendTo failed with error %d", error ? nw_error_get_error_code(error) : 0);
        connection->send(Messages::LibWebRTCNetwork::SignalSentPacket { identifier, options.packet_id, rtc::TimeMillis() }, 0);
    }).get());
}

} // namespace WebKit

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
