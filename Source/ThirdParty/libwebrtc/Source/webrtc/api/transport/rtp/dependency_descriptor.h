/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_RTP_DEPENDENCY_DESCRIPTOR_H_
#define API_TRANSPORT_RTP_DEPENDENCY_DESCRIPTOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"

namespace webrtc {
// Structures to build and parse dependency descriptor as described in
// https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension
class RenderResolution {
 public:
  constexpr RenderResolution() = default;
  constexpr RenderResolution(int width, int height)
      : width_(width), height_(height) {}
  RenderResolution(const RenderResolution&) = default;
  RenderResolution& operator=(const RenderResolution&) = default;

  friend bool operator==(const RenderResolution& lhs,
                         const RenderResolution& rhs) {
    return lhs.width_ == rhs.width_ && lhs.height_ == rhs.height_;
  }

  constexpr int Width() const { return width_; }
  constexpr int Height() const { return height_; }

 private:
  int width_ = 0;
  int height_ = 0;
};

// Relationship of a frame to a Decode target.
enum class DecodeTargetIndication {
  kNotPresent = 0,   // DecodeTargetInfo symbol '-'
  kDiscardable = 1,  // DecodeTargetInfo symbol 'D'
  kSwitch = 2,       // DecodeTargetInfo symbol 'S'
  kRequired = 3      // DecodeTargetInfo symbol 'R'
};

struct FrameDependencyTemplate {
  friend bool operator==(const FrameDependencyTemplate& lhs,
                         const FrameDependencyTemplate& rhs) {
    return lhs.spatial_id == rhs.spatial_id &&
           lhs.temporal_id == rhs.temporal_id &&
           lhs.decode_target_indications == rhs.decode_target_indications &&
           lhs.frame_diffs == rhs.frame_diffs &&
           lhs.chain_diffs == rhs.chain_diffs;
  }

  int spatial_id = 0;
  int temporal_id = 0;
  absl::InlinedVector<DecodeTargetIndication, 10> decode_target_indications;
  absl::InlinedVector<int, 4> frame_diffs;
  absl::InlinedVector<int, 4> chain_diffs;
};

struct FrameDependencyStructure {
  friend bool operator==(const FrameDependencyStructure& lhs,
                         const FrameDependencyStructure& rhs) {
    return lhs.num_decode_targets == rhs.num_decode_targets &&
           lhs.num_chains == rhs.num_chains &&
           lhs.decode_target_protected_by_chain ==
               rhs.decode_target_protected_by_chain &&
           lhs.resolutions == rhs.resolutions && lhs.templates == rhs.templates;
  }

  int structure_id = 0;
  int num_decode_targets = 0;
  int num_chains = 0;
  // If chains are used (num_chains > 0), maps decode target index into index of
  // the chain protecting that target or |num_chains| value if decode target is
  // not protected by a chain.
  absl::InlinedVector<int, 10> decode_target_protected_by_chain;
  absl::InlinedVector<RenderResolution, 4> resolutions;
  std::vector<FrameDependencyTemplate> templates;
};

struct DependencyDescriptor {
  bool first_packet_in_frame = true;
  bool last_packet_in_frame = true;
  int frame_number = 0;
  FrameDependencyTemplate frame_dependencies;
  absl::optional<RenderResolution> resolution;
  absl::optional<uint32_t> active_decode_targets_bitmask;
  std::unique_ptr<FrameDependencyStructure> attached_structure;
};

}  // namespace webrtc

#endif  // API_TRANSPORT_RTP_DEPENDENCY_DESCRIPTOR_H_
