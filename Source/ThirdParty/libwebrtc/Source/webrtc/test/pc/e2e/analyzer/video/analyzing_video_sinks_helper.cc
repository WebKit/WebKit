/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/analyzing_video_sinks_helper.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/video/video_frame_writer.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void AnalyzingVideoSinksHelper::AddConfig(absl::string_view sender_peer_name,
                                          VideoConfig config) {
  MutexLock lock(&mutex_);
  auto it = video_configs_.find(*config.stream_label);
  if (it == video_configs_.end()) {
    std::string stream_label = *config.stream_label;
    video_configs_.emplace(
        std::move(stream_label),
        std::pair{std::string(sender_peer_name), std::move(config)});
  } else {
    it->second = std::pair{std::string(sender_peer_name), std::move(config)};
  }
}

std::optional<std::pair<std::string, VideoConfig>>
AnalyzingVideoSinksHelper::GetPeerAndConfig(absl::string_view stream_label) {
  MutexLock lock(&mutex_);
  auto it = video_configs_.find(std::string(stream_label));
  if (it == video_configs_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void AnalyzingVideoSinksHelper::RemoveConfig(absl::string_view stream_label) {
  MutexLock lock(&mutex_);
  video_configs_.erase(std::string(stream_label));
}

test::VideoFrameWriter* AnalyzingVideoSinksHelper::AddVideoWriter(
    std::unique_ptr<test::VideoFrameWriter> video_writer) {
  MutexLock lock(&mutex_);
  test::VideoFrameWriter* out = video_writer.get();
  video_writers_.push_back(std::move(video_writer));
  return out;
}

void AnalyzingVideoSinksHelper::CloseAndRemoveVideoWriters(
    std::set<test::VideoFrameWriter*> writers_to_close) {
  MutexLock lock(&mutex_);
  for (auto it = video_writers_.cbegin(); it != video_writers_.cend();) {
    if (writers_to_close.find(it->get()) != writers_to_close.end()) {
      (*it)->Close();
      it = video_writers_.erase(it);
    } else {
      ++it;
    }
  }
}

void AnalyzingVideoSinksHelper::Clear() {
  MutexLock lock(&mutex_);
  video_configs_.clear();
  for (const auto& video_writer : video_writers_) {
    video_writer->Close();
  }
  video_writers_.clear();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
