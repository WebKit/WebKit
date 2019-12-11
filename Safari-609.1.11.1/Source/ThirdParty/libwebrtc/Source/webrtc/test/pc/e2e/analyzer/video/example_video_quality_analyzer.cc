/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/example_video_quality_analyzer.h"

#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {

ExampleVideoQualityAnalyzer::ExampleVideoQualityAnalyzer() = default;
ExampleVideoQualityAnalyzer::~ExampleVideoQualityAnalyzer() = default;

void ExampleVideoQualityAnalyzer::Start(std::string test_case_name,
                                        int max_threads_count) {}

uint16_t ExampleVideoQualityAnalyzer::OnFrameCaptured(
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  uint16_t frame_id = next_frame_id_++;
  auto it = frames_in_flight_.find(frame_id);
  if (it == frames_in_flight_.end()) {
    frames_in_flight_.insert(frame_id);
    frames_to_stream_label_.insert({frame_id, stream_label});
  } else {
    RTC_LOG(WARNING) << "Meet new frame with the same id: " << frame_id
                     << ". Assumes old one as dropped";
    // We needn't insert frame to frames_in_flight_, because it is already
    // there.
    ++frames_dropped_;
    auto stream_it = frames_to_stream_label_.find(frame_id);
    RTC_CHECK(stream_it != frames_to_stream_label_.end());
    stream_it->second = stream_label;
  }
  ++frames_captured_;
  return frame_id;
}

void ExampleVideoQualityAnalyzer::OnFramePreEncode(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  ++frames_pre_encoded_;
}

void ExampleVideoQualityAnalyzer::OnFrameEncoded(
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image) {
  rtc::CritScope crit(&lock_);
  ++frames_encoded_;
}

void ExampleVideoQualityAnalyzer::OnFrameDropped(
    webrtc::EncodedImageCallback::DropReason reason) {
  RTC_LOG(INFO) << "Frame dropped by encoder";
  rtc::CritScope crit(&lock_);
  ++frames_dropped_;
}

void ExampleVideoQualityAnalyzer::OnFrameReceived(
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image) {
  rtc::CritScope crit(&lock_);
  ++frames_received_;
}

void ExampleVideoQualityAnalyzer::OnFrameDecoded(
    const webrtc::VideoFrame& frame,
    absl::optional<int32_t> decode_time_ms,
    absl::optional<uint8_t> qp) {
  rtc::CritScope crit(&lock_);
  ++frames_decoded_;
}

void ExampleVideoQualityAnalyzer::OnFrameRendered(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  frames_in_flight_.erase(frame.id());
  ++frames_rendered_;
}

void ExampleVideoQualityAnalyzer::OnEncoderError(
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Failed to encode frame " << frame.id()
                    << ". Code: " << error_code;
}

void ExampleVideoQualityAnalyzer::OnDecoderError(uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Failed to decode frame " << frame_id
                    << ". Code: " << error_code;
}

void ExampleVideoQualityAnalyzer::Stop() {
  rtc::CritScope crit(&lock_);
  RTC_LOG(INFO) << "There are " << frames_in_flight_.size()
                << " frames in flight, assuming all of them are dropped";
  frames_dropped_ += frames_in_flight_.size();
}

std::string ExampleVideoQualityAnalyzer::GetStreamLabel(uint16_t frame_id) {
  rtc::CritScope crit(&lock_);
  auto it = frames_to_stream_label_.find(frame_id);
  RTC_DCHECK(it != frames_to_stream_label_.end())
      << "Unknown frame_id=" << frame_id;
  return it->second;
}

uint64_t ExampleVideoQualityAnalyzer::frames_captured() const {
  rtc::CritScope crit(&lock_);
  return frames_captured_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_pre_encoded() const {
  rtc::CritScope crit(&lock_);
  return frames_pre_encoded_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_encoded() const {
  rtc::CritScope crit(&lock_);
  return frames_encoded_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_received() const {
  rtc::CritScope crit(&lock_);
  return frames_received_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_decoded() const {
  rtc::CritScope crit(&lock_);
  return frames_decoded_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_rendered() const {
  rtc::CritScope crit(&lock_);
  return frames_rendered_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_dropped() const {
  rtc::CritScope crit(&lock_);
  return frames_dropped_;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
