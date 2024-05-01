/*
 *  Copyright (C) 2021-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
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
#include "GStreamerSctpTransportBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GStreamerDtlsTransportBackend.h"
#include "GStreamerWebRTCUtils.h"

GST_DEBUG_CATEGORY(webkit_webrtc_sctp_transport_debug);
#define GST_CAT_DEFAULT webkit_webrtc_sctp_transport_debug

namespace WebCore {

static inline RTCSctpTransportState toRTCSctpTransportState(GstWebRTCSCTPTransportState state)
{
    switch (state) {
    case GST_WEBRTC_SCTP_TRANSPORT_STATE_NEW:
    case GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTING:
        return RTCSctpTransportState::Connecting;
    case GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTED:
        return RTCSctpTransportState::Connected;
    case GST_WEBRTC_SCTP_TRANSPORT_STATE_CLOSED:
        return RTCSctpTransportState::Closed;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GStreamerSctpTransportBackend::GStreamerSctpTransportBackend(GRefPtr<GstWebRTCSCTPTransport>&& transport)
    : m_backend(WTFMove(transport))
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_sctp_transport_debug, "webkitwebrtcsctp", 0, "WebKit WebRTC SCTP transport");
    });
    ASSERT(m_backend);
}

GStreamerSctpTransportBackend::~GStreamerSctpTransportBackend()
{
    unregisterClient();
}

UniqueRef<RTCDtlsTransportBackend> GStreamerSctpTransportBackend::dtlsTransportBackend()
{
    GRefPtr<GstWebRTCDTLSTransport> transport;
    g_object_get(m_backend.get(), "transport", &transport.outPtr(), nullptr);
    return makeUniqueRef<GStreamerDtlsTransportBackend>(WTFMove(transport));
}

void GStreamerSctpTransportBackend::registerClient(RTCSctpTransportBackendClient& client)
{
    ASSERT(!m_client);
    m_client = client;
    g_signal_connect_swapped(m_backend.get(), "notify::state", G_CALLBACK(+[](GStreamerSctpTransportBackend* backend) {
        backend->stateChanged();
    }), this);
}

void GStreamerSctpTransportBackend::unregisterClient()
{
    g_signal_handlers_disconnect_by_data(m_backend.get(), this);
    m_client.clear();
}

void GStreamerSctpTransportBackend::stateChanged()
{
    if (!m_client)
        return;

    GstWebRTCSCTPTransportState transportState;
    guint16 maxChannels;
    uint64_t maxMessageSize;
    g_object_get(m_backend.get(), "state", &transportState, "max-message-size", &maxMessageSize, "max-channels", &maxChannels, nullptr);
    GST_DEBUG("Notifying SCTP transport state, max-message-size: %" G_GUINT64_FORMAT " max-channels: %" G_GUINT16_FORMAT, maxMessageSize, maxChannels);
    callOnMainThread([client = m_client, transportState, maxChannels, maxMessageSize] {
        if (!client)
            return;

        client->onStateChanged(toRTCSctpTransportState(transportState), maxMessageSize, maxChannels);
    });
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
