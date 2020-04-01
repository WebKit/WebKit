/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_
#define COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_

#include <initializer_list>

#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_codec_constants.h"

namespace webrtc {

// Describes how a certain encoder buffer was used when encoding a frame.
struct CodecBufferUsage {
  constexpr CodecBufferUsage(int id, bool referenced, bool updated)
      : id(id), referenced(referenced), updated(updated) {}

  int id = 0;
  bool referenced = false;
  bool updated = false;
};

struct GenericFrameInfo : public FrameDependencyTemplate {
  static absl::InlinedVector<DecodeTargetIndication, 10> DecodeTargetInfo(
      absl::string_view indication_symbols);

  class Builder;

  GenericFrameInfo();
  GenericFrameInfo(const GenericFrameInfo&);
  ~GenericFrameInfo();

  int64_t frame_id = 0;
  absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers> encoder_buffers;
};

class GenericFrameInfo::Builder {
 public:
  Builder();
  ~Builder();

  GenericFrameInfo Build() const;
  Builder& T(int temporal_id);
  Builder& S(int spatial_id);
  Builder& Dtis(absl::string_view indication_symbols);
  Builder& Fdiffs(std::initializer_list<int> frame_diffs);

 private:
  GenericFrameInfo info_;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_
