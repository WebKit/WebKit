/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_

#include <list>
#include <limits>
#include <memory>
#include <set>
#include <string>

#include "modules/include/module.h"
#include "modules/remote_bitrate_estimator/test/bwe.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace testing {
namespace bwe {

class MetricRecorder;

class PacketSender : public PacketProcessor {
 public:
  PacketSender(PacketProcessorListener* listener, int flow_id)
      : PacketProcessor(listener, flow_id, kSender),
        running_(true),
        // For Packet::send_time_us() to be comparable with timestamps from
        // clock_, the clock of the PacketSender and the Source must be aligned.
        // We assume that both start at time 0.
        clock_(0),
        metric_recorder_(nullptr) {}
  virtual ~PacketSender() {}
  // Call GiveFeedback() with the returned interval in milliseconds, provided
  // there is a new estimate available.
  // Note that changing the feedback interval affects the timing of when the
  // output of the estimators is sampled and therefore the baseline files may
  // have to be regenerated.
  virtual int GetFeedbackIntervalMs() const = 0;
  void SetSenderTimestamps(Packets* in_out);

  virtual uint32_t TargetBitrateKbps() { return 0; }

  virtual void Pause();
  virtual void Resume(int64_t paused_time_ms);

  void set_metric_recorder(MetricRecorder* metric_recorder);
  virtual void RecordBitrate();

 protected:
  bool running_;  // Initialized by default as true.
  SimulatedClock clock_;

 private:
  MetricRecorder* metric_recorder_;
};

class VideoSender : public PacketSender, public BitrateObserver {
 public:
  VideoSender(PacketProcessorListener* listener,
              VideoSource* source,
              BandwidthEstimatorType estimator);
  virtual ~VideoSender();

  int GetFeedbackIntervalMs() const override;
  void RunFor(int64_t time_ms, Packets* in_out) override;

  virtual VideoSource* source() const { return source_; }

  uint32_t TargetBitrateKbps() override;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt) override;
  void Pause() override;
  void Resume(int64_t paused_time_ms) override;

 protected:
  void ProcessFeedbackAndGeneratePackets(int64_t time_ms,
                                         std::list<FeedbackPacket*>* feedbacks,
                                         Packets* generated);

  VideoSource* source_;
  std::unique_ptr<BweSender> bwe_;
  int64_t start_of_run_ms_;
  std::list<Module*> modules_;

 private:
  uint32_t previous_sending_bitrate_;
  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSender);
};

class PacedVideoSender : public VideoSender, public PacedSender::PacketSender {
 public:
  PacedVideoSender(PacketProcessorListener* listener,
                   VideoSource* source,
                   BandwidthEstimatorType estimator);
  virtual ~PacedVideoSender();

  void RunFor(int64_t time_ms, Packets* in_out) override;

  // Implements PacedSender::Callback.
  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission,
                        const PacedPacketInfo& pacing_info) override;
  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& pacing_info) override;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt) override;

  void OnNetworkChanged(uint32_t bitrate_for_encoder_bps,
                        uint32_t bitrate_for_pacer_bps,
                        bool in_probe_rtt,
                        int64_t rtt,
                        uint64_t congestion_window) override;
  size_t pacer_queue_size_in_bytes() override {
    return pacer_queue_size_in_bytes_;
  }
  void OnBytesAcked(size_t bytes) override;

 private:
  int64_t TimeUntilNextProcess(const std::list<Module*>& modules);
  void CallProcess(const std::list<Module*>& modules);
  void QueuePackets(Packets* batch, int64_t end_of_batch_time_us);

  size_t pacer_queue_size_in_bytes_ = 0;
  std::unique_ptr<Pacer> pacer_;
  Packets queue_;
  Packets pacer_queue_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PacedVideoSender);
};

class TcpSender : public PacketSender {
 public:
  TcpSender(PacketProcessorListener* listener, int flow_id, int64_t offset_ms);
  TcpSender(PacketProcessorListener* listener,
            int flow_id,
            int64_t offset_ms,
            int send_limit_bytes);
  virtual ~TcpSender() {}

  void RunFor(int64_t time_ms, Packets* in_out) override;
  int GetFeedbackIntervalMs() const override { return 10; }

  uint32_t TargetBitrateKbps() override;

 private:
  struct InFlight {
   public:
    explicit InFlight(const MediaPacket& packet)
        : sequence_number(packet.header().sequenceNumber),
          time_ms(packet.send_time_ms()) {}

    InFlight(uint16_t seq_num, int64_t now_ms)
        : sequence_number(seq_num), time_ms(now_ms) {}

    bool operator<(const InFlight& rhs) const {
      return sequence_number < rhs.sequence_number;
    }

    uint16_t sequence_number;  // Sequence number of a packet in flight, or a
                               // packet which has just been acked.
    int64_t time_ms;  // Time of when the packet left the sender, or when the
                      // ack was received.
  };

  void SendPackets(Packets* in_out);
  void UpdateCongestionControl(const FeedbackPacket* fb);
  int TriggerTimeouts();
  void HandleLoss();
  Packets GeneratePackets(size_t num_packets);
  void UpdateSendBitrateEstimate(size_t num_packets);

  float cwnd_;
  int ssthresh_;
  std::set<InFlight> in_flight_;
  bool ack_received_;
  uint16_t last_acked_seq_num_;
  uint16_t next_sequence_number_;
  int64_t offset_ms_;
  int64_t last_reduction_time_ms_;
  int64_t last_rtt_ms_;
  int total_sent_bytes_;
  int send_limit_bytes_;  // Initialized by default as kNoLimit.
  int64_t last_generated_packets_ms_;
  size_t num_recent_sent_packets_;
  uint32_t bitrate_kbps_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_
