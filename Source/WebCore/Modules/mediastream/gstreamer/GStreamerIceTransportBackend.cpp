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
#include "RTCIceTcpCandidateType.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakPtr.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_ice_transport_debug);
#define GST_CAT_DEFAULT webkit_webrtc_ice_transport_debug

class GStreamerIceTransportBackendObserver final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerIceTransportBackendObserver> {
public:
    static Ref<GStreamerIceTransportBackendObserver> create(RTCIceTransportBackendClient& client, GRefPtr<GstWebRTCICETransport>&& iceTransport) { return adoptRef(*new GStreamerIceTransportBackendObserver(client, WTF::move(iceTransport))); }

    void start();
    void stop();

private:
    GStreamerIceTransportBackendObserver(RTCIceTransportBackendClient&, GRefPtr<GstWebRTCICETransport>&&);

    void onIceTransportStateChanged();
    void onGatheringStateChanged();
    void onSelectedCandidatePairChanged();

    GRefPtr<GstWebRTCICETransport> m_iceTransport;
    WeakPtr<RTCIceTransportBackendClient> m_client;
};

GStreamerIceTransportBackendObserver::GStreamerIceTransportBackendObserver(RTCIceTransportBackendClient& client, GRefPtr<GstWebRTCICETransport>&& iceTransport)
    : m_iceTransport(WTF::move(iceTransport))
    , m_client(client)
{
    ASSERT(m_iceTransport);
}

void GStreamerIceTransportBackendObserver::start()
{
    // Setting same libnice socket size options as LibWebRTC. 1MB for incoming streams and 256Kb for outgoing streams.
    if (gstObjectHasProperty(GST_OBJECT_CAST(m_iceTransport.get()), "receive-buffer-size"_s))
        g_object_set(m_iceTransport.get(), "receive-buffer-size", 1048576, nullptr);

    if (gstObjectHasProperty(GST_OBJECT_CAST(m_iceTransport.get()), "send-buffer-size"_s))
        g_object_set(m_iceTransport.get(), "send-buffer-size", 262144, nullptr);

    g_signal_connect_swapped(m_iceTransport.get(), "notify::state", G_CALLBACK(+[](GStreamerIceTransportBackendObserver* backend) {
        backend->onIceTransportStateChanged();
    }), this);
    g_signal_connect_swapped(m_iceTransport.get(), "notify::gathering-state", G_CALLBACK(+[](GStreamerIceTransportBackendObserver* backend) {
        backend->onGatheringStateChanged();
    }), this);
    g_signal_connect_swapped(m_iceTransport.get(), "on-selected-candidate-pair-change", G_CALLBACK(+[](GStreamerIceTransportBackendObserver* backend) {
        backend->onSelectedCandidatePairChanged();
    }), this);
}

void GStreamerIceTransportBackendObserver::stop()
{
    m_client = nullptr;
    g_signal_handlers_disconnect_by_data(m_iceTransport.get(), this);
}

void GStreamerIceTransportBackendObserver::onIceTransportStateChanged()
{
    if (!m_client)
        return;

    GstWebRTCICEConnectionState transportState;
    g_object_get(m_iceTransport.get(), "state", &transportState, nullptr);

#ifndef GST_DISABLE_GST_DEBUG
    auto desc = GMallocString::unsafeAdoptFromUTF8(g_enum_to_string(GST_TYPE_WEBRTC_ICE_CONNECTION_STATE, transportState));
    GST_DEBUG_OBJECT(m_iceTransport.get(), "ICE transport state changed to %s", desc.utf8());
#endif

    callOnMainThread([protectedThis = Ref { *this }, transportState] {
        if (RefPtr client = protectedThis->m_client.get())
            client->onStateChanged(toRTCIceTransportState(transportState));
    });
}

void GStreamerIceTransportBackendObserver::onGatheringStateChanged()
{
    if (!m_client)
        return;

    GstWebRTCICEGatheringState gatheringState;
    g_object_get(m_iceTransport.get(), "gathering-state", &gatheringState, nullptr);
    callOnMainThread([protectedThis = Ref { *this }, gatheringState] {
        if (RefPtr client = protectedThis->m_client.get())
            client->onGatheringStateChanged(toRTCIceGatheringState(gatheringState));
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

void GStreamerIceTransportBackendObserver::onSelectedCandidatePairChanged()
{
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/8484
#if GST_CHECK_VERSION(1, 28, 0)
    GUniquePtr<GstWebRTCICECandidatePair> selectedPair(gst_webrtc_ice_transport_get_selected_candidate_pair(m_iceTransport.get()));
    if (!selectedPair)
        return;

    auto localCandidate = candidateFromGstWebRTC(selectedPair->local);
    auto remoteCandidate = candidateFromGstWebRTC(selectedPair->remote);
    WTF::callOnMainThreadAndWait([protectedThis = Ref { *this }, localCandidate = WTF::move(localCandidate), remoteCandidate = WTF::move(remoteCandidate)] mutable {
        if (RefPtr client = protectedThis->m_client.get())
            client->onSelectedCandidatePairChanged(WTF::move(localCandidate), WTF::move(remoteCandidate));
    });
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerIceTransportBackend);

GStreamerIceTransportBackend::GStreamerIceTransportBackend(GRefPtr<GstWebRTCDTLSTransport>&& transport)
    : m_dtlsTransport(WTF::move(transport))
{
    ASSERT(m_dtlsTransport);

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_ice_transport_debug, "webkitwebrtcicetransport", 0, "WebKit WebRTC ICE Transport");
    });
}

GStreamerIceTransportBackend::~GStreamerIceTransportBackend() = default;

void GStreamerIceTransportBackend::registerClient(RTCIceTransportBackendClient& client)
{
    ASSERT(!m_observer);
    GRefPtr<GstWebRTCICETransport> iceTransport;
    g_object_get(m_dtlsTransport.get(), "transport", &iceTransport.outPtr(), nullptr);
    lazyInitialize(m_observer, GStreamerIceTransportBackendObserver::create(client, WTF::move(iceTransport)));
    m_observer->start();
}

void GStreamerIceTransportBackend::unregisterClient()
{
    ASSERT(m_observer);
    m_observer->stop();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
