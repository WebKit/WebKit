/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

namespace webrtc {

const size_t kMtu = 1200;
const uint32_t kAcceptedBitrateErrorBps = 50000;

// Number of packets needed before we have a valid estimate.
const int kNumInitialPackets = 2;

namespace testing {

void TestBitrateObserver::OnReceiveBitrateChanged(
    const std::vector<uint32_t>& ssrcs,
    uint32_t bitrate) {
  latest_bitrate_ = bitrate;
  updated_ = true;
}

RtpStream::RtpStream(int fps,
                     int bitrate_bps,
                     uint32_t ssrc,
                     uint32_t frequency,
                     uint32_t timestamp_offset,
                     int64_t rtcp_receive_time)
    : fps_(fps),
      bitrate_bps_(bitrate_bps),
      ssrc_(ssrc),
      frequency_(frequency),
      next_rtp_time_(0),
      next_rtcp_time_(rtcp_receive_time),
      rtp_timestamp_offset_(timestamp_offset),
      kNtpFracPerMs(4.294967296E6) {
  RTC_DCHECK_GT(fps_, 0);
}

void RtpStream::set_rtp_timestamp_offset(uint32_t offset) {
  rtp_timestamp_offset_ = offset;
}

// Generates a new frame for this stream. If called too soon after the
// previous frame, no frame will be generated. The frame is split into
// packets.
int64_t RtpStream::GenerateFrame(int64_t time_now_us, PacketList* packets) {
  if (time_now_us < next_rtp_time_) {
    return next_rtp_time_;
  }
  RTC_DCHECK(packets);
  size_t bits_per_frame = (bitrate_bps_ + fps_ / 2) / fps_;
  size_t n_packets =
      std::max<size_t>((bits_per_frame + 4 * kMtu) / (8 * kMtu), 1u);
  size_t packet_size = (bits_per_frame + 4 * n_packets) / (8 * n_packets);
  for (size_t i = 0; i < n_packets; ++i) {
    RtpPacket* packet = new RtpPacket;
    packet->send_time = time_now_us + kSendSideOffsetUs;
    packet->size = packet_size;
    packet->rtp_timestamp =
        rtp_timestamp_offset_ +
        static_cast<uint32_t>(((frequency_ / 1000) * packet->send_time + 500) /
                              1000);
    packet->ssrc = ssrc_;
    packets->push_back(packet);
  }
  next_rtp_time_ = time_now_us + (1000000 + fps_ / 2) / fps_;
  return next_rtp_time_;
}

// The send-side time when the next frame can be generated.
int64_t RtpStream::next_rtp_time() const {
  return next_rtp_time_;
}

// Generates an RTCP packet.
RtpStream::RtcpPacket* RtpStream::Rtcp(int64_t time_now_us) {
  if (time_now_us < next_rtcp_time_) {
    return NULL;
  }
  RtcpPacket* rtcp = new RtcpPacket;
  int64_t send_time_us = time_now_us + kSendSideOffsetUs;
  rtcp->timestamp =
      rtp_timestamp_offset_ +
      static_cast<uint32_t>(((frequency_ / 1000) * send_time_us + 500) / 1000);
  rtcp->ntp_secs = send_time_us / 1000000;
  rtcp->ntp_frac =
      static_cast<int64_t>((send_time_us % 1000000) * kNtpFracPerMs);
  rtcp->ssrc = ssrc_;
  next_rtcp_time_ = time_now_us + kRtcpIntervalUs;
  return rtcp;
}

void RtpStream::set_bitrate_bps(int bitrate_bps) {
  ASSERT_GE(bitrate_bps, 0);
  bitrate_bps_ = bitrate_bps;
}

int RtpStream::bitrate_bps() const {
  return bitrate_bps_;
}

uint32_t RtpStream::ssrc() const {
  return ssrc_;
}

bool RtpStream::Compare(const std::pair<uint32_t, RtpStream*>& left,
                        const std::pair<uint32_t, RtpStream*>& right) {
  return left.second->next_rtp_time_ < right.second->next_rtp_time_;
}

StreamGenerator::StreamGenerator(int capacity, int64_t time_now)
    : capacity_(capacity), prev_arrival_time_us_(time_now) {}

StreamGenerator::~StreamGenerator() {
  for (StreamMap::iterator it = streams_.begin(); it != streams_.end(); ++it) {
    delete it->second;
  }
  streams_.clear();
}

// Add a new stream.
void StreamGenerator::AddStream(RtpStream* stream) {
  streams_[stream->ssrc()] = stream;
}

// Set the link capacity.
void StreamGenerator::set_capacity_bps(int capacity_bps) {
  ASSERT_GT(capacity_bps, 0);
  capacity_ = capacity_bps;
}

// Divides `bitrate_bps` among all streams. The allocated bitrate per stream
// is decided by the current allocation ratios.
void StreamGenerator::SetBitrateBps(int bitrate_bps) {
  ASSERT_GE(streams_.size(), 0u);
  int total_bitrate_before = 0;
  for (StreamMap::iterator it = streams_.begin(); it != streams_.end(); ++it) {
    total_bitrate_before += it->second->bitrate_bps();
  }
  int64_t bitrate_before = 0;
  int total_bitrate_after = 0;
  for (StreamMap::iterator it = streams_.begin(); it != streams_.end(); ++it) {
    bitrate_before += it->second->bitrate_bps();
    int64_t bitrate_after =
        (bitrate_before * bitrate_bps + total_bitrate_before / 2) /
        total_bitrate_before;
    it->second->set_bitrate_bps(bitrate_after - total_bitrate_after);
    total_bitrate_after += it->second->bitrate_bps();
  }
  ASSERT_EQ(bitrate_before, total_bitrate_before);
  EXPECT_EQ(total_bitrate_after, bitrate_bps);
}

// Set the RTP timestamp offset for the stream identified by `ssrc`.
void StreamGenerator::set_rtp_timestamp_offset(uint32_t ssrc, uint32_t offset) {
  streams_[ssrc]->set_rtp_timestamp_offset(offset);
}

// TODO(holmer): Break out the channel simulation part from this class to make
// it possible to simulate different types of channels.
int64_t StreamGenerator::GenerateFrame(RtpStream::PacketList* packets,
                                       int64_t time_now_us) {
  RTC_DCHECK(packets);
  RTC_DCHECK(packets->empty());
  RTC_DCHECK_GT(capacity_, 0);
  StreamMap::iterator it =
      std::min_element(streams_.begin(), streams_.end(), RtpStream::Compare);
  (*it).second->GenerateFrame(time_now_us, packets);
  for (RtpStream::PacketList::iterator packet_it = packets->begin();
       packet_it != packets->end(); ++packet_it) {
    int capacity_bpus = capacity_ / 1000;
    int64_t required_network_time_us =
        (8 * 1000 * (*packet_it)->size + capacity_bpus / 2) / capacity_bpus;
    prev_arrival_time_us_ =
        std::max(time_now_us + required_network_time_us,
                 prev_arrival_time_us_ + required_network_time_us);
    (*packet_it)->arrival_time = prev_arrival_time_us_;
  }
  it = std::min_element(streams_.begin(), streams_.end(), RtpStream::Compare);
  return std::max((*it).second->next_rtp_time(), time_now_us);
}
}  // namespace testing

RemoteBitrateEstimatorTest::RemoteBitrateEstimatorTest()
    : clock_(100000000),
      bitrate_observer_(new testing::TestBitrateObserver),
      stream_generator_(
          new testing::StreamGenerator(1e6,  // Capacity.
                                       clock_.TimeInMicroseconds())),
      arrival_time_offset_ms_(0) {}

RemoteBitrateEstimatorTest::~RemoteBitrateEstimatorTest() {}

void RemoteBitrateEstimatorTest::AddDefaultStream() {
  stream_generator_->AddStream(
      new testing::RtpStream(30,          // Frames per second.
                             3e5,         // Bitrate.
                             1,           // SSRC.
                             90000,       // RTP frequency.
                             0xFFFFF000,  // Timestamp offset.
                             0));         // RTCP receive time.
}

uint32_t RemoteBitrateEstimatorTest::AbsSendTime(int64_t t, int64_t denom) {
  return (((t << 18) + (denom >> 1)) / denom) & 0x00fffffful;
}

uint32_t RemoteBitrateEstimatorTest::AddAbsSendTime(uint32_t t1, uint32_t t2) {
  return (t1 + t2) & 0x00fffffful;
}

const uint32_t RemoteBitrateEstimatorTest::kDefaultSsrc = 1;

void RemoteBitrateEstimatorTest::IncomingPacket(uint32_t ssrc,
                                                size_t payload_size,
                                                int64_t arrival_time,
                                                uint32_t rtp_timestamp,
                                                uint32_t absolute_send_time) {
  RtpHeaderExtensionMap extensions;
  extensions.Register<AbsoluteSendTime>(1);
  RtpPacketReceived rtp_packet(&extensions);
  rtp_packet.SetSsrc(ssrc);
  rtp_packet.SetTimestamp(rtp_timestamp);
  rtp_packet.SetExtension<AbsoluteSendTime>(absolute_send_time);
  rtp_packet.SetPayloadSize(payload_size);
  rtp_packet.set_arrival_time(
      Timestamp::Millis(arrival_time + arrival_time_offset_ms_));

  bitrate_estimator_->IncomingPacket(rtp_packet);
}

// Generates a frame of packets belonging to a stream at a given bitrate and
// with a given ssrc. The stream is pushed through a very simple simulated
// network, and is then given to the receive-side bandwidth estimator.
// Returns true if an over-use was seen, false otherwise.
// The StreamGenerator::updated() should be used to check for any changes in
// target bitrate after the call to this function.
bool RemoteBitrateEstimatorTest::GenerateAndProcessFrame(uint32_t ssrc,
                                                         uint32_t bitrate_bps) {
  RTC_DCHECK_GT(bitrate_bps, 0);
  stream_generator_->SetBitrateBps(bitrate_bps);
  testing::RtpStream::PacketList packets;
  int64_t next_time_us =
      stream_generator_->GenerateFrame(&packets, clock_.TimeInMicroseconds());
  bool overuse = false;
  while (!packets.empty()) {
    testing::RtpStream::RtpPacket* packet = packets.front();
    bitrate_observer_->Reset();
    // The simulated clock should match the time of packet->arrival_time
    // since both are used in IncomingPacket().
    clock_.AdvanceTimeMicroseconds(packet->arrival_time -
                                   clock_.TimeInMicroseconds());
    IncomingPacket(packet->ssrc, packet->size,
                   (packet->arrival_time + 500) / 1000, packet->rtp_timestamp,
                   AbsSendTime(packet->send_time, 1000000));
    if (bitrate_observer_->updated()) {
      if (bitrate_observer_->latest_bitrate() < bitrate_bps)
        overuse = true;
    }
    delete packet;
    packets.pop_front();
  }
  bitrate_estimator_->Process();
  clock_.AdvanceTimeMicroseconds(next_time_us - clock_.TimeInMicroseconds());
  return overuse;
}

// Run the bandwidth estimator with a stream of `number_of_frames` frames, or
// until it reaches `target_bitrate`.
// Can for instance be used to run the estimator for some time to get it
// into a steady state.
uint32_t RemoteBitrateEstimatorTest::SteadyStateRun(uint32_t ssrc,
                                                    int max_number_of_frames,
                                                    uint32_t start_bitrate,
                                                    uint32_t min_bitrate,
                                                    uint32_t max_bitrate,
                                                    uint32_t target_bitrate) {
  uint32_t bitrate_bps = start_bitrate;
  bool bitrate_update_seen = false;
  // Produce `number_of_frames` frames and give them to the estimator.
  for (int i = 0; i < max_number_of_frames; ++i) {
    bool overuse = GenerateAndProcessFrame(ssrc, bitrate_bps);
    if (overuse) {
      EXPECT_LT(bitrate_observer_->latest_bitrate(), max_bitrate);
      EXPECT_GT(bitrate_observer_->latest_bitrate(), min_bitrate);
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_update_seen = true;
    } else if (bitrate_observer_->updated()) {
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
    if (bitrate_update_seen && bitrate_bps > target_bitrate) {
      break;
    }
  }
  EXPECT_TRUE(bitrate_update_seen);
  return bitrate_bps;
}

void RemoteBitrateEstimatorTest::InitialBehaviorTestHelper(
    uint32_t expected_converge_bitrate) {
  const int kFramerate = 50;  // 50 fps to avoid rounding errors.
  const int kFrameIntervalMs = 1000 / kFramerate;
  const uint32_t kFrameIntervalAbsSendTime = AbsSendTime(1, kFramerate);
  uint32_t timestamp = 0;
  uint32_t absolute_send_time = 0;
  EXPECT_EQ(bitrate_estimator_->LatestEstimate(), DataRate::Zero());
  clock_.AdvanceTimeMilliseconds(1000);
  bitrate_estimator_->Process();
  EXPECT_EQ(bitrate_estimator_->LatestEstimate(), DataRate::Zero());
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  clock_.AdvanceTimeMilliseconds(1000);
  // Inserting packets for 5 seconds to get a valid estimate.
  for (int i = 0; i < 5 * kFramerate + 1 + kNumInitialPackets; ++i) {
    if (i == kNumInitialPackets) {
      bitrate_estimator_->Process();
      EXPECT_EQ(bitrate_estimator_->LatestEstimate(), DataRate::Zero());
      EXPECT_FALSE(bitrate_observer_->updated());
      bitrate_observer_->Reset();
    }

    IncomingPacket(kDefaultSsrc, kMtu, clock_.TimeInMilliseconds(), timestamp,
                   absolute_send_time);
    clock_.AdvanceTimeMilliseconds(1000 / kFramerate);
    timestamp += 90 * kFrameIntervalMs;
    absolute_send_time =
        AddAbsSendTime(absolute_send_time, kFrameIntervalAbsSendTime);
  }
  bitrate_estimator_->Process();
  uint32_t bitrate_bps = bitrate_estimator_->LatestEstimate().bps<uint32_t>();
  EXPECT_NEAR(expected_converge_bitrate, bitrate_bps, kAcceptedBitrateErrorBps);
  EXPECT_TRUE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  EXPECT_EQ(bitrate_observer_->latest_bitrate(), bitrate_bps);
  bitrate_estimator_->RemoveStream(kDefaultSsrc);
  EXPECT_EQ(bitrate_estimator_->LatestEstimate(), DataRate::Zero());
}

void RemoteBitrateEstimatorTest::RateIncreaseReorderingTestHelper(
    uint32_t expected_bitrate_bps) {
  const int kFramerate = 50;  // 50 fps to avoid rounding errors.
  const int kFrameIntervalMs = 1000 / kFramerate;
  const uint32_t kFrameIntervalAbsSendTime = AbsSendTime(1, kFramerate);
  uint32_t timestamp = 0;
  uint32_t absolute_send_time = 0;
  // Inserting packets for five seconds to get a valid estimate.
  for (int i = 0; i < 5 * kFramerate + 1 + kNumInitialPackets; ++i) {
    // TODO(sprang): Remove this hack once the single stream estimator is gone,
    // as it doesn't do anything in Process().
    if (i == kNumInitialPackets) {
      // Process after we have enough frames to get a valid input rate estimate.
      bitrate_estimator_->Process();
      EXPECT_FALSE(bitrate_observer_->updated());  // No valid estimate.
    }

    IncomingPacket(kDefaultSsrc, kMtu, clock_.TimeInMilliseconds(), timestamp,
                   absolute_send_time);
    clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
    timestamp += 90 * kFrameIntervalMs;
    absolute_send_time =
        AddAbsSendTime(absolute_send_time, kFrameIntervalAbsSendTime);
  }
  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_NEAR(expected_bitrate_bps, bitrate_observer_->latest_bitrate(),
              kAcceptedBitrateErrorBps);
  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds(2 * kFrameIntervalMs);
    timestamp += 2 * 90 * kFrameIntervalMs;
    absolute_send_time =
        AddAbsSendTime(absolute_send_time, 2 * kFrameIntervalAbsSendTime);
    IncomingPacket(kDefaultSsrc, 1000, clock_.TimeInMilliseconds(), timestamp,
                   absolute_send_time);
    IncomingPacket(
        kDefaultSsrc, 1000, clock_.TimeInMilliseconds(),
        timestamp - 90 * kFrameIntervalMs,
        AddAbsSendTime(absolute_send_time,
                       -static_cast<int>(kFrameIntervalAbsSendTime)));
  }
  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_NEAR(expected_bitrate_bps, bitrate_observer_->latest_bitrate(),
              kAcceptedBitrateErrorBps);
}

// Make sure we initially increase the bitrate as expected.
void RemoteBitrateEstimatorTest::RateIncreaseRtpTimestampsTestHelper(
    int expected_iterations) {
  // This threshold corresponds approximately to increasing linearly with
  // bitrate(i) = 1.04 * bitrate(i-1) + 1000
  // until bitrate(i) > 500000, with bitrate(1) ~= 30000.
  uint32_t bitrate_bps = 30000;
  int iterations = 0;
  AddDefaultStream();
  // Feed the estimator with a stream of packets and verify that it reaches
  // 500 kbps at the expected time.
  while (bitrate_bps < 5e5) {
    bool overuse = GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    if (overuse) {
      EXPECT_GT(bitrate_observer_->latest_bitrate(), bitrate_bps);
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    } else if (bitrate_observer_->updated()) {
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
    ++iterations;
    ASSERT_LE(iterations, expected_iterations);
  }
  ASSERT_EQ(expected_iterations, iterations);
}

void RemoteBitrateEstimatorTest::CapacityDropTestHelper(
    int number_of_streams,
    bool wrap_time_stamp,
    uint32_t expected_bitrate_drop_delta,
    int64_t receiver_clock_offset_change_ms) {
  const int kFramerate = 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const uint32_t kInitialCapacityBps = 1000e3;
  const uint32_t kReducedCapacityBps = 500e3;

  int steady_state_time = 0;
  if (number_of_streams <= 1) {
    steady_state_time = 10;
    AddDefaultStream();
  } else {
    steady_state_time = 10 * number_of_streams;
    int bitrate_sum = 0;
    int kBitrateDenom = number_of_streams * (number_of_streams - 1);
    for (int i = 0; i < number_of_streams; i++) {
      // First stream gets half available bitrate, while the rest share the
      // remaining half i.e.: 1/2 = Sum[n/(N*(N-1))] for n=1..N-1 (rounded up)
      int bitrate = kStartBitrate / 2;
      if (i > 0) {
        bitrate = (kStartBitrate * i + kBitrateDenom / 2) / kBitrateDenom;
      }
      uint32_t mask = ~0ull << (32 - i);
      stream_generator_->AddStream(
          new testing::RtpStream(kFramerate,          // Frames per second.
                                 bitrate,             // Bitrate.
                                 kDefaultSsrc + i,    // SSRC.
                                 90000,               // RTP frequency.
                                 0xFFFFF000u ^ mask,  // Timestamp offset.
                                 0));                 // RTCP receive time.
      bitrate_sum += bitrate;
    }
    ASSERT_EQ(bitrate_sum, kStartBitrate);
  }
  if (wrap_time_stamp) {
    stream_generator_->set_rtp_timestamp_offset(
        kDefaultSsrc,
        std::numeric_limits<uint32_t>::max() - steady_state_time * 90000);
  }

  // Run in steady state to make the estimator converge.
  stream_generator_->set_capacity_bps(kInitialCapacityBps);
  uint32_t bitrate_bps = SteadyStateRun(
      kDefaultSsrc, steady_state_time * kFramerate, kStartBitrate,
      kMinExpectedBitrate, kMaxExpectedBitrate, kInitialCapacityBps);
  EXPECT_GE(bitrate_bps, 0.85 * kInitialCapacityBps);
  EXPECT_LE(bitrate_bps, 1.05 * kInitialCapacityBps);
  bitrate_observer_->Reset();

  // Add an offset to make sure the BWE can handle it.
  arrival_time_offset_ms_ += receiver_clock_offset_change_ms;

  // Reduce the capacity and verify the decrease time.
  stream_generator_->set_capacity_bps(kReducedCapacityBps);
  int64_t overuse_start_time = clock_.TimeInMilliseconds();
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 100 * number_of_streams; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    if (bitrate_drop_time == -1 &&
        bitrate_observer_->latest_bitrate() <= kReducedCapacityBps) {
      bitrate_drop_time = clock_.TimeInMilliseconds();
    }
    if (bitrate_observer_->updated())
      bitrate_bps = bitrate_observer_->latest_bitrate();
  }

  EXPECT_NEAR(expected_bitrate_drop_delta,
              bitrate_drop_time - overuse_start_time, 33);

  // Remove stream one by one.
  for (int i = 0; i < number_of_streams; i++) {
    EXPECT_EQ(bitrate_estimator_->LatestEstimate().bps(), bitrate_bps);
    bitrate_estimator_->RemoveStream(kDefaultSsrc + i);
  }
  EXPECT_EQ(bitrate_estimator_->LatestEstimate(), DataRate::Zero());
}

void RemoteBitrateEstimatorTest::TestTimestampGroupingTestHelper() {
  const int kFramerate = 50;  // 50 fps to avoid rounding errors.
  const int kFrameIntervalMs = 1000 / kFramerate;
  const uint32_t kFrameIntervalAbsSendTime = AbsSendTime(1, kFramerate);
  uint32_t timestamp = 0;
  // Initialize absolute_send_time (24 bits) so that it will definitely wrap
  // during the test.
  uint32_t absolute_send_time = AddAbsSendTime(
      (1 << 24), -static_cast<int>(50 * kFrameIntervalAbsSendTime));
  // Initial set of frames to increase the bitrate. 6 seconds to have enough
  // time for the first estimate to be generated and for Process() to be called.
  for (int i = 0; i <= 6 * kFramerate; ++i) {
    IncomingPacket(kDefaultSsrc, 1000, clock_.TimeInMilliseconds(), timestamp,
                   absolute_send_time);
    bitrate_estimator_->Process();
    clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
    timestamp += 90 * kFrameIntervalMs;
    absolute_send_time =
        AddAbsSendTime(absolute_send_time, kFrameIntervalAbsSendTime);
  }
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_GE(bitrate_observer_->latest_bitrate(), 400000u);

  // Insert batches of frames which were sent very close in time. Also simulate
  // capacity over-use to see that we back off correctly.
  const int kTimestampGroupLength = 15;
  const uint32_t kTimestampGroupLengthAbsSendTime =
      AbsSendTime(kTimestampGroupLength, 90000);
  const uint32_t kSingleRtpTickAbsSendTime = AbsSendTime(1, 90000);
  for (int i = 0; i < 100; ++i) {
    for (int j = 0; j < kTimestampGroupLength; ++j) {
      // Insert `kTimestampGroupLength` frames with just 1 timestamp ticks in
      // between. Should be treated as part of the same group by the estimator.
      IncomingPacket(kDefaultSsrc, 100, clock_.TimeInMilliseconds(), timestamp,
                     absolute_send_time);
      clock_.AdvanceTimeMilliseconds(kFrameIntervalMs / kTimestampGroupLength);
      timestamp += 1;
      absolute_send_time =
          AddAbsSendTime(absolute_send_time, kSingleRtpTickAbsSendTime);
    }
    // Increase time until next batch to simulate over-use.
    clock_.AdvanceTimeMilliseconds(10);
    timestamp += 90 * kFrameIntervalMs - kTimestampGroupLength;
    absolute_send_time = AddAbsSendTime(
        absolute_send_time,
        AddAbsSendTime(kFrameIntervalAbsSendTime,
                       -static_cast<int>(kTimestampGroupLengthAbsSendTime)));
    bitrate_estimator_->Process();
  }
  EXPECT_TRUE(bitrate_observer_->updated());
  // Should have reduced the estimate.
  EXPECT_LT(bitrate_observer_->latest_bitrate(), 400000u);
}

void RemoteBitrateEstimatorTest::TestWrappingHelper(int silence_time_s) {
  const int kFramerate = 100;
  const int kFrameIntervalMs = 1000 / kFramerate;
  const uint32_t kFrameIntervalAbsSendTime = AbsSendTime(1, kFramerate);
  uint32_t absolute_send_time = 0;
  uint32_t timestamp = 0;

  for (size_t i = 0; i < 3000; ++i) {
    IncomingPacket(kDefaultSsrc, 1000, clock_.TimeInMilliseconds(), timestamp,
                   absolute_send_time);
    timestamp += kFrameIntervalMs;
    clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
    absolute_send_time =
        AddAbsSendTime(absolute_send_time, kFrameIntervalAbsSendTime);
    bitrate_estimator_->Process();
  }
  DataRate bitrate_before = bitrate_estimator_->LatestEstimate();

  clock_.AdvanceTimeMilliseconds(silence_time_s * 1000);
  absolute_send_time =
      AddAbsSendTime(absolute_send_time, AbsSendTime(silence_time_s, 1));
  bitrate_estimator_->Process();
  for (size_t i = 0; i < 21; ++i) {
    IncomingPacket(kDefaultSsrc, 1000, clock_.TimeInMilliseconds(), timestamp,
                   absolute_send_time);
    timestamp += kFrameIntervalMs;
    clock_.AdvanceTimeMilliseconds(2 * kFrameIntervalMs);
    absolute_send_time =
        AddAbsSendTime(absolute_send_time, kFrameIntervalAbsSendTime);
    bitrate_estimator_->Process();
  }
  DataRate bitrate_after = bitrate_estimator_->LatestEstimate();
  EXPECT_LT(bitrate_after, bitrate_before);
}
}  // namespace webrtc
