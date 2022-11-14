/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCUtils.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RTCDtlsTransportState.h"
#include "RTCError.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCRtpSendParameters.h"
#include <wtf/text/WTFString.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
ALLOW_COMMA_BEGIN

#include <webrtc/api/rtp_parameters.h>
#include <webrtc/api/rtp_transceiver_interface.h>
#include <webrtc/p2p/base/p2p_constants.h>
#include <webrtc/pc/webrtc_sdp.h>

ALLOW_COMMA_END
ALLOW_DEPRECATED_DECLARATIONS_END
ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

webrtc::Priority fromRTCPriorityType(RTCPriorityType priority)
{
    switch (priority) {
    case RTCPriorityType::VeryLow:
        return webrtc::Priority::kVeryLow;
    case RTCPriorityType::Low:
        return webrtc::Priority::kLow;
    case RTCPriorityType::Medium:
        return webrtc::Priority::kMedium;
    case RTCPriorityType::High:
        return webrtc::Priority::kHigh;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

RTCPriorityType toRTCPriorityType(webrtc::Priority priority)
{
    switch (priority) {
    case webrtc::Priority::kVeryLow:
        return RTCPriorityType::VeryLow;
    case webrtc::Priority::kLow:
        return RTCPriorityType::Low;
    case webrtc::Priority::kMedium:
        return RTCPriorityType::Medium;
    case webrtc::Priority::kHigh:
        return RTCPriorityType::High;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static inline double toWebRTCBitRatePriority(RTCPriorityType priority)
{
    switch (priority) {
    case RTCPriorityType::VeryLow:
        return 0.5;
    case RTCPriorityType::Low:
        return 1;
    case RTCPriorityType::Medium:
        return 2;
    case RTCPriorityType::High:
        return 4;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static inline RTCPriorityType fromWebRTCBitRatePriority(double priority)
{
    if (priority < 0.7)
        return RTCPriorityType::VeryLow;
    if (priority < 1.5)
        return RTCPriorityType::Low;
    if (priority < 2.5)
        return RTCPriorityType::Medium;
    return RTCPriorityType::High;
}

static inline RTCRtpEncodingParameters toRTCEncodingParameters(const webrtc::RtpEncodingParameters& rtcParameters)
{
    RTCRtpEncodingParameters parameters;

    if (rtcParameters.ssrc)
        parameters.ssrc = *rtcParameters.ssrc;

    parameters.active = rtcParameters.active;
    if (rtcParameters.max_bitrate_bps)
        parameters.maxBitrate = *rtcParameters.max_bitrate_bps;
    if (rtcParameters.max_framerate)
        parameters.maxFramerate = *rtcParameters.max_framerate;
    parameters.rid = fromStdString(rtcParameters.rid);
    if (rtcParameters.scale_resolution_down_by)
        parameters.scaleResolutionDownBy = *rtcParameters.scale_resolution_down_by;

    parameters.priority = fromWebRTCBitRatePriority(rtcParameters.bitrate_priority);
    parameters.networkPriority = toRTCPriorityType(rtcParameters.network_priority);

    return parameters;
}

static inline webrtc::RtpEncodingParameters fromRTCEncodingParameters(const RTCRtpEncodingParameters& parameters)
{
    webrtc::RtpEncodingParameters rtcParameters;

    if (parameters.ssrc)
        rtcParameters.ssrc = parameters.ssrc;

    rtcParameters.active = parameters.active;
    if (parameters.maxBitrate)
        rtcParameters.max_bitrate_bps = parameters.maxBitrate;
    if (parameters.maxFramerate)
        rtcParameters.max_framerate = parameters.maxFramerate;
    rtcParameters.rid = parameters.rid.utf8().data();
    if (parameters.scaleResolutionDownBy)
        rtcParameters.scale_resolution_down_by = parameters.scaleResolutionDownBy;

    rtcParameters.bitrate_priority = toWebRTCBitRatePriority(parameters.priority);
    if (parameters.networkPriority)
        rtcParameters.network_priority = fromRTCPriorityType(*parameters.networkPriority);
    return rtcParameters;
}

static inline RTCRtpHeaderExtensionParameters toRTCHeaderExtensionParameters(const webrtc::RtpExtension& rtcParameters)
{
    RTCRtpHeaderExtensionParameters parameters;

    parameters.uri = fromStdString(rtcParameters.uri);
    parameters.id = rtcParameters.id;

    return parameters;
}

static inline webrtc::RtpExtension fromRTCHeaderExtensionParameters(const RTCRtpHeaderExtensionParameters& parameters)
{
    webrtc::RtpExtension rtcParameters;

    rtcParameters.uri = parameters.uri.utf8().data();
    rtcParameters.id = parameters.id;

    return rtcParameters;
}

static inline RTCRtpCodecParameters toRTCCodecParameters(const webrtc::RtpCodecParameters& rtcParameters)
{
    RTCRtpCodecParameters parameters;

    parameters.payloadType = rtcParameters.payload_type;
    parameters.mimeType = fromStdString(rtcParameters.mime_type());
    if (rtcParameters.clock_rate)
        parameters.clockRate = *rtcParameters.clock_rate;
    if (rtcParameters.num_channels)
        parameters.channels = *rtcParameters.num_channels;

    StringBuilder sdpFmtpLineBuilder;
    sdpFmtpLineBuilder.append("a=fmtp:", parameters.payloadType, ' ');

    bool isFirst = true;
    for (auto& keyValue : rtcParameters.parameters) {
        if (!isFirst)
            sdpFmtpLineBuilder.append(';');
        else
            isFirst = false;
        sdpFmtpLineBuilder.append(keyValue.first.c_str(), '=', keyValue.second.c_str());
    }
    parameters.sdpFmtpLine = sdpFmtpLineBuilder.toString();

    return parameters;
}

RTCRtpParameters toRTCRtpParameters(const webrtc::RtpParameters& rtcParameters)
{
    RTCRtpParameters parameters;

    for (auto& extension : rtcParameters.header_extensions)
        parameters.headerExtensions.append(toRTCHeaderExtensionParameters(extension));
    for (auto& codec : rtcParameters.codecs)
        parameters.codecs.append(toRTCCodecParameters(codec));

    parameters.rtcp.reducedSize = rtcParameters.rtcp.reduced_size;

    return parameters;
}

RTCRtpSendParameters toRTCRtpSendParameters(const webrtc::RtpParameters& rtcParameters)
{
    RTCRtpSendParameters parameters { toRTCRtpParameters(rtcParameters) };
    parameters.rtcp.cname = fromStdString(rtcParameters.rtcp.cname);

    parameters.transactionId = fromStdString(rtcParameters.transaction_id);
    for (auto& rtcEncoding : rtcParameters.encodings)
        parameters.encodings.append(toRTCEncodingParameters(rtcEncoding));

    if (rtcParameters.degradation_preference) {
        switch (*rtcParameters.degradation_preference) {
        // FIXME: Support DegradationPreference::DISABLED.
        case webrtc::DegradationPreference::DISABLED:
        case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
            parameters.degradationPreference = RTCDegradationPreference::MaintainFramerate;
            break;
        case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
            parameters.degradationPreference = RTCDegradationPreference::MaintainResolution;
            break;
        case webrtc::DegradationPreference::BALANCED:
            parameters.degradationPreference = RTCDegradationPreference::Balanced;
            break;
        };
    }
    return parameters;
}

void updateRTCRtpSendParameters(const RTCRtpSendParameters& parameters, webrtc::RtpParameters& rtcParameters)
{
    rtcParameters.transaction_id = parameters.transactionId.utf8().data();

    if (parameters.encodings.size() != rtcParameters.encodings.size()) {
        // If encodings size is different, setting parameters will fail. Let's make it so.
        rtcParameters.encodings.clear();
        return;
    }

    // We copy all current encodings parameters and only update parameters that can actually be usefully updated.
    for (size_t i = 0; i < parameters.encodings.size(); ++i) {
        rtcParameters.encodings[i].active = parameters.encodings[i].active;
        if (parameters.encodings[i].maxBitrate)
            rtcParameters.encodings[i].max_bitrate_bps = parameters.encodings[i].maxBitrate;
        if (parameters.encodings[i].maxFramerate)
            rtcParameters.encodings[i].max_framerate = parameters.encodings[i].maxFramerate;
        if (parameters.encodings[i].scaleResolutionDownBy)
            rtcParameters.encodings[i].scale_resolution_down_by = parameters.encodings[i].scaleResolutionDownBy;
        rtcParameters.encodings[i].bitrate_priority = toWebRTCBitRatePriority(parameters.encodings[i].priority);
        if (parameters.encodings[i].networkPriority)
            rtcParameters.encodings[i].network_priority = fromRTCPriorityType(*parameters.encodings[i].networkPriority);
    }

    rtcParameters.header_extensions.clear();
    for (auto& extension : parameters.headerExtensions)
        rtcParameters.header_extensions.push_back(fromRTCHeaderExtensionParameters(extension));
    // Codecs parameters are readonly

    switch (parameters.degradationPreference) {
    case RTCDegradationPreference::MaintainFramerate:
        rtcParameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
        break;
    case RTCDegradationPreference::MaintainResolution:
        rtcParameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
        break;
    case RTCDegradationPreference::Balanced:
        rtcParameters.degradation_preference = webrtc::DegradationPreference::BALANCED;
        break;
    }

    if (parameters.rtcp.reducedSize)
        rtcParameters.rtcp.reduced_size = *parameters.rtcp.reducedSize;
    if (!parameters.rtcp.cname.isNull())
        rtcParameters.rtcp.cname = parameters.rtcp.cname.utf8().data();
}

RTCRtpTransceiverDirection toRTCRtpTransceiverDirection(webrtc::RtpTransceiverDirection rtcDirection)
{
    switch (rtcDirection) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
        return RTCRtpTransceiverDirection::Sendrecv;
    case webrtc::RtpTransceiverDirection::kSendOnly:
        return RTCRtpTransceiverDirection::Sendonly;
    case webrtc::RtpTransceiverDirection::kRecvOnly:
        return RTCRtpTransceiverDirection::Recvonly;
    case webrtc::RtpTransceiverDirection::kInactive:
    case webrtc::RtpTransceiverDirection::kStopped:
        return RTCRtpTransceiverDirection::Inactive;
    };

    RELEASE_ASSERT_NOT_REACHED();
}

webrtc::RtpTransceiverDirection fromRTCRtpTransceiverDirection(RTCRtpTransceiverDirection direction)
{
    switch (direction) {
    case RTCRtpTransceiverDirection::Sendrecv:
        return webrtc::RtpTransceiverDirection::kSendRecv;
    case RTCRtpTransceiverDirection::Sendonly:
        return webrtc::RtpTransceiverDirection::kSendOnly;
    case RTCRtpTransceiverDirection::Recvonly:
        return webrtc::RtpTransceiverDirection::kRecvOnly;
    case RTCRtpTransceiverDirection::Inactive:
        return webrtc::RtpTransceiverDirection::kInactive;
    };

    RELEASE_ASSERT_NOT_REACHED();
}

webrtc::RtpTransceiverInit fromRtpTransceiverInit(const RTCRtpTransceiverInit& init, cricket::MediaType type)
{
    webrtc::RtpTransceiverInit rtcInit;
    rtcInit.direction = fromRTCRtpTransceiverDirection(init.direction);
    for (auto& stream : init.streams)
        rtcInit.stream_ids.push_back(stream->id().utf8().data());

    if (type == cricket::MediaType::MEDIA_TYPE_AUDIO) {
        if (!init.sendEncodings.isEmpty())
            rtcInit.send_encodings.push_back(fromRTCEncodingParameters(init.sendEncodings[0]));
    } else {
        for (auto& encoding : init.sendEncodings)
            rtcInit.send_encodings.push_back(fromRTCEncodingParameters(encoding));
    }
    return rtcInit;
}

ExceptionCode toExceptionCode(webrtc::RTCErrorType type)
{
    switch (type) {
    case webrtc::RTCErrorType::INVALID_PARAMETER:
        return InvalidAccessError;
    case webrtc::RTCErrorType::INVALID_RANGE:
        return RangeError;
    case webrtc::RTCErrorType::SYNTAX_ERROR:
        return SyntaxError;
    case webrtc::RTCErrorType::INVALID_STATE:
        return InvalidStateError;
    case webrtc::RTCErrorType::INVALID_MODIFICATION:
        return InvalidModificationError;
    case webrtc::RTCErrorType::NETWORK_ERROR:
        return NetworkError;
    default:
        return OperationError;
    }
}

Exception toException(const webrtc::RTCError& error)
{
    ASSERT(!error.ok());

    return Exception { toExceptionCode(error.type()), String::fromLatin1(error.message()) };
}

static inline RTCIceComponent toRTCIceComponent(int component)
{
    return component == cricket::ICE_CANDIDATE_COMPONENT_RTP ? RTCIceComponent::Rtp : RTCIceComponent::Rtcp;
}

static inline std::optional<RTCIceProtocol> toRTCIceProtocol(const std::string& protocol)
{
    if (protocol == "")
        return { };
    if (protocol == "udp")
        return RTCIceProtocol::Udp;
    ASSERT(protocol == "tcp" || protocol == "ssltcp");
    return RTCIceProtocol::Tcp;
}

static inline std::optional<RTCIceTcpCandidateType> toRTCIceTcpCandidateType(const std::string& type)
{
    if (type == "")
        return { };
    if (type == "active")
        return RTCIceTcpCandidateType::Active;
    if (type == "passive")
        return RTCIceTcpCandidateType::Passive;
    ASSERT(type == "so");
    return RTCIceTcpCandidateType::So;
}

static inline std::optional<RTCIceCandidateType> toRTCIceCandidateType(const std::string& type)
{
    if (type == "")
        return { };
    if (type == "local")
        return RTCIceCandidateType::Host;
    if (type == "stun")
        return RTCIceCandidateType::Srflx;
    if (type == "prflx")
        return RTCIceCandidateType::Prflx;
    ASSERT(type == "relay");
    return RTCIceCandidateType::Relay;
}

RTCIceCandidateFields convertIceCandidate(const cricket::Candidate& candidate)
{
    RTCIceCandidateFields fields;
    fields.foundation = fromStdString(candidate.foundation());
    fields.component = toRTCIceComponent(candidate.component());
    fields.priority = candidate.priority();
    fields.protocol = toRTCIceProtocol(candidate.protocol());
    if (!candidate.address().IsNil()) {
        fields.address = fromStdString(candidate.address().HostAsURIString());
        fields.port = candidate.address().port();
    }
    fields.type = toRTCIceCandidateType(candidate.type());
    fields.tcpType = toRTCIceTcpCandidateType(candidate.tcptype());
    if (!candidate.related_address().IsNil()) {
        fields.relatedAddress = fromStdString(candidate.related_address().HostAsURIString());
        fields.relatedPort = candidate.related_address().port();
    }

    fields.usernameFragment = fromStdString(candidate.username());
    return fields;
}

static std::optional<RTCErrorDetailType> toRTCErrorDetailType(webrtc::RTCErrorDetailType type)
{
    switch (type) {
    case webrtc::RTCErrorDetailType::DATA_CHANNEL_FAILURE:
        return RTCErrorDetailType::DataChannelFailure;
    case webrtc::RTCErrorDetailType::DTLS_FAILURE:
        return RTCErrorDetailType::DtlsFailure;
    case webrtc::RTCErrorDetailType::FINGERPRINT_FAILURE:
        return RTCErrorDetailType::FingerprintFailure;
    case webrtc::RTCErrorDetailType::SCTP_FAILURE:
        return RTCErrorDetailType::SctpFailure;
    case webrtc::RTCErrorDetailType::SDP_SYNTAX_ERROR:
        return RTCErrorDetailType::SdpSyntaxError;
    case webrtc::RTCErrorDetailType::HARDWARE_ENCODER_NOT_AVAILABLE:
    case webrtc::RTCErrorDetailType::HARDWARE_ENCODER_ERROR:
    case webrtc::RTCErrorDetailType::NONE:
        return { };
    };
}

RefPtr<RTCError> toRTCError(const webrtc::RTCError& rtcError)
{
    auto detail = toRTCErrorDetailType(rtcError.error_detail());
    if (!detail)
        return nullptr;
    return RTCError::create(*detail, String::fromLatin1(rtcError.message()));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
