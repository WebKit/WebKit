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

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerIceCandidateStats.h"
#include "GStreamerIceStream.h"
#include "GStreamerIceTransport.h"
#include "NotImplemented.h"
#include "RTCIceCandidateType.h"
#include "RTCIceProtocol.h"
#include "ScriptExecutionContext.h"
#include "SocketProvider.h"
#include <gst/webrtc/ice.h>
#include <gst/webrtc/webrtc.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

typedef struct _WebKitGstIceAgentPrivate {
    RefPtr<GStreamerIceBackendClient> backendClient;

    GstWebRTCICEOnCandidateFunc onCandidate;
    gpointer onCandidateData;
    GDestroyNotify onCandidateNotify;

    RefPtr<SocketProvider> socketProvider;
    RefPtr<GStreamerIceBackend> iceBackend;
    HashMap<unsigned, GRefPtr<GstWebRTCICEStream>> streams;
    String stunServer;
    String turnServer;
    bool isController;
} WebKitGstIceAgentPrivate;

typedef struct _WebKitGstIceAgent {
    GstWebRTCICE parent;
    WebKitGstIceAgentPrivate* priv;
} WebKitGstIceAgent;

typedef struct _WebKitGstIceAgentClass {
    GstWebRTCICEClass parentClass;
} WebKitGstIceAgentClass;

GST_DEBUG_CATEGORY(webkit_webrtc_ice_agent_debug);
#define GST_CAT_DEFAULT webkit_webrtc_ice_agent_debug

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGstIceAgent, webkit_gst_webrtc_ice_backend, GST_TYPE_WEBRTC_ICE, GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_agent_debug, "webkitwebrtciceagent", 0, "WebRTC ICE agent"))

static void webkitGstWebRTCIceAgentSetOnIceCandidate(GstWebRTCICE* ice, GstWebRTCICEOnCandidateFunc callback, gpointer userData, GDestroyNotify notifyCallback)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    auto priv = backend->priv;
    if (priv->onCandidateNotify)
        priv->onCandidateNotify(priv->onCandidateData);
    priv->onCandidateNotify = notifyCallback;
    priv->onCandidateData = userData;
    priv->onCandidate = callback;
    priv->backendClient->setOnIceCandidateCallback([ice = GThreadSafeWeakPtr(ice)](unsigned sessionId, const String& candidate) mutable {
        auto self = ice.get();
        if (!self)
            return;
        auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(self.get());
        backend->priv->onCandidate(self.get(), sessionId, candidate.utf8().data(), backend->priv->onCandidateData);
    });
}

static void webkitGstWebRTCIceAgentSetForceRelay(GstWebRTCICE* ice, gboolean forceRelay)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return;

    backend->priv->iceBackend->setForceRelay(forceRelay);
}

static void webkitGstWebRTCIceAgentSetStunServer(GstWebRTCICE* ice, const gchar* uri)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return;

    backend->priv->stunServer = String::fromUTF8(uri);
    backend->priv->iceBackend->setStunServer(backend->priv->stunServer);
}

static gchar* webkitGstWebRTCIceAgentGetStunServer(GstWebRTCICE* ice)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return g_strdup(backend->priv->stunServer.utf8().data());
}

static gboolean webkitGstWebRTCIceAgentAddTurnServer(GstWebRTCICE* ice, const gchar* uri)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return FALSE;

    auto result = backend->priv->iceBackend->addTurnServer(String::fromUTF8(uri));
    if (!result.has_value()) {
        GST_ERROR_OBJECT(ice, "%s", result.error().message.utf8().data());
        return FALSE;
    }

    bool wasAdded = *result;
    if (!wasAdded)
        GST_WARNING_OBJECT(ice, "%s was already registered, no need to add it again", uri);
    return wasAdded;
}

static void webkitGstWebRTCIceAgentSetTurnServer(GstWebRTCICE* ice, const gchar* uri)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return;

    backend->priv->turnServer = String::fromUTF8(uri);
    backend->priv->iceBackend->setTurnServer(backend->priv->turnServer);
}

static gchar* webkitGstWebRTCIceAgentGetTurnServer(GstWebRTCICE* ice)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return g_strdup(backend->priv->turnServer.utf8().data());
}

static GstWebRTCICEStream* webkitGstWebRTCIceAgentAddStream(GstWebRTCICE* ice, guint sessionId)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return nullptr;

    auto streamId = backend->priv->iceBackend->addStream(sessionId);
    if (!streamId)
        return nullptr;

    auto stream = GST_WEBRTC_ICE_STREAM(webkitGstWebRTCCreateIceStream(backend, *streamId));
    backend->priv->streams.add(*streamId, GRefPtr(stream));
    return stream;
}

static gboolean webkitGstWebRTCIceAgentGetIsController(GstWebRTCICE* ice)
{
    return WEBKIT_GST_WEBRTC_ICE_BACKEND(ice)->priv->isController;
}

static void webkitGstWebRTCIceAgentSetIsController(GstWebRTCICE* ice, gboolean isController)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    if (!backend->priv->iceBackend)
        return;

    backend->priv->iceBackend->setIsController(isController);
    backend->priv->isController = isController;
}

static void webkitGstWebRTCIceAgentAddCandidate(GstWebRTCICE* ice, GstWebRTCICEStream* iceStream, const gchar* candidate, GstPromise* promise)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    GStreamerIceBackend::AddCandidateCallback completionHandler([promise = GRefPtr(promise)](auto&& result) mutable {
        if (result.hasException()) {
            auto& errorMessage = result.exception().message();
            GUniquePtr<GError> error(g_error_new(GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE, "%s", errorMessage.utf8().data()));

            gst_promise_reply(promise.get(), gst_structure_new("application/x-gst-promise", "error", error.get(), nullptr));
            return;
        }
    }, CompletionHandlerCallThread::AnyThread);
    backend->priv->iceBackend->addCandidate(iceStream->stream_id, String::fromUTF8(candidate), WTFMove(completionHandler));
}

static GstWebRTCICETransport* webkitGstWebRTCIceAgentFindTransport(GstWebRTCICE* ice, GstWebRTCICEStream* stream, GstWebRTCICEComponent component)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    auto iceStream = backend->priv->streams.getOptional(stream->stream_id);
    if (!iceStream)
        return nullptr;
    return webkitGstWebRTCIceStreamFindTransport(iceStream->get(), component);
}

static void webkitGstWebRTCIceAgentSetTos(GstWebRTCICE* ice, GstWebRTCICEStream* stream, guint tos)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    backend->priv->iceBackend->setTos(stream->stream_id, tos);
}

static gboolean webkitGstWebRTCIceAgentSetLocalCredentials(GstWebRTCICE* ice, GstWebRTCICEStream* stream, const gchar* ufrag, const gchar* pwd)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return backend->priv->iceBackend->setLocalCredentials(stream->stream_id, String::fromLatin1(ufrag), String::fromLatin1(pwd));
}

static gboolean webkitGstWebRTCIceAgentSetRemoteCredentials(GstWebRTCICE* ice, GstWebRTCICEStream* stream, const gchar* ufrag, const gchar* pwd)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    return backend->priv->iceBackend->setRemoteCredentials(stream->stream_id, String::fromLatin1(ufrag), String::fromLatin1(pwd));
}

static gboolean webkitGstWebRTCIceAgentGatherCandidates(GstWebRTCICE* ice, GstWebRTCICEStream* stream)
{
    return webkitGstWebRTCIceAgentGatherCandidates(WEBKIT_GST_WEBRTC_ICE_BACKEND(ice), stream->stream_id);
}

static void webkitGstWebRTCIceAgentSetHttpProxy(GstWebRTCICE*, const gchar*)
{
    notImplemented();
}

static gchar* webkitGstWebRTCIceAgentGetHttpProxy(GstWebRTCICE*)
{
    notImplemented();
    return nullptr;
}

static void populateCandidateStats(const GStreamerIceCandidateStats& stats, GstWebRTCICECandidateStats* gstStats)
{
    gstStats->ipaddr = g_strdup(stats.ipAddress.utf8().data());
    gstStats->port = stats.port;
    gstStats->stream_id = stats.streamId;

    switch (stats.type) {
    case RTCIceCandidateType::Host:
        gstStats->type = "host";
        break;
    case RTCIceCandidateType::Prflx:
        gstStats->type = "prflx";
        break;
    case RTCIceCandidateType::Relay:
        gstStats->type = "relay";
        break;
    case RTCIceCandidateType::Srflx:
        gstStats->type = "srflx";
        break;
    };
    switch (stats.protocol) {
    case RTCIceProtocol::Tcp:
        gstStats->proto = "tcp";
        break;
    case RTCIceProtocol::Udp:
        gstStats->proto = "udp";
        break;
    }
    gstStats->relay_proto = stats.relayProtocol.ascii().data();
    gstStats->prio = stats.priority;
    gstStats->url = g_strdup(stats.url.utf8().data());
#if GST_CHECK_VERSION(1, 27, 0)
    gstStats->foundation = g_strdup(stats.foundation.utf8().data());
    gstStats->related_address = g_strdup(stats.relatedAddress.utf8().data());
    gstStats->related_port = stats.relatedPort;
    gstStats->user_name_fragment = g_strdup(stats.usernameFragment.utf8().data());
    if (!stats.tcpType)
        return;
    switch (stats.tcpType) {
    case RTCIceTcpCandidateType::Active:
        gstStats->tcp_type = GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_ACTIVE;
        break;
    case RTCIceTcpCandidateType::Passive:
        gstStats->tcp_type = GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_PASSIVE;
        break;
    case RTCIceTcpCandidateType::So:
        gstStats->tcp_type = GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_SO;
        break;
    };
#endif
}

static gboolean webkitGstWebRTCIceAgentGetSelectedPair(GstWebRTCICE* ice,
    GstWebRTCICEStream* stream, GstWebRTCICECandidateStats** localStats,
    GstWebRTCICECandidateStats** remoteStats)
{
    if (!stream)
        return FALSE;

    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(ice);
    auto stats = backend->priv->iceBackend->getSelectedPairStats(stream->stream_id);
    if (!stats)
        return FALSE;

    *localStats = g_new0(GstWebRTCICECandidateStats, 1);
    populateCandidateStats(stats->local, *localStats);
    *remoteStats = g_new0(GstWebRTCICECandidateStats, 1);
    populateCandidateStats(stats->remote, *remoteStats);
    return TRUE;
}

bool webkitGstWebRTCIceAgentGatherCandidates(WebKitGstIceAgent* agent, unsigned streamId)
{
    if (!agent->priv->iceBackend)
        return false;

    return agent->priv->iceBackend->gatherCandidatesForStream(streamId);
}

GstWebRTCICETransport* webkitGstWebRTCIceAgentCreateTransport(WebKitGstIceAgent* agent, unsigned streamId, GstWebRTCICEComponent component)
{
    if (!agent->priv->iceBackend)
        return nullptr;

    return GST_WEBRTC_ICE_TRANSPORT(webkitGstWebRTCCreateIceTransport(agent, streamId, component, agent->priv->isController));
}

void webkitGstWebRTCIceAgentSend(WebKitGstIceAgent* agent, unsigned streamId, RTCIceComponent component, Vector<GStreamerIceBuffer>&& buffers)
{
    if (!agent->priv->iceBackend)
        return;

    agent->priv->iceBackend->send(streamId, component, WTFMove(buffers));
}

void webkitGstWebRTCIceAgentFinalizeStream(WebKitGstIceAgent* agent, unsigned streamId)
{
    if (!agent->priv->iceBackend)
        return;

    agent->priv->iceBackend->finalizeStream(streamId);
}

static void webkitGstWebRTCIceAgentFinalize(GObject* object)
{
    auto backend = WEBKIT_GST_WEBRTC_ICE_BACKEND(object);
    if (backend->priv->onCandidateNotify)
        backend->priv->onCandidateNotify(backend->priv->onCandidateData);

    G_OBJECT_CLASS(webkit_gst_webrtc_ice_backend_parent_class)->finalize(object);
}

static void webkitGstWebRTCIceAgentConfigure(WebKitGstIceAgent* backend, RefPtr<SocketProvider>&& socketProvider)
{
    auto priv = backend->priv;
    priv->socketProvider = WTFMove(socketProvider);
    priv->backendClient = GStreamerIceBackendClient::create();
    priv->iceBackend = priv->socketProvider->createGStreamerIceBackend(*priv->backendClient);
    priv->backendClient->setOnStreamGatheringDone([weakThis = GThreadSafeWeakPtr(backend)](unsigned streamId) mutable {
        auto self = weakThis.get();
        if (!self)
            return;
        auto stream = self->priv->streams.getOptional(streamId);
        if (!stream)
            return;
        webkitGstWebRTCIceStreamGatheringDone(WEBKIT_GST_WEBRTC_ICE_STREAM(stream->get()));
    });
    priv->backendClient->setOnComponentStateChanged([weakThis = GThreadSafeWeakPtr(backend)](unsigned streamId, RTCIceComponent component, RTCIceConnectionState state) mutable {
        auto self = weakThis.get();
        if (!self)
            return;
        auto stream = self->priv->streams.getOptional(streamId);
        if (!stream)
            return;
        webkitGstWebRTCIceStreamComponentStateChanged(WEBKIT_GST_WEBRTC_ICE_STREAM(stream->get()), component, state);
    });
    priv->backendClient->setOnNewSelectedPair([weakThis = GThreadSafeWeakPtr(backend)](unsigned streamId, RTCIceComponent component) mutable {
        auto self = weakThis.get();
        if (!self)
            return;
        auto stream = self->priv->streams.getOptional(streamId);
        if (!stream)
            return;
        webkitGstWebRTCIceStreamNewSelectedPair(WEBKIT_GST_WEBRTC_ICE_STREAM(stream->get()), component);
    });
    priv->backendClient->setReadDataCallback([weakThis = GThreadSafeWeakPtr(backend)](unsigned streamId, RTCIceComponent component, std::span<const uint8_t>&& data) mutable {
        auto self = weakThis.get();
        if (!self)
            return;
        auto stream = self->priv->streams.getOptional(streamId);
        if (!stream)
            return;
        webkitGstWebRTCIceStreamHandleIncomingData(WEBKIT_GST_WEBRTC_ICE_STREAM(stream->get()), component, WTFMove(data));
    });
}

static void webkit_gst_webrtc_ice_backend_class_init(WebKitGstIceAgentClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
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
    // - close (pending https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/9379)
}

WebKitGstIceAgent* webkitGstWebRTCCreateIceAgent(const String& name, ScriptExecutionContext* context)
{
    if (!context)
        return nullptr;

    RefPtr socketProvider = context->socketProvider();
    if (!socketProvider)
        return nullptr;

    auto backend = reinterpret_cast<WebKitGstIceAgent*>(g_object_new(WEBKIT_TYPE_GST_WEBRTC_ICE_BACKEND, "name", name.ascii().data(), nullptr));

    gst_object_ref_sink(backend);
    webkitGstWebRTCIceAgentConfigure(backend, WTFMove(socketProvider));

    return backend;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
