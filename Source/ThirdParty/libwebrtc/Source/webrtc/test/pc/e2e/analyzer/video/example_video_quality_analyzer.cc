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

#include "api/array_view.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {

ExampleVideoQualityAnalyzer::ExampleVideoQualityAnalyzer() = default;
ExampleVideoQualityAnalyzer::~ExampleVideoQualityAnalyzer() = default;

void ExampleVideoQualityAnalyzer::Start(
    std::string test_case_name,
    rtc::ArrayView<const std::string> peer_names,
    int max_threads_count) {}

uint16_t ExampleVideoQualityAnalyzer::OnFrameCaptured(
    absl::string_view peer_name,
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&lock_);
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
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&lock_);
  ++frames_pre_encoded_;
}

void ExampleVideoQualityAnalyzer::OnFrameEncoded(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image,
    const EncoderStats& stats) {
  MutexLock lock(&lock_);
  ++frames_encoded_;
}

void ExampleVideoQualityAnalyzer::OnFrameDropped(
    absl::string_view peer_name,
    webrtc::EncodedImageCallback::DropReason reason) {
  RTC_LOG(INFO) << "Frame dropped by encoder";
  MutexLock lock(&lock_);
  ++frames_dropped_;
}

void ExampleVideoQualityAnalyzer::OnFramePreDecode(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image) {
  MutexLock lock(&lock_);
  ++frames_received_;
}

void ExampleVideoQualityAnalyzer::OnFrameDecoded(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    const DecoderStats& stats) {
  MutexLock lock(&lock_);
  ++frames_decoded_;
}

void ExampleVideoQualityAnalyzer::OnFrameRendered(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&lock_);
  frames_in_flight_.erase(frame.id());
  ++frames_rendered_;
}

void ExampleVideoQualityAnalyzer::OnEncoderError(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Failed to encode frame " << frame.id()
                    << ". Code: " << error_code;
}

void ExampleVideoQualityAnalyzer::OnDecoderError(absl::string_view peer_name,
                                                 uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Failed to decode frame " << frame_id
                    << ". Code: " << error_code;
}

void ExampleVideoQualityAnalyzer::Stop() {
  MutexLock lock(&lock_);
  RTC_LOG(INFO) << "There are " << frames_in_flight_.size()
                << " frames in flight, assuming all of them are dropped";
  frames_dropped_ += frames_in_flight_.size();
}

std::string ExampleVideoQualityAnalyzer::GetStreamLabel(uint16_t frame_id) {
  MutexLock lock(&lock_);
  auto it = frames_to_stream_label_.find(frame_id);
  RTC_DCHECK(it != frames_to_stream_label_.end())
      << "Unknown frame_id=" << frame_id;
  return it->second;
}

uint64_t ExampleVideoQualityAnalyzer::frames_captured() const {
  MutexLock lock(&lock_);
  return frames_captured_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_pre_encoded() const {
  MutexLock lock(&lock_);
  return frames_pre_encoded_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_encoded() const {
  MutexLock lock(&lock_);
  return frames_encoded_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_received() const {
  MutexLock lock(&lock_);
  return frames_received_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_decoded() const {
  MutexLock lock(&lock_);
  return frames_decoded_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_rendered() const {
  MutexLock lock(&lock_);
  return frames_rendered_;
}

uint64_t ExampleVideoQualityAnalyzer::frames_dropped() const {
  MutexLock lock(&lock_);
  return frames_dropped_;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
