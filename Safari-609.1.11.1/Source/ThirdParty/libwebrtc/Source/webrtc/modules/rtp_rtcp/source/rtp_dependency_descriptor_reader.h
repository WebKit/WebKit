/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_

#include <cstdint>

#include "api/array_view.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {
// Keeps and updates state required to deserialize DependencyDescriptor
// rtp header extension.
class RtpDependencyDescriptorReader {
 public:
  // Parses the dependency descriptor. Returns false on failure.
  // Updates frame dependency structures if parsed descriptor has a new one.
  // Doesn't update own state when Parse fails.
  bool Parse(rtc::ArrayView<const uint8_t> raw_data,
             DependencyDescriptor* descriptor) {
    // TODO(bugs.webrtc.org/10342): Implement.
    return false;
  }

  // Returns latest valid parsed structure or nullptr if none was parsed so far.
  const FrameDependencyStructure* GetStructure() const {
    // TODO(bugs.webrtc.org/10342): Implement.
    return nullptr;
  }
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
