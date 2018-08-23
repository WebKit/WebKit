/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACED_SENDER_H_
#define MODULES_PACING_PACED_SENDER_H_

#include <memory>

#include "absl/types/optional.h"
#include "modules/pacing/pacer.h"
#include "modules/pacing/packet_queue_interface.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class AlrDetector;
class BitrateProber;
class Clock;
class RtcEventLog;
class IntervalBudget;

class PacedSender : public Pacer {
 public:
  class PacketSender {
   public:
    // Note: packets sent as a result of a callback should not pass by this
    // module again.
    // Called when it's time to send a queued packet.
    // Returns false if packet cannot be sent.
    virtual bool TimeToSendPacket(uint32_t ssrc,
                                  uint16_t sequence_number,
                                  int64_t capture_time_ms,
                                  bool retransmission,
                                  const PacedPacketInfo& cluster_info) = 0;
    // Called when it's a good time to send a padding data.
    // Returns the number of bytes sent.
    virtual size_t TimeToSendPadding(size_t bytes,
                                     const PacedPacketInfo& cluster_info) = 0;

   protected:
    virtual ~PacketSender() {}
  };
  static constexpr int64_t kNoCongestionWindow = -1;

  // Expected max pacer delay in ms. If ExpectedQueueTimeMs() is higher than
  // this value, the packet producers should wait (eg drop frames rather than
  // encoding them). Bitrate sent may temporarily exceed target set by
  // UpdateBitrate() so that this limit will be upheld.
  static const int64_t kMaxQueueLengthMs;
  // Pacing-rate relative to our target send rate.
  // Multiplicative factor that is applied to the target bitrate to calculate
  // the number of bytes that can be transmitted per interval.
  // Increasing this factor will result in lower delays in cases of bitrate
  // overshoots from the encoder.
  static const float kDefaultPaceMultiplier;

  PacedSender(const Clock* clock,
              PacketSender* packet_sender,
              RtcEventLog* event_log);

  PacedSender(const Clock* clock,
              PacketSender* packet_sender,
              RtcEventLog* event_log,
              std::unique_ptr<PacketQueueInterface> packets);

  ~PacedSender() override;

  virtual void CreateProbeCluster(int bitrate_bps);

  // Temporarily pause all sending.
  void Pause();

  // Resume sending packets.
  void Resume();

  void SetCongestionWindow(int64_t congestion_window_bytes);
  void UpdateOutstandingData(int64_t outstanding_bytes);

  // Enable bitrate probing. Enabled by default, mostly here to simplify
  // testing. Must be called before any packets are being sent to have an
  // effect.
  void SetProbingEnabled(bool enabled);

  // Deprecated, SetPacingRates should be used instead.
  void SetEstimatedBitrate(uint32_t bitrate_bps) override;
  // Deprecated, SetPacingRates should be used instead.
  void SetSendBitrateLimits(int min_send_bitrate_bps,
                            int max_padding_bitrate_bps);

  // Sets the pacing rates. Must be called once before packets can be sent.
  void SetPacingRates(uint32_t pacing_rate_bps,
                      uint32_t padding_rate_bps) override;

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  void InsertPacket(RtpPacketSender::Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override;

  // Currently audio traffic is not accounted by pacer and passed through.
  // With the introduction of audio BWE audio traffic will be accounted for
  // the pacer budget calculation. The audio traffic still will be injected
  // at high priority.
  void SetAccountForAudioPackets(bool account_for_audio) override;

  // Returns the time since the oldest queued packet was enqueued.
  virtual int64_t QueueInMs() const;

  virtual size_t QueueSizePackets() const;

  // Returns the time when the first packet was sent, or -1 if no packet is
  // sent.
  virtual int64_t FirstSentPacketTimeMs() const;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  virtual int64_t ExpectedQueueTimeMs() const;

  // Deprecated, alr detection will be moved out of the pacer.
  virtual absl::optional<int64_t> GetApplicationLimitedRegionStartTime() const;

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  int64_t TimeUntilNextProcess() override;

  // Process any pending packets in the queue(s).
  void Process() override;

  // Called when the prober is associated with a process thread.
  void ProcessThreadAttached(ProcessThread* process_thread) override;
  // Deprecated, SetPacingRates should be used instead.
  void SetPacingFactor(float pacing_factor);
  void SetQueueTimeLimit(int limit_ms);

 private:
  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBudgetWithElapsedTime(int64_t delta_time_in_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  void UpdateBudgetWithBytesSent(size_t bytes)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  bool SendPacket(const PacketQueueInterface::Packet& packet,
                  const PacedPacketInfo& cluster_info)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  size_t SendPadding(size_t padding_needed, const PacedPacketInfo& cluster_info)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  void OnBytesSent(size_t bytes_sent) RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  bool Congested() const RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  const Clock* const clock_;
  PacketSender* const packet_sender_;
  const std::unique_ptr<AlrDetector> alr_detector_ RTC_PT_GUARDED_BY(critsect_);

  const bool drain_large_queues_;
  const bool send_padding_if_silent_;
  const bool video_blocks_audio_;
  rtc::CriticalSection critsect_;
  bool paused_ RTC_GUARDED_BY(critsect_);
  // This is the media budget, keeping track of how many bits of media
  // we can pace out during the current interval.
  const std::unique_ptr<IntervalBudget> media_budget_
      RTC_PT_GUARDED_BY(critsect_);
  // This is the padding budget, keeping track of how many bits of padding we're
  // allowed to send out during the current interval. This budget will be
  // utilized when there's no media to send.
  const std::unique_ptr<IntervalBudget> padding_budget_
      RTC_PT_GUARDED_BY(critsect_);

  const std::unique_ptr<BitrateProber> prober_ RTC_PT_GUARDED_BY(critsect_);
  bool probing_send_failure_ RTC_GUARDED_BY(critsect_);
  // Actual configured bitrates (media_budget_ may temporarily be higher in
  // order to meet pace time constraint).
  uint32_t estimated_bitrate_bps_ RTC_GUARDED_BY(critsect_);
  uint32_t min_send_bitrate_kbps_ RTC_GUARDED_BY(critsect_);
  uint32_t max_padding_bitrate_kbps_ RTC_GUARDED_BY(critsect_);
  uint32_t pacing_bitrate_kbps_ RTC_GUARDED_BY(critsect_);

  int64_t time_last_process_us_ RTC_GUARDED_BY(critsect_);
  int64_t last_send_time_us_ RTC_GUARDED_BY(critsect_);
  int64_t first_sent_packet_ms_ RTC_GUARDED_BY(critsect_);

  const std::unique_ptr<PacketQueueInterface> packets_
      RTC_PT_GUARDED_BY(critsect_);
  uint64_t packet_counter_ RTC_GUARDED_BY(critsect_);

  int64_t congestion_window_bytes_ RTC_GUARDED_BY(critsect_) =
      kNoCongestionWindow;
  int64_t outstanding_bytes_ RTC_GUARDED_BY(critsect_) = 0;
  float pacing_factor_ RTC_GUARDED_BY(critsect_);
  // Lock to avoid race when attaching process thread. This can happen due to
  // the Call class setting network state on SendSideCongestionController, which
  // in turn calls Pause/Resume on Pacedsender, before actually starting the
  // pacer process thread. If SendSideCongestionController is running on a task
  // queue separate from the thread used by Call, this causes a race.
  rtc::CriticalSection process_thread_lock_;
  ProcessThread* process_thread_ RTC_GUARDED_BY(process_thread_lock_) = nullptr;

  int64_t queue_time_limit RTC_GUARDED_BY(critsect_);
  bool account_for_audio_ RTC_GUARDED_BY(critsect_);
};
}  // namespace webrtc
#endif  // MODULES_PACING_PACED_SENDER_H_
