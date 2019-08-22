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
#include "RTCRtpSendParameters.h"
#include <wtf/text/WTFString.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/rtpparameters.h>
#include <webrtc/api/rtptransceiverinterface.h>

ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

static inline RTCRtpEncodingParameters toRTCEncodingParameters(const webrtc::RtpEncodingParameters& rtcParameters)
{
    RTCRtpEncodingParameters parameters;

    if (rtcParameters.ssrc)
        parameters.ssrc = *rtcParameters.ssrc;
    if (rtcParameters.rtx && rtcParameters.rtx->ssrc)
        parameters.rtx.ssrc = *rtcParameters.rtx->ssrc;
    if (rtcParameters.fec && rtcParameters.fec->ssrc)
        parameters.fec.ssrc = *rtcParameters.fec->ssrc;
    if (rtcParameters.dtx) {
        switch (*rtcParameters.dtx) {
        case webrtc::DtxStatus::DISABLED:
            parameters.dtx = RTCDtxStatus::Disabled;
            break;
        case webrtc::DtxStatus::ENABLED:
            parameters.dtx = RTCDtxStatus::Enabled;
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

static inline RTCRtpHeaderExtensionParameters toRTCHeaderExtensionParameters(const webrtc::RtpHeaderExtensionParameters& rtcParameters)
{
    RTCRtpHeaderExtensionParameters parameters;

    parameters.uri = fromStdString(rtcParameters.uri);
    parameters.id = rtcParameters.id;

    return parameters;
}

static inline webrtc::RtpHeaderExtensionParameters fromRTCHeaderExtensionParameters(const RTCRtpHeaderExtensionParameters& parameters)
{
    webrtc::RtpHeaderExtensionParameters rtcParameters;

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

    return parameters;
}

RTCRtpSendParameters toRTCRtpSendParameters(const webrtc::RtpParameters& rtcParameters)
{
    RTCRtpSendParameters parameters { toRTCRtpParameters(rtcParameters) };

    parameters.transactionId = fromStdString(rtcParameters.transaction_id);
    for (auto& rtcEncoding : rtcParameters.encodings)
        parameters.encodings.append(toRTCEncodingParameters(rtcEncoding));

    switch (rtcParameters.degradation_preference) {
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
    for (auto& stream : init.streams)
        rtcInit.stream_ids.push_back(stream->id().utf8().data());
    return rtcInit;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
