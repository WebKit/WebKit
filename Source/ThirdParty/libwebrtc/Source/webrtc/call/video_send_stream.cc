/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/video_send_stream.h"

namespace webrtc {

VideoSendStream::StreamStats::StreamStats() = default;
VideoSendStream::StreamStats::~StreamStats() = default;

std::string VideoSendStream::StreamStats::ToString() const {
  std::stringstream ss;
  ss << "width: " << width << ", ";
  ss << "height: " << height << ", ";
  ss << "key: " << frame_counts.key_frames << ", ";
  ss << "delta: " << frame_counts.delta_frames << ", ";
  ss << "total_bps: " << total_bitrate_bps << ", ";
  ss << "retransmit_bps: " << retransmit_bitrate_bps << ", ";
  ss << "avg_delay_ms: " << avg_delay_ms << ", ";
  ss << "max_delay_ms: " << max_delay_ms << ", ";
  ss << "cum_loss: " << rtcp_stats.packets_lost << ", ";
  ss << "max_ext_seq: " << rtcp_stats.extended_highest_sequence_number << ", ";
  ss << "nack: " << rtcp_packet_type_counts.nack_packets << ", ";
  ss << "fir: " << rtcp_packet_type_counts.fir_packets << ", ";
  ss << "pli: " << rtcp_packet_type_counts.pli_packets;
  return ss.str();
}

VideoSendStream::Stats::Stats() = default;
VideoSendStream::Stats::~Stats() = default;

std::string VideoSendStream::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "VideoSendStream stats: " << time_ms << ", {";
  ss << "input_fps: " << input_frame_rate << ", ";
  ss << "encode_fps: " << encode_frame_rate << ", ";
  ss << "encode_ms: " << avg_encode_time_ms << ", ";
  ss << "encode_usage_perc: " << encode_usage_percent << ", ";
  ss << "target_bps: " << target_media_bitrate_bps << ", ";
  ss << "media_bps: " << media_bitrate_bps << ", ";
  ss << "preferred_media_bitrate_bps: " << preferred_media_bitrate_bps << ", ";
  ss << "suspended: " << (suspended ? "true" : "false") << ", ";
  ss << "bw_adapted: " << (bw_limited_resolution ? "true" : "false");
  ss << '}';
  for (const auto& substream : substreams) {
    if (!substream.second.is_rtx && !substream.second.is_flexfec) {
      ss << " {ssrc: " << substream.first << ", ";
      ss << substream.second.ToString();
      ss << '}';
    }
  }
  return ss.str();
}

VideoSendStream::Config::Config(const Config&) = default;
VideoSendStream::Config::Config(Config&&) = default;
VideoSendStream::Config::Config(Transport* send_transport)
    : send_transport(send_transport) {}

VideoSendStream::Config& VideoSendStream::Config::operator=(Config&&) = default;
VideoSendStream::Config::Config::~Config() = default;

std::string VideoSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{encoder_settings: " << encoder_settings.ToString();
  ss << ", rtp: " << rtp.ToString();
  ss << ", pre_encode_callback: "
     << (pre_encode_callback ? "(VideoSinkInterface)" : "nullptr");
  ss << ", post_encode_callback: "
     << (post_encode_callback ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << ", suspend_below_min_bitrate: "
     << (suspend_below_min_bitrate ? "on" : "off");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::EncoderSettings::ToString() const {
  std::stringstream ss;
  ss << "{payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;
  ss << ", encoder: " << (encoder ? "(VideoEncoder)" : "nullptr");
  ss << '}';
  return ss.str();
}

VideoSendStream::Config::Rtp::Rtp() = default;
VideoSendStream::Config::Rtp::Rtp(const Rtp&) = default;
VideoSendStream::Config::Rtp::~Rtp() = default;

VideoSendStream::Config::Rtp::Flexfec::Flexfec() = default;
VideoSendStream::Config::Rtp::Flexfec::Flexfec(const Flexfec&) = default;
VideoSendStream::Config::Rtp::Flexfec::~Flexfec() = default;

std::string VideoSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", ulpfec: " << ulpfec.ToString();

  ss << ", flexfec: {payload_type: " << flexfec.payload_type;
  ss << ", ssrc: " << flexfec.ssrc;
  ss << ", protected_media_ssrcs: [";
  for (size_t i = 0; i < flexfec.protected_media_ssrcs.size(); ++i) {
    ss << flexfec.protected_media_ssrcs[i];
    if (i != flexfec.protected_media_ssrcs.size() - 1)
      ss << ", ";
  }
  ss << "]}";

  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

VideoSendStream::Config::Rtp::Rtx::Rtx() = default;
VideoSendStream::Config::Rtp::Rtx::Rtx(const Rtx&) = default;
VideoSendStream::Config::Rtp::Rtx::~Rtx() = default;

std::string VideoSendStream::Config::Rtp::Rtx::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

}  // namespace webrtc
