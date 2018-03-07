/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_

#include <list>
#include <map>
#include <sstream>
#include <string>

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "modules/remote_bitrate_estimator/test/packet.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {
namespace testing {
namespace bwe {

// Overload map comparator.
class SequenceNumberOlderThan {
 public:
  bool operator()(uint16_t seq_num_1, uint16_t seq_num_2) const {
    return IsNewerSequenceNumber(seq_num_2, seq_num_1);
  }
};

// Holds information for computing global packet loss.
struct LossAccount {
  LossAccount() : num_total(0), num_lost(0) {}
  LossAccount(size_t num_total, size_t num_lost)
      : num_total(num_total), num_lost(num_lost) {}
  void Add(LossAccount rhs);
  void Subtract(LossAccount rhs);
  float LossRatio();
  size_t num_total;
  size_t num_lost;
};

// Holds only essential information about packets to be saved for
// further use, e.g. for calculating packet loss and receiving rate.
struct PacketIdentifierNode {
  PacketIdentifierNode(uint16_t sequence_number,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t payload_size)
      : sequence_number(sequence_number),
        send_time_ms(send_time_ms),
        arrival_time_ms(arrival_time_ms),
        payload_size(payload_size) {}

  uint16_t sequence_number;
  int64_t send_time_ms;
  int64_t arrival_time_ms;
  size_t payload_size;
};

typedef std::list<PacketIdentifierNode*>::iterator PacketNodeIt;

// FIFO implementation for a limited capacity set.
// Used for keeping the latest arrived packets while avoiding duplicates.
// Allows efficient insertion, deletion and search.
class LinkedSet {
 public:
  explicit LinkedSet(int capacity) : capacity_(capacity) {}
  ~LinkedSet();

  // If the arriving packet (identified by its sequence number) is already
  // in the LinkedSet, move its Node to the head of the list. Else, create
  // a PacketIdentifierNode n_ and then UpdateHead(n_), calling RemoveTail()
  // if the LinkedSet reached its maximum capacity.
  void Insert(uint16_t sequence_number,
              int64_t send_time_ms,
              int64_t arrival_time_ms,
              size_t payload_size);

  void Insert(PacketIdentifierNode packet_identifier);

  PacketNodeIt begin() { return list_.begin(); }
  PacketNodeIt end() { return list_.end(); }

  bool empty() const { return list_.empty(); }
  size_t size() const { return list_.size(); }
  size_t capacity() const { return capacity_; }

  uint16_t OldestSeqNumber() const { return empty() ? 0 : map_.begin()->first; }
  uint16_t NewestSeqNumber() const {
    return empty() ? 0 : map_.rbegin()->first;
  }

  void Erase(PacketNodeIt node_it);

 private:
  // Pop oldest element from the back of the list and remove it from the map.
  void RemoveTail();
  // Add new element to the front of the list and insert it in the map.
  void UpdateHead(PacketIdentifierNode* new_head);
  size_t capacity_;
  std::map<uint16_t, PacketNodeIt, SequenceNumberOlderThan> map_;
  std::list<PacketIdentifierNode*> list_;
};

const int kMinBitrateKbps = 10;
const int kMaxBitrateKbps = 25000;

class BweSender : public Module {
 public:
  BweSender() {}
  explicit BweSender(int bitrate_kbps) : bitrate_kbps_(bitrate_kbps) {}
  virtual ~BweSender() {}

  virtual int GetFeedbackIntervalMs() const = 0;
  virtual void GiveFeedback(const FeedbackPacket& feedback) = 0;
  virtual void OnPacketsSent(const Packets& packets) = 0;

 protected:
  int bitrate_kbps_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(BweSender);
};

class BweReceiver {
 public:
  explicit BweReceiver(int flow_id);
  BweReceiver(int flow_id, int64_t window_size_ms);

  virtual ~BweReceiver() {}

  virtual void ReceivePacket(int64_t arrival_time_ms,
                             const MediaPacket& media_packet);
  virtual FeedbackPacket* GetFeedback(int64_t now_ms) { return NULL; }

  size_t GetSetCapacity() { return received_packets_.capacity(); }
  double BitrateWindowS() const { return rate_counter_.BitrateWindowS(); }
  uint32_t RecentKbps() const;  // Receiving Rate.

  // Computes packet loss during an entire simulation, up to 4 billion packets.
  float GlobalReceiverPacketLossRatio();  // Plot histogram.
  float RecentPacketLossRatio();          // Plot dynamics.

  static const int64_t kPacketLossTimeWindowMs = 500;
  static const int64_t kReceivingRateTimeWindowMs = 1000;

 protected:
  int flow_id_;
  // Deals with packets sent more than once.
  LinkedSet received_packets_;
  // Used for calculating recent receiving rate.
  RateCounter rate_counter_;

 private:
  FRIEND_TEST_ALL_PREFIXES(BweReceiverTest, RecentKbps);
  FRIEND_TEST_ALL_PREFIXES(BweReceiverTest, Loss);

  void UpdateLoss();
  void RelieveSetAndUpdateLoss();
  // Packet loss for packets stored in the LinkedSet, up to 1000 packets.
  // Used to update global loss account whenever the set is filled and cleared.
  LossAccount LinkedSetPacketLossRatio();

  // Used for calculating global packet loss ratio.
  LossAccount loss_account_;
};

enum BandwidthEstimatorType {
  kNullEstimator,
  kNadaEstimator,
  kRembEstimator,
  kSendSideEstimator,
  kTcpEstimator,
  kBbrEstimator
};

const char* const bwe_names[] = {"Null", "NADA", "REMB", "GCC", "TCP", "BBR"};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time);

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock);

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot);
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_
