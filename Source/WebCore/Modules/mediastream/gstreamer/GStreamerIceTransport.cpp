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

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include "GStreamerIceBuffer.h"
#include "RTCIceComponent.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/webrtc/ice.h>
#include <gst/webrtc/webrtc.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

typedef struct _WebKitGstIceTransportPrivate {
    GThreadSafeWeakPtr<WebKitGstIceAgent> agent;
    unsigned streamId;
    bool isController;
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

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGstIceTransport, webkit_gst_webrtc_ice_transport, GST_TYPE_WEBRTC_ICE_TRANSPORT, GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_transport_debug, "webkitwebrtcicetransport", 0, "WebRTC ICE transport"))

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

    GstWebRTCICEComponent gstComponent;
    g_object_get(self, "component", &gstComponent, nullptr);

    RTCIceComponent component;
    switch (gstComponent) {
    case GST_WEBRTC_ICE_COMPONENT_RTP:
        component = RTCIceComponent::Rtp;
        break;
    case GST_WEBRTC_ICE_COMPONENT_RTCP:
        component = RTCIceComponent::Rtcp;
        break;
    };

    Vector<GStreamerIceBuffer> buffers;
    auto bufferList = gst_sample_get_buffer_list(sample.get());
    if (!GST_IS_BUFFER_LIST(bufferList)) {
        GstMappedBuffer mappedBuffer(gst_sample_get_buffer(sample.get()), GST_MAP_READ);
        GStreamerIceBuffer item;
        item.data = mappedBuffer.mutableSpan<uint8_t>();
        buffers.append(WTFMove(item));
        webkitGstWebRTCIceAgentSend(agent.get(), self->priv->streamId, component, WTFMove(buffers));
        return GST_FLOW_OK;
    }

    unsigned length = gst_buffer_list_length(bufferList);
    for (unsigned i = 0; i < length; i++) {
        GstMappedBuffer mappedBuffer(gst_buffer_list_get(bufferList, i), GST_MAP_READ);
        GStreamerIceBuffer item;
        item.data = mappedBuffer.mutableSpan<uint8_t>();
        buffers.append(WTFMove(item));
    }
    webkitGstWebRTCIceAgentSend(agent.get(), self->priv->streamId, component, WTFMove(buffers));
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

    transport->sink = makeGStreamerElement("appsink"_s, makeString("ice-sink-"_s, id));
    transport->src = makeGStreamerElement("appsrc"_s, makeString("ice-src-"_s, id));
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

void webkitGstWebRTCIceTransportHandleIncomingData(WebKitGstIceTransport* transport, std::span<const uint8_t>&& data)
{
    auto buffer = wrapSpanData(data);
    gst_app_src_push_buffer(GST_APP_SRC(GST_WEBRTC_ICE_TRANSPORT(transport)->src), buffer.leakRef());
}

static void webkit_gst_webrtc_ice_transport_class_init(WebKitGstIceTransportClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = webkitGstWebRTCIceTransportConstructed;
}

WebKitGstIceTransport* webkitGstWebRTCCreateIceTransport(WebKitGstIceAgent* agent, unsigned streamId, GstWebRTCICEComponent component, bool isController)
{
    auto transport = reinterpret_cast<WebKitGstIceTransport*>(g_object_new(WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT, "component", component, nullptr));

    gst_object_ref_sink(transport);

    auto priv = transport->priv;
    priv->agent.reset(agent);
    priv->streamId = streamId;
    priv->isController = isController;
    return transport;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
