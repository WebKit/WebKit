/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_OBJECT_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_OBJECT_H_

#include "webrtc/base/optional.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/encoded_frame.h"

namespace webrtc {
namespace video_coding {

class FrameObject : public webrtc::VCMEncodedFrame {
 public:
  static const uint8_t kMaxFrameReferences = 5;

  FrameObject();
  virtual ~FrameObject() {}

  virtual bool GetBitstream(uint8_t* destination) const = 0;

  // The capture timestamp of this frame.
  virtual uint32_t Timestamp() const = 0;

  // When this frame was received.
  virtual int64_t ReceivedTime() const = 0;

  // When this frame should be rendered.
  virtual int64_t RenderTime() const = 0;

  // This information is currently needed by the timing calculation class.
  // TODO(philipel): Remove this function when a new timing class has
  //                 been implemented.
  virtual bool delayed_by_retransmission() const { return 0; }

  size_t size() { return _length; }

  // The tuple (|picture_id|, |spatial_layer|) uniquely identifies a frame
  // object. For codec types that don't necessarily have picture ids they
  // have to be constructed from the header data relevant to that codec.
  uint16_t picture_id;
  uint8_t spatial_layer;
  uint32_t timestamp;

  // TODO(philipel): Add simple modify/access functions to prevent adding too
  // many |references|.
  size_t num_references;
  uint16_t references[kMaxFrameReferences];
  bool inter_layer_predicted;
};

class PacketBuffer;

class RtpFrameObject : public FrameObject {
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
  bool GetBitstream(uint8_t* destination) const override;
  uint32_t Timestamp() const override;
  int64_t ReceivedTime() const override;
  int64_t RenderTime() const override;
  bool delayed_by_retransmission() const override;
  rtc::Optional<RTPVideoTypeHeader> GetCodecHeader() const;

 private:
  rtc::scoped_refptr<PacketBuffer> packet_buffer_;
  enum FrameType frame_type_;
  VideoCodecType codec_type_;
  uint16_t first_seq_num_;
  uint16_t last_seq_num_;
  uint32_t timestamp_;
  int64_t received_time_;

  // Equal to times nacked of the packet with the highet times nacked
  // belonging to this frame.
  int times_nacked_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_FRAME_OBJECT_H_
