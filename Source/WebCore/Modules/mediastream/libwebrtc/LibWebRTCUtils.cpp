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
#include "RTCRtpParameters.h"
#include <webrtc/api/rtpparameters.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static inline RTCRtpParameters::EncodingParameters fillEncodingParameters(const webrtc::RtpEncodingParameters& rtcParameters)
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

static inline RTCRtpParameters::HeaderExtensionParameters fillHeaderExtensionParameters(const webrtc::RtpHeaderExtensionParameters& rtcParameters)
{
    RTCRtpParameters::HeaderExtensionParameters parameters;

    parameters.uri = fromStdString(rtcParameters.uri);
    parameters.id = rtcParameters.id;

    return parameters;
}

static inline RTCRtpParameters::CodecParameters fillCodecParameters(const webrtc::RtpCodecParameters& rtcParameters)
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

RTCRtpParameters fillRtpParameters(const webrtc::RtpParameters& rtcParameters)
{
    RTCRtpParameters parameters;

    parameters.transactionId = fromStdString(rtcParameters.transaction_id);
    for (auto& rtcEncoding : rtcParameters.encodings)
        parameters.encodings.append(fillEncodingParameters(rtcEncoding));
    for (auto& extension : rtcParameters.header_extensions)
        parameters.headerExtensions.append(fillHeaderExtensionParameters(extension));
    for (auto& codec : rtcParameters.codecs)
        parameters.codecs.append(fillCodecParameters(codec));

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

}; // namespace WebCore

#endif // USE(LIBWEBRTC)
