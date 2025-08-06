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

#if USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"
#include <gst/webrtc/ice.h>
#include <gst/webrtc/webrtc.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

typedef struct _WebKitGstIceStreamPrivate {
    GThreadSafeWeakPtr<WebKitGstIceAgent> agent;
    GRefPtr<GstWebRTCICETransport> rtpTransport;
    GRefPtr<GstWebRTCICETransport> rtcpTransport;
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

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGstIceStream, webkit_gst_webrtc_ice_stream, GST_TYPE_WEBRTC_ICE_STREAM, GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_stream_debug, "webkitwebrtcicestream", 0, "WebRTC ICE stream"))

GstWebRTCICETransport* webkitGstWebRTCIceStreamFindTransport(GstWebRTCICEStream* ice, GstWebRTCICEComponent component)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    auto agent = stream->priv->agent.get();
    if (!agent)
        return nullptr;

    switch (component) {
    case GST_WEBRTC_ICE_COMPONENT_RTP:
        if (!stream->priv->rtpTransport)
            stream->priv->rtpTransport = adoptGRef(GST_WEBRTC_ICE_TRANSPORT(webkitGstWebRTCIceAgentCreateTransport(agent.get(), ice->stream_id, component)));
        return stream->priv->rtpTransport.get();
    case GST_WEBRTC_ICE_COMPONENT_RTCP:
        if (!stream->priv->rtcpTransport)
            stream->priv->rtcpTransport = adoptGRef(GST_WEBRTC_ICE_TRANSPORT(webkitGstWebRTCIceAgentCreateTransport(agent.get(), ice->stream_id, component)));
        return stream->priv->rtcpTransport.get();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void webkitGstWebRTCIceStreamGatheringDone(WebKitGstIceStream* ice)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);
    if (stream->priv->rtpTransport)
        gst_webrtc_ice_transport_gathering_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE);
    if (stream->priv->rtcpTransport)
        gst_webrtc_ice_transport_gathering_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtcpTransport.get()), GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE);
}

static gboolean webkitGstWebRTCIceStreamGatherCandidates(GstWebRTCICEStream* ice)
{
    auto stream = WEBKIT_GST_WEBRTC_ICE_STREAM(ice);

    if (stream->priv->rtpTransport)
        gst_webrtc_ice_transport_gathering_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), GST_WEBRTC_ICE_GATHERING_STATE_GATHERING);

    if (stream->priv->rtcpTransport)
        gst_webrtc_ice_transport_gathering_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtcpTransport.get()), GST_WEBRTC_ICE_GATHERING_STATE_GATHERING);

    auto agent = stream->priv->agent.get();
    if (!agent)
        return FALSE;

    return webkitGstWebRTCIceAgentGatherCandidates(agent.get(), ice->stream_id);
}

void webkitGstWebRTCIceStreamComponentStateChanged(WebKitGstIceStream* stream, RTCIceComponent component, RTCIceConnectionState state)
{
    GstWebRTCICEConnectionState gstState;

    switch (state) {
    case RTCIceConnectionState::New:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_NEW;
        break;
    case RTCIceConnectionState::Checking:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING;
        break;
    case RTCIceConnectionState::Connected:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED;
        break;
    case RTCIceConnectionState::Completed:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED;
        break;
    case RTCIceConnectionState::Failed:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_FAILED;
        break;
    case RTCIceConnectionState::Disconnected:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED;
        break;
    case RTCIceConnectionState::Closed:
        gstState = GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED;
        break;
    }

    switch (component) {
    case RTCIceComponent::Rtp:
        if (stream->priv->rtpTransport)
            gst_webrtc_ice_transport_connection_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), gstState);
        break;
    case RTCIceComponent::Rtcp:
        if (stream->priv->rtcpTransport)
            gst_webrtc_ice_transport_connection_state_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtcpTransport.get()), gstState);
        break;
    }
}

void webkitGstWebRTCIceStreamNewSelectedPair(WebKitGstIceStream* stream, RTCIceComponent component)
{
    switch (component) {
    case RTCIceComponent::Rtp:
        if (stream->priv->rtpTransport)
            gst_webrtc_ice_transport_selected_pair_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()));
        break;
    case RTCIceComponent::Rtcp:
        if (stream->priv->rtcpTransport)
            gst_webrtc_ice_transport_selected_pair_change(GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtcpTransport.get()));
        break;
    }
}

void webkitGstWebRTCIceStreamHandleIncomingData(WebKitGstIceStream* stream, RTCIceComponent component, std::span<const uint8_t>&& data)
{
    switch (component) {
    case RTCIceComponent::Rtp:
        if (stream->priv->rtpTransport)
            webkitGstWebRTCIceTransportHandleIncomingData(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtpTransport.get()), WTFMove(data));
        break;
    case RTCIceComponent::Rtcp:
        if (stream->priv->rtcpTransport)
            webkitGstWebRTCIceTransportHandleIncomingData(WEBKIT_GST_WEBRTC_ICE_TRANSPORT(stream->priv->rtcpTransport.get()), WTFMove(data));
        break;
    }
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

WebKitGstIceStream* webkitGstWebRTCCreateIceStream(WebKitGstIceAgent* agent, unsigned streamId)
{
    auto stream = reinterpret_cast<WebKitGstIceStream*>(g_object_new(WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM, "stream-id", streamId, nullptr));

    gst_object_ref_sink(stream);

    stream->priv->agent.reset(agent);
    return stream;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
