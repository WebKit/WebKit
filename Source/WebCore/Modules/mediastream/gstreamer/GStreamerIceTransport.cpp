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
#include "GStreamerIceTransport.h"

#if USE(GSTREAMER_WEBRTC) && USE(LIBRICE)

#include "GRefPtrRice.h"
#include "GStreamerCommon.h"
#include "GStreamerIceStream.h"
#include "GUniquePtrRice.h"
#include "RiceUtilities.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/webrtc/ice.h>
#include <gst/webrtc/webrtc.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

typedef struct _WebKitGstIceTransportPrivate {
    GThreadSafeWeakPtr<WebKitGstIceAgent> agent;
    GThreadSafeWeakPtr<WebKitGstIceStream> stream;
    bool isController;
    std::pair<GUniquePtr<RiceCandidate>, GUniquePtr<RiceCandidate>> selectedPair;
    GRefPtr<GstElement> src;
    GRefPtr<GstElement> sink;
} WebKitGstIceTransportPrivate;

typedef struct _WebKitGstIceTransport {
    GstWebRTCICETransport parent;
    WebKitGstIceTransportPrivate* priv;
} WebKitGstIceTransport;

typedef struct _WebKitGstIceTransportClass {
    GstWebRTCICETransportClass parentClass;
} WebKitGstIceTransportClass;

GST_DEBUG_CATEGORY(webkit_webrtc_ice_transport_debug);
#define GST_CAT_DEFAULT webkit_webrtc_ice_transport_debug

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGstIceTransport, webkit_gst_webrtc_ice_transport, GST_TYPE_WEBRTC_ICE_TRANSPORT, GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_transport_debug, "webkitwebrtcricetransport", 0, "WebRTC ICE transport"))

static GstFlowReturn iceTransportHandleSample(WebKitGstIceTransport* self, GstAppSink* sink, bool isPreroll)
{
    GRefPtr<GstSample> sample;
    if (isPreroll)
        sample = adoptGRef(gst_app_sink_try_pull_preroll(sink, 0));
    else
        sample = adoptGRef(gst_app_sink_try_pull_sample(sink, 0));

    if (!sample)
        return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_ERROR;

    auto agent = self->priv->agent.get();
    if (!agent)
        return GST_FLOW_ERROR;

    auto stream = self->priv->stream.get();
    if (!stream)
        return GST_FLOW_ERROR;

    const auto& riceStream = webkitGstWebRTCIceStreamGetRiceStream(stream.get());
    auto component = adoptGRef(rice_stream_get_component(riceStream.get(), 1));
    if (!component)
        return GST_FLOW_ERROR;

    Vector<uint8_t> bufferData;
    auto bufferList = gst_sample_get_buffer_list(sample.get());
    if (GST_IS_BUFFER_LIST(bufferList)) {
        unsigned length = gst_buffer_list_length(bufferList);
        for (unsigned i = 0; i < length; i++) {
            GstMappedBuffer mappedBuffer(gst_buffer_list_get(bufferList, i), GST_MAP_READ);
            bufferData.append(mappedBuffer.mutableSpan<uint8_t>());
        }
    } else {
        GstMappedBuffer mappedBuffer(gst_sample_get_buffer(sample.get()), GST_MAP_READ);
        bufferData.append(mappedBuffer.mutableSpan<uint8_t>());
    }

    RiceTransmit transmit;
    rice_transmit_init(&transmit);

    auto now = WTF::MonotonicTime::now().secondsSinceEpoch();
    auto spanData = bufferData.mutableSpan();
    if (rice_component_send(component.get(), spanData.data(), spanData.size_bytes(), now.nanoseconds(), &transmit) != RICE_ERROR_SUCCESS) {
        GST_ERROR_OBJECT(self, "Failed to send data");
        rice_transmit_clear(&transmit);
        return GST_FLOW_ERROR;
    }

    auto from = riceAddressToString(transmit.from);
    auto to = riceAddressToString(transmit.to);
    auto protocol = riceTransmitTransportToIceProtocol(transmit);
    auto handle = riceTransmitToSharedMemoryHandle(&transmit);
    if (!handle) [[unlikely]] {
        GST_ERROR_OBJECT(self, "Failed to create shared memory handle");
        return GST_FLOW_ERROR;
    }

    auto data = WTFMove(*handle);
    webkitGstWebRTCIceAgentSend(agent.get(), rice_stream_get_id(riceStream.get()), protocol, WTFMove(from), WTFMove(to), WTFMove(data));
    return GST_FLOW_OK;
}

static void webkitGstWebRTCIceTransportConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_gst_webrtc_ice_transport_parent_class)->constructed(object);

    auto self = WEBKIT_GST_WEBRTC_ICE_TRANSPORT(object);
    auto transport = GST_WEBRTC_ICE_TRANSPORT(object);

    transport->role = self->priv->isController ? GST_WEBRTC_ICE_ROLE_CONTROLLING : GST_WEBRTC_ICE_ROLE_CONTROLLED;

    static Atomic<uint32_t> counter = 0;
    auto id = counter.load();

    self->priv->sink = makeGStreamerElement("appsink"_s, makeString("ice-sink-"_s, id));
    self->priv->src = makeGStreamerElement("appsrc"_s, makeString("ice-src-"_s, id));
    transport->sink = self->priv->sink.get();
    transport->src = self->priv->src.get();
    counter.exchangeAdd(1);

    static GstAppSinkCallbacks sinkCallbacks = {
        nullptr, // eos
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            return iceTransportHandleSample(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(userData), sink, true);
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            return iceTransportHandleSample(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(userData), sink, false);
        },
#if GST_CHECK_VERSION(1, 20, 0)
        // new_event
        nullptr,
#endif
#if GST_CHECK_VERSION(1, 24, 0)
        // propose_allocation
        nullptr,
#endif
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(transport->sink), &sinkCallbacks, self, nullptr);
    g_object_set(transport->sink, "buffer-list", TRUE, "sync", FALSE, "async", FALSE, "enable-last-sample", FALSE, nullptr);
}

void webkitGstWebRTCIceTransportHandleIncomingData(WebKitGstIceTransport* transport, GRefPtr<GstBuffer>&& buffer)
{
    auto iceTransport = GST_WEBRTC_ICE_TRANSPORT(transport);
    if (!GST_IS_APP_SRC(iceTransport->src)) [[unlikely]]
        return;

    gst_app_src_push_buffer(GST_APP_SRC(iceTransport->src), buffer.leakRef());
}

void webkitGstWebRTCIceTransportNewSelectedPair(WebKitGstIceTransport* transport, RiceAgentSelectedPair& pair)
{
    transport->priv->selectedPair = { GUniquePtr<RiceCandidate>(rice_candidate_copy(&pair.local)), GUniquePtr<RiceCandidate>(rice_candidate_copy(&pair.remote)) };
    gst_webrtc_ice_transport_selected_pair_change(GST_WEBRTC_ICE_TRANSPORT(transport));
}

static void populateCandidateStats(const RiceCandidate* candidate, GstWebRTCICECandidateStats* gstStats)
{
    if (candidate->address) {
        auto address = riceAddressToString(candidate->address, false);
        gstStats->ipaddr = g_strdup(address.ascii().data());
        gstStats->port = rice_address_get_port(candidate->address);
    }

    switch (candidate->candidate_type) {
    case RICE_CANDIDATE_TYPE_HOST:
        gstStats->type = "host";
        break;
    case RICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
        gstStats->type = "prflx";
        break;
    case RICE_CANDIDATE_TYPE_RELAYED: {
        gstStats->type = "relay";
        // TODO: no API for candidate relay address?
        // gstStats->url =
        break;
    }
    case RICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
        gstStats->type = "srflx";
        // TODO: get stun address from the candidate (no API for this?) and fallback to agent stun server address.
        // gstStats->url =
        break;
    };
    switch (candidate->transport_type) {
    case RICE_TRANSPORT_TYPE_TCP:
        gstStats->proto = "tcp";
        break;
    case RICE_TRANSPORT_TYPE_UDP:
        gstStats->proto = "udp";
        break;
    }
    gstStats->prio = candidate->priority;

#if GST_CHECK_VERSION(1, 27, 0)
    GST_WEBRTC_ICE_CANDIDATE_STATS_FOUNDATION(gstStats) = g_strdup(candidate->foundation);
    if (candidate->related_address) {
        auto relatedAddress = riceAddressToString(candidate->related_address, false);
        GST_WEBRTC_ICE_CANDIDATE_STATS_RELATED_ADDRESS(gstStats) = g_strdup(relatedAddress.ascii().data());
        GST_WEBRTC_ICE_CANDIDATE_STATS_RELATED_PORT(gstStats) = rice_address_get_port(candidate->related_address);
    }
    switch (candidate->tcp_type) {
    case RICE_TCP_TYPE_ACTIVE:
        GST_WEBRTC_ICE_CANDIDATE_STATS_TCP_TYPE(gstStats) = GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_ACTIVE;
        break;
    case RICE_TCP_TYPE_PASSIVE:
        GST_WEBRTC_ICE_CANDIDATE_STATS_TCP_TYPE(gstStats) = GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_PASSIVE;
        break;
    case RICE_TCP_TYPE_SO:
        GST_WEBRTC_ICE_CANDIDATE_STATS_TCP_TYPE(gstStats) = GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_SO;
        break;
    case RICE_TCP_TYPE_NONE:
        break;
    };
#endif
}

#if GST_CHECK_VERSION(1, 27, 0)
static void fillCredentials(const GRefPtr<RiceStream>& stream, bool isLocal, GstWebRTCICECandidateStats*)
{
    GUniquePtr<RiceCredentials> credentials(isLocal ? rice_stream_get_local_credentials(stream.get()) : rice_stream_get_remote_credentials(stream.get()));
    // rice_credentials_get_ufrag_bytes(credentials.get(), stats->);
}
#endif

bool webkitGstWebRTCIceTransportGetSelectedPair(WebKitGstIceTransport* transport, GstWebRTCICECandidateStats** localStats, GstWebRTCICECandidateStats** remoteStats)
{
    const auto& [localCandidate, remoteCandidate] = transport->priv->selectedPair;
    if (!localCandidate || !remoteStats) [[unlikely]]
        return false;

    auto iceStream = transport->priv->stream.get();
    if (!iceStream) [[unlikely]]
        return false;

    auto riceStream = webkitGstWebRTCIceStreamGetRiceStream(iceStream.get());
    auto streamId = rice_stream_get_id(riceStream.get());

    *localStats = g_new0(GstWebRTCICECandidateStats, 1);
    populateCandidateStats(localCandidate.get(), *localStats);
#if GST_CHECK_VERSION(1, 27, 0)
    fillCredentials(riceStream, true, *localStats);
#endif
    (*localStats)->stream_id = streamId;

    *remoteStats = g_new0(GstWebRTCICECandidateStats, 1);
    populateCandidateStats(remoteCandidate.get(), *remoteStats);
#if GST_CHECK_VERSION(1, 27, 0)
    fillCredentials(riceStream, false, *remoteStats);
#endif
    (*remoteStats)->stream_id = streamId;

    return true;
}

static void webkit_gst_webrtc_ice_transport_class_init(WebKitGstIceTransportClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = webkitGstWebRTCIceTransportConstructed;
}

WebKitGstIceTransport* webkitGstWebRTCCreateIceTransport(WebKitGstIceAgent* agent, GThreadSafeWeakPtr<WebKitGstIceStream>&& stream, GstWebRTCICEComponent component, bool isController)
{
    auto transport = reinterpret_cast<WebKitGstIceTransport*>(g_object_new(WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT, "component", component, nullptr));

    gst_object_ref_sink(transport);

    auto priv = transport->priv;
    priv->agent.reset(agent);
    if (auto iceStream = stream.get())
        priv->stream.reset(iceStream.get());
    priv->isController = isController;
    return transport;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
