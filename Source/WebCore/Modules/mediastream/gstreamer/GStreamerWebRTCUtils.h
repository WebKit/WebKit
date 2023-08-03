/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
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

#pragma once

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include "GUniquePtrGStreamer.h"
#include "MediaEndpointConfiguration.h"
#include "PeerConnectionBackend.h"
#include "RTCBundlePolicy.h"
#include "RTCCertificate.h"
#include "RTCDtlsTransport.h"
#include "RTCError.h"
#include "RTCIceConnectionState.h"
#include "RTCIceTransportPolicy.h"
#include "RTCIceTransportState.h"
#include "RTCRtpSendParameters.h"
#include "RTCRtpTransceiverDirection.h"
#include "RTCSdpType.h"
#include "RTCSignalingState.h"

#include <gst/rtp/rtp.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

inline RTCRtpTransceiverDirection toRTCRtpTransceiverDirection(GstWebRTCRTPTransceiverDirection direction)
{
    switch (direction) {
    case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE:
        // ¯\_(ツ)_/¯
        return RTCRtpTransceiverDirection::Inactive;
    case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE:
        return RTCRtpTransceiverDirection::Inactive;
    case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY:
        return RTCRtpTransceiverDirection::Sendonly;
    case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY:
        return RTCRtpTransceiverDirection::Recvonly;
    case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV:
        return RTCRtpTransceiverDirection::Sendrecv;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return RTCRtpTransceiverDirection::Inactive;
}

inline GstWebRTCRTPTransceiverDirection fromRTCRtpTransceiverDirection(RTCRtpTransceiverDirection direction)
{
    switch (direction) {
    case RTCRtpTransceiverDirection::Inactive:
        return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE;
    case RTCRtpTransceiverDirection::Sendonly:
        return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    case RTCRtpTransceiverDirection::Recvonly:
        return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
    case RTCRtpTransceiverDirection::Sendrecv:
        return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE;
}

static inline GstWebRTCSDPType toSessionDescriptionType(RTCSdpType sdpType)
{
    switch (sdpType) {
    case RTCSdpType::Offer:
        return GST_WEBRTC_SDP_TYPE_OFFER;
    case RTCSdpType::Pranswer:
        return GST_WEBRTC_SDP_TYPE_PRANSWER;
    case RTCSdpType::Answer:
        return GST_WEBRTC_SDP_TYPE_ANSWER;
    case RTCSdpType::Rollback:
        return GST_WEBRTC_SDP_TYPE_ROLLBACK;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return GST_WEBRTC_SDP_TYPE_OFFER;
}

static inline RTCSdpType fromSessionDescriptionType(const GstWebRTCSessionDescription& description)
{
    auto type = description.type;
    if (type == GST_WEBRTC_SDP_TYPE_OFFER)
        return RTCSdpType::Offer;
    if (type == GST_WEBRTC_SDP_TYPE_ANSWER)
        return RTCSdpType::Answer;
    ASSERT(type == GST_WEBRTC_SDP_TYPE_PRANSWER);
    return RTCSdpType::Pranswer;
}

static inline RTCSignalingState toSignalingState(GstWebRTCSignalingState state)
{
    switch (state) {
    case GST_WEBRTC_SIGNALING_STATE_STABLE:
        return RTCSignalingState::Stable;
    case GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_OFFER:
        return RTCSignalingState::HaveLocalOffer;
    case GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_PRANSWER:
        return RTCSignalingState::HaveLocalPranswer;
    case GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_OFFER:
        return RTCSignalingState::HaveRemoteOffer;
    case GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_PRANSWER:
        return RTCSignalingState::HaveRemotePranswer;
    case GST_WEBRTC_SIGNALING_STATE_CLOSED:
        return RTCSignalingState::Closed;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return RTCSignalingState::Stable;
}

static inline RTCIceConnectionState toRTCIceConnectionState(GstWebRTCICEConnectionState state)
{
    switch (state) {
    case GST_WEBRTC_ICE_CONNECTION_STATE_NEW:
        return RTCIceConnectionState::New;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING:
        return RTCIceConnectionState::Checking;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED:
        return RTCIceConnectionState::Connected;
    case GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED:
        return RTCIceConnectionState::Completed;
    case GST_WEBRTC_ICE_CONNECTION_STATE_FAILED:
        return RTCIceConnectionState::Failed;
    case GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED:
        return RTCIceConnectionState::Disconnected;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED:
        return RTCIceConnectionState::Closed;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return RTCIceConnectionState::New;
}

static inline RTCDtlsTransportState toRTCDtlsTransportState(GstWebRTCDTLSTransportState state)
{
    switch (state) {
    case GST_WEBRTC_DTLS_TRANSPORT_STATE_NEW:
        return RTCDtlsTransportState::New;
    case GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTING:
        return RTCDtlsTransportState::Connecting;
    case GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTED:
        return RTCDtlsTransportState::Connected;
    case GST_WEBRTC_DTLS_TRANSPORT_STATE_CLOSED:
        return RTCDtlsTransportState::Closed;
    case GST_WEBRTC_DTLS_TRANSPORT_STATE_FAILED:
        return RTCDtlsTransportState::Failed;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static inline RTCIceTransportState toRTCIceTransportState(GstWebRTCICEConnectionState state)
{
    // FIXME: No GstWebRTCICETransportState ?
    switch (state) {
    case GST_WEBRTC_ICE_CONNECTION_STATE_NEW:
        return RTCIceTransportState::New;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING:
        return RTCIceTransportState::Checking;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED:
        return RTCIceTransportState::Connected;
    case GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED:
        return RTCIceTransportState::Completed;
    case GST_WEBRTC_ICE_CONNECTION_STATE_FAILED:
        return RTCIceTransportState::Failed;
    case GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED:
        return RTCIceTransportState::Disconnected;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED:
        return RTCIceTransportState::Closed;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return RTCIceTransportState::New;
}

static inline RTCIceGatheringState toRTCIceGatheringState(GstWebRTCICEGatheringState state)
{
    switch (state) {
    case GST_WEBRTC_ICE_GATHERING_STATE_NEW:
        return RTCIceGatheringState::New;
    case GST_WEBRTC_ICE_GATHERING_STATE_GATHERING:
        return RTCIceGatheringState::Gathering;
    case GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE:
        return RTCIceGatheringState::Complete;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static inline GstWebRTCBundlePolicy bundlePolicyFromConfiguration(const MediaEndpointConfiguration& configuration)
{
    switch (configuration.bundlePolicy) {
    case RTCBundlePolicy::Balanced:
        return GST_WEBRTC_BUNDLE_POLICY_BALANCED;
    case RTCBundlePolicy::MaxCompat:
        return GST_WEBRTC_BUNDLE_POLICY_MAX_COMPAT;
    case RTCBundlePolicy::MaxBundle:
        return GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return GST_WEBRTC_BUNDLE_POLICY_NONE;
}

static inline GstWebRTCICETransportPolicy iceTransportPolicyFromConfiguration(const MediaEndpointConfiguration& configuration)
{
    switch (configuration.iceTransportPolicy) {
    case RTCIceTransportPolicy::All:
        return GST_WEBRTC_ICE_TRANSPORT_POLICY_ALL;
    case RTCIceTransportPolicy::Relay:
        return GST_WEBRTC_ICE_TRANSPORT_POLICY_RELAY;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return GST_WEBRTC_ICE_TRANSPORT_POLICY_ALL;
}

static inline std::optional<RTCErrorDetailType> toRTCErrorDetailType(GstWebRTCError code)
{
    switch (code) {
    case GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE:
        return RTCErrorDetailType::DataChannelFailure;
    case GST_WEBRTC_ERROR_DTLS_FAILURE:
        return RTCErrorDetailType::DtlsFailure;
    case GST_WEBRTC_ERROR_FINGERPRINT_FAILURE:
        return RTCErrorDetailType::FingerprintFailure;
    case GST_WEBRTC_ERROR_SCTP_FAILURE:
        return RTCErrorDetailType::SctpFailure;
    case GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR:
        return RTCErrorDetailType::SdpSyntaxError;
    case GST_WEBRTC_ERROR_HARDWARE_ENCODER_NOT_AVAILABLE:
    case GST_WEBRTC_ERROR_ENCODER_ERROR:
        // FIXME
    default:
        return { };
    };
}

RefPtr<RTCError> toRTCError(GError*);

GUniquePtr<GstStructure> fromRTCEncodingParameters(const RTCRtpEncodingParameters&);
RTCRtpSendParameters toRTCRtpSendParameters(const GstStructure*);
GUniquePtr<GstStructure> fromRTCSendParameters(const RTCRtpSendParameters&);

std::optional<Ref<RTCCertificate>> generateCertificate(Ref<SecurityOrigin>&&, const PeerConnectionBackend::CertificateInformation&);

bool sdpMediaHasAttributeKey(const GstSDPMedia*, const char* key);

class UniqueSSRCGenerator : public ThreadSafeRefCounted<UniqueSSRCGenerator> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<UniqueSSRCGenerator> create() { return adoptRef(*new UniqueSSRCGenerator()); }

    uint32_t generateSSRC();

private:
    Lock m_lock;
    Vector<uint32_t> m_knownIds WTF_GUARDED_BY_LOCK(m_lock);
};

std::optional<int> payloadTypeForEncodingName(const char* encodingName);

WARN_UNUSED_RETURN GRefPtr<GstCaps> capsFromRtpCapabilities(RefPtr<UniqueSSRCGenerator>, const RTCRtpCapabilities&, Function<void(GstStructure*)> supplementCapsCallback);

GstWebRTCRTPTransceiverDirection getDirectionFromSDPMedia(const GstSDPMedia*);
WARN_UNUSED_RETURN GRefPtr<GstCaps> capsFromSDPMedia(const GstSDPMedia*);

inline gboolean mapRtpBuffer(GstBuffer* buffer, GstRTPBuffer* rtpBuffer, GstMapFlags flags)
{
    *rtpBuffer = GST_RTP_BUFFER_INIT;
    return gst_rtp_buffer_map(buffer, flags, rtpBuffer);
}

inline void unmapRtpBuffer(GstBuffer*, GstRTPBuffer* rtpBuffer)
{
    gst_rtp_buffer_unmap(rtpBuffer);
}

using GstMappedRtpBuffer = GstBufferMapper<GstRTPBuffer, mapRtpBuffer, unmapRtpBuffer>;

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
