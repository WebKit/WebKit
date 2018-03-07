/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BBR_PACED_SENDER_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BBR_PACED_SENDER_H_

#include <list>
#include <memory>

#include "modules/pacing/paced_sender.h"
#include "modules/pacing/pacer.h"

namespace webrtc {
namespace testing {
namespace bwe {
class CongestionWindow;
}
}  // namespace testing

struct Packet {
  Packet(RtpPacketSender::Priority priority,
         uint32_t ssrc,
         uint16_t seq_number,
         int64_t capture_time_ms,
         int64_t enqueue_time_ms,
         size_t size_in_bytes,
         bool retransmission)
      : priority(priority),
        ssrc(ssrc),
        sequence_number(seq_number),
        capture_time_ms(capture_time_ms),
        enqueue_time_ms(enqueue_time_ms),
        size_in_bytes(size_in_bytes),
        retransmission(retransmission) {}
  RtpPacketSender::Priority priority;
  uint32_t ssrc;
  uint16_t sequence_number;
  int64_t capture_time_ms;
  int64_t enqueue_time_ms;
  size_t size_in_bytes;
  bool retransmission;
};

class Clock;
class RtcEventLog;
struct Packet;
class BbrPacedSender : public Pacer {
 public:
  BbrPacedSender(const Clock* clock,
                 PacedSender::PacketSender* packet_sender,
                 RtcEventLog* event_log);
  ~BbrPacedSender() override;
  void SetEstimatedBitrateAndCongestionWindow(
      uint32_t bitrate_bps,
      bool in_probe_rtt,
      uint64_t congestion_window) override;
  void SetMinBitrate(int min_send_bitrate_bps);
  void InsertPacket(RtpPacketSender::Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override;
  void SetAccountForAudioPackets(bool account_for_audio) override {}
  int64_t TimeUntilNextProcess() override;
  void OnBytesAcked(size_t bytes) override;
  void Process() override;
  bool TryToSendPacket(Packet* packet);

 private:
  const Clock* const clock_;
  PacedSender::PacketSender* const packet_sender_;
  uint32_t estimated_bitrate_bps_;
  uint32_t min_send_bitrate_kbps_;
  uint32_t pacing_bitrate_kbps_;
  int64_t time_last_update_us_;
  int64_t time_last_update_ms_;
  int64_t next_packet_send_time_;
  float rounding_error_time_ms_;
  std::list<Packet*> packets_;
  // TODO(gnish): integrate |max_data_inflight| into congestion window class.
  size_t max_data_inflight_bytes_;
  std::unique_ptr<testing::bwe::CongestionWindow> congestion_window_;
};
}  // namespace webrtc
#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BBR_PACED_SENDER_H_
