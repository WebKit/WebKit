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
#include "GStreamerIceAgent.h"

#if USE(GSTREAMER_WEBRTC) && USE(LIBRICE)

#include "GRefPtrGStreamer.h"
#include "GRefPtrRice.h"
#include "GStreamerIceStream.h"
#include "GStreamerWebRTCUtils.h"
#include "GUniquePtrRice.h"
#include "NotImplemented.h"
#include "RiceGioBackend.h"
#include "RiceUtilities.h"
#include "ScriptExecutionContext.h"
#include "SharedMemory.h"
#include "SocketProvider.h"
#include <gst/webrtc/webrtc.h>
#include <wtf/HashSet.h>
#include <wtf/Markable.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

using WebKitGstRiceStream = struct _WebKitGstRiceStream {
    WTF_MAKE_NONCOPYABLE(_WebKitGstRiceStream);
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(_WebKitGstRiceStream);

    _WebKitGstRiceStream(unsigned streamId, GRefPtr<GstWebRTCICEStream>&& stream)
        : riceStreamId(streamId)
        , stream(WTFMove(stream))
    {
    }

    unsigned riceStreamId;
    GRefPtr<GstWebRTCICEStream> stream;
};

typedef struct _WebKitGstIceAgentPrivate {
    RefPtr<RiceBackendClient> backendClient;
    Markable<ScriptExecutionContextIdentifier> identifier;
    RefPtr<SocketProvider> socketProvider;
    GRefPtr<RiceAgent> agent;

    Vector<std::unique_ptr<WebKitGstRiceStream>> streams;

    RefPtr<RunLoop> runLoop;

    Atomic<bool> agentIsClosed;
    GRefPtr<GstPromise> closePromise;

    GstWebRTCICEOnCandidateFunc onCandidate;
    gpointer onCandidateData;
    GDestroyNotify onCandidateNotify;

    RefPtr<RiceBackend> iceBackend;

    String stunServer;
    String turnServer;

    HashSet<URL> turnServers;
    Vector<GRefPtr<RiceTurnConfig>> turnConfigs;
} WebKitGstIceAgentPrivate;

typedef struct _WebKitGstIceAgent {
    GstWebRTCICE parent;
    WebKitGstIceAgentPrivate* priv;
} WebKitGstIceAgent;

typedef struct _WebKitGstIceAgentClass {
    GstWebRTCICEClass parentClass;
} WebKitGstIceAgentClass;

GST_DEBUG_CATEGORY(webkit_webrtc_rice_debug);
#define GST_CAT_DEFAULT webkit_webrtc_rice_debug

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGstIceAgent, webkit_gst_webrtc_ice_backend, GST_TYPE_WEBRTC_ICE, GST_DEBUG_CATEGORY_INIT(webkit_webrtc_rice_debug, "webkitwebrtcrice", 0, "WebRTC Rice ICE backend"))

using namespace WebCore;

static void webkitGstWebRTCIceAgentSetOnIceCandidate(GstWebRTCICE* ice, GstWebRTCICEOnCandidateFunc callback, gpointer userData, GDestroyNotify notifyCallback)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    auto priv = backend->priv;
    if (priv->onCandidateNotify)
        priv->onCandidateNotify(priv->onCandidateData);
    priv->onCandidateNotify = notifyCallback;
    priv->onCandidateData = userData;
    priv->onCandidate = callback;
}

static void webkitGstWebRTCIceAgentSetForceRelay(GstWebRTCICE*, gboolean)
{
    GST_FIXME("Not implemented yet.");
}

static void webkitGstWebRTCIceAgentSetStunServer(GstWebRTCICE* ice, const gchar* uri)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    backend->priv->stunServer = String::fromUTF8(uri);
    GST_INFO_OBJECT(ice, "Setting STUN server address to %s", uri);

    URL url(backend->priv->stunServer);
    ASSERT(url.isValid());
    const auto& host = url.host();
    auto port = url.port().value_or(3478);

    backend->priv->iceBackend->resolveAddress(host.toString(), [weakAgent = GThreadSafeWeakPtr(backend), port](ExceptionOr<String>&& result) mutable {
        auto agent = weakAgent.get();
        if (!agent) [[unlikely]]
            return;
        if (result.hasException()) {
            GST_WARNING_OBJECT(agent.get(), "Unable to configure STUN server on ICE agent: %s", result.exception().message().utf8().data());
            return;
        }
        const auto& iceAgent = agent->priv->agent;
        if (!iceAgent) [[unlikely]]
            return;

        auto& host = result.returnValue();
        auto address = makeString(host, ':', port);
        auto addressString = address.ascii();
        GST_DEBUG_OBJECT(agent.get(), "STUN server address resolved to %s", addressString.data());
        GUniquePtr<RiceAddress> stunAddress(rice_address_new_from_string(addressString.data()));
        if (stunAddress) {
            rice_agent_add_stun_server(iceAgent.get(), RICE_TRANSPORT_TYPE_UDP, stunAddress.get());
            rice_agent_add_stun_server(iceAgent.get(), RICE_TRANSPORT_TYPE_TCP, stunAddress.get());
        } else
            GST_WARNING_OBJECT(agent.get(), "Unable to make use of STUN server %s", address.ascii().data());
    });
}

static gchar* webkitGstWebRTCIceAgentGetStunServer(GstWebRTCICE* ice)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return g_strdup(backend->priv->stunServer.utf8().data());
}

enum class ValidationErrorCode {
    ParseError,
    UnknownScheme,
    UnknownTransport,
    UnknownParameter,
    MissingUsername,
    MissingPassword
};

struct URLValidationError {
    ValidationErrorCode code;
    String data;
};

Expected<URL, URLValidationError> validateTurnServerURL(const String& turnUrl)
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

static void addTurnServer(WebKitGstIceAgent* agent, const URL& url)
{
    GST_INFO_OBJECT(agent, "Adding TURN server %s", url.string().utf8().data());
    if (!url.host())
        return;

    std::array<RiceTransportType, 4> relays = { static_cast<RiceTransportType>(0), };
    unsigned nRelay = 0;
    bool isTurns = url.protocolIs("turns"_s);
    StringView transport;
    for (const auto& [key, value] : queryParameters(url)) {
        if (key == "transport"_s) {
            transport = value;
            break;
        }
    }
    if (!transport || transport == "udp"_s)
        relays[nRelay++] = RICE_TRANSPORT_TYPE_UDP;
    if (!transport || transport == "tcp"_s)
        relays[nRelay++] = RICE_TRANSPORT_TYPE_TCP;

    RELEASE_ASSERT(url.port());
    auto port = url.port().value();
    agent->priv->iceBackend->resolveAddress(url.host().toString(), [weakAgent = GThreadSafeWeakPtr(agent), isTurns, port, nRelay, user = url.user(), password = url.password(), relays = WTFMove(relays)](ExceptionOr<String>&& result) mutable {
        auto agent = weakAgent.get();
        if (!agent) [[unlikely]]
            return;
        if (result.hasException()) {
            GST_WARNING_OBJECT(agent.get(), "Unable to configure TURN server on ICE agent: %s", result.exception().message().utf8().data());
            return;
        }
        const auto& iceAgent = agent->priv->agent;
        if (!iceAgent) [[unlikely]]
            return;

        auto turnAddress = makeString(result.returnValue(), ':', port);
        auto turnAddressString = turnAddress.ascii();
        GST_DEBUG_OBJECT(agent.get(), "TURN address resolved to %s", turnAddressString.data());
        GUniquePtr<RiceAddress> address(rice_address_new_from_string(turnAddressString.data()));
        GUniquePtr<RiceCredentials> credentials(rice_credentials_new(g_strdup(user.utf8().data()), g_strdup(password.utf8().data())));
        GRefPtr<RiceTlsConfig> tlsConfig;
        if (isTurns)
            tlsConfig = adoptGRef(rice_tls_config_new_rustls_with_ip(address.get()));

        auto family = rice_address_get_family(address.get());
        for (unsigned i = 0; i < nRelay; i++) {
            auto config = adoptGRef(rice_turn_config_new(relays[i], address.get(), credentials.get(), 1, &family, tlsConfig.get()));
            agent->priv->turnConfigs.append(WTFMove(config));
        }
    });
}

static gboolean webkitGstWebRTCIceAgentAddTurnServer(GstWebRTCICE* ice, const gchar* uri)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return FALSE;

    auto validationResult = validateTurnServerURL(String::fromUTF8(uri));
    if (!validationResult.has_value()) {
        GST_ERROR_OBJECT(ice, "Error validating TURN URI: %s", validationResult.error().data.utf8().data());
        return FALSE;
    }
    auto url = *validationResult;
    auto wasAdded = backend->priv->turnServers.add(url).isNewEntry;
    if (!wasAdded) {
        GST_DEBUG_OBJECT(ice, "%s was already registered, no need to add it again", uri);
        return FALSE;
    }

    addTurnServer(backend, url);
    return TRUE;
}

static void webkitGstWebRTCIceAgentSetTurnServer(GstWebRTCICE* ice, const gchar* uri)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return;

    auto turnUrl = String::fromUTF8(uri);
    auto validationResult = validateTurnServerURL(turnUrl);
    if (!validationResult.has_value()) {
        GST_ERROR_OBJECT(ice, "Error validating TURN URI: %s", validationResult.error().data.utf8().data());
        return;
    }
    backend->priv->turnServer = WTFMove(turnUrl);
}

static gchar* webkitGstWebRTCIceAgentGetTurnServer(GstWebRTCICE* ice)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return g_strdup(backend->priv->turnServer.utf8().data());
}

static GstWebRTCICEStream* webkitGstWebRTCIceAgentAddStream(GstWebRTCICE* ice, guint)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return nullptr;

    auto riceStream = adoptGRef(rice_agent_add_stream(backend->priv->agent.get()));
    auto streamId = static_cast<unsigned>(rice_stream_get_id(riceStream.get()));
    [[maybe_unused]] auto component = adoptGRef(rice_stream_add_component(riceStream.get()));

    auto stream = GST_WEBRTC_ICE_STREAM(webkitGstWebRTCCreateIceStream(backend, WTFMove(riceStream)));
    backend->priv->streams.append(WTF::makeUnique<WebKitGstRiceStream>(streamId, GRefPtr(stream)));
    return stream;
}

static gboolean webkitGstWebRTCIceAgentGetIsController(GstWebRTCICE* ice)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return static_cast<gboolean>(rice_agent_get_controlling(backend->priv->agent.get()));
}

static void webkitGstWebRTCIceAgentSetIsController(GstWebRTCICE*, gboolean)
{
    GST_FIXME("Not implemented yet.");
}

struct CandidateAddress {
    String prefix;
    String address;
    String suffix;
};

static Expected<CandidateAddress, ExceptionData> getCandidateAddress(StringView candidate)
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

    StringBuilder suffixBuilder;
    for (unsigned i = 5; i < tokens.size(); i++)
        suffixBuilder.append(tokens[i]);
    result.suffix = suffixBuilder.toString();
    return result;
}

static void webkitGstWebRTCIceAgentAddCandidate(GstWebRTCICE* ice, GstWebRTCICEStream* iceStream, const gchar* candidateSdp, GstPromise* promise)
{
    GRefPtr riceStream = webkitGstWebRTCIceStreamGetRiceStream(WEBKIT_GST_WEBRTC_ICE_STREAM(iceStream));
    if (!riceStream) [[unlikely]] {
        GST_DEBUG_OBJECT(ice, "ICE stream not found");
        gst_promise_reply(promise, nullptr);
        return;
    }
    if (!candidateSdp) {
        GST_DEBUG_OBJECT(ice, "Signaling end-of-candidates");
        rice_stream_end_of_remote_candidates(riceStream.get());
        gst_promise_reply(promise, nullptr);
        return;
    }

    GST_DEBUG_OBJECT(ice, "Processing SDP ICE candidate: %s", candidateSdp);
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    GUniquePtr<RiceCandidate> candidate(rice_candidate_new_from_sdp_string(candidateSdp));
    if (candidate) {
        GST_DEBUG_OBJECT(ice, "Adding remote candidate: %s", candidateSdp);
        rice_stream_add_remote_candidate(riceStream.get(), candidate.get());
        g_main_context_wakeup(backend->priv->runLoop->mainContext());
        gst_promise_reply(promise, nullptr);
        return;
    }

    GST_DEBUG_OBJECT(ice, "Failed to build RiceCandidate from SDP, it might contain a FQDN. Attempting address resolution");
    auto localAddressResult = getCandidateAddress(StringView::fromLatin1(candidateSdp));
    if (!localAddressResult.has_value()) {
        auto errorMessage = makeString("Failed to retrieve address from candidate: "_s, localAddressResult.error().message);
        auto errorMessageString = errorMessage.utf8();
        GST_ERROR_OBJECT(ice, "%s", errorMessageString.data());
        GUniquePtr<GError> error(g_error_new(GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE, "%s", errorMessageString.data()));
        gst_promise_reply(promise, gst_structure_new("application/x-gst-promise", "error", G_TYPE_ERROR, error.get(), nullptr));
        return;
    }

    auto localAddress = localAddressResult.value();
    if (!localAddress.address.endsWith(".local"_s)) {
        auto errorMessage = makeString("Candidate address \""_s, localAddress.address, "\" does not end with '.local'"_s);
        auto errorMessageString = errorMessage.utf8();
        GST_ERROR_OBJECT(ice, "%s", errorMessageString.data());
        GUniquePtr<GError> error(g_error_new(GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE, "%s", errorMessageString.data()));
        gst_promise_reply(promise, gst_structure_new("application/x-gst-promise", "error", G_TYPE_ERROR, error.get(), nullptr));
        return;
    }

    auto iceBackend = backend->priv->iceBackend;
    if (!iceBackend) [[unlikely]] {
        gst_promise_reply(promise, nullptr);
        return;
    }

    iceBackend->resolveAddress(WTFMove(localAddress.address), [promise = GRefPtr(promise), riceStream = WTFMove(riceStream), prefix = WTFMove(localAddress.prefix), suffix = WTFMove(localAddress.suffix), backend](auto&& result) mutable {
        if (result.hasException()) {
            auto& errorMessage = result.exception().message();
            auto errorMessageString = errorMessage.utf8();
            GST_ERROR("%s", errorMessageString.data());
            GUniquePtr<GError> error(g_error_new(GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE, "%s", errorMessageString.data()));

            gst_promise_reply(promise.get(), gst_structure_new("application/x-gst-promise", "error", G_TYPE_ERROR, error.get(), nullptr));
            return;
        }

        auto newCandidateSdp = makeString(prefix, ' ', result.returnValue(), ' ', suffix);
        auto newCandidateSdpString = newCandidateSdp.utf8();
        GST_DEBUG("SDP for resolved address: %s", newCandidateSdpString.data());
        GUniquePtr<RiceCandidate> newCandidate(rice_candidate_new_from_sdp_string(newCandidateSdpString.data()));
        if (newCandidate) {
            rice_stream_add_remote_candidate(riceStream.get(), newCandidate.get());
            gst_promise_reply(promise.get(), nullptr);
            g_main_context_wakeup(backend->priv->runLoop->mainContext());
        } else {
            auto errorMessage = "Unable to create Rice candidate from SDP"_s;
            GST_ERROR("%s", errorMessage.characters());
            GUniquePtr<GError> error(g_error_new(GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE, "%s", errorMessage.characters()));
            gst_promise_reply(promise.get(), gst_structure_new("application/x-gst-promise", "error", G_TYPE_ERROR, error.get(), nullptr));
        }
    });
}

static GstWebRTCICETransport* webkitGstWebRTCIceAgentFindTransport(GstWebRTCICE*, GstWebRTCICEStream* stream, GstWebRTCICEComponent component)
{
    return webkitGstWebRTCIceStreamFindTransport(stream, component);
}

static void webkitGstWebRTCIceAgentSetTos(GstWebRTCICE*, GstWebRTCICEStream*, guint)
{
    GST_FIXME("Not implemented yet.");
}

static gboolean webkitGstWebRTCIceAgentSetLocalCredentials(GstWebRTCICE*, GstWebRTCICEStream* stream, const gchar* ufrag, const gchar* pwd)
{
    webkitGstWebRTCIceStreamSetLocalCredentials(WEBKIT_GST_WEBRTC_ICE_STREAM(stream), String::fromLatin1(ufrag), String::fromLatin1(pwd));
    return TRUE;
}

static gboolean webkitGstWebRTCIceAgentSetRemoteCredentials(GstWebRTCICE*, GstWebRTCICEStream* stream, const gchar* ufrag, const gchar* pwd)
{
    webkitGstWebRTCIceStreamSetRemoteCredentials(WEBKIT_GST_WEBRTC_ICE_STREAM(stream), String::fromLatin1(ufrag), String::fromLatin1(pwd));
    return TRUE;
}

static gboolean webkitGstWebRTCIceAgentGatherCandidates(GstWebRTCICE*, GstWebRTCICEStream* stream)
{
    return webkitGstWebRTCIceStreamGatherCandidates(WEBKIT_GST_WEBRTC_ICE_STREAM(stream));
}

static void webkitGstWebRTCIceAgentSetHttpProxy(GstWebRTCICE*, const gchar*)
{
    GST_FIXME("Not implemented yet.");
}

static gchar* webkitGstWebRTCIceAgentGetHttpProxy(GstWebRTCICE*)
{
    GST_FIXME("Not implemented yet.");
    return nullptr;
}

static ASCIILiteral getRelayProtocol(WebKitGstIceAgent* agent)
{
    if (agent->priv->turnServer.isEmpty())
        return "none"_s;

    URL url(agent->priv->turnServer);
    if (url.protocolIs("turns"_s))
        return "tls"_s;

    ASSERT(url.protocolIs("turn"_s));
    StringView transport;
    for (const auto& [key, value] : queryParameters(url)) {
        if (key == "transport"_s) {
            transport = value;
            break;
        }
    }
    if (!transport || transport == "udp"_s)
        return "udp"_s;
    if (!transport || transport == "tcp"_s)
        return "tcp"_s;

    return "none"_s;
}

static gboolean webkitGstWebRTCIceAgentGetSelectedPair(GstWebRTCICE* ice, GstWebRTCICEStream* stream, GstWebRTCICECandidateStats** localStats, GstWebRTCICECandidateStats** remoteStats)
{
    if (!stream)
        return FALSE;

    auto result = webkitGstWebRTCIceStreamGetSelectedPair(WEBKIT_GST_WEBRTC_ICE_STREAM(stream), localStats, remoteStats);
    if (!result)
        return result;

    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    auto relayProtocol = getRelayProtocol(backend);
    (*localStats)->relay_proto = relayProtocol;
    (*remoteStats)->relay_proto = relayProtocol;
    return TRUE;
}

void webkitGstWebRTCIceAgentClosed(WebKitGstIceAgent* agent)
{
    agent->priv->agentIsClosed.exchange(true);
    agent->priv->streams.clear();

    if (!agent->priv->closePromise)
        return;

    gst_promise_reply(agent->priv->closePromise.get(), nullptr);
    agent->priv->closePromise.clear();
}

#if GST_CHECK_VERSION(1, 27, 0)
static void webkitGstWebRTCIceAgentClose(GstWebRTCICE* ice, GstPromise* promise)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);

    auto isClosed = backend->priv->agentIsClosed.load();
    if (isClosed)
        return;

    bool shouldWait = promise == nullptr;
    backend->priv->closePromise = adoptGRef(promise);
    auto now = WTF::MonotonicTime::now().secondsSinceEpoch();
    rice_agent_close(backend->priv->agent.get(), now.nanoseconds());
    webkitGstWebRTCIceAgentWakeup(backend);

    isClosed = backend->priv->agentIsClosed.load();
    if (!shouldWait || isClosed)
        return;

    while (!isClosed) {
        g_main_context_iteration(backend->priv->runLoop->mainContext(), FALSE);
        isClosed = backend->priv->agentIsClosed.load();
    }
}
#endif

static void webkitGstWebRTCIceAgentFinalize(GObject* object)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(object);

    if (backend->priv->onCandidateNotify)
        backend->priv->onCandidateNotify(backend->priv->onCandidateData);
    G_OBJECT_CLASS(webkit_gst_webrtc_ice_backend_parent_class)->finalize(object);
}

static void webkitGstWebRTCIceAgentConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_gst_webrtc_ice_backend_parent_class)->constructed(object);

    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(object);
    auto priv = backend->priv;

    priv->agentIsClosed.exchange(false);

    static Atomic<uint32_t> counter = 0;
    auto id = counter.load();
    auto threadName = makeString("webrtc-rice-"_s, id);
    auto threadNameCString = threadName.ascii();
    counter.exchangeAdd(1);

    static HashSet<CString> threadNames;
    threadNames.add(threadNameCString);

    // FIXME: We are abusing ASCIILiteral here, it would be good to have String support for the
    // RunLoop name.
    priv->runLoop = RunLoop::create(ASCIILiteral::fromLiteralUnsafe(threadNameCString.data()));
    priv->agent = adoptGRef(rice_agent_new(true, true));
}

static void findStreamAndApply(const Vector<std::unique_ptr<WebKitGstRiceStream>>& streams, unsigned streamId, Function<void(const WebKitGstIceStream*)> callback)
{
    auto index = streams.findIf([streamId](const auto& item) {
        return item->riceStreamId == streamId;
    });
    if (index == notFound) [[unlikely]]
        return;
    callback(WEBKIT_GST_WEBRTC_ICE_STREAM(streams[index]->stream.get()));
}

static void webkitGstWebRTCIceAgentConfigure(WebKitGstIceAgent* backend, RefPtr<SocketProvider>&& socketProvider, ScriptExecutionContextIdentifier identifier)
{
    auto priv = backend->priv;
    priv->socketProvider = WTFMove(socketProvider);
    priv->identifier = identifier;
    priv->backendClient = RiceBackendClient::create();
    priv->iceBackend = priv->socketProvider->createRiceBackend(*priv->backendClient);
    priv->backendClient->setIncomingDataCallback([weakThis = GThreadSafeWeakPtr(backend)](unsigned streamId, RTCIceProtocol protocol, String&& from, String&& to, SharedMemory::Handle&& data) mutable {
        auto self = weakThis.get();
        if (!self)
            return;
        findStreamAndApply(self->priv->streams, streamId, [protocol, from = WTFMove(from), to = WTFMove(to), data = WTFMove(data)](const auto* stream) mutable {
            webkitGstWebRTCIceStreamHandleIncomingData(stream, protocol, WTFMove(from), WTFMove(to), WTFMove(data));
        });
    });

    auto source = agentSourceNew(GThreadSafeWeakPtr(backend));
    g_source_attach(source.get(), priv->runLoop->mainContext());
}

static void webkit_gst_webrtc_ice_backend_class_init(WebKitGstIceAgentClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = webkitGstWebRTCIceAgentConstructed;
    gobjectClass->finalize = webkitGstWebRTCIceAgentFinalize;

    auto iceClass = GST_WEBRTC_ICE_CLASS(klass);
    iceClass->set_on_ice_candidate = webkitGstWebRTCIceAgentSetOnIceCandidate;
    iceClass->set_force_relay = webkitGstWebRTCIceAgentSetForceRelay;
    iceClass->set_stun_server = webkitGstWebRTCIceAgentSetStunServer;
    iceClass->get_stun_server = webkitGstWebRTCIceAgentGetStunServer;
    iceClass->add_turn_server = webkitGstWebRTCIceAgentAddTurnServer;
    iceClass->add_stream = webkitGstWebRTCIceAgentAddStream;
    iceClass->get_is_controller = webkitGstWebRTCIceAgentGetIsController;
    iceClass->set_is_controller = webkitGstWebRTCIceAgentSetIsController;
    iceClass->add_candidate = webkitGstWebRTCIceAgentAddCandidate;
    iceClass->find_transport = webkitGstWebRTCIceAgentFindTransport;
    iceClass->gather_candidates = webkitGstWebRTCIceAgentGatherCandidates;
    iceClass->get_turn_server = webkitGstWebRTCIceAgentGetTurnServer;
    iceClass->set_turn_server = webkitGstWebRTCIceAgentSetTurnServer;
    iceClass->set_tos = webkitGstWebRTCIceAgentSetTos;
    iceClass->set_local_credentials = webkitGstWebRTCIceAgentSetLocalCredentials;
    iceClass->set_remote_credentials = webkitGstWebRTCIceAgentSetRemoteCredentials;
    iceClass->set_http_proxy = webkitGstWebRTCIceAgentSetHttpProxy;
    iceClass->get_http_proxy = webkitGstWebRTCIceAgentGetHttpProxy;
    iceClass->get_selected_pair = webkitGstWebRTCIceAgentGetSelectedPair;
    // TODO:
    // - get_local_candidates
    // - get_remote_candidates
#if GST_CHECK_VERSION(1, 27, 0)
    iceClass->close = webkitGstWebRTCIceAgentClose;
#endif
}

WebKitGstIceAgent* webkitGstWebRTCCreateIceAgent(const String& name, ScriptExecutionContext* context)
{
    if (!context)
        return nullptr;

    RefPtr socketProvider = context->socketProvider();
    if (!socketProvider)
        return nullptr;

    auto agent = reinterpret_cast<WebKitGstIceAgent*>(g_object_new(WEBKIT_TYPE_GST_WEBRTC_ICE_BACKEND, "name", name.ascii().data(), nullptr));
    gst_object_ref_sink(agent);
    webkitGstWebRTCIceAgentConfigure(agent, WTFMove(socketProvider), context->identifier());
    return agent;
}

const GRefPtr<RiceAgent>& webkitGstWebRTCIceAgentGetRiceAgent(WebKitGstIceAgent* agent)
{
    return agent->priv->agent;
}

Vector<GRefPtr<RiceTurnConfig>> webkitGstWebRTCIceAgentGetTurnConfigs(WebKitGstIceAgent* agent)
{
    Vector<GRefPtr<RiceTurnConfig>> result;
    result.reserveInitialCapacity(agent->priv->turnConfigs.size());
    for (const auto& config : agent->priv->turnConfigs)
        result.append(GRefPtr(config));

    return result;
}

Vector<String> webkitGstWebRTCIceAgentGatherSocketAddresses(WebKitGstIceAgent* agent, unsigned streamId)
{
    auto backend = agent->priv->iceBackend;
    if (!backend)
        return { };

    return backend->gatherSocketAddresses(streamId);
}

GstWebRTCICETransport* webkitGstWebRTCIceAgentCreateTransport(WebKitGstIceAgent* agent, GThreadSafeWeakPtr<WebKitGstIceStream>&& stream, RTCIceComponent component)
{
    if (!agent->priv->iceBackend)
        return nullptr;

    GstWebRTCICEComponent gstComponent;
    switch (component) {
    case RTCIceComponent::Rtp:
        gstComponent = GST_WEBRTC_ICE_COMPONENT_RTP;
        break;
    case RTCIceComponent::Rtcp:
        gstComponent = GST_WEBRTC_ICE_COMPONENT_RTCP;
        break;
    };
    auto isController = webkitGstWebRTCIceAgentGetIsController(GST_WEBRTC_ICE(agent));
    return GST_WEBRTC_ICE_TRANSPORT(webkitGstWebRTCCreateIceTransport(agent, WTFMove(stream), gstComponent, isController));
}

void webkitGstWebRTCIceAgentSend(WebKitGstIceAgent* agent, unsigned streamId, RTCIceProtocol protocol, String&& from, String&& to, SharedMemory::Handle&& data)
{
    auto backend = agent->priv->iceBackend;
    if (!backend)
        return;

    backend->send(streamId, protocol, WTFMove(from), WTFMove(to), WTFMove(data));
}

void webkitGstWebRTCIceAgentWakeup(WebKitGstIceAgent* agent)
{
    g_main_context_wakeup(agent->priv->runLoop->mainContext());
}

void webkitGstWebRTCIceAgentFinalizeStream(WebKitGstIceAgent* agent, unsigned streamId)
{
    auto backend = agent->priv->iceBackend;
    if (!backend)
        return;

    backend->finalizeStream(streamId);

    agent->priv->streams.removeAllMatching([streamId](const auto& item) {
        return item->riceStreamId == streamId;
    });
}

void webkitGstWebRTCIceAgentGatheringDoneForStream(WebKitGstIceAgent* agent, unsigned streamId)
{
    findStreamAndApply(agent->priv->streams, streamId, [](const auto* stream) {
        webkitGstWebRTCIceStreamGatheringDone(stream);
    });
}

void webkitGstWebRTCIceAgentLocalCandidateGatheredForStream(WebKitGstIceAgent* agent, unsigned streamId, RiceAgentGatheredCandidate& candidate)
{
    findStreamAndApply(agent->priv->streams, streamId, [&](const auto* stream) {
        GUniquePtr<char> sdpCandidate(rice_candidate_to_sdp_string(&candidate.gathered.candidate));
        auto sdp = CStringView::unsafeFromUTF8(sdpCandidate.get());
        ASSERT(startsWith(sdp.span(), "a="_s));
        String strippedSdp(sdp.span().subspan(2));

        agent->priv->onCandidate(GST_WEBRTC_ICE(agent), streamId, strippedSdp.utf8().data(), agent->priv->onCandidateData);
        webkitGstWebRTCIceStreamAddLocalGatheredCandidate(stream, candidate.gathered);
    });
}

void webkitGstWebRTCIceAgentNewSelectedPairForStream(WebKitGstIceAgent* agent, unsigned streamId, RiceAgentSelectedPair& selectedPair)
{
    findStreamAndApply(agent->priv->streams, streamId, [&](const auto* stream) {
        webkitGstWebRTCIceStreamNewSelectedPair(stream, selectedPair);
    });
}

void webkitGstWebRTCIceAgentComponentStateChangedForStream(WebKitGstIceAgent* agent, unsigned streamId, RiceAgentComponentStateChange& change)
{
    findStreamAndApply(agent->priv->streams, streamId, [&](const auto* stream) {
        webkitGstWebRTCIceStreamComponentStateChanged(stream, change);
    });
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
