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
#include "RTCIceTcpCandidateType.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_ice_transport_debug);
#define GST_CAT_DEFAULT webkit_webrtc_ice_transport_debug

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerIceTransportBackend);

GStreamerIceTransportBackend::GStreamerIceTransportBackend(GRefPtr<GstWebRTCDTLSTransport>&& transport)
    : m_backend(WTF::move(transport))
{
    ASSERT(m_backend);

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_transport_debug, "webkitwebrtcice", 0, "WebKit WebRTC ICE Transport");
    });

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

    // Setting same libnice socket size options as LibWebRTC. 1MB for incoming streams and 256Kb for outgoing streams.
    if (gstObjectHasProperty(GST_OBJECT_CAST(m_iceTransport.get()), "receive-buffer-size"_s))
        g_object_set(m_iceTransport.get(), "receive-buffer-size", 1048576, nullptr);

    if (gstObjectHasProperty(GST_OBJECT_CAST(m_iceTransport.get()), "send-buffer-size"_s))
        g_object_set(m_iceTransport.get(), "send-buffer-size", 262144, nullptr);

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

void GStreamerIceTransportBackend::registerClient(RTCIceTransportBackendClient& client)
{
    ASSERT(!m_client);
    m_client = client;

    GstWebRTCICEConnectionState transportState;
    GstWebRTCICEGatheringState gatheringState;
    g_object_get(m_iceTransport.get(), "state", &transportState, "gathering-state", &gatheringState, nullptr);

    callOnMainThread([weakThis = WeakPtr { *this }, transportState, gatheringState] {
        if (!weakThis || !weakThis->m_client)
            return;

#ifndef GST_DISABLE_GST_DEBUG
        auto desc = GMallocString::unsafeAdoptFromUTF8(g_enum_to_string(GST_TYPE_WEBRTC_ICE_CONNECTION_STATE, transportState));
        GST_DEBUG_OBJECT(weakThis->m_backend.get(), "Initial ICE transport state: %s", desc.utf8());
#endif

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

#ifndef GST_DISABLE_GST_DEBUG
    auto desc = GMallocString::unsafeAdoptFromUTF8(g_enum_to_string(GST_TYPE_WEBRTC_ICE_CONNECTION_STATE, transportState));
    GST_DEBUG_OBJECT(m_backend.get(), "ICE transport state changed to %s", desc.utf8());
#endif

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

#if GST_CHECK_VERSION(1, 28, 0)
static Ref<RTCIceCandidate> candidateFromGstWebRTC(const GstWebRTCICECandidate* candidate)
{
    RTCIceCandidate::Fields fields;

    fields.component = toRTCIceComponent(candidate->component);

    if (candidate->stats) [[likely]] {
        fields.foundation = String::fromUTF8(GST_WEBRTC_ICE_CANDIDATE_STATS_FOUNDATION(candidate->stats));
        fields.priority = GST_WEBRTC_ICE_CANDIDATE_STATS_PRIORITY(candidate->stats);
        fields.address = String::fromUTF8(GST_WEBRTC_ICE_CANDIDATE_STATS_ADDRESS(candidate->stats));
        fields.protocol = toRTCIceProtocol(StringView::fromLatin1(GST_WEBRTC_ICE_CANDIDATE_STATS_PROTOCOL(candidate->stats)));
        fields.port = GST_WEBRTC_ICE_CANDIDATE_STATS_PORT(candidate->stats);

        fields.type = toRTCIceCandidateType(StringView::fromLatin1(GST_WEBRTC_ICE_CANDIDATE_STATS_TYPE(candidate->stats)));

        fields.usernameFragment = String::fromUTF8(GST_WEBRTC_ICE_CANDIDATE_STATS_USERNAME_FRAGMENT(candidate->stats));

        switch (GST_WEBRTC_ICE_CANDIDATE_STATS_TCP_TYPE(candidate->stats)) {
        case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_ACTIVE:
            fields.tcpType = RTCIceTcpCandidateType::Active;
            break;
        case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_PASSIVE:
            fields.tcpType = RTCIceTcpCandidateType::Passive;
            break;
        case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_SO:
            fields.tcpType = RTCIceTcpCandidateType::So;
            break;
        case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_NONE:
            break;
        };

        auto relatedAddress = CStringView::unsafeFromUTF8(GST_WEBRTC_ICE_CANDIDATE_STATS_RELATED_ADDRESS(candidate->stats));
        if (!relatedAddress.isNull()) {
            fields.relatedAddress = relatedAddress.span();
            fields.relatedPort = GST_WEBRTC_ICE_CANDIDATE_STATS_RELATED_PORT(candidate->stats);
        }
    }

    // FIXME: relayProtocol is not exposed in RTCIceCandidate::Fields.

    auto sdpMid = emptyString();
    auto candidateString = String::fromUTF8(candidate->candidate);
    return RTCIceCandidate::create(candidateString, sdpMid, WTF::move(fields));
}
#endif

void GStreamerIceTransportBackend::selectedCandidatePairChanged()
{
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/8484
#if GST_CHECK_VERSION(1, 28, 0)
    GUniquePtr<GstWebRTCICECandidatePair> selectedPair(gst_webrtc_ice_transport_get_selected_candidate_pair(m_iceTransport.get()));
    if (!selectedPair)
        return;

    auto localCandidate = candidateFromGstWebRTC(selectedPair->local);
    auto remoteCandidate = candidateFromGstWebRTC(selectedPair->remote);
    WTF::callOnMainThreadAndWait([weakThis = WeakPtr { *this }, localCandidate = WTF::move(localCandidate), remoteCandidate = WTF::move(remoteCandidate)] mutable {
        if (!weakThis || !weakThis->m_client)
            return;
        weakThis->m_client->onSelectedCandidatePairChanged(WTF::move(localCandidate), WTF::move(remoteCandidate));
    });
#endif
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
