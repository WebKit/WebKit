/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ortc/testrtpparameters.h"

#include <algorithm>
#include <utility>

namespace webrtc {

RtpParameters MakeMinimalOpusParameters() {
  RtpParameters parameters;
  RtpCodecParameters opus_codec;
  opus_codec.name = "opus";
  opus_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  opus_codec.payload_type = 111;
  opus_codec.clock_rate.emplace(48000);
  opus_codec.num_channels.emplace(2);
  parameters.codecs.push_back(std::move(opus_codec));
  RtpEncodingParameters encoding;
  encoding.codec_payload_type.emplace(111);
  parameters.encodings.push_back(std::move(encoding));
  return parameters;
}

RtpParameters MakeMinimalIsacParameters() {
  RtpParameters parameters;
  RtpCodecParameters isac_codec;
  isac_codec.name = "ISAC";
  isac_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  isac_codec.payload_type = 103;
  isac_codec.clock_rate.emplace(16000);
  parameters.codecs.push_back(std::move(isac_codec));
  RtpEncodingParameters encoding;
  encoding.codec_payload_type.emplace(111);
  parameters.encodings.push_back(std::move(encoding));
  return parameters;
}

RtpParameters MakeMinimalOpusParametersWithSsrc(uint32_t ssrc) {
  RtpParameters parameters = MakeMinimalOpusParameters();
  parameters.encodings[0].ssrc.emplace(ssrc);
  return parameters;
}

RtpParameters MakeMinimalIsacParametersWithSsrc(uint32_t ssrc) {
  RtpParameters parameters = MakeMinimalIsacParameters();
  parameters.encodings[0].ssrc.emplace(ssrc);
  return parameters;
}

RtpParameters MakeMinimalVideoParameters(const char* codec_name) {
  RtpParameters parameters;
  RtpCodecParameters vp8_codec;
  vp8_codec.name = codec_name;
  vp8_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp8_codec.payload_type = 96;
  parameters.codecs.push_back(std::move(vp8_codec));
  RtpEncodingParameters encoding;
  encoding.codec_payload_type.emplace(96);
  parameters.encodings.push_back(std::move(encoding));
  return parameters;
}

RtpParameters MakeMinimalVp8Parameters() {
  return MakeMinimalVideoParameters("VP8");
}

RtpParameters MakeMinimalVp9Parameters() {
  return MakeMinimalVideoParameters("VP9");
}

RtpParameters MakeMinimalVp8ParametersWithSsrc(uint32_t ssrc) {
  RtpParameters parameters = MakeMinimalVp8Parameters();
  parameters.encodings[0].ssrc.emplace(ssrc);
  return parameters;
}

RtpParameters MakeMinimalVp9ParametersWithSsrc(uint32_t ssrc) {
  RtpParameters parameters = MakeMinimalVp9Parameters();
  parameters.encodings[0].ssrc.emplace(ssrc);
  return parameters;
}

// Note: Currently, these "WithNoSsrc" methods are identical to the normal
// "MakeMinimal" methods, but with the added guarantee that they will never be
// changed to include an SSRC.

RtpParameters MakeMinimalOpusParametersWithNoSsrc() {
  RtpParameters parameters = MakeMinimalOpusParameters();
  RTC_DCHECK(!parameters.encodings[0].ssrc);
  return parameters;
}

RtpParameters MakeMinimalIsacParametersWithNoSsrc() {
  RtpParameters parameters = MakeMinimalIsacParameters();
  RTC_DCHECK(!parameters.encodings[0].ssrc);
  return parameters;
}

RtpParameters MakeMinimalVp8ParametersWithNoSsrc() {
  RtpParameters parameters = MakeMinimalVp8Parameters();
  RTC_DCHECK(!parameters.encodings[0].ssrc);
  return parameters;
}

RtpParameters MakeMinimalVp9ParametersWithNoSsrc() {
  RtpParameters parameters = MakeMinimalVp9Parameters();
  RTC_DCHECK(!parameters.encodings[0].ssrc);
  return parameters;
}

// Make audio parameters with all the available properties configured and
// features used, and with multiple codecs offered. Obtained by taking a
// snapshot of a default PeerConnection offer (and adding other things, like
// bitrate limit).
//
// See "MakeFullOpusParameters"/"MakeFullIsacParameters" below.
RtpParameters MakeFullAudioParameters(int preferred_payload_type) {
  RtpParameters parameters;

  RtpCodecParameters opus_codec;
  opus_codec.name = "opus";
  opus_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  opus_codec.payload_type = 111;
  opus_codec.clock_rate.emplace(48000);
  opus_codec.num_channels.emplace(2);
  opus_codec.parameters["minptime"] = "10";
  opus_codec.parameters["useinbandfec"] = "1";
  opus_codec.parameters["usedtx"] = "1";
  opus_codec.parameters["stereo"] = "1";
  opus_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::TRANSPORT_CC);
  parameters.codecs.push_back(std::move(opus_codec));

  RtpCodecParameters isac_codec;
  isac_codec.name = "ISAC";
  isac_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  isac_codec.payload_type = 103;
  isac_codec.clock_rate.emplace(16000);
  parameters.codecs.push_back(std::move(isac_codec));

  RtpCodecParameters cn_codec;
  cn_codec.name = "CN";
  cn_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  cn_codec.payload_type = 106;
  cn_codec.clock_rate.emplace(32000);
  parameters.codecs.push_back(std::move(cn_codec));

  RtpCodecParameters dtmf_codec;
  dtmf_codec.name = "telephone-event";
  dtmf_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  dtmf_codec.payload_type = 126;
  dtmf_codec.clock_rate.emplace(8000);
  parameters.codecs.push_back(std::move(dtmf_codec));

  // "codec_payload_type" isn't implemented, so we need to reorder codecs to
  // cause one to be used.
  // TODO(deadbeef): Remove this when it becomes unnecessary.
  auto it = std::find_if(parameters.codecs.begin(), parameters.codecs.end(),
                         [preferred_payload_type](const RtpCodecParameters& p) {
                           return p.payload_type == preferred_payload_type;
                         });
  RtpCodecParameters preferred = *it;
  parameters.codecs.erase(it);
  parameters.codecs.insert(parameters.codecs.begin(), preferred);

  // Intentionally leave out SSRC so one's chosen automatically.
  RtpEncodingParameters encoding;
  encoding.codec_payload_type.emplace(preferred_payload_type);
  encoding.dtx.emplace(DtxStatus::ENABLED);
  // 20 kbps.
  encoding.max_bitrate_bps.emplace(20000);
  parameters.encodings.push_back(std::move(encoding));

  parameters.header_extensions.emplace_back(
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level", 1);
  return parameters;
}

RtpParameters MakeFullOpusParameters() {
  return MakeFullAudioParameters(111);
}

RtpParameters MakeFullIsacParameters() {
  return MakeFullAudioParameters(103);
}

// Make video parameters with all the available properties configured and
// features used, and with multiple codecs offered. Obtained by taking a
// snapshot of a default PeerConnection offer (and adding other things, like
// bitrate limit).
//
// See "MakeFullVp8Parameters"/"MakeFullVp9Parameters" below.
RtpParameters MakeFullVideoParameters(int preferred_payload_type) {
  RtpParameters parameters;

  RtpCodecParameters vp8_codec;
  vp8_codec.name = "VP8";
  vp8_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp8_codec.payload_type = 100;
  vp8_codec.clock_rate.emplace(90000);
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::CCM,
                                       RtcpFeedbackMessageType::FIR);
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::NACK,
                                       RtcpFeedbackMessageType::GENERIC_NACK);
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::NACK,
                                       RtcpFeedbackMessageType::PLI);
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::REMB);
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::TRANSPORT_CC);
  parameters.codecs.push_back(std::move(vp8_codec));

  RtpCodecParameters vp8_rtx_codec;
  vp8_rtx_codec.name = "rtx";
  vp8_rtx_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp8_rtx_codec.payload_type = 96;
  vp8_rtx_codec.clock_rate.emplace(90000);
  vp8_rtx_codec.parameters["apt"] = "100";
  parameters.codecs.push_back(std::move(vp8_rtx_codec));

  RtpCodecParameters vp9_codec;
  vp9_codec.name = "VP9";
  vp9_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp9_codec.payload_type = 101;
  vp9_codec.clock_rate.emplace(90000);
  vp9_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::CCM,
                                       RtcpFeedbackMessageType::FIR);
  vp9_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::NACK,
                                       RtcpFeedbackMessageType::GENERIC_NACK);
  vp9_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::NACK,
                                       RtcpFeedbackMessageType::PLI);
  vp9_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::REMB);
  vp9_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::TRANSPORT_CC);
  parameters.codecs.push_back(std::move(vp9_codec));

  RtpCodecParameters vp9_rtx_codec;
  vp9_rtx_codec.name = "rtx";
  vp9_rtx_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp9_rtx_codec.payload_type = 97;
  vp9_rtx_codec.clock_rate.emplace(90000);
  vp9_rtx_codec.parameters["apt"] = "101";
  parameters.codecs.push_back(std::move(vp9_rtx_codec));

  RtpCodecParameters red_codec;
  red_codec.name = "red";
  red_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  red_codec.payload_type = 116;
  red_codec.clock_rate.emplace(90000);
  parameters.codecs.push_back(std::move(red_codec));

  RtpCodecParameters red_rtx_codec;
  red_rtx_codec.name = "rtx";
  red_rtx_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  red_rtx_codec.payload_type = 98;
  red_rtx_codec.clock_rate.emplace(90000);
  red_rtx_codec.parameters["apt"] = "116";
  parameters.codecs.push_back(std::move(red_rtx_codec));

  RtpCodecParameters ulpfec_codec;
  ulpfec_codec.name = "ulpfec";
  ulpfec_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  ulpfec_codec.payload_type = 117;
  ulpfec_codec.clock_rate.emplace(90000);
  parameters.codecs.push_back(std::move(ulpfec_codec));

  // "codec_payload_type" isn't implemented, so we need to reorder codecs to
  // cause one to be used.
  // TODO(deadbeef): Remove this when it becomes unnecessary.
  auto it = std::find_if(parameters.codecs.begin(), parameters.codecs.end(),
                         [preferred_payload_type](const RtpCodecParameters& p) {
                           return p.payload_type == preferred_payload_type;
                         });
  RtpCodecParameters preferred = *it;
  parameters.codecs.erase(it);
  parameters.codecs.insert(parameters.codecs.begin(), preferred);

  // Intentionally leave out SSRC so one's chosen automatically.
  RtpEncodingParameters encoding;
  encoding.codec_payload_type.emplace(preferred_payload_type);
  encoding.fec.emplace(FecMechanism::RED_AND_ULPFEC);
  // Will create default RtxParameters, with unset SSRC.
  encoding.rtx.emplace();
  // 100 kbps.
  encoding.max_bitrate_bps.emplace(100000);
  parameters.encodings.push_back(std::move(encoding));

  parameters.header_extensions.emplace_back(
      "urn:ietf:params:rtp-hdrext:toffset", 2);
  parameters.header_extensions.emplace_back(
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time", 3);
  parameters.header_extensions.emplace_back("urn:3gpp:video-orientation", 4);
  parameters.header_extensions.emplace_back(
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01",
      5);
  parameters.header_extensions.emplace_back(
      "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay", 6);
  return parameters;
}

RtpParameters MakeFullVp8Parameters() {
  return MakeFullVideoParameters(100);
}

RtpParameters MakeFullVp9Parameters() {
  return MakeFullVideoParameters(101);
}

}  // namespace webrtc
