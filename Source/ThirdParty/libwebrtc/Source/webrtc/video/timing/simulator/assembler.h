/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_ASSEMBLER_H_
#define VIDEO_TIMING_SIMULATOR_ASSEMBLER_H_

#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_frame.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "rtc_base/thread_annotations.h"
#include "video/rtp_video_stream_receiver2.h"

namespace webrtc::video_timing_simulator {

// Callback for observer events. Implemented by the metadata collector.
class AssemblerEvents {
 public:
  virtual ~AssemblerEvents() = default;
  virtual void OnAssembledFrame(const EncodedFrame& assembled_frame) = 0;
};

// Callback for assembled frames. Implemented by consumers of the class.
class AssembledFrameCallback {
 public:
  virtual ~AssembledFrameCallback() = default;
  virtual void OnAssembledFrame(
      std::unique_ptr<EncodedFrame> encoded_frame) = 0;
};

// Callback for decoded frame ids. Implemented by this class.
class DecodedFrameIdCallback {
 public:
  virtual ~DecodedFrameIdCallback() = default;
  virtual void OnDecodedFrameId(int64_t frame_id) = 0;
};

// The `Assembler` takes a sequence of `RtpPacketReceived`s belonging to the
// same stream and produces a sequence of assembled `EncodedFrame`s. The work is
// delegated to the `RtpVideoStreamReceiver2`.
class Assembler : public DecodedFrameIdCallback,
                  public Transport,
                  public RtpVideoStreamReceiver2::OnCompleteFrameCallback {
 public:
  Assembler(const Environment& env,
            uint32_t ssrc,
            AssemblerEvents* absl_nonnull observer,
            AssembledFrameCallback* absl_nonnull assembled_frame_cb);
  ~Assembler() override;

  Assembler(const Assembler&) = delete;
  Assembler& operator=(const Assembler&) = delete;

  // Inserts `rtp_packet` into `RtpVideoStreamReceiver2` and calls
  // `assembled_frame_cb` if the insertion resulted in one or more
  // `EncodedFrame`s.
  void InsertPacket(const RtpPacketReceived& rtp_packet);

  // Implements `DecodedFrameIdCallback`.
  // Lets the `RtpVideoStreamReceiver2` know that `frame_id` has been "decoded",
  // so that it can be flushed from the `PacketBuffer`.
  void OnDecodedFrameId(int64_t frame_id) override;

 private:
  // Trivially implements `Transport`.
  // We need to implement this due to an RTC_DCHECK in rtcp_sender.cc.
  bool SendRtp(ArrayView<const uint8_t>, const PacketOptions&) override {
    return true;
  }
  bool SendRtcp(ArrayView<const uint8_t>, const PacketOptions&) override {
    return true;
  }

  // Implements `RtpVideoStreamReceiver2::OnCompleteFrameCallback`. Logs any
  // assembled frames to the `observer_`.
  void OnCompleteFrame(std::unique_ptr<EncodedFrame> encoded_frame) override;

  // Environment.
  SequenceChecker sequence_checker_;
  const Environment env_;

  // Worker objects.
  const VideoReceiveStreamInterface::Config video_receive_stream_config_
      RTC_GUARDED_BY(sequence_checker_);
  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_
      RTC_GUARDED_BY(sequence_checker_);
  absl::flat_hash_set<uint8_t> registered_payload_types_
      RTC_GUARDED_BY(sequence_checker_);
  RtpVideoStreamReceiver2 rtp_video_stream_receiver2_
      RTC_GUARDED_BY(sequence_checker_);

  // Outputs.
  AssemblerEvents& observer_;
  AssembledFrameCallback& assembled_frame_cb_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_ASSEMBLER_H_
