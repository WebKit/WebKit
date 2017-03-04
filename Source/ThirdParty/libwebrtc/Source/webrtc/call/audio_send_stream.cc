/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/audio_send_stream.h"

#include <string>

namespace {

std::string ToString(const webrtc::CodecInst& codec_inst) {
  std::stringstream ss;
  ss << "{pltype: " << codec_inst.pltype;
  ss << ", plname: \"" << codec_inst.plname << "\"";
  ss << ", plfreq: " << codec_inst.plfreq;
  ss << ", pacsize: " << codec_inst.pacsize;
  ss << ", channels: " << codec_inst.channels;
  ss << ", rate: " << codec_inst.rate;
  ss << '}';
  return ss.str();
}
}  // namespace

namespace webrtc {

AudioSendStream::Stats::Stats() = default;
AudioSendStream::Stats::~Stats() = default;

AudioSendStream::Config::Config(Transport* send_transport)
    : send_transport(send_transport) {}

AudioSendStream::Config::~Config() = default;

std::string AudioSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", send_transport: " << (send_transport ? "(Transport)" : "null");
  ss << ", voe_channel_id: " << voe_channel_id;
  ss << ", min_bitrate_bps: " << min_bitrate_bps;
  ss << ", max_bitrate_bps: " << max_bitrate_bps;
  ss << ", send_codec_spec: " << send_codec_spec.ToString();
  ss << '}';
  return ss.str();
}

AudioSendStream::Config::Rtp::Rtp() = default;

AudioSendStream::Config::Rtp::~Rtp() = default;

std::string AudioSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrc: " << ssrc;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << ", nack: " << nack.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

AudioSendStream::Config::SendCodecSpec::SendCodecSpec() {
  webrtc::CodecInst empty_inst = {0};
  codec_inst = empty_inst;
  codec_inst.pltype = -1;
}

std::string AudioSendStream::Config::SendCodecSpec::ToString() const {
  std::stringstream ss;
  ss << "{nack_enabled: " << (nack_enabled ? "true" : "false");
  ss << ", transport_cc_enabled: " << (transport_cc_enabled ? "true" : "false");
  ss << ", enable_codec_fec: " << (enable_codec_fec ? "true" : "false");
  ss << ", enable_opus_dtx: " << (enable_opus_dtx ? "true" : "false");
  ss << ", opus_max_playback_rate: " << opus_max_playback_rate;
  ss << ", cng_payload_type: " << cng_payload_type;
  ss << ", cng_plfreq: " << cng_plfreq;
  ss << ", min_ptime: " << min_ptime_ms;
  ss << ", max_ptime: " << max_ptime_ms;
  ss << ", codec_inst: " << ::ToString(codec_inst);
  ss << '}';
  return ss.str();
}

bool AudioSendStream::Config::SendCodecSpec::operator==(
    const AudioSendStream::Config::SendCodecSpec& rhs) const {
  if (nack_enabled == rhs.nack_enabled &&
      transport_cc_enabled == rhs.transport_cc_enabled &&
      enable_codec_fec == rhs.enable_codec_fec &&
      enable_opus_dtx == rhs.enable_opus_dtx &&
      opus_max_playback_rate == rhs.opus_max_playback_rate &&
      cng_payload_type == rhs.cng_payload_type &&
      cng_plfreq == rhs.cng_plfreq && max_ptime_ms == rhs.max_ptime_ms &&
      min_ptime_ms == rhs.min_ptime_ms && codec_inst == rhs.codec_inst) {
    return true;
  }
  return false;
}
}  // namespace webrtc
