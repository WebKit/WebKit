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
#include "GStreamerIceBackendNice.h"

#if USE(GSTREAMER_WEBRTC) && USE(LIBNICE)

#include "GStreamerIceBackendProxyMessages.h"
#include <WebCore/RTCIceConnectionState.h>
#include <nice.h>
#include <wtf/CompletionHandler.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebKit {

GStreamerIceBackendNice::GStreamerIceBackendNice()
{
    {
        Locker locker(m_lock);
        static Atomic<uint32_t> counter = 0;
        auto id = counter.load();
        auto threadName = makeString("webrtc-nice-"_s, id);
        counter.exchangeAdd(1);
        m_thread = Thread::create(ASCIILiteral::fromLiteralUnsafe(threadName.ascii().data()), [&] {
            Locker locker(m_lock);
            m_mainContext = adoptGRef(g_main_context_new());
            m_loop = adoptGRef(g_main_loop_new(m_mainContext.get(), FALSE));
            m_condition.notifyAll();
            g_main_context_invoke(m_mainContext.get(), reinterpret_cast<GSourceFunc>(+[](gpointer data) -> gboolean {
                reinterpret_cast<Locker<Lock>*>(data)->unlockEarly();
                return G_SOURCE_REMOVE;
            }), &locker);

            g_main_context_push_thread_default(m_mainContext.get());
            g_main_loop_run(m_loop.get());
            g_main_context_pop_thread_default(m_mainContext.get());
        });
        m_thread->detach();

        while (!m_loop)
            m_condition.wait(m_lock);
    }

    auto options = static_cast<NiceAgentOption>(NICE_AGENT_OPTION_ICE_TRICKLE | NICE_AGENT_OPTION_REGULAR_NOMINATION | NICE_AGENT_OPTION_CONSENT_FRESHNESS);
    m_agent = adoptGRef(nice_agent_new_full(m_mainContext.get(), NICE_COMPATIBILITY_RFC5245, options));
    g_signal_connect(m_agent.get(), "new-candidate-full", G_CALLBACK(+[](NiceAgent*, NiceCandidate* candidate, gpointer userData) {
        auto self = reinterpret_cast<GStreamerIceBackendNice*>(userData);
        self->notifyNewCandidate(*candidate);
    }), this);
    g_signal_connect(m_agent.get(), "candidate-gathering-done", G_CALLBACK(+[](NiceAgent*, unsigned streamId, gpointer userData) {
        auto self = reinterpret_cast<GStreamerIceBackendNice*>(userData);
        self->notifyGatheringDone(streamId);
    }), this);

    g_signal_connect(m_agent.get(), "component-state-changed", G_CALLBACK(+[](NiceAgent*, unsigned streamId, NiceComponentType component, NiceComponentState state, gpointer userData) {
        auto self = reinterpret_cast<GStreamerIceBackendNice*>(userData);
        self->notifyComponentStateChanged(streamId, component, state);
    }), this);
    g_signal_connect(m_agent.get(), "new-selected-pair-full", G_CALLBACK(+[](NiceAgent*, unsigned streamId, NiceComponentType component, NiceCandidate*, NiceCandidate*, gpointer userData) {
        auto self = reinterpret_cast<GStreamerIceBackendNice*>(userData);
        self->notifyNewSelectedPair(streamId, component);
    }), this);
}

GStreamerIceBackendNice::~GStreamerIceBackendNice()
{
    g_signal_handlers_disconnect_by_data(m_agent.get(), this);
}

void GStreamerIceBackendNice::notifyNewCandidate(const NiceCandidate& candidate)
{
    std::optional<unsigned> sessionId;
    for (const auto& stream : m_streams) {
        if (stream.streamId == candidate.stream_id) {
            sessionId = stream.sessionId;
            break;
        }
    }
    if (!sessionId) [[unlikely]]
        return;

    GUniqueOutPtr<NiceCandidate> filledCandidate;
    fillLocalCandidateCredentials(candidate, filledCandidate);

    GUniquePtr<char> sdp(nice_agent_generate_local_candidate_sdp(m_agent.get(), filledCandidate.get()));
    callOnMainRunLoop([&, sdp = WTFMove(sdp), sessionId = *sessionId] {
        connection()->send(Messages::GStreamerIceBackendProxy::NotifyNewCandidate { sessionId, String::fromUTF8(sdp.get()) }, destination());
    });
}

void GStreamerIceBackendNice::notifyGatheringDone(unsigned streamId)
{
    callOnMainRunLoop([&] {
        connection()->send(Messages::GStreamerIceBackendProxy::NotifyGatheringDone { streamId }, destination());
    });
}

static WebCore::RTCIceComponent niceComponentToRTCIceComponent(NiceComponentType component)
{
    WebCore::RTCIceComponent iceComponent;
    switch (component) {
    case NICE_COMPONENT_TYPE_RTCP:
        iceComponent = WebCore::RTCIceComponent::Rtcp;
        break;
    case NICE_COMPONENT_TYPE_RTP:
        iceComponent = WebCore::RTCIceComponent::Rtp;
        break;
    };
    return iceComponent;
}

void GStreamerIceBackendNice::notifyComponentStateChanged(unsigned streamId, NiceComponentType component, NiceComponentState state)
{
    WebCore::RTCIceConnectionState iceState;
    switch (state) {
    case NICE_COMPONENT_STATE_CONNECTING:
        iceState = WebCore::RTCIceConnectionState::Checking;
        break;
    case NICE_COMPONENT_STATE_CONNECTED:
        iceState = WebCore::RTCIceConnectionState::Connected;
        break;
    case NICE_COMPONENT_STATE_DISCONNECTED:
        iceState = WebCore::RTCIceConnectionState::Disconnected;
        break;
    case NICE_COMPONENT_STATE_GATHERING:
        iceState = WebCore::RTCIceConnectionState::New;
        break;
    case NICE_COMPONENT_STATE_READY:
        iceState = WebCore::RTCIceConnectionState::Completed;
        break;
    case NICE_COMPONENT_STATE_FAILED:
        iceState = WebCore::RTCIceConnectionState::Failed;
        break;
    case NICE_COMPONENT_STATE_LAST:
        ASSERT_NOT_REACHED();
        return;
    };
    callOnMainRunLoopAndWait([&, component = niceComponentToRTCIceComponent(component), iceState] {
        connection()->send(Messages::GStreamerIceBackendProxy::NotifyComponentStateChanged { streamId, component, iceState }, destination());
    });
}

void GStreamerIceBackendNice::notifyNewSelectedPair(unsigned streamId, NiceComponentType component)
{
    callOnMainRunLoopAndWait([&, component = niceComponentToRTCIceComponent(component)] {
        connection()->send(Messages::GStreamerIceBackendProxy::NotifyNewSelectedPair { streamId, component }, destination());
    });
}

void GStreamerIceBackendNice::fillLocalCandidateCredentials(const NiceCandidate& candidate, GUniqueOutPtr<NiceCandidate>& result)
{
    result.outPtr() = nice_candidate_copy(&candidate);
    if (candidate.username && candidate.password)
        return;

    GUniqueOutPtr<char> ufrag;
    GUniqueOutPtr<char> password;
    [[maybe_unused]] auto gotCredentials = nice_agent_get_local_credentials(m_agent.get(), candidate.stream_id, &ufrag.outPtr(), &password.outPtr());
    ASSERT(gotCredentials);

    if (!candidate.username)
        result->username = ufrag.release();
    if (!candidate.password)
        result->password = password.release();
}

void GStreamerIceBackendNice::fillRemoteCandidateCredentials(unsigned streamId, const NiceCandidate& candidate, GUniqueOutPtr<NiceCandidate>& result)
{
    auto credentials = m_streamCredentials.getOptional(streamId);
    if (!credentials) [[unlikely]]
        return;

    result.outPtr() = nice_candidate_copy(&candidate);
    result->username = g_strdup(credentials->ufrag.ascii().data());
    result->password = g_strdup(credentials->pwd.ascii().data());
}

void GStreamerIceBackendNice::setForceRelay(bool forceRelay)
{
    g_object_set(m_agent.get(), "force-relay", forceRelay, nullptr);
}

void GStreamerIceBackendNice::setStunServer(const String& uri)
{
    m_stunServer = uri;
}

void GStreamerIceBackendNice::addTurnServer(const String& uri, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
{
    auto validationResult = validateTurnServerURL(uri);
    if (!validationResult.has_value()) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::DataError, makeString("Error validating TURN URI: "_s, validationResult.error().data) }));
        return;
    }
    auto url = *validationResult;
    auto wasAdded = m_turnServers.add(url).isNewEntry;
    if (!wasAdded) {
        completionHandler(false);
        return;
    }

    for (const auto& item : m_streams)
        addTurnServerForStream(item.streamId, url);
    completionHandler(true);
}

void GStreamerIceBackendNice::setTurnServer(const String& uri)
{
    m_turnServer = uri;
}

void GStreamerIceBackendNice::setTos(unsigned streamId, unsigned tos)
{
    nice_agent_set_stream_tos(m_agent.get(), streamId, tos);
}

void GStreamerIceBackendNice::setLocalCredentials(unsigned streamId, const String& ufrag, const String& pwd, CompletionHandler<void(bool)>&& completionHandler)
{
    bool result = nice_agent_set_local_credentials(m_agent.get(), streamId, ufrag.ascii().data(), pwd.ascii().data());
    completionHandler(result);
}

void GStreamerIceBackendNice::setRemoteCredentials(unsigned streamId, const String& ufrag, const String& pwd, CompletionHandler<void(bool)>&& completionHandler)
{
    bool result = nice_agent_set_remote_credentials(m_agent.get(), streamId, ufrag.ascii().data(), pwd.ascii().data());
    m_streamCredentials.add(streamId, StreamCredentials { ufrag, pwd });
    completionHandler(result);
}

void GStreamerIceBackendNice::addStream(unsigned sessionId, CompletionHandler<void(std::optional<unsigned>)>&& completionHandler)
{
    auto streamId = nice_agent_add_stream(m_agent.get(), 1);
    if (!streamId) {
        completionHandler(std::nullopt);
        return;
    }

    if (!m_stunServer.isEmpty()) {
        URL url(m_stunServer);
        ASSERT(url.isValid());
        const auto& host = url.host();
        auto port = url.port().value_or(3478);
        g_object_set(m_agent.get(), "stun-server", host.utf8().data(), "stun-server-port", port, nullptr);
    }

    if (!m_turnServer.isEmpty())
        addTurnServerForStream(streamId, URL(m_turnServer));

    m_streams.append({ sessionId, streamId });
    for (const auto& url : m_turnServers)
        addTurnServerForStream(streamId, url);

    nice_agent_attach_recv(m_agent.get(), streamId, NICE_COMPONENT_TYPE_RTP, m_mainContext.get(), [](NiceAgent*, unsigned streamId, unsigned component, unsigned size, char* buffer, gpointer userData) {
        auto self = reinterpret_cast<GStreamerIceBackendNice*>(userData);
        auto data = WTF::unsafeMakeSpan(reinterpret_cast<const uint8_t*>(buffer), size);
        self->handleIncomingData(streamId, static_cast<NiceComponentType>(component), WTFMove(data));
    }, this);

    completionHandler({ streamId });
}

void GStreamerIceBackendNice::handleIncomingData(unsigned streamId, NiceComponentType component, std::span<const uint8_t>&& data)
{
    callOnMainRunLoopAndWait([&, component = niceComponentToRTCIceComponent(component)] {
        connection()->send(Messages::GStreamerIceBackendProxy::NotifyDataRead { streamId, component, WTFMove(data) }, destination());
    });
}

void GStreamerIceBackendNice::gatherCandidatesForStream(unsigned streamId, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!nice_agent_gather_candidates(m_agent.get(), streamId)) {
        completionHandler(false);
        return;
    }

    completionHandler(true);
}

void GStreamerIceBackendNice::setIsController(bool isController)
{
    g_object_set(m_agent.get(), "controlling-mode", isController, nullptr);
}

Expected<GStreamerIceBackendNice::CandidateAddress, ExceptionData> GStreamerIceBackendNice::getCandidateAddress(StringView candidate)
{
    if (!candidate.startsWith("a=candidate:"_s))
        return makeUnexpected(ExceptionData { ExceptionCode::NotSupportedError, "Candidate does not start with \"a=candidate:\""_s });

    auto tokens = candidate.toStringWithoutCopying().substring(12).split(' ');
    if (tokens.size() < 6)
        return makeUnexpected(ExceptionData { ExceptionCode::DataError, makeString("Candidate \""_s, candidate, "\" tokenization resulted in not enough tokens"_s) });

    CandidateAddress result;
    result.address = tokens[4];

    StringBuilder prefixBuilder;
    for (unsigned i = 0; i < 4; i++)
        prefixBuilder.append(tokens[i]);
    result.prefix = prefixBuilder.toString();

    StringBuilder postfixBuilder;
    for (unsigned i = 5; i < tokens.size(); i++)
        postfixBuilder.append(tokens[i]);
    result.postfix = postfixBuilder.toString();
    return result;
}

void GStreamerIceBackendNice::addIceCandidateToAgent(NiceAgent* agent, unsigned streamId, NiceCandidate& candidate)
{
    // We only support rtcp-mux so rtcp candidates are useless for us.
    if (candidate.component_id == NICE_COMPONENT_TYPE_RTCP)
        return;

    GSList* candidates = nullptr;
    candidates = g_slist_append(candidates, &candidate);
    nice_agent_set_remote_candidates(agent, streamId, candidate.component_id, candidates);
    g_slist_free(candidates);
}

struct ResolveAddressData {
    GRefPtr<GResolver> resolver;
    String address;
    CompletionHandler<void(Expected<String, WebCore::ExceptionData>&&)> callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ResolveAddressData);

struct ResolveAddressDataInner {
    CompletionHandler<void(Expected<String, WebCore::ExceptionData>&&)> callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ResolveAddressDataInner);

void GStreamerIceBackendNice::resolveAddress(String&& address, CompletionHandler<void(Expected<String, WebCore::ExceptionData>&&)>&& completionHandler)
{
    auto data = createResolveAddressData();
    data->resolver = adoptGRef(g_resolver_get_default());
    data->address = WTFMove(address);
    data->callback = WTFMove(completionHandler);
    g_main_context_invoke_full(m_mainContext.get(), G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(+[](gpointer userData) -> gboolean {
        auto data = reinterpret_cast<ResolveAddressData*>(userData);
        auto innerData = createResolveAddressDataInner();
        innerData->callback = WTFMove(data->callback);
        g_resolver_lookup_by_name_async(data->resolver.get(), data->address.utf8().data(), nullptr,
            reinterpret_cast<GAsyncReadyCallback>(+[](GResolver* resolver, GAsyncResult* result, gpointer userData) {
                auto data = reinterpret_cast<ResolveAddressDataInner*>(userData);
                GUniqueOutPtr<GError> error;
                GList* addresses = g_resolver_lookup_by_name_finish(resolver, result, &error.outPtr());
                if (!addresses) {
                    data->callback(makeUnexpected(ExceptionData { ExceptionCode::NetworkError, "Unable to resolve local address"_s }));
                    destroyResolveAddressDataInner(data);
                    return;
                }
                GUniquePtr<char> address(g_inet_address_to_string(G_INET_ADDRESS(addresses->data)));
                data->callback(String::fromUTF8(address.get()));
                g_resolver_free_addresses(addresses);
                destroyResolveAddressDataInner(data);
            }), innerData);
        return G_SOURCE_REMOVE;
    }), data, reinterpret_cast<GDestroyNotify>(destroyResolveAddressData));
}

void GStreamerIceBackendNice::addCandidate(unsigned streamId, const String& candidateSdp, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (candidateSdp.isEmpty()) {
        nice_agent_peer_candidate_gathering_done(m_agent.get(), streamId);
        completionHandler(true);
        return;
    }

    GUniquePtr<NiceCandidate> candidate(nice_agent_parse_remote_candidate_sdp(m_agent.get(), streamId, candidateSdp.utf8().data()));
    if (candidate) {
        addIceCandidateToAgent(m_agent.get(), streamId, *candidate.get());
        completionHandler(true);
        return;
    }

    auto localAddressResult = getCandidateAddress(candidateSdp);
    if (!localAddressResult.has_value()) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::DataError, makeString("Failed to retrieve address from candidate: "_s, localAddressResult.error().message) }));
        return;
    }

    auto localAddress = localAddressResult.value();
    if (!localAddress.address.endsWith(".local"_s)) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::DataError, makeString("Candidate address \""_s, localAddress.address, "\" does not end with '.local'"_s) }));
        return;
    }

    resolveAddress(WTFMove(localAddress.address), [weakAgent = GThreadSafeWeakPtr(m_agent.get()), streamId, completionHandler = WTFMove(completionHandler), prefix = WTFMove(localAddress.prefix), postfix = WTFMove(localAddress.postfix)](auto&& result) mutable {
        if (!result.has_value()) {
            completionHandler(makeUnexpected(ExceptionData { ExceptionCode::DataError, makeString("Failed to resolve local candidate address: "_s, result.error().message) }));
            return;
        }

        GRefPtr agent = weakAgent.get();
        if (!agent) {
            completionHandler(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "ICE agent is gone"_s }));
            return;
        }

        auto address = result.value();
        auto newCandidate = makeString(WTFMove(prefix), ' ', address, ' ', WTFMove(postfix));
        GUniquePtr<NiceCandidate> candidate(nice_agent_parse_remote_candidate_sdp(agent.get(), streamId, newCandidate.utf8().data()));
        if (!candidate) {
            completionHandler(makeUnexpected(ExceptionData { ExceptionCode::DataError, makeString("Could not parse \""_s, newCandidate, '\"') }));
            return;
        }

        addIceCandidateToAgent(agent.get(), streamId, *candidate.get());
        completionHandler(true);
    });
}

Expected<URL, GStreamerIceBackendNice::URLValidationError> GStreamerIceBackendNice::validateTurnServerURL(const String& turnUrl)
{
    URL url(turnUrl);

    if (!url.isValid())
        return makeUnexpected(URLValidationError { ValidationErrorCode::ParseError, { } });

    bool isTLS = false;
    if (url.protocolIs("turns"_s))
        isTLS = true;
    else if (url.protocol() != "turn"_s)
        return makeUnexpected(URLValidationError { ValidationErrorCode::UnknownScheme, url.protocol().toStringWithoutCopying() });

    for (const auto& [key, value] : queryParameters(url)) {
        if (key != "transport"_s)
            return makeUnexpected(URLValidationError { ValidationErrorCode::UnknownParameter, key });
        if (value != "udp"_s && value != "tcp"_s)
            return makeUnexpected(URLValidationError { ValidationErrorCode::UnknownTransport, value });
    }

    if (url.user().isEmpty())
        return makeUnexpected(URLValidationError { ValidationErrorCode::MissingUsername, { } });
    if (url.password().isEmpty())
        return makeUnexpected(URLValidationError { ValidationErrorCode::MissingPassword, { } });

    if (url.port())
        return url;

    if (isTLS)
        url.setPort(5349);
    else
        url.setPort(3478);

    return url;
}

void GStreamerIceBackendNice::addTurnServerForStream(unsigned streamId, const URL& url)
{
    if (!url.host())
        return;

    NiceRelayType relays[4] = { static_cast<NiceRelayType>(0), };
    int nRelay = 0;

    if (url.protocolIs("turns"_s))
        relays[nRelay++] = NICE_RELAY_TYPE_TURN_TLS;
    else {
        ASSERT(url.protocolIs("turn"_s));
        StringView transport;
        for (const auto& [key, value] : queryParameters(url)) {
            if (key == "transport"_s) {
                transport = value;
                break;
            }
        }
        if (!transport || transport == "udp"_s)
            relays[nRelay++] = NICE_RELAY_TYPE_TURN_UDP;
        if (!transport || transport == "tcp"_s)
            relays[nRelay++] = NICE_RELAY_TYPE_TURN_TCP;
    }
    for (int i = 0; i < nRelay; i++) {
        if (!nice_agent_set_relay_info(m_agent.get(), streamId, NICE_COMPONENT_TYPE_RTP, url.host().utf8().data(), *url.port(), url.user().utf8().data(), url.password().utf8().data(), relays[i]))
            g_printerr("Unable to use TURN server %s for stream %u\n", url.string().utf8().data(), streamId);
    }
}

void GStreamerIceBackendNice::sendData(unsigned streamId, RTCIceComponent component, Vector<GStreamerIceBuffer>&& data)
{
    NiceComponentType niceComponent;
    switch (component) {
    case RTCIceComponent::Rtp:
        niceComponent = NICE_COMPONENT_TYPE_RTP;
        break;
    case RTCIceComponent::Rtcp:
        niceComponent = NICE_COMPONENT_TYPE_RTCP;
        break;
    };
    Vector<GOutputVector> buffers;
    Vector<NiceOutputMessage> messages;
    messages.reserveInitialCapacity(data.size());
    buffers.reserveInitialCapacity(data.size());
    unsigned i = 0;
    for (const auto& chunk : data) {
        buffers.append({ chunk.data.data(), chunk.data.size_bytes() });
        messages.append({ &buffers[i], 1 });
        i++;
    }
    auto messageData = messages.span();
    nice_agent_send_messages_nonblocking(m_agent.get(), streamId, niceComponent, messageData.data(), messageData.size(), nullptr, nullptr);
}

void GStreamerIceBackendNice::finalizeStream(unsigned streamId)
{
    nice_agent_attach_recv(m_agent.get(), streamId, NICE_COMPONENT_TYPE_RTP, m_mainContext.get(), nullptr, nullptr);
}

void GStreamerIceBackendNice::populateCandidateStats(WebCore::GStreamerIceCandidateStats& stats, const NiceCandidate& candidate, bool isLocal)
{
    char ipaddr[INET6_ADDRSTRLEN];
    nice_address_to_string(&candidate.addr, ipaddr);
    stats.port = nice_address_get_port(&candidate.addr);
    stats.ipAddress = String::fromUTF8(ipaddr);
    switch (candidate.type) {
    case NICE_CANDIDATE_TYPE_HOST:
        stats.type = RTCIceCandidateType::Host;
        break;
    case NICE_CANDIDATE_TYPE_RELAYED:
        stats.type = RTCIceCandidateType::Relay;
        break;
    case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
        stats.type = RTCIceCandidateType::Prflx;
        break;
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
        stats.type = RTCIceCandidateType::Srflx;
        break;
    };
    stats.priority = candidate.priority;
    switch (candidate.transport) {
    case NICE_CANDIDATE_TRANSPORT_UDP:
        stats.protocol = RTCIceProtocol::Udp;
        break;
    default:
        stats.protocol = RTCIceProtocol::Tcp;
        break;
    };

    stats.foundation = String::fromUTF8(candidate.foundation);

    switch (candidate.transport) {
    case NICE_CANDIDATE_TRANSPORT_UDP:
        break;
    case NICE_CANDIDATE_TRANSPORT_TCP_ACTIVE:
        stats.tcpType = RTCIceTcpCandidateType::Active;
        break;
    case NICE_CANDIDATE_TRANSPORT_TCP_PASSIVE:
        stats.tcpType = RTCIceTcpCandidateType::Passive;
        break;
    case NICE_CANDIDATE_TRANSPORT_TCP_SO:
        stats.tcpType = RTCIceTcpCandidateType::So;
        break;
    };

    stats.usernameFragment = String::fromUTF8(candidate.username);

    if (!isLocal)
        return;

    if (candidate.type == NICE_CANDIDATE_TYPE_RELAYED) {
        NiceAddress relayAddress;
        nice_candidate_relay_address(&candidate, &relayAddress);
        GUniquePtr<char> addr(nice_address_dup_string(&relayAddress));
        stats.relatedAddress = String::fromUTF8(addr.get());
        stats.relatedPort = nice_address_get_port(&relayAddress);

        URL turnServer(m_turnServer);
        if (turnServer.isValid()) {
            auto proto = turnServer.protocol();
            if (proto == "turns"_s)
                stats.relayProtocol = "tls"_s;
            else {
                StringView transport;
                for (const auto& [key, value] : queryParameters(turnServer)) {
                    if (key == "transport"_s) {
                        transport = value;
                        break;
                    }
                }

                if (!transport || transport == "udp"_s)
                    stats.relayProtocol = "udp"_s;
                else if (!transport || transport == "tcp"_s)
                    stats.relayProtocol = "tcp"_s;
            }
        } else
            stats.relayProtocol = "none"_s;
    }
    switch (candidate.type) {
    case NICE_CANDIDATE_TYPE_RELAYED: {
        NiceAddress addr;
        char ipaddr[NICE_ADDRESS_STRING_LEN];
        nice_candidate_relay_address(&candidate, &addr);
        nice_address_to_string(&addr, ipaddr);
        stats.url = String::fromUTF8(ipaddr);
        break;
    }
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE: {
        NiceAddress addr;
        char ipaddr[NICE_ADDRESS_STRING_LEN];
        if (nice_candidate_stun_server_address(&candidate, &addr)) {
            nice_address_to_string(&addr, ipaddr);
            stats.url = String::fromUTF8(ipaddr);
            break;
        }
        URL stunServer(m_stunServer);
        stats.url = stunServer.host().toString();
        break;
    }
    default:
        break;
    };
}

void GStreamerIceBackendNice::getSelectedPairStats(unsigned streamId, CompletionHandler<void(std::optional<WebCore::GStreamerIceCandidateStatsPair>&&)>&& completionHandler)
{
    NiceCandidate* localCandidate = nullptr;
    NiceCandidate* remoteCandidate = nullptr;
    if (!nice_agent_get_selected_pair(m_agent.get(), streamId, NICE_COMPONENT_TYPE_RTP, &localCandidate, &remoteCandidate)) {
        completionHandler(std::nullopt);
        return;
    }

    GUniqueOutPtr<NiceCandidate> filledLocalCandidate, filledRemoteCandidate;
    fillLocalCandidateCredentials(*localCandidate, filledLocalCandidate);
    fillRemoteCandidateCredentials(streamId, *remoteCandidate, filledRemoteCandidate);

    GStreamerIceCandidateStatsPair result;
    result.local.streamId = streamId;
    result.remote.streamId = streamId;
    populateCandidateStats(result.local, *filledLocalCandidate.get(), true);
    populateCandidateStats(result.remote, *filledRemoteCandidate.get(), false);
    completionHandler(WTFMove(result));
}

} // namespace WebKit

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBNICE)
