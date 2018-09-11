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

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RTCPeerConnection.h"
#include "RTCRtpParameters.h"
#include <wtf/text/WTFString.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <webrtc/api/rtpparameters.h>
#include <webrtc/api/rtptransceiverinterface.h>

#pragma GCC diagnostic pop

namespace WebCore {

static inline RTCRtpParameters::EncodingParameters toRTCEncodingParameters(const webrtc::RtpEncodingParameters& rtcParameters)
{
    RTCRtpParameters::EncodingParameters parameters;

    if (rtcParameters.ssrc)
        parameters.ssrc = *rtcParameters.ssrc;
    if (rtcParameters.rtx && rtcParameters.rtx->ssrc)
        parameters.rtx.ssrc = *rtcParameters.rtx->ssrc;
    if (rtcParameters.fec && rtcParameters.fec->ssrc)
        parameters.fec.ssrc = *rtcParameters.fec->ssrc;
    if (rtcParameters.dtx) {
        switch (*rtcParameters.dtx) {
        case webrtc::DtxStatus::DISABLED:
            parameters.dtx = RTCRtpParameters::DtxStatus::Disabled;
            break;
        case webrtc::DtxStatus::ENABLED:
            parameters.dtx = RTCRtpParameters::DtxStatus::Enabled;
        }
    }
    parameters.active = rtcParameters.active;
    if (rtcParameters.max_bitrate_bps)
        parameters.maxBitrate = *rtcParameters.max_bitrate_bps;
    if (rtcParameters.max_framerate)
        parameters.maxFramerate = *rtcParameters.max_framerate;
    parameters.rid = fromStdString(rtcParameters.rid);
    if (rtcParameters.scale_resolution_down_by)
        parameters.scaleResolutionDownBy = *rtcParameters.scale_resolution_down_by;

    return parameters;
}

static inline webrtc::RtpEncodingParameters fromRTCEncodingParameters(const RTCRtpParameters::EncodingParameters& parameters)
{
    webrtc::RtpEncodingParameters rtcParameters;

    if (parameters.dtx) {
        switch (*parameters.dtx) {
        case RTCRtpParameters::DtxStatus::Disabled:
            rtcParameters.dtx = webrtc::DtxStatus::DISABLED;
            break;
        case RTCRtpParameters::DtxStatus::Enabled:
            rtcParameters.dtx = webrtc::DtxStatus::ENABLED;
        }
    }
    rtcParameters.active = parameters.active;
    if (parameters.maxBitrate)
        rtcParameters.max_bitrate_bps = parameters.maxBitrate;
    if (parameters.maxFramerate)
        rtcParameters.max_framerate = parameters.maxFramerate;
    rtcParameters.rid = parameters.rid.utf8().data();
    if (parameters.scaleResolutionDownBy != 1)
        rtcParameters.scale_resolution_down_by = parameters.scaleResolutionDownBy;

    return rtcParameters;
}

static inline RTCRtpParameters::HeaderExtensionParameters toRTCHeaderExtensionParameters(const webrtc::RtpHeaderExtensionParameters& rtcParameters)
{
    RTCRtpParameters::HeaderExtensionParameters parameters;

    parameters.uri = fromStdString(rtcParameters.uri);
    parameters.id = rtcParameters.id;

    return parameters;
}

static inline webrtc::RtpHeaderExtensionParameters fromRTCHeaderExtensionParameters(const RTCRtpParameters::HeaderExtensionParameters& parameters)
{
    webrtc::RtpHeaderExtensionParameters rtcParameters;

    rtcParameters.uri = parameters.uri.utf8().data();
    rtcParameters.id = parameters.id;

    return rtcParameters;
}

static inline RTCRtpParameters::CodecParameters toRTCCodecParameters(const webrtc::RtpCodecParameters& rtcParameters)
{
    RTCRtpParameters::CodecParameters parameters;

    parameters.payloadType = rtcParameters.payload_type;
    parameters.mimeType = fromStdString(rtcParameters.mime_type());
    if (rtcParameters.clock_rate)
        parameters.clockRate = *rtcParameters.clock_rate;
    if (rtcParameters.num_channels)
        parameters.channels = *rtcParameters.num_channels;

    return parameters;
}

RTCRtpParameters toRTCRtpParameters(const webrtc::RtpParameters& rtcParameters)
{
    RTCRtpParameters parameters;

    parameters.transactionId = fromStdString(rtcParameters.transaction_id);
    for (auto& rtcEncoding : rtcParameters.encodings)
        parameters.encodings.append(toRTCEncodingParameters(rtcEncoding));
    for (auto& extension : rtcParameters.header_extensions)
        parameters.headerExtensions.append(toRTCHeaderExtensionParameters(extension));
    for (auto& codec : rtcParameters.codecs)
        parameters.codecs.append(toRTCCodecParameters(codec));

    switch (rtcParameters.degradation_preference) {
    // FIXME: Support DegradationPreference::DISABLED.
    case webrtc::DegradationPreference::DISABLED:
    case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
        parameters.degradationPreference = RTCRtpParameters::DegradationPreference::MaintainFramerate;
        break;
    case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
        parameters.degradationPreference = RTCRtpParameters::DegradationPreference::MaintainResolution;
        break;
    case webrtc::DegradationPreference::BALANCED:
        parameters.degradationPreference = RTCRtpParameters::DegradationPreference::Balanced;
        break;
    };
    return parameters;
}

webrtc::RtpParameters fromRTCRtpParameters(const RTCRtpParameters& parameters)
{
    webrtc::RtpParameters rtcParameters;
    rtcParameters.transaction_id = parameters.transactionId.utf8().data();

    for (auto& encoding : parameters.encodings)
        rtcParameters.encodings.push_back(fromRTCEncodingParameters(encoding));
    for (auto& extension : parameters.headerExtensions)
        rtcParameters.header_extensions.push_back(fromRTCHeaderExtensionParameters(extension));
    // Codecs parameters are readonly

    switch (parameters.degradationPreference) {
    case RTCRtpParameters::DegradationPreference::MaintainFramerate:
        rtcParameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
        break;
    case RTCRtpParameters::DegradationPreference::MaintainResolution:
        rtcParameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
        break;
    case RTCRtpParameters::DegradationPreference::Balanced:
        rtcParameters.degradation_preference = webrtc::DegradationPreference::BALANCED;
        break;
    }
    return rtcParameters;
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

webrtc::RtpTransceiverInit fromRtpTransceiverInit(const RTCRtpTransceiverInit& init)
{
    webrtc::RtpTransceiverInit rtcInit;
    rtcInit.direction = fromRTCRtpTransceiverDirection(init.direction);
    return rtcInit;
}

}; // namespace WebCore

#endif // USE(LIBWEBRTC)
