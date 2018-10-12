/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_OBJECT_H_
#define MODULES_VIDEO_CODING_FRAME_OBJECT_H_

#include "absl/types/optional.h"
#include "api/video/encoded_frame.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"

namespace webrtc {
namespace video_coding {

class PacketBuffer;

class RtpFrameObject : public EncodedFrame {
 public:
  RtpFrameObject(PacketBuffer* packet_buffer,
                 uint16_t first_seq_num,
                 uint16_t last_seq_num,
                 size_t frame_size,
                 int times_nacked,
                 int64_t received_time);

  ~RtpFrameObject();
  uint16_t first_seq_num() const;
  uint16_t last_seq_num() const;
  int times_nacked() const;
  enum FrameType frame_type() const;
  VideoCodecType codec_type() const;
  void SetBitstream(rtc::ArrayView<const uint8_t> bitstream);
  bool GetBitstream(uint8_t* destination) const override;
  int64_t ReceivedTime() const override;
  int64_t RenderTime() const override;
  bool delayed_by_retransmission() const override;
  absl::optional<RTPVideoHeader> GetRtpVideoHeader() const;
  absl::optional<RtpGenericFrameDescriptor> GetGenericFrameDescriptor() const;
  absl::optional<FrameMarking> GetFrameMarking() const;

 private:
  void AllocateBitstreamBuffer(size_t frame_size);

  rtc::scoped_refptr<PacketBuffer> packet_buffer_;
  enum FrameType frame_type_;
  VideoCodecType codec_type_;
  uint16_t first_seq_num_;
  uint16_t last_seq_num_;
  int64_t received_time_;

  // Equal to times nacked of the packet with the highet times nacked
  // belonging to this frame.
  int times_nacked_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_OBJECT_H_
