/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_H_

#include <map>
#include <string>
#include <vector>

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/test/bwe.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "rtc_base/constructormagic.h"
#include "test/gtest.h"

namespace webrtc {

namespace testing {
namespace bwe {

class BweReceiver;
class PacketReceiver;
class PacketSender;

class PacketProcessorRunner {
 public:
  explicit PacketProcessorRunner(PacketProcessor* processor);
  PacketProcessorRunner(const PacketProcessorRunner&);
  ~PacketProcessorRunner();

  bool RunsProcessor(const PacketProcessor* processor) const;
  void RunFor(int64_t time_ms, int64_t time_now_ms, Packets* in_out);

 private:
  void FindPacketsToProcess(const FlowIds& flow_ids, Packets* in, Packets* out);
  void QueuePackets(Packets* batch, int64_t end_of_batch_time_us);

  PacketProcessor* processor_;
  Packets queue_;
};

class Link : public PacketProcessorListener {
 public:
  Link();
  ~Link() override;

  void AddPacketProcessor(PacketProcessor* processor,
                          ProcessorType type) override;
  void RemovePacketProcessor(PacketProcessor* processor) override;

  void Run(int64_t run_for_ms, int64_t now_ms, Packets* packets);

  const std::vector<PacketSender*>& senders() { return senders_; }
  const std::vector<PacketProcessorRunner>& processors() { return processors_; }

 private:
  std::vector<PacketSender*> senders_;
  std::vector<PacketReceiver*> receivers_;
  std::vector<PacketProcessorRunner> processors_;
};

class BweTest {
 public:
  BweTest();
  explicit BweTest(bool plot_capacity);
  ~BweTest();

  void RunChoke(BandwidthEstimatorType bwe_type,
                std::vector<int> capacities_kbps);

  void RunVariableCapacity1SingleFlow(BandwidthEstimatorType bwe_type);
  void RunVariableCapacity2MultipleFlows(BandwidthEstimatorType bwe_type,
                                         size_t num_flows);
  void RunBidirectionalFlow(BandwidthEstimatorType bwe_type);
  void RunSelfFairness(BandwidthEstimatorType bwe_type);
  void RunRoundTripTimeFairness(BandwidthEstimatorType bwe_type);
  void RunLongTcpFairness(BandwidthEstimatorType bwe_type);
  void RunMultipleShortTcpFairness(BandwidthEstimatorType bwe_type,
                                   std::vector<int> tcp_file_sizes_bytes,
                                   std::vector<int64_t> tcp_starting_times_ms);
  void RunPauseResumeFlows(BandwidthEstimatorType bwe_type);

  void RunFairnessTest(BandwidthEstimatorType bwe_type,
                       size_t num_media_flows,
                       size_t num_tcp_flows,
                       int64_t run_time_seconds,
                       uint32_t capacity_kbps,
                       int64_t max_delay_ms,
                       int64_t rtt_ms,
                       int64_t max_jitter_ms,
                       const int64_t* offsets_ms);

  void RunFairnessTest(BandwidthEstimatorType bwe_type,
                       size_t num_media_flows,
                       size_t num_tcp_flows,
                       int64_t run_time_seconds,
                       uint32_t capacity_kbps,
                       int64_t max_delay_ms,
                       int64_t rtt_ms,
                       int64_t max_jitter_ms,
                       const int64_t* offsets_ms,
                       const std::string& title,
                       const std::string& flow_name);

  static std::vector<int> GetFileSizesBytes(int num_files);
  static std::vector<int64_t> GetStartingTimesMs(int num_files);

 protected:
  void SetUp();

  void VerboseLogging(bool enable);
  void RunFor(int64_t time_ms);
  std::string GetTestName() const;

  void PrintResults(double max_throughput_kbps,
                    Stats<double> throughput_kbps,
                    int flow_id,
                    Stats<double> flow_delay_ms,
                    Stats<double> flow_throughput_kbps);

  void PrintResults(double max_throughput_kbps,
                    Stats<double> throughput_kbps,
                    std::map<int, Stats<double>> flow_delay_ms,
                    std::map<int, Stats<double>> flow_throughput_kbps);

  Link downlink_;
  Link uplink_;

 private:
  void FindPacketsToProcess(const FlowIds& flow_ids, Packets* in, Packets* out);
  void GiveFeedbackToAffectedSenders(PacketReceiver* receiver);

  int64_t run_time_ms_;
  int64_t time_now_ms_;
  int64_t simulation_interval_ms_;
  std::vector<Link*> links_;
  Packets packets_;
  bool plot_total_available_capacity_;

  RTC_DISALLOW_COPY_AND_ASSIGN(BweTest);
};

// Default Evaluation parameters:
// Link capacity: 4000ms;
// Queueing delay capacity: 300ms.
// One-Way propagation delay: 50ms.
// Jitter model: Truncated gaussian.
// Maximum end-to-end jitter: 30ms = 2*standard_deviation.
// Bottleneck queue type: Drop tail.
// Path loss ratio: 0%.

const int kOneWayDelayMs = 50;
const int kMaxQueueingDelayMs = 300;
const int kMaxCapacityKbps = 4000;
const int kMaxJitterMs = 15;

struct DefaultEvaluationFilter {
  DefaultEvaluationFilter(PacketProcessorListener* listener, int flow_id)
      : choke(listener, flow_id),
        delay(listener, flow_id),
        jitter(listener, flow_id) {
    SetDefaultParameters();
  }

  DefaultEvaluationFilter(PacketProcessorListener* listener,
                          const FlowIds& flow_ids)
      : choke(listener, flow_ids),
        delay(listener, flow_ids),
        jitter(listener, flow_ids) {
    SetDefaultParameters();
  }

  void SetDefaultParameters() {
    delay.SetOneWayDelayMs(kOneWayDelayMs);
    choke.set_max_delay_ms(kMaxQueueingDelayMs);
    choke.set_capacity_kbps(kMaxCapacityKbps);
    jitter.SetMaxJitter(kMaxJitterMs);
  }

  ChokeFilter choke;
  DelayFilter delay;
  JitterFilter jitter;
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_H_
