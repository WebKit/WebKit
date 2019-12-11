/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_EXTENSION_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_EXTENSION_H_

#include <cstdint>

#include "api/array_view.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_reader.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_writer.h"

namespace webrtc {
// Trait to read/write the dependency descriptor extension as described in
// https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension
// While the format is still in design, the code might change without backward
// compatibility.
class RtpDependencyDescriptorExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionGenericFrameDescriptor02;
  // TODO(bugs.webrtc.org/10342): Use uri from the spec when there is one.
  static constexpr char kUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/"
      "generic-frame-descriptor-02";

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    RtpDependencyDescriptorReader* reader,
                    DependencyDescriptor* descriptor) {
    return reader->Parse(data, descriptor);
  }

  static size_t ValueSize(RtpDependencyDescriptorWriter* writer,
                          const DependencyDescriptor& descriptor) {
    return writer->ValueSizeBytes(descriptor);
  }
  static bool Write(rtc::ArrayView<uint8_t> data,
                    RtpDependencyDescriptorWriter* writer,
                    const DependencyDescriptor& descriptor) {
    return writer->Write(descriptor, data);
  }
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_EXTENSION_H_
