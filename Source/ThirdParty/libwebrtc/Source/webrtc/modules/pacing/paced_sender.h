/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_PACED_SENDER_H_
#define WEBRTC_MODULES_PACING_PACED_SENDER_H_

#include <list>
#include <memory>
#include <set>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class AlrDetector;
class BitrateProber;
class Clock;
class ProbeClusterCreatedObserver;
class RtcEventLog;

namespace paced_sender {
class IntervalBudget;
struct Packet;
class PacketQueue;
}  // namespace paced_sender

class PacedSender : public Module, public RtpPacketSender {
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

  ~PacedSender() override;

  virtual void CreateProbeCluster(int bitrate_bps);

  // Temporarily pause all sending.
  void Pause();

  // Resume sending packets.
  void Resume();

  // Enable bitrate probing. Enabled by default, mostly here to simplify
  // testing. Must be called before any packets are being sent to have an
  // effect.
  void SetProbingEnabled(bool enabled);

  // Sets the estimated capacity of the network. Must be called once before
  // packets can be sent.
  // |bitrate_bps| is our estimate of what we are allowed to send on average.
  // We will pace out bursts of packets at a bitrate of
  // |bitrate_bps| * kDefaultPaceMultiplier.
  virtual void SetEstimatedBitrate(uint32_t bitrate_bps);

  // Sets the minimum send bitrate and maximum padding bitrate requested by send
  // streams.
  // |min_send_bitrate_bps| might be higher that the estimated available network
  // bitrate and if so, the pacer will send with |min_send_bitrate_bps|.
  // |max_padding_bitrate_bps| might be higher than the estimate available
  // network bitrate and if so, the pacer will send padding packets to reach
  // the min of the estimated available bitrate and |max_padding_bitrate_bps|.
  void SetSendBitrateLimits(int min_send_bitrate_bps,
                            int max_padding_bitrate_bps);

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  void InsertPacket(RtpPacketSender::Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override;

  // Returns the time since the oldest queued packet was enqueued.
  virtual int64_t QueueInMs() const;

  virtual size_t QueueSizePackets() const;

  // Returns the time when the first packet was sent, or -1 if no packet is
  // sent.
  virtual int64_t FirstSentPacketTimeMs() const;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  virtual int64_t ExpectedQueueTimeMs() const;

  // Returns time in milliseconds when the current application-limited region
  // started or empty result if the sender is currently not application-limited.
  //
  // Application Limited Region (ALR) refers to operating in a state where the
  // traffic on network is limited due to application not having enough
  // traffic to meet the current channel capacity.
  virtual rtc::Optional<int64_t> GetApplicationLimitedRegionStartTime() const;

  // Returns the average time since being enqueued, in milliseconds, for all
  // packets currently in the pacer queue, or 0 if queue is empty.
  virtual int64_t AverageQueueTimeMs();

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  int64_t TimeUntilNextProcess() override;

  // Process any pending packets in the queue(s).
  void Process() override;

  // Called when the prober is associated with a process thread.
  void ProcessThreadAttached(ProcessThread* process_thread) override;

 private:
  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBudgetWithElapsedTime(int64_t delta_time_in_ms)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  void UpdateBudgetWithBytesSent(size_t bytes)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  bool SendPacket(const paced_sender::Packet& packet,
                  const PacedPacketInfo& cluster_info)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  size_t SendPadding(size_t padding_needed, const PacedPacketInfo& cluster_info)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  const Clock* const clock_;
  PacketSender* const packet_sender_;
  std::unique_ptr<AlrDetector> alr_detector_ GUARDED_BY(critsect_);

  rtc::CriticalSection critsect_;
  bool paused_ GUARDED_BY(critsect_);
  // This is the media budget, keeping track of how many bits of media
  // we can pace out during the current interval.
  std::unique_ptr<paced_sender::IntervalBudget> media_budget_
      GUARDED_BY(critsect_);
  // This is the padding budget, keeping track of how many bits of padding we're
  // allowed to send out during the current interval. This budget will be
  // utilized when there's no media to send.
  std::unique_ptr<paced_sender::IntervalBudget> padding_budget_
      GUARDED_BY(critsect_);

  std::unique_ptr<BitrateProber> prober_ GUARDED_BY(critsect_);
  bool probing_send_failure_;
  // Actual configured bitrates (media_budget_ may temporarily be higher in
  // order to meet pace time constraint).
  uint32_t estimated_bitrate_bps_ GUARDED_BY(critsect_);
  uint32_t min_send_bitrate_kbps_ GUARDED_BY(critsect_);
  uint32_t max_padding_bitrate_kbps_ GUARDED_BY(critsect_);
  uint32_t pacing_bitrate_kbps_ GUARDED_BY(critsect_);

  int64_t time_last_update_us_ GUARDED_BY(critsect_);
  int64_t first_sent_packet_ms_ GUARDED_BY(critsect_);

  std::unique_ptr<paced_sender::PacketQueue> packets_ GUARDED_BY(critsect_);
  uint64_t packet_counter_;
  ProcessThread* process_thread_ = nullptr;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACING_PACED_SENDER_H_
