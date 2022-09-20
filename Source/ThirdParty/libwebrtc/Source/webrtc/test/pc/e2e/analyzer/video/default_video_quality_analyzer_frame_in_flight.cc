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

#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame_type.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"

namespace webrtc {
namespace {

template <typename T>
absl::optional<T> MaybeGetValue(const std::map<size_t, T>& map, size_t key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return absl::nullopt;
  }
  return it->second;
}

}  // namespace

FrameInFlight::FrameInFlight(size_t stream,
                             VideoFrame frame,
                             Timestamp captured_time,
                             std::set<size_t> expected_receivers)
    : stream_(stream),
      expected_receivers_(std::move(expected_receivers)),
      frame_(std::move(frame)),
      captured_time_(captured_time) {}

bool FrameInFlight::RemoveFrame() {
  if (!frame_) {
    return false;
  }
  frame_ = absl::nullopt;
  return true;
}

void FrameInFlight::SetFrameId(uint16_t id) {
  if (frame_) {
    frame_->set_id(id);
  }
}

std::vector<size_t> FrameInFlight::GetPeersWhichDidntReceive() const {
  std::vector<size_t> out;
  for (size_t peer : expected_receivers_) {
    auto it = receiver_stats_.find(peer);
    if (it == receiver_stats_.end() ||
        (!it->second.dropped && it->second.rendered_time.IsInfinite())) {
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

    if (!it->second.dropped && it->second.rendered_time.IsInfinite()) {
      return false;
    }
  }
  return true;
}

void FrameInFlight::OnFrameEncoded(webrtc::Timestamp time,
                                   VideoFrameType frame_type,
                                   DataSize encoded_image_size,
                                   uint32_t target_encode_bitrate,
                                   StreamCodecInfo used_encoder) {
  encoded_time_ = time;
  frame_type_ = frame_type;
  encoded_image_size_ = encoded_image_size;
  target_encode_bitrate_ += target_encode_bitrate;
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
                                   StreamCodecInfo used_decoder) {
  receiver_stats_[peer].decode_end_time = time;
  receiver_stats_[peer].used_decoder = used_decoder;
}

bool FrameInFlight::HasDecodeEndTime(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.decode_end_time.IsFinite();
}

void FrameInFlight::OnFrameRendered(size_t peer,
                                    webrtc::Timestamp time,
                                    int width,
                                    int height) {
  receiver_stats_[peer].rendered_time = time;
  receiver_stats_[peer].rendered_frame_width = width;
  receiver_stats_[peer].rendered_frame_height = height;
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
  FrameStats stats(captured_time_);
  stats.pre_encode_time = pre_encode_time_;
  stats.encoded_time = encoded_time_;
  stats.target_encode_bitrate = target_encode_bitrate_;
  stats.encoded_frame_type = frame_type_;
  stats.encoded_image_size = encoded_image_size_;
  stats.used_encoder = used_encoder_;

  absl::optional<ReceiverFrameStats> receiver_stats =
      MaybeGetValue<ReceiverFrameStats>(receiver_stats_, peer);
  if (receiver_stats.has_value()) {
    stats.received_time = receiver_stats->received_time;
    stats.decode_start_time = receiver_stats->decode_start_time;
    stats.decode_end_time = receiver_stats->decode_end_time;
    stats.rendered_time = receiver_stats->rendered_time;
    stats.prev_frame_rendered_time = receiver_stats->prev_frame_rendered_time;
    stats.rendered_frame_width = receiver_stats->rendered_frame_width;
    stats.rendered_frame_height = receiver_stats->rendered_frame_height;
    stats.used_decoder = receiver_stats->used_decoder;
    stats.pre_decoded_frame_type = receiver_stats->frame_type;
    stats.pre_decoded_image_size = receiver_stats->encoded_image_size;
  }
  return stats;
}

}  // namespace webrtc
