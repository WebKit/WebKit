/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/internalencoderfactory.h"

#include <utility>

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace cricket {

namespace {

// If this field trial is enabled, the "flexfec-03" codec will be advertised
// as being supported by the InternalEncoderFactory. This means that
// "flexfec-03" will appear in the default local SDP, and we therefore need to
// be ready to receive FlexFEC packets from the remote.
bool IsFlexfecAdvertisedFieldTrialEnabled() {
  return webrtc::field_trial::FindFullName("WebRTC-FlexFEC-03-Advertised") ==
         "Enabled";
}

}  // namespace

InternalEncoderFactory::InternalEncoderFactory() {
  if (webrtc::VP8Decoder::IsSupported())
    supported_codecs_.push_back(cricket::VideoCodec(kVp8CodecName));
  if (webrtc::VP9Encoder::IsSupported())
    supported_codecs_.push_back(cricket::VideoCodec(kVp9CodecName));
  if (webrtc::H264Encoder::IsSupported()) {
    cricket::VideoCodec codec(kH264CodecName);
    // TODO(magjed): Move setting these parameters into webrtc::H264Encoder
    // instead.
    codec.SetParam(kH264FmtpProfileLevelId,
                   kH264ProfileLevelConstrainedBaseline);
    codec.SetParam(kH264FmtpLevelAsymmetryAllowed, "1");
    supported_codecs_.push_back(std::move(codec));
  }

  supported_codecs_.push_back(cricket::VideoCodec(kRedCodecName));
  supported_codecs_.push_back(cricket::VideoCodec(kUlpfecCodecName));

  if (IsFlexfecAdvertisedFieldTrialEnabled()) {
    cricket::VideoCodec flexfec_codec(kFlexfecCodecName);
    // This value is currently arbitrarily set to 10 seconds. (The unit
    // is microseconds.) This parameter MUST be present in the SDP, but
    // we never use the actual value anywhere in our code however.
    // TODO(brandtr): Consider honouring this value in the sender and receiver.
    flexfec_codec.SetParam(kFlexfecFmtpRepairWindow, "10000000");
    flexfec_codec.AddFeedbackParam(
        FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
    flexfec_codec.AddFeedbackParam(
        FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
    supported_codecs_.push_back(flexfec_codec);
  }
}

InternalEncoderFactory::~InternalEncoderFactory() {}

// WebRtcVideoEncoderFactory implementation.
webrtc::VideoEncoder* InternalEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  const webrtc::VideoCodecType codec_type =
      webrtc::PayloadNameToCodecType(codec.name)
          .value_or(webrtc::kVideoCodecUnknown);
  switch (codec_type) {
    case webrtc::kVideoCodecH264:
      return webrtc::H264Encoder::Create(codec);
    case webrtc::kVideoCodecVP8:
      return webrtc::VP8Encoder::Create();
    case webrtc::kVideoCodecVP9:
      return webrtc::VP9Encoder::Create();
    default:
      return nullptr;
  }
}

const std::vector<cricket::VideoCodec>&
InternalEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void InternalEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace cricket
