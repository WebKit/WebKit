/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"

#include "rtc_base/checks.h"

namespace webrtc {

constexpr size_t RtpGenericFrameDescriptor::kMaxNumFrameDependencies;

RtpGenericFrameDescriptor::RtpGenericFrameDescriptor() = default;

int RtpGenericFrameDescriptor::TemporalLayer() const {
  RTC_DCHECK(FirstPacketInSubFrame());
  return temporal_layer_;
}

void RtpGenericFrameDescriptor::SetTemporalLayer(int temporal_layer) {
  RTC_DCHECK_GE(temporal_layer, 0);
  RTC_DCHECK_LE(temporal_layer, 7);
  temporal_layer_ = temporal_layer;
}

uint8_t RtpGenericFrameDescriptor::SpatialLayersBitmask() const {
  RTC_DCHECK(FirstPacketInSubFrame());
  return spatial_layers_;
}

void RtpGenericFrameDescriptor::SetSpatialLayersBitmask(
    uint8_t spatial_layers) {
  RTC_DCHECK(FirstPacketInSubFrame());
  spatial_layers_ = spatial_layers;
}

uint16_t RtpGenericFrameDescriptor::FrameId() const {
  RTC_DCHECK(FirstPacketInSubFrame());
  return frame_id_;
}

void RtpGenericFrameDescriptor::SetFrameId(uint16_t frame_id) {
  RTC_DCHECK(FirstPacketInSubFrame());
  frame_id_ = frame_id;
}

rtc::ArrayView<const uint16_t>
RtpGenericFrameDescriptor::FrameDependenciesDiffs() const {
  RTC_DCHECK(FirstPacketInSubFrame());
  return rtc::MakeArrayView(frame_deps_id_diffs_, num_frame_deps_);
}

bool RtpGenericFrameDescriptor::AddFrameDependencyDiff(uint16_t fdiff) {
  RTC_DCHECK(FirstPacketInSubFrame());
  if (num_frame_deps_ == kMaxNumFrameDependencies)
    return false;
  if (fdiff == 0)
    return false;
  RTC_DCHECK_LT(fdiff, 1 << 14);
  RTC_DCHECK_GT(fdiff, 0);
  frame_deps_id_diffs_[num_frame_deps_] = fdiff;
  num_frame_deps_++;
  return true;
}

}  // namespace webrtc
