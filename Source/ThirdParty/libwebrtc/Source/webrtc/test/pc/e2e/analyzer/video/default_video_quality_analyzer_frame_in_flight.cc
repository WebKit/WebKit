/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frame_in_flight.h"

#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"

namespace webrtc {
namespace {

template <typename T>
std::optional<T> MaybeGetValue(const std::unordered_map<size_t, T>& map,
                               size_t key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace

FrameInFlight::FrameInFlight(
    size_t stream,
    uint16_t frame_id,
    Timestamp captured_time,
    std::optional<TimeDelta> time_between_captured_frames,
    std::set<size_t> expected_receivers)
    : stream_(stream),
      expected_receivers_(std::move(expected_receivers)),
      frame_id_(frame_id),
      captured_time_(captured_time),
      time_between_captured_frames_(time_between_captured_frames) {}

std::vector<size_t> FrameInFlight::GetPeersWhichDidntReceive() const {
  std::vector<size_t> out;
  for (size_t peer : expected_receivers_) {
    auto it = receiver_stats_.find(peer);
    if (it == receiver_stats_.end() ||
        (!it->second.dropped && !it->second.superfluous &&
         it->second.rendered_time.IsInfinite())) {
      out.push_back(peer);
    }
  }
  return out;
}

bool FrameInFlight::HaveAllPeersReceived() const {
  for (size_t peer : expected_receivers_) {
    auto it = receiver_stats_.find(peer);
    if (it == receiver_stats_.end()) {
      return false;
    }

    if (!it->second.dropped && !it->second.superfluous &&
        it->second.rendered_time.IsInfinite()) {
      return false;
    }
  }
  return true;
}

void FrameInFlight::OnFrameEncoded(
    webrtc::Timestamp time,
    std::optional<TimeDelta> time_between_encoded_frames,
    VideoFrameType frame_type,
    DataSize encoded_image_size,
    uint32_t target_encode_bitrate,
    int stream_index,
    int qp,
    StreamCodecInfo used_encoder) {
  encoded_time_ = time;
  if (time_between_encoded_frames.has_value()) {
    time_between_encoded_frames_ =
        time_between_encoded_frames_.value_or(TimeDelta::Zero()) +
        *time_between_encoded_frames;
  }
  frame_type_ = frame_type;
  encoded_image_size_ += encoded_image_size;
  target_encode_bitrate_ += target_encode_bitrate;
  stream_layers_qp_[stream_index].AddSample(SamplesStatsCounter::StatsSample{
      .value = static_cast<double>(qp), .time = time});
  // Update used encoder info. If simulcast/SVC is used, this method can
  // be called multiple times, in such case we should preserve the value
  // of `used_encoder_.switched_on_at` from the first invocation as the
  // smallest one.
  Timestamp encoder_switched_on_at = used_encoder_.has_value()
                                         ? used_encoder_->switched_on_at
                                         : Timestamp::PlusInfinity();
  RTC_DCHECK(used_encoder.switched_on_at.IsFinite());
  RTC_DCHECK(used_encoder.switched_from_at.IsFinite());
  used_encoder_ = used_encoder;
  if (encoder_switched_on_at < used_encoder_->switched_on_at) {
    used_encoder_->switched_on_at = encoder_switched_on_at;
  }
}

void FrameInFlight::OnFramePreDecode(size_t peer,
                                     webrtc::Timestamp received_time,
                                     webrtc::Timestamp decode_start_time,
                                     VideoFrameType frame_type,
                                     DataSize encoded_image_size) {
  receiver_stats_[peer].received_time = received_time;
  receiver_stats_[peer].decode_start_time = decode_start_time;
  receiver_stats_[peer].frame_type = frame_type;
  receiver_stats_[peer].encoded_image_size = encoded_image_size;
}

bool FrameInFlight::HasReceivedTime(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.received_time.IsFinite();
}

void FrameInFlight::OnFrameDecoded(size_t peer,
                                   webrtc::Timestamp time,
                                   int width,
                                   int height,
                                   const StreamCodecInfo& used_decoder,
                                   const std::optional<uint8_t> qp) {
  receiver_stats_[peer].decode_end_time = time;
  receiver_stats_[peer].used_decoder = used_decoder;
  receiver_stats_[peer].decoded_frame_width = width;
  receiver_stats_[peer].decoded_frame_height = height;
  receiver_stats_[peer].decoded_frame_qp = qp;
}

void FrameInFlight::OnDecoderError(size_t peer,
                                   const StreamCodecInfo& used_decoder) {
  receiver_stats_[peer].decoder_failed = true;
  receiver_stats_[peer].used_decoder = used_decoder;
}

bool FrameInFlight::HasDecodeEndTime(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.decode_end_time.IsFinite();
}

void FrameInFlight::OnFrameRendered(size_t peer, webrtc::Timestamp time) {
  receiver_stats_[peer].rendered_time = time;
}

bool FrameInFlight::HasRenderedTime(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.rendered_time.IsFinite();
}

bool FrameInFlight::IsDropped(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.dropped;
}

FrameStats FrameInFlight::GetStatsForPeer(size_t peer) const {
  RTC_DCHECK_NE(frame_id_, VideoFrame::kNotSetId)
      << "Frame id isn't initialized";
  RTC_DCHECK(!IsSuperfluous(peer))
      << "This frame is superfluous for peer " << peer;
  FrameStats stats(frame_id_, captured_time_);
  stats.time_between_captured_frames = time_between_captured_frames_;
  stats.time_between_encoded_frames = time_between_encoded_frames_;
  stats.pre_encode_time = pre_encode_time_;
  stats.encoded_time = encoded_time_;
  stats.target_encode_bitrate = target_encode_bitrate_;
  stats.encoded_frame_type = frame_type_;
  stats.encoded_image_size = encoded_image_size_;
  stats.used_encoder = used_encoder_;
  stats.spatial_layers_qp = stream_layers_qp_;

  std::optional<ReceiverFrameStats> receiver_stats =
      MaybeGetValue<ReceiverFrameStats>(receiver_stats_, peer);
  if (receiver_stats.has_value()) {
    stats.received_time = receiver_stats->received_time;
    stats.decode_start_time = receiver_stats->decode_start_time;
    stats.decode_end_time = receiver_stats->decode_end_time;
    stats.rendered_time = receiver_stats->rendered_time;
    stats.prev_frame_rendered_time = receiver_stats->prev_frame_rendered_time;
    stats.time_between_rendered_frames =
        receiver_stats->time_between_rendered_frames;
    stats.decoded_frame_width = receiver_stats->decoded_frame_width;
    stats.decoded_frame_height = receiver_stats->decoded_frame_height;
    stats.decoded_frame_qp = receiver_stats->decoded_frame_qp;
    stats.used_decoder = receiver_stats->used_decoder;
    stats.pre_decoded_frame_type = receiver_stats->frame_type;
    stats.pre_decoded_image_size = receiver_stats->encoded_image_size;
    stats.decoder_failed = receiver_stats->decoder_failed;
  }
  return stats;
}

bool FrameInFlight::IsSuperfluous(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.superfluous;
}

}  // namespace webrtc
