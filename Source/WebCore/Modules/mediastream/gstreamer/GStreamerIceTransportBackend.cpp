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
#include "GStreamerIceTransportBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GStreamerWebRTCUtils.h"
#include "NotImplemented.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

GStreamerIceTransportBackend::GStreamerIceTransportBackend(const GRefPtr<GstWebRTCDTLSTransport>& transport)
    : m_backend(transport)
{
    ASSERT(m_backend);

    iceTransportChanged();
    g_signal_connect_swapped(m_backend.get(), "notify::transport", G_CALLBACK(+[](GStreamerIceTransportBackend* backend) {
        backend->iceTransportChanged();
    }), this);
}

GStreamerIceTransportBackend::~GStreamerIceTransportBackend()
{
    if (G_IS_OBJECT(m_iceTransport.get()))
        g_signal_handlers_disconnect_by_data(m_iceTransport.get(), this);
    g_signal_handlers_disconnect_by_data(m_backend.get(), this);
}

void GStreamerIceTransportBackend::iceTransportChanged()
{
    if (G_IS_OBJECT(m_iceTransport.get()))
        g_signal_handlers_disconnect_by_data(m_iceTransport.get(), this);

    g_object_get(m_backend.get(), "transport", &m_iceTransport.outPtr(), nullptr);

    g_signal_connect_swapped(m_iceTransport.get(), "notify::state", G_CALLBACK(+[](GStreamerIceTransportBackend* backend) {
        backend->stateChanged();
    }), this);
    g_signal_connect_swapped(m_iceTransport.get(), "notify::gathering-state", G_CALLBACK(+[](GStreamerIceTransportBackend* backend) {
        backend->gatheringStateChanged();
    }), this);
    g_signal_connect_swapped(m_iceTransport.get(), "on-selected-candidate-pair-change", G_CALLBACK(+[](GStreamerIceTransportBackend* backend) {
        backend->selectedCandidatePairChanged();
    }), this);
}

void GStreamerIceTransportBackend::registerClient(Client& client)
{
    ASSERT(!m_client);
    m_client = client;

    GstWebRTCICEConnectionState transportState;
    GstWebRTCICEGatheringState gatheringState;
    g_object_get(m_iceTransport.get(), "state", &transportState, "gathering-state", &gatheringState, nullptr);

    callOnMainThread([weakThis = WeakPtr { *this }, transportState, gatheringState] {
        if (!weakThis || !weakThis->m_client)
            return;

        // We start observing a bit late and might miss the checking state. Synthesize it as needed.
        if (transportState > GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING && transportState != GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED)
            weakThis->m_client->onStateChanged(RTCIceTransportState::Checking);

        weakThis->m_client->onStateChanged(toRTCIceTransportState(transportState));
        weakThis->m_client->onGatheringStateChanged(toRTCIceGatheringState(gatheringState));
    });
}

void GStreamerIceTransportBackend::unregisterClient()
{
    ASSERT(m_client);
    m_client.clear();
}

void GStreamerIceTransportBackend::stateChanged() const
{
    if (!m_client)
        return;

    GstWebRTCICEConnectionState transportState;
    g_object_get(m_iceTransport.get(), "state", &transportState, nullptr);
    callOnMainThread([weakThis = WeakPtr { *this }, transportState] {
        if (!weakThis || !weakThis->m_client)
            return;
        weakThis->m_client->onStateChanged(toRTCIceTransportState(transportState));
    });
}

void GStreamerIceTransportBackend::gatheringStateChanged() const
{
    if (!m_client)
        return;

    GstWebRTCICEGatheringState gatheringState;
    g_object_get(m_iceTransport.get(), "gathering-state", &gatheringState, nullptr);
    callOnMainThread([weakThis = WeakPtr { *this }, gatheringState] {
        if (!weakThis || !weakThis->m_client)
            return;
        weakThis->m_client->onGatheringStateChanged(toRTCIceGatheringState(gatheringState));
    });
}

void GStreamerIceTransportBackend::selectedCandidatePairChanged()
{
    // FIXME: call m_client->onSelectedCandidatePairChanged(). See also
    // https://github.com/WebKit/WebKit/commit/0692fae10c8e53deba214fd080a35f7c54bd6985
    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
