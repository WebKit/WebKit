/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RECEIVER_H_
#define VIDEO_TIMING_SIMULATOR_RECEIVER_H_

#include <cstdint>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/rtx_receive_stream.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/thread_annotations.h"
#include "video/timing/simulator/rtp_packet_simulator.h"

namespace webrtc::video_timing_simulator {

// Callback for received RTP packets. Implemented by consumers of this class.
class ReceivedRtpPacketCallback {
 public:
  virtual ~ReceivedRtpPacketCallback() = default;
  virtual void OnReceivedRtpPacket(const RtpPacketReceived& rtp_packet) = 0;
};

// The `Receiver` takes a sequence of muxed
// `RtpPacketSimulator::SimulatedPacket`s (containing either video or RTX)
// and produces a sequence of decapsulated `RtpPacketReceived`s (video only).
class Receiver {
 public:
  Receiver(const Environment& env,
           uint32_t ssrc,
           uint32_t rtx_ssrc,
           ReceivedRtpPacketCallback* absl_nonnull received_rtp_packet_cb);
  ~Receiver();

  void InsertSimulatedPacket(
      const RtpPacketSimulator::SimulatedPacket& simulated_packet);

 private:
  // Trivial translation from `RtpPacketSinkInterface` to
  // `ReceivedRtpPacketCallback`.
  class Adapter : public RtpPacketSinkInterface {
   public:
    explicit Adapter(
        ReceivedRtpPacketCallback* absl_nonnull received_rtp_packet_cb)
        : received_rtp_packet_cb_(*received_rtp_packet_cb) {}
    ~Adapter() override = default;

    void OnRtpPacket(const RtpPacketReceived& rtp_packet) override {
      received_rtp_packet_cb_.OnReceivedRtpPacket(rtp_packet);
    }

   private:
    ReceivedRtpPacketCallback& received_rtp_packet_cb_;
  };

  void InsertVideoPacket(const RtpPacketReceived& rtp_packet);
  void InsertRtxPacket(const RtpPacketReceived& rtp_packet);

  // Environment.
  SequenceChecker sequence_checker_;
  const Environment env_;

  // Config.
  const uint32_t ssrc_;
  const uint32_t rtx_ssrc_;

  // Worker objects.
  Adapter adapter_ RTC_GUARDED_BY(sequence_checker_);
  RtxReceiveStream rtx_receive_stream_ RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RECEIVER_H_
