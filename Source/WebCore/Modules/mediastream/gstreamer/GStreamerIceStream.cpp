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
#include "GStreamerIceStream.h"

#if USE(GSTREAMER_WEBRTC) && USE(LIBRICE)

#include "GRefPtrGStreamer.h"
#include "GRefPtrRice.h"
#include "GUniquePtrRice.h"
#include "RTCIceComponent.h"
#include "RiceUtilities.h"
#include "SharedBuffer.h"
#include <gst/webrtc/ice.h>
#include <gst/webrtc/webrtc.h>
#include <wtf/MonotonicTime.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

typedef struct _WebKitGstIceStreamPrivate {
    GThreadSafeWeakPtr<WebKitGstIceAgent> agent;
    GRefPtr<RiceStream> riceStream;
    GRefPtr<GstWebRTCICETransport> rtpTransport;
    GRefPtr<GstWebRTCICETransport> rtcpTransport;
    bool haveLocalCredentials { false };
    bool haveRemoteCredentials { false };
    bool gatheringRequested { false };
    bool gatheringStarted { false };
} WebKitGstIceStreamPrivate;

typedef struct _WebKitGstIceStream {
    GstWebRTCICEStream parent;
    WebKitGstIceStreamPrivate* priv;
} WebKitGstIceStream;

typedef struct _WebKitGstIceStreamClass {
    GstWebRTCICEStreamClass parentClass;
} WebKitGstIceStreamClass;

GST_DEBUG_CATEGORY(webkit_webrtc_ice_stream_debug);
#define GST_CAT_DEFAULT webkit_webrtc_ice_stream_debug

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGstIceStream, webkit_gst_webrtc_ice_stream, GST_TYPE_WEBRTC_ICE_STREAM, GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_stream_debug, "webkitwebrtcricestream", 0, "WebRTC ICE stream"))

GstWebRTCICETransport* webkitGstWebRTCIceStreamFindTransport(GstWebRTCICEStream* ice, GstWebRTCICEComponent component)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    auto agent = stream->priv->agent.get();
    if (!agent)
        return nullptr;

    switch (component) {
    case GST_WEBRTC_ICE_COMPONENT_RTP:
        if (!stream->priv->rtpTransport)
            stream->priv->rtpTransport = adoptGRef(GST_WEBRTC_ICE_TRANSPORT(webkitGstWebRTCIceAgentCreateTransport(agent.get(), GThreadSafeWeakPtr(stream), RTCIceComponent::Rtp)));
        return stream->priv->rtpTransport.ref();
    case GST_WEBRTC_ICE_COMPONENT_RTCP:
        if (!stream->priv->rtcpTransport)
            stream->priv->rtcpTransport = adoptGRef(GST_WEBRTC_ICE_TRANSPORT(webkitGstWebRTCIceAgentCreateTransport(agent.get(), GThreadSafeWeakPtr(stream), RTCIceComponent::Rtcp)));
        return stream->priv->rtcpTransport.ref();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void webkitGstWebRTCIceStreamGatheringDone(const WebKitGstIceStream* ice)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    if (stream->priv->rtpTransport)
        gst_webrtc_ice_transport_gathering_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE);
    if (stream->priv->rtcpTransport)
        gst_webrtc_ice_transport_gathering_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtcpTransport.get()), GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE);
}

void webkitGstWebRTCIceStreamAddLocalGatheredCandidate(const WebKitGstIceStream* ice, RiceGatheredCandidate& candidate)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    rice_stream_add_local_gathered_candidate(stream->priv->riceStream.get(), &candidate);
}

void webkitGstWebRTCIceStreamNewSelectedPair(const WebKitGstIceStream* ice, RiceAgentSelectedPair& pair)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    if (!stream->priv->rtpTransport) [[unlikely]]
        return;
    webkitGstWebRTCIceTransportNewSelectedPair(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), pair);
}

void webkitGstWebRTCIceStreamComponentStateChanged(const WebKitGstIceStream* ice, RiceAgentComponentStateChange& change)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    if (!stream->priv->rtpTransport) [[unlikely]]
        return;

    GstWebRTCICEConnectionState gstState;
    switch (change.state) {
    case RICE_COMPONENT_CONNECTION_STATE_NEW:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_NEW;
        break;
    case RICE_COMPONENT_CONNECTION_STATE_CONNECTING:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING;
        break;
    case RICE_COMPONENT_CONNECTION_STATE_CONNECTED:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED;
        break;
    case RICE_COMPONENT_CONNECTION_STATE_FAILED:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_FAILED;
        break;
    }

    gst_webrtc_ice_transport_connection_state_change(stream->priv->rtpTransport.get(), gstState);
}

static gboolean webkitGstWebRTCIceStreamGatherCandidates(GstWebRTCICEStream* ice)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);

    if (stream->priv->rtpTransport)
        gst_webrtc_ice_transport_gathering_state_change(stream->priv->rtpTransport.get(), GST_WEBRTC_ICE_GATHERING_STATE_GATHERING);

    if (stream->priv->rtcpTransport)
        gst_webrtc_ice_transport_gathering_state_change(stream->priv->rtcpTransport.get(), GST_WEBRTC_ICE_GATHERING_STATE_GATHERING);

    auto agent = stream->priv->agent.get();
    if (!agent)
        return FALSE;

    auto addresses = webkitGstWebRTCIceAgentGatherSocketAddresses(agent.get(), ice->stream_id);
    auto component = adoptGRef(rice_stream_get_component(stream->priv->riceStream.get(), 1));

    Vector<GUniquePtr<RiceAddress>> riceAddresses;
    Vector<RiceTransportType> riceTransports;
    for (const auto& address : addresses) {
        GUniquePtr<RiceAddress> addr(rice_address_new_from_string(address.ascii().data()));
        if (!addr) [[unlikely]]
            continue;

        riceAddresses.append(WTFMove(addr));
        riceTransports.append(RICE_TRANSPORT_TYPE_UDP);
        riceTransports.append(RICE_TRANSPORT_TYPE_TCP);
    }
    Vector<const RiceAddress*> riceAddressValues;
    for (const auto& addr : riceAddresses)
        riceAddressValues.append(addr.get());
    auto addressDataStorage = riceAddressValues.span();
    auto riceTransportStorage = riceTransports.span();

    Vector<GUniquePtr<RiceAddress>> turnAddresses;
    auto turnConfigs = webkitGstWebRTCIceAgentGetTurnConfigs(agent.get());
    for (const auto& config : turnConfigs) {
        GUniquePtr<RiceAddress> address(rice_turn_config_get_addr(config.get()));
        turnAddresses.append(WTFMove(address));
    }

    Vector<const RiceAddress*> turnAddressValues;
    for (const auto& addr : turnAddresses)
        turnAddressValues.append(addr.get());
    auto turnAddressDataStorage = turnAddressValues.span();

    Vector<RiceTurnConfig*> turnConfigValues;
    for (const auto& config : turnConfigs)
        turnConfigValues.append(config.get());
    auto turnConfigDataStorage = turnConfigValues.span();

    auto error = rice_component_gather_candidates(component.get(), riceAddressValues.size(), addressDataStorage.data(), riceTransportStorage.data(), turnConfigs.size(), turnAddressDataStorage.data(), turnConfigDataStorage.data());
    webkitGstWebRTCIceAgentWakeup(agent.get());
    return (error == RICE_ERROR_SUCCESS || error == RICE_ERROR_ALREADY_IN_PROGRESS);
}

bool webkitGstWebRTCIceStreamGatherCandidates(WebKitGstIceStream* stream)
{
    return webkitGstWebRTCIceStreamGatherCandidates(GST_WEBRTC_ICE_STREAM(stream));
}

void webkitGstWebRTCIceStreamHandleIncomingData(const WebKitGstIceStream* stream, WebCore::RTCIceProtocol protocol, String&& from, String&& to, SharedMemory::Handle&& handle)
{
    RiceTransportType transport;
    switch (protocol) {
    case WebCore::RTCIceProtocol::Tcp:
        transport = RICE_TRANSPORT_TYPE_TCP;
        break;
    case WebCore::RTCIceProtocol::Udp:
        transport = RICE_TRANSPORT_TYPE_UDP;
        break;
    };
    auto riceFrom = riceAddressFromString(from);
    auto riceTo = riceAddressFromString(to);

    auto now = WTF::MonotonicTime::now().secondsSinceEpoch();
    GST_TRACE_OBJECT(stream, "Received %zu bytes", handle.size());
    RiceStreamIncomingData result;

    auto sharedMemory = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return;

    auto buffer = sharedMemory->createSharedBuffer(sharedMemory->size());

    // We do rtcp muxing into rtp, so the component ID is always 1.
    size_t componentId = 1;
    rice_stream_handle_incoming_data(stream->priv->riceStream.get(), componentId, transport, riceFrom.get(),
        riceTo.get(), buffer->span().data(), buffer->size(), now.nanoseconds(), &result);

    if (result.handled) {
        auto agent = stream->priv->agent.get();
        // May result in either the gather or conncheck sources making further progress.
        if (agent) [[likely]]
            webkitGstWebRTCIceAgentWakeup(agent.get());
    }

    // Forward any non-STUN data to the pipeline for handling.
    if (result.data.size > 0 && result.data.ptr) {
        auto buffer = adoptGRef(gst_buffer_new_memdup(result.data.ptr, result.data.size));
        webkitGstWebRTCIceTransportHandleIncomingData(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), WTFMove(buffer));
    }

    gsize dataSize;
    auto recvData = rice_stream_poll_recv(stream->priv->riceStream.get(), &componentId, &dataSize);
    while (recvData) {
        auto transport = webkitGstWebRTCIceStreamFindTransport(GST_WEBRTC_ICE_STREAM(stream), (GstWebRTCICEComponent)componentId);
        if (transport) [[likely]] {
            auto buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), recvData, dataSize, 0, dataSize,
                recvData, reinterpret_cast<GDestroyNotify>(rice_free_data)));
            webkitGstWebRTCIceTransportHandleIncomingData(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(transport), WTFMove(buffer));
        }
        recvData = rice_stream_poll_recv(stream->priv->riceStream.get(), &componentId, &dataSize);
    }
}

const GRefPtr<RiceStream>& webkitGstWebRTCIceStreamGetRiceStream(WebKitGstIceStream* stream)
{
    return stream->priv->riceStream;
}

void webkitGstWebRTCIceStreamSetLocalCredentials(WebKitGstIceStream* stream, const String& ufrag, const String& pwd)
{
    GUniquePtr<RiceCredentials> credentials(rice_credentials_new(ufrag.ascii().data(), pwd.ascii().data()));
    rice_stream_set_local_credentials(stream->priv->riceStream.get(), credentials.get());

    stream->priv->haveLocalCredentials = true;
    if (stream->priv->haveRemoteCredentials && stream->priv->gatheringRequested)
        webkitGstWebRTCIceStreamGatherCandidates(GST_WEBRTC_ICE_STREAM(stream));
}

void webkitGstWebRTCIceStreamSetRemoteCredentials(WebKitGstIceStream* stream, const String& ufrag, const String& pwd)
{
    GUniquePtr<RiceCredentials> credentials(rice_credentials_new(ufrag.ascii().data(), pwd.ascii().data()));
    rice_stream_set_remote_credentials(stream->priv->riceStream.get(), credentials.get());

    stream->priv->haveRemoteCredentials = true;
    if (stream->priv->haveLocalCredentials && stream->priv->gatheringRequested)
        webkitGstWebRTCIceStreamGatherCandidates(GST_WEBRTC_ICE_STREAM(stream));
}

bool webkitGstWebRTCIceStreamGetSelectedPair(WebKitGstIceStream* stream, GstWebRTCICECandidateStats** localStats, GstWebRTCICECandidateStats** remoteStats)
{
    if (!stream->priv->rtpTransport) [[unlikely]]
        return false;

    return webkitGstWebRTCIceTransportGetSelectedPair(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), localStats, remoteStats);
}

static void webkitGstWebRTCIceStreamFinalize(GObject* object)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(object);
    auto agent = stream->priv->agent.get();
    if (agent)
        webkitGstWebRTCIceAgentFinalizeStream(agent.get(), GST_WEBRTC_ICE_STREAM(object)->stream_id);

    G_OBJECT_CLASS(webkit_gst_webrtc_ice_stream_parent_class)->finalize(object);
}

static void webkit_gst_webrtc_ice_stream_class_init(WebKitGstIceStreamClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->finalize = webkitGstWebRTCIceStreamFinalize;

    auto iceClass = GST_WEBRTC_ICE_STREAM_CLASS(klass);
    iceClass->find_transport = webkitGstWebRTCIceStreamFindTransport;
    iceClass->gather_candidates = webkitGstWebRTCIceStreamGatherCandidates;
}

WebKitGstIceStream* webkitGstWebRTCCreateIceStream(WebKitGstIceAgent* agent, GRefPtr<RiceStream>&& riceStream)
{
    unsigned streamId = rice_stream_get_id(riceStream.get());
    auto stream = reinterpret_cast<WebKitGstIceStream*>(g_object_new(WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM, "stream-id", streamId, nullptr));

    gst_object_ref_sink(stream);

    stream->priv->agent.reset(agent);
    stream->priv->riceStream = WTFMove(riceStream);
    return stream;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
