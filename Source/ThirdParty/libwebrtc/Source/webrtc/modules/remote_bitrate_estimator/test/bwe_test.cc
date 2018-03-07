/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/bwe_test.h"

#include <memory>
#include <sstream>

#include "modules/include/module_common_types.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "modules/remote_bitrate_estimator/test/metric_recorder.h"
#include "modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "modules/remote_bitrate_estimator/test/packet_sender.h"
#include "rtc_base/arraysize.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "test/testsupport/perf_test.h"

using std::vector;

namespace {
const int kQuickTestTimeoutMs = 500;
}

namespace webrtc {
namespace testing {
namespace bwe {

PacketProcessorRunner::PacketProcessorRunner(PacketProcessor* processor)
    : processor_(processor) {
}

PacketProcessorRunner::~PacketProcessorRunner() {
  for (Packet* packet : queue_)
    delete packet;
}

bool PacketProcessorRunner::RunsProcessor(
    const PacketProcessor* processor) const {
  return processor == processor_;
}

void PacketProcessorRunner::RunFor(int64_t time_ms,
                                   int64_t time_now_ms,
                                   Packets* in_out) {
  Packets to_process;
  FindPacketsToProcess(processor_->flow_ids(), in_out, &to_process);
  processor_->RunFor(time_ms, &to_process);
  QueuePackets(&to_process, time_now_ms * 1000);
  if (!to_process.empty()) {
    processor_->Plot(to_process.back()->send_time_ms());
  }
  in_out->merge(to_process, DereferencingComparator<Packet>);
}

void PacketProcessorRunner::FindPacketsToProcess(const FlowIds& flow_ids,
                                                 Packets* in,
                                                 Packets* out) {
  assert(out->empty());
  for (Packets::iterator it = in->begin(); it != in->end();) {
    // TODO(holmer): Further optimize this by looking for consecutive flow ids
    // in the packet list and only doing the binary search + splice once for a
    // sequence.
    if (flow_ids.find((*it)->flow_id()) != flow_ids.end()) {
      Packets::iterator next = it;
      ++next;
      out->splice(out->end(), *in, it);
      it = next;
    } else {
      ++it;
    }
  }
}

void PacketProcessorRunner::QueuePackets(Packets* batch,
                                         int64_t end_of_batch_time_us) {
  queue_.merge(*batch, DereferencingComparator<Packet>);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if ((*it)->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  batch->merge(to_transfer, DereferencingComparator<Packet>);
}

// Plot link capacity by default.
BweTest::BweTest() : BweTest(true) {
}

BweTest::BweTest(bool plot_capacity)
    : run_time_ms_(0),
      time_now_ms_(-1),
      simulation_interval_ms_(-1),
      plot_total_available_capacity_(plot_capacity) {
  links_.push_back(&uplink_);
  links_.push_back(&downlink_);
}

BweTest::~BweTest() {
  for (Packet* packet : packets_)
    delete packet;
}

void BweTest::SetUp() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  std::string test_name =
      std::string(test_info->test_case_name()) + "_" +
      std::string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);
  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void Link::AddPacketProcessor(PacketProcessor* processor,
                              ProcessorType processor_type) {
  assert(processor);
  switch (processor_type) {
    case kSender:
      senders_.push_back(static_cast<PacketSender*>(processor));
      break;
    case kReceiver:
      receivers_.push_back(static_cast<PacketReceiver*>(processor));
      break;
    case kRegular:
      break;
  }
  processors_.push_back(PacketProcessorRunner(processor));
}

void Link::RemovePacketProcessor(PacketProcessor* processor) {
  for (std::vector<PacketProcessorRunner>::iterator it = processors_.begin();
       it != processors_.end(); ++it) {
    if (it->RunsProcessor(processor)) {
      processors_.erase(it);
      return;
    }
  }
  assert(false);
}

// Ownership of the created packets is handed over to the caller.
void Link::Run(int64_t run_for_ms, int64_t now_ms, Packets* packets) {
  for (auto& processor : processors_) {
    processor.RunFor(run_for_ms, now_ms, packets);
  }
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::RunFor(int64_t time_ms) {
  // Set simulation interval from first packet sender.
  // TODO(holmer): Support different feedback intervals for different flows.

  // For quick perf tests ignore passed timeout
  if (field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
    time_ms = kQuickTestTimeoutMs;
  }
  if (!uplink_.senders().empty()) {
    simulation_interval_ms_ = uplink_.senders()[0]->GetFeedbackIntervalMs();
  } else if (!downlink_.senders().empty()) {
    simulation_interval_ms_ = downlink_.senders()[0]->GetFeedbackIntervalMs();
  }
  assert(simulation_interval_ms_ > 0);
  if (time_now_ms_ == -1) {
    time_now_ms_ = simulation_interval_ms_;
  }
  for (run_time_ms_ += time_ms;
       time_now_ms_ <= run_time_ms_ - simulation_interval_ms_;
       time_now_ms_ += simulation_interval_ms_) {
    // Packets are first generated on the first link, passed through all the
    // PacketProcessors and PacketReceivers. The PacketReceivers produces
    // FeedbackPackets which are then processed by the next link, where they
    // at some point will be consumed by a PacketSender.
    for (Link* link : links_)
      link->Run(simulation_interval_ms_, time_now_ms_, &packets_);
  }
}

std::string BweTest::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return std::string(test_info->name());
}

void BweTest::PrintResults(double max_throughput_kbps,
                           Stats<double> throughput_kbps,
                           int flow_id,
                           Stats<double> flow_delay_ms,
                           Stats<double> flow_throughput_kbps) {
  std::map<int, Stats<double>> flow_delays_ms;
  flow_delays_ms[flow_id] = flow_delay_ms;
  std::map<int, Stats<double>> flow_throughputs_kbps;
  flow_throughputs_kbps[flow_id] = flow_throughput_kbps;
  PrintResults(max_throughput_kbps, throughput_kbps, flow_delays_ms,
               flow_throughputs_kbps);
}

void BweTest::PrintResults(double max_throughput_kbps,
                           Stats<double> throughput_kbps,
                           std::map<int, Stats<double>> flow_delay_ms,
                           std::map<int, Stats<double>> flow_throughput_kbps) {
  double utilization = throughput_kbps.GetMean() / max_throughput_kbps;
  webrtc::test::PrintResult("BwePerformance", GetTestName(), "Utilization",
                            utilization * 100.0, "%", false);
  webrtc::test::PrintResult(
      "BwePerformance", GetTestName(), "Utilization var coeff",
      throughput_kbps.GetStdDev() / throughput_kbps.GetMean(), "", false);
  std::stringstream ss;
  for (auto& kv : flow_throughput_kbps) {
    ss.str("");
    ss << "Throughput flow " << kv.first;
    webrtc::test::PrintResultMeanAndError("BwePerformance", GetTestName(),
                                          ss.str(), kv.second.GetMean(),
                                          kv.second.GetStdDev(), "kbps", false);
  }
  for (auto& kv : flow_delay_ms) {
    ss.str("");
    ss << "Delay flow " << kv.first;
    webrtc::test::PrintResultMeanAndError("BwePerformance", GetTestName(),
                                          ss.str(), kv.second.GetMean(),
                                          kv.second.GetStdDev(), "ms", false);
  }
  double fairness_index = 1.0;
  if (!flow_throughput_kbps.empty()) {
    double squared_bitrate_sum = 0.0;
    fairness_index = 0.0;
    for (auto kv : flow_throughput_kbps) {
      squared_bitrate_sum += kv.second.GetMean() * kv.second.GetMean();
      fairness_index += kv.second.GetMean();
    }
    fairness_index *= fairness_index;
    fairness_index /= flow_throughput_kbps.size() * squared_bitrate_sum;
  }
  webrtc::test::PrintResult("BwePerformance", GetTestName(), "Fairness",
                            fairness_index * 100, "%", false);
}

void BweTest::RunFairnessTest(BandwidthEstimatorType bwe_type,
                              size_t num_media_flows,
                              size_t num_tcp_flows,
                              int64_t run_time_seconds,
                              uint32_t capacity_kbps,
                              int64_t max_delay_ms,
                              int64_t rtt_ms,
                              int64_t max_jitter_ms,
                              const int64_t* offsets_ms) {
  RunFairnessTest(bwe_type, num_media_flows, num_tcp_flows, run_time_seconds,
                  capacity_kbps, max_delay_ms, rtt_ms, max_jitter_ms,
                  offsets_ms, "Fairness_test", bwe_names[bwe_type]);
}

void BweTest::RunFairnessTest(BandwidthEstimatorType bwe_type,
                              size_t num_media_flows,
                              size_t num_tcp_flows,
                              int64_t run_time_seconds,
                              uint32_t capacity_kbps,
                              int64_t max_delay_ms,
                              int64_t rtt_ms,
                              int64_t max_jitter_ms,
                              const int64_t* offsets_ms,
                              const std::string& title,
                              const std::string& flow_name) {
  std::set<int> all_flow_ids;
  std::set<int> media_flow_ids;
  std::set<int> tcp_flow_ids;
  int next_flow_id = 0;
  for (size_t i = 0; i < num_media_flows; ++i) {
    media_flow_ids.insert(next_flow_id);
    all_flow_ids.insert(next_flow_id);
    ++next_flow_id;
  }
  for (size_t i = 0; i < num_tcp_flows; ++i) {
    tcp_flow_ids.insert(next_flow_id);
    all_flow_ids.insert(next_flow_id);
    ++next_flow_id;
  }

  std::vector<VideoSource*> sources;
  std::vector<PacketSender*> senders;
  std::vector<MetricRecorder*> metric_recorders;

  int64_t max_offset_ms = 0;

  for (int media_flow : media_flow_ids) {
    sources.push_back(new AdaptiveVideoSource(media_flow, 30, 300, 0,
                                              offsets_ms[media_flow]));
    senders.push_back(new PacedVideoSender(&uplink_, sources.back(), bwe_type));
    max_offset_ms = std::max(max_offset_ms, offsets_ms[media_flow]);
  }

  for (int tcp_flow : tcp_flow_ids) {
    senders.push_back(new TcpSender(&uplink_, tcp_flow, offsets_ms[tcp_flow]));
    max_offset_ms = std::max(max_offset_ms, offsets_ms[tcp_flow]);
  }

  ChokeFilter choke(&uplink_, all_flow_ids);
  choke.set_capacity_kbps(capacity_kbps);
  choke.set_max_delay_ms(max_delay_ms);
  LinkShare link_share(&choke);

  int64_t one_way_delay_ms = rtt_ms / 2;
  DelayFilter delay_uplink(&uplink_, all_flow_ids);
  delay_uplink.SetOneWayDelayMs(one_way_delay_ms);

  JitterFilter jitter(&uplink_, all_flow_ids);
  jitter.SetMaxJitter(max_jitter_ms);

  std::vector<RateCounterFilter*> rate_counters;
  for (int flow : media_flow_ids) {
    rate_counters.push_back(
        new RateCounterFilter(&uplink_, flow, "Receiver", bwe_names[bwe_type]));
  }
  for (int flow : tcp_flow_ids) {
    rate_counters.push_back(new RateCounterFilter(&uplink_, flow, "Receiver",
                                                  bwe_names[kTcpEstimator]));
  }

  RateCounterFilter total_utilization(
      &uplink_, all_flow_ids, "total_utilization", "Total_link_utilization");

  std::vector<PacketReceiver*> receivers;
  // Delays is being plotted only for the first flow.
  // To plot all of them, replace "i == 0" with "true" on new PacketReceiver().
  for (int media_flow : media_flow_ids) {
    metric_recorders.push_back(
        new MetricRecorder(bwe_names[bwe_type], static_cast<int>(media_flow),
                           senders[media_flow], &link_share));
    receivers.push_back(new PacketReceiver(&uplink_, media_flow, bwe_type,
                                           media_flow == 0, false,
                                           metric_recorders[media_flow]));
    metric_recorders[media_flow]->set_plot_available_capacity(
        media_flow == 0 && plot_total_available_capacity_);
    metric_recorders[media_flow]->set_start_computing_metrics_ms(max_offset_ms);
  }
  // Delays is not being plotted only for TCP flows. To plot all of them,
  // replace first "false" occurence with "true" on new PacketReceiver().
  for (int tcp_flow : tcp_flow_ids) {
    metric_recorders.push_back(
        new MetricRecorder(bwe_names[kTcpEstimator], static_cast<int>(tcp_flow),
                           senders[tcp_flow], &link_share));
    receivers.push_back(new PacketReceiver(&uplink_, tcp_flow, kTcpEstimator,
                                           false, false,
                                           metric_recorders[tcp_flow]));
    metric_recorders[tcp_flow]->set_plot_available_capacity(
        tcp_flow == 0 && plot_total_available_capacity_);
  }

  DelayFilter delay_downlink(&downlink_, all_flow_ids);
  delay_downlink.SetOneWayDelayMs(one_way_delay_ms);

  RunFor(run_time_seconds * 1000);

  std::map<int, Stats<double>> flow_throughput_kbps;
  for (RateCounterFilter* rate_counter : rate_counters) {
    int flow_id = *rate_counter->flow_ids().begin();
    flow_throughput_kbps[flow_id] = rate_counter->GetBitrateStats();
  }

  std::map<int, Stats<double>> flow_delay_ms;
  for (PacketReceiver* receiver : receivers) {
    int flow_id = *receiver->flow_ids().begin();
    flow_delay_ms[flow_id] = receiver->GetDelayStats();
  }

  PrintResults(capacity_kbps, total_utilization.GetBitrateStats(),
               flow_delay_ms, flow_throughput_kbps);

  if (!field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
    for (int i : all_flow_ids) {
      metric_recorders[i]->PlotThroughputHistogram(
          title, flow_name, static_cast<int>(num_media_flows), 0);

      metric_recorders[i]->PlotLossHistogram(title, flow_name,
                                             static_cast<int>(num_media_flows),
                                             receivers[i]->GlobalPacketLoss());
    }

    // Pointless to show delay histogram for TCP flow.
    for (int i : media_flow_ids) {
      metric_recorders[i]->PlotDelayHistogram(title, bwe_names[bwe_type],
                                              static_cast<int>(num_media_flows),
                                              one_way_delay_ms);
      BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], one_way_delay_ms, i);
    }
  }

  for (VideoSource* source : sources)
    delete source;
  for (PacketSender* sender : senders)
    delete sender;
  for (RateCounterFilter* rate_counter : rate_counters)
    delete rate_counter;
  for (PacketReceiver* receiver : receivers)
    delete receiver;
  for (MetricRecorder* recorder : metric_recorders)
    delete recorder;
}

void BweTest::RunChoke(BandwidthEstimatorType bwe_type,
                       std::vector<int> capacities_kbps) {
  int flow_id = bwe_type;
  AdaptiveVideoSource source(flow_id, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, bwe_type);
  ChokeFilter choke(&uplink_, flow_id);
  LinkShare link_share(&choke);
  MetricRecorder metric_recorder(bwe_names[bwe_type], flow_id, &sender,
                                 &link_share);
  PacketReceiver receiver(&uplink_, flow_id, bwe_type, true, false,
                          &metric_recorder);
  metric_recorder.set_plot_available_capacity(plot_total_available_capacity_);

  choke.set_max_delay_ms(500);
  const int64_t kRunTimeMs = 60 * 1000;

  std::stringstream title("Choke");
  char delimiter = '_';

  for (auto it = capacities_kbps.begin(); it != capacities_kbps.end(); ++it) {
    choke.set_capacity_kbps(*it);
    RunFor(kRunTimeMs);
    title << delimiter << (*it);
    delimiter = '-';
  }

  title << "_kbps,_" << (kRunTimeMs / 1000) << "s_each";
  metric_recorder.PlotThroughputHistogram(title.str(), bwe_names[bwe_type], 1,
                                          0);
  metric_recorder.PlotDelayHistogram(title.str(), bwe_names[bwe_type], 1, 0);
  // receiver.PlotLossHistogram(title, bwe_names[bwe_type], 1);
  // receiver.PlotObjectiveHistogram(title, bwe_names[bwe_type], 1);
}

// 5.1. Single Video and Audio media traffic, forward direction.
void BweTest::RunVariableCapacity1SingleFlow(BandwidthEstimatorType bwe_type) {
  const int kFlowId = 0;  // Arbitrary value.
  AdaptiveVideoSource source(kFlowId, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, bwe_type);

  DefaultEvaluationFilter up_filter(&uplink_, kFlowId);
  LinkShare link_share(&(up_filter.choke));
  MetricRecorder metric_recorder(bwe_names[bwe_type], kFlowId, &sender,
                                 &link_share);

  PacketReceiver receiver(&uplink_, kFlowId, bwe_type, true, true,
                          &metric_recorder);

  metric_recorder.set_plot_available_capacity(plot_total_available_capacity_);

  DelayFilter down_filter(&downlink_, kFlowId);
  down_filter.SetOneWayDelayMs(kOneWayDelayMs);

  // Test also with one way propagation delay = 100ms.
  // up_filter.delay.SetOneWayDelayMs(100);
  // down_filter.SetOneWayDelayMs(100);

  up_filter.choke.set_capacity_kbps(1000);
  RunFor(40 * 1000);  // 0-40s.
  up_filter.choke.set_capacity_kbps(2500);
  RunFor(20 * 1000);  // 40-60s.
  up_filter.choke.set_capacity_kbps(600);
  RunFor(20 * 1000);  // 60-80s.
  up_filter.choke.set_capacity_kbps(1000);
  RunFor(20 * 1000);  // 80-100s.

  std::string title("5.1_Variable_capacity_single_flow");
  metric_recorder.PlotThroughputHistogram(title, bwe_names[bwe_type], 1, 0);
  metric_recorder.PlotDelayHistogram(title, bwe_names[bwe_type], 1,
                                     kOneWayDelayMs);
  metric_recorder.PlotLossHistogram(title, bwe_names[bwe_type], 1,
                                    receiver.GlobalPacketLoss());
  BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], kOneWayDelayMs, kFlowId);
}

// 5.2. Two forward direction competing flows, variable capacity.
void BweTest::RunVariableCapacity2MultipleFlows(BandwidthEstimatorType bwe_type,
                                                size_t num_flows) {
  std::vector<VideoSource*> sources;
  std::vector<PacketSender*> senders;
  std::vector<MetricRecorder*> metric_recorders;
  std::vector<PacketReceiver*> receivers;

  const int64_t kStartingApartMs = 0;  // Flows initialized simultaneously.

  for (size_t i = 0; i < num_flows; ++i) {
    sources.push_back(new AdaptiveVideoSource(static_cast<int>(i), 30, 300, 0,
                                              i * kStartingApartMs));
    senders.push_back(new VideoSender(&uplink_, sources[i], bwe_type));
  }

  FlowIds flow_ids = CreateFlowIdRange(0, static_cast<int>(num_flows - 1));

  DefaultEvaluationFilter up_filter(&uplink_, flow_ids);
  LinkShare link_share(&(up_filter.choke));

  RateCounterFilter total_utilization(&uplink_, flow_ids, "Total_utilization",
                                      "Total_link_utilization");

  // Delays is being plotted only for the first flow.
  // To plot all of them, replace "i == 0" with "true" on new PacketReceiver().
  for (size_t i = 0; i < num_flows; ++i) {
    metric_recorders.push_back(new MetricRecorder(
        bwe_names[bwe_type], static_cast<int>(i), senders[i], &link_share));

    receivers.push_back(new PacketReceiver(&uplink_, static_cast<int>(i),
                                           bwe_type, i == 0, false,
                                           metric_recorders[i]));
    metric_recorders[i]->set_plot_available_capacity(
        i == 0 && plot_total_available_capacity_);
  }

  DelayFilter down_filter(&downlink_, flow_ids);
  down_filter.SetOneWayDelayMs(kOneWayDelayMs);
  // Test also with one way propagation delay = 100ms.
  // up_filter.delay.SetOneWayDelayMs(100);
  // down_filter.SetOneWayDelayMs(100);

  up_filter.choke.set_capacity_kbps(4000);
  RunFor(25 * 1000);  // 0-25s.
  up_filter.choke.set_capacity_kbps(2000);
  RunFor(25 * 1000);  // 25-50s.
  up_filter.choke.set_capacity_kbps(3500);
  RunFor(25 * 1000);  // 50-75s.
  up_filter.choke.set_capacity_kbps(1000);
  RunFor(25 * 1000);  // 75-100s.
  up_filter.choke.set_capacity_kbps(2000);
  RunFor(25 * 1000);  // 100-125s.

  std::string title("5.2_Variable_capacity_two_flows");
  for (size_t i = 0; i < num_flows; ++i) {
    metric_recorders[i]->PlotThroughputHistogram(title, bwe_names[bwe_type],
                                                 num_flows, 0);
    metric_recorders[i]->PlotDelayHistogram(title, bwe_names[bwe_type],
                                            num_flows, kOneWayDelayMs);
    metric_recorders[i]->PlotLossHistogram(title, bwe_names[bwe_type],
                                           num_flows,
                                           receivers[i]->GlobalPacketLoss());
    BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], kOneWayDelayMs, i);
  }

  for (VideoSource* source : sources)
    delete source;
  for (PacketSender* sender : senders)
    delete sender;
  for (MetricRecorder* recorder : metric_recorders)
    delete recorder;
  for (PacketReceiver* receiver : receivers)
    delete receiver;
}

// 5.3. Bi-directional RMCAT flows.
void BweTest::RunBidirectionalFlow(BandwidthEstimatorType bwe_type) {
  enum direction { kForward = 0, kBackward };
  const size_t kNumFlows = 2;
  std::unique_ptr<AdaptiveVideoSource> sources[kNumFlows];
  std::unique_ptr<VideoSender> senders[kNumFlows];
  std::unique_ptr<MetricRecorder> metric_recorders[kNumFlows];
  std::unique_ptr<PacketReceiver> receivers[kNumFlows];

  sources[kForward].reset(new AdaptiveVideoSource(kForward, 30, 300, 0, 0));
  senders[kForward].reset(
      new VideoSender(&uplink_, sources[kForward].get(), bwe_type));

  sources[kBackward].reset(new AdaptiveVideoSource(kBackward, 30, 300, 0, 0));
  senders[kBackward].reset(
      new VideoSender(&downlink_, sources[kBackward].get(), bwe_type));

  DefaultEvaluationFilter up_filter(&uplink_, kForward);
  LinkShare up_link_share(&(up_filter.choke));

  metric_recorders[kForward].reset(new MetricRecorder(
      bwe_names[bwe_type], kForward, senders[kForward].get(), &up_link_share));
  receivers[kForward].reset(
      new PacketReceiver(&uplink_, kForward, bwe_type, true, false,
                         metric_recorders[kForward].get()));

  metric_recorders[kForward].get()->set_plot_available_capacity(
      plot_total_available_capacity_);

  DefaultEvaluationFilter down_filter(&downlink_, kBackward);
  LinkShare down_link_share(&(down_filter.choke));

  metric_recorders[kBackward].reset(
      new MetricRecorder(bwe_names[bwe_type], kBackward,
                         senders[kBackward].get(), &down_link_share));
  receivers[kBackward].reset(
      new PacketReceiver(&downlink_, kBackward, bwe_type, true, false,
                         metric_recorders[kBackward].get()));

  metric_recorders[kBackward].get()->set_plot_available_capacity(
      plot_total_available_capacity_);

  // Test also with one way propagation delay = 100ms.
  // up_filter.delay.SetOneWayDelayMs(100);
  // down_filter.delay.SetOneWayDelayMs(100);

  up_filter.choke.set_capacity_kbps(2000);
  down_filter.choke.set_capacity_kbps(2000);
  RunFor(20 * 1000);  // 0-20s.

  up_filter.choke.set_capacity_kbps(1000);
  RunFor(15 * 1000);  // 20-35s.

  down_filter.choke.set_capacity_kbps(800);
  RunFor(5 * 1000);  // 35-40s.

  up_filter.choke.set_capacity_kbps(500);
  RunFor(20 * 1000);  // 40-60s.

  up_filter.choke.set_capacity_kbps(2000);
  RunFor(10 * 1000);  // 60-70s.

  down_filter.choke.set_capacity_kbps(2000);
  RunFor(30 * 1000);  // 70-100s.

  std::string title("5.3_Bidirectional_flows");
  for (size_t i = 0; i < kNumFlows; ++i) {
    metric_recorders[i].get()->PlotThroughputHistogram(
        title, bwe_names[bwe_type], kNumFlows, 0);
    metric_recorders[i].get()->PlotDelayHistogram(title, bwe_names[bwe_type],
                                                  kNumFlows, kOneWayDelayMs);
    metric_recorders[i].get()->PlotLossHistogram(
        title, bwe_names[bwe_type], kNumFlows,
        receivers[i].get()->GlobalPacketLoss());
    BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], kOneWayDelayMs, i);
  }
}

// 5.4. Three forward direction competing flows, constant capacity.
void BweTest::RunSelfFairness(BandwidthEstimatorType bwe_type) {
  const int kNumRmcatFlows = 3;
  const int kNumTcpFlows = 0;
  const int64_t kRunTimeS = 120;
  const int kLinkCapacity = 3500;

  int64_t max_delay_ms = kMaxQueueingDelayMs;
  int64_t rtt_ms = 2 * kOneWayDelayMs;

  const int64_t kStartingApartMs = 20 * 1000;
  int64_t offsets_ms[kNumRmcatFlows];
  for (int i = 0; i < kNumRmcatFlows; ++i) {
    offsets_ms[i] = kStartingApartMs * i;
  }

  // Test also with one way propagation delay = 100ms.
  // rtt_ms = 2 * 100;
  // Test also with bottleneck queue size = 20ms and 1000ms.
  // max_delay_ms = 20;
  // max_delay_ms = 1000;

  std::string title("5.4_Self_fairness_test");

  // Test also with one way propagation delay = 100ms.
  RunFairnessTest(bwe_type, kNumRmcatFlows, kNumTcpFlows, kRunTimeS,
                  kLinkCapacity, max_delay_ms, rtt_ms, kMaxJitterMs, offsets_ms,
                  title, bwe_names[bwe_type]);
}

// 5.5. Five competing RMCAT flows under different RTTs.
void BweTest::RunRoundTripTimeFairness(BandwidthEstimatorType bwe_type) {
  const int kAllFlowIds[] = {0, 1, 2, 3, 4};  // Five RMCAT flows.
  const int64_t kAllOneWayDelayMs[] = {10, 25, 50, 100, 150};
  const size_t kNumFlows = arraysize(kAllFlowIds);
  std::unique_ptr<AdaptiveVideoSource> sources[kNumFlows];
  std::unique_ptr<VideoSender> senders[kNumFlows];
  std::unique_ptr<MetricRecorder> metric_recorders[kNumFlows];

  // Flows initialized 10 seconds apart.
  const int64_t kStartingApartMs = 10 * 1000;

  for (size_t i = 0; i < kNumFlows; ++i) {
    sources[i].reset(new AdaptiveVideoSource(kAllFlowIds[i], 30, 300, 0,
                                             i * kStartingApartMs));
    senders[i].reset(new VideoSender(&uplink_, sources[i].get(), bwe_type));
  }

  ChokeFilter choke_filter(&uplink_, CreateFlowIds(kAllFlowIds, kNumFlows));
  LinkShare link_share(&choke_filter);

  JitterFilter jitter_filter(&uplink_, CreateFlowIds(kAllFlowIds, kNumFlows));

  std::unique_ptr<DelayFilter> up_delay_filters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    up_delay_filters[i].reset(new DelayFilter(&uplink_, kAllFlowIds[i]));
  }

  RateCounterFilter total_utilization(
      &uplink_, CreateFlowIds(kAllFlowIds, kNumFlows), "Total_utilization",
      "Total_link_utilization");

  // Delays is being plotted only for the first flow.
  // To plot all of them, replace "i == 0" with "true" on new PacketReceiver().
  std::unique_ptr<PacketReceiver> receivers[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    metric_recorders[i].reset(
        new MetricRecorder(bwe_names[bwe_type], static_cast<int>(i),
                           senders[i].get(), &link_share));

    receivers[i].reset(new PacketReceiver(&uplink_, kAllFlowIds[i], bwe_type,
                                          i == 0, false,
                                          metric_recorders[i].get()));
    metric_recorders[i].get()->set_start_computing_metrics_ms(kStartingApartMs *
                                                              (kNumFlows - 1));
    metric_recorders[i].get()->set_plot_available_capacity(
        i == 0 && plot_total_available_capacity_);
  }

  std::unique_ptr<DelayFilter> down_delay_filters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    down_delay_filters[i].reset(new DelayFilter(&downlink_, kAllFlowIds[i]));
  }

  jitter_filter.SetMaxJitter(kMaxJitterMs);
  choke_filter.set_max_delay_ms(kMaxQueueingDelayMs);

  for (size_t i = 0; i < kNumFlows; ++i) {
    up_delay_filters[i]->SetOneWayDelayMs(kAllOneWayDelayMs[i]);
    down_delay_filters[i]->SetOneWayDelayMs(kAllOneWayDelayMs[i]);
  }

  choke_filter.set_capacity_kbps(3500);

  RunFor(300 * 1000);  // 0-300s.

  std::string title("5.5_Round_Trip_Time_Fairness");
  for (size_t i = 0; i < kNumFlows; ++i) {
    metric_recorders[i].get()->PlotThroughputHistogram(
        title, bwe_names[bwe_type], kNumFlows, 0);
    metric_recorders[i].get()->PlotDelayHistogram(title, bwe_names[bwe_type],
                                                  kNumFlows, kOneWayDelayMs);
    metric_recorders[i].get()->PlotLossHistogram(
        title, bwe_names[bwe_type], kNumFlows,
        receivers[i].get()->GlobalPacketLoss());
    BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], kAllOneWayDelayMs[i],
                                 i);
  }
}

// 5.6. RMCAT Flow competing with a long TCP Flow.
void BweTest::RunLongTcpFairness(BandwidthEstimatorType bwe_type) {
  const size_t kNumRmcatFlows = 1;
  const size_t kNumTcpFlows = 1;
  const int64_t kRunTimeS = 120;
  const int kCapacityKbps = 2000;
  // Tcp starts at t = 0, media flow at t = 5s.
  const int64_t kOffSetsMs[] = {5000, 0};

  int64_t max_delay_ms = kMaxQueueingDelayMs;
  int64_t rtt_ms = 2 * kOneWayDelayMs;

  // Test also with one way propagation delay = 100ms.
  // rtt_ms = 2 * 100;
  // Test also with bottleneck queue size = 20ms and 1000ms.
  // max_delay_ms = 20;
  // max_delay_ms = 1000;

  std::string title("5.6_Long_TCP_Fairness");
  std::string flow_name = std::string() +
      bwe_names[bwe_type] + 'x' + bwe_names[kTcpEstimator];

  RunFairnessTest(bwe_type, kNumRmcatFlows, kNumTcpFlows, kRunTimeS,
                  kCapacityKbps, max_delay_ms, rtt_ms, kMaxJitterMs, kOffSetsMs,
                  title, flow_name);
}

// 5.7. RMCAT Flows competing with multiple short TCP Flows.
void BweTest::RunMultipleShortTcpFairness(
    BandwidthEstimatorType bwe_type,
    std::vector<int> tcp_file_sizes_bytes,
    std::vector<int64_t> tcp_starting_times_ms) {
  // Two RMCAT flows and ten TCP flows.
  const int kAllRmcatFlowIds[] = {0, 1};
  const int kAllTcpFlowIds[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  assert(tcp_starting_times_ms.size() == tcp_file_sizes_bytes.size() &&
         tcp_starting_times_ms.size() == arraysize(kAllTcpFlowIds));

  const size_t kNumRmcatFlows = arraysize(kAllRmcatFlowIds);
  const size_t kNumTotalFlows = kNumRmcatFlows + arraysize(kAllTcpFlowIds);

  std::unique_ptr<AdaptiveVideoSource> sources[kNumRmcatFlows];
  std::unique_ptr<PacketSender> senders[kNumTotalFlows];
  std::unique_ptr<MetricRecorder> metric_recorders[kNumTotalFlows];
  std::unique_ptr<PacketReceiver> receivers[kNumTotalFlows];

  // RMCAT Flows are initialized simultaneosly at t=5 seconds.
  const int64_t kRmcatStartingTimeMs = 5 * 1000;
  for (size_t id : kAllRmcatFlowIds) {
    sources[id].reset(new AdaptiveVideoSource(static_cast<int>(id), 30, 300, 0,
                                              kRmcatStartingTimeMs));
    senders[id].reset(new VideoSender(&uplink_, sources[id].get(), bwe_type));
  }

  for (size_t id : kAllTcpFlowIds) {
    senders[id].reset(new TcpSender(&uplink_, static_cast<int>(id),
                                    tcp_starting_times_ms[id - kNumRmcatFlows],
                                    tcp_file_sizes_bytes[id - kNumRmcatFlows]));
  }

  FlowIds flow_ids = CreateFlowIdRange(0, static_cast<int>(kNumTotalFlows - 1));
  DefaultEvaluationFilter up_filter(&uplink_, flow_ids);

  LinkShare link_share(&(up_filter.choke));

  RateCounterFilter total_utilization(&uplink_, flow_ids, "Total_utilization",
                                      "Total_link_utilization");

  // Delays is being plotted only for the first flow.
  // To plot all of them, replace "i == 0" with "true" on new PacketReceiver().
  for (size_t id : kAllRmcatFlowIds) {
    metric_recorders[id].reset(
        new MetricRecorder(bwe_names[bwe_type], static_cast<int>(id),
                           senders[id].get(), &link_share));
    receivers[id].reset(new PacketReceiver(&uplink_, static_cast<int>(id),
                                           bwe_type, id == 0, false,
                                           metric_recorders[id].get()));
    metric_recorders[id].get()->set_start_computing_metrics_ms(
        kRmcatStartingTimeMs);
    metric_recorders[id].get()->set_plot_available_capacity(
        id == 0 && plot_total_available_capacity_);
  }

  // Delays is not being plotted only for TCP flows. To plot all of them,
  // replace first "false" occurence with "true" on new PacketReceiver().
  for (size_t id : kAllTcpFlowIds) {
    metric_recorders[id].reset(
        new MetricRecorder(bwe_names[kTcpEstimator], static_cast<int>(id),
                           senders[id].get(), &link_share));
    receivers[id].reset(new PacketReceiver(&uplink_, static_cast<int>(id),
                                           kTcpEstimator, false, false,
                                           metric_recorders[id].get()));
    metric_recorders[id].get()->set_plot_available_capacity(
        id == 0 && plot_total_available_capacity_);
  }

  DelayFilter down_filter(&downlink_, flow_ids);
  down_filter.SetOneWayDelayMs(kOneWayDelayMs);

  // Test also with one way propagation delay = 100ms.
  // up_filter.delay.SetOneWayDelayMs(100);
  // down_filter.SetOneWayDelayms(100);

  // Test also with bottleneck queue size = 20ms and 1000ms.
  // up_filter.choke.set_max_delay_ms(20);
  // up_filter.choke.set_max_delay_ms(1000);

  // Test also with no Jitter:
  // up_filter.jitter.SetMaxJitter(0);

  up_filter.choke.set_capacity_kbps(2000);

  RunFor(300 * 1000);  // 0-300s.

  std::string title("5.7_Multiple_short_TCP_flows");
  for (size_t id : kAllRmcatFlowIds) {
    metric_recorders[id].get()->PlotThroughputHistogram(
        title, bwe_names[bwe_type], kNumRmcatFlows, 0);
    metric_recorders[id].get()->PlotDelayHistogram(
        title, bwe_names[bwe_type], kNumRmcatFlows, kOneWayDelayMs);
    metric_recorders[id].get()->PlotLossHistogram(
        title, bwe_names[bwe_type], kNumRmcatFlows,
        receivers[id].get()->GlobalPacketLoss());
    BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], kOneWayDelayMs, id);
  }
}

// 5.8. Three forward direction competing flows, constant capacity.
// During the test, one of the flows is paused and later resumed.
void BweTest::RunPauseResumeFlows(BandwidthEstimatorType bwe_type) {
  const int kAllFlowIds[] = {0, 1, 2};  // Three RMCAT flows.
  const size_t kNumFlows = arraysize(kAllFlowIds);

  std::unique_ptr<AdaptiveVideoSource> sources[kNumFlows];
  std::unique_ptr<VideoSender> senders[kNumFlows];
  std::unique_ptr<MetricRecorder> metric_recorders[kNumFlows];
  std::unique_ptr<PacketReceiver> receivers[kNumFlows];

  // Flows initialized simultaneously.
  const int64_t kStartingApartMs = 0;

  for (size_t i = 0; i < kNumFlows; ++i) {
    sources[i].reset(new AdaptiveVideoSource(kAllFlowIds[i], 30, 300, 0,
                                             i * kStartingApartMs));
    senders[i].reset(new VideoSender(&uplink_, sources[i].get(), bwe_type));
  }

  DefaultEvaluationFilter filter(&uplink_,
                                 CreateFlowIds(kAllFlowIds, kNumFlows));

  LinkShare link_share(&(filter.choke));

  RateCounterFilter total_utilization(
      &uplink_, CreateFlowIds(kAllFlowIds, kNumFlows), "Total_utilization",
      "Total_link_utilization");

  // Delays is being plotted only for the first flow.
  // To plot all of them, replace "i == 0" with "true" on new PacketReceiver().
  for (size_t i = 0; i < kNumFlows; ++i) {
    metric_recorders[i].reset(
        new MetricRecorder(bwe_names[bwe_type], static_cast<int>(i),
                           senders[i].get(), &link_share));
    receivers[i].reset(new PacketReceiver(&uplink_, kAllFlowIds[i], bwe_type,
                                          i == 0, false,
                                          metric_recorders[i].get()));
    metric_recorders[i].get()->set_start_computing_metrics_ms(kStartingApartMs *
                                                              (kNumFlows - 1));
    metric_recorders[i].get()->set_plot_available_capacity(
        i == 0 && plot_total_available_capacity_);
  }

  // Test also with one way propagation delay = 100ms.
  // filter.delay.SetOneWayDelayMs(100);
  filter.choke.set_capacity_kbps(3500);

  RunFor(40 * 1000);  // 0-40s.
  senders[0].get()->Pause();
  RunFor(20 * 1000);  // 40-60s.
  senders[0].get()->Resume(20 * 1000);
  RunFor(60 * 1000);  // 60-120s.

  int64_t paused[] = {20 * 1000, 0, 0};

  // First flow is being paused, hence having a different optimum.
  const std::string optima_lines[] = {"1", "2", "2"};

  std::string title("5.8_Pause_and_resume_media_flow");
  for (size_t i = 0; i < kNumFlows; ++i) {
    metric_recorders[i].get()->PlotThroughputHistogram(
        title, bwe_names[bwe_type], kNumFlows, paused[i], optima_lines[i]);
    metric_recorders[i].get()->PlotDelayHistogram(title, bwe_names[bwe_type],
                                                  kNumFlows, kOneWayDelayMs);
    metric_recorders[i].get()->PlotLossHistogram(
        title, bwe_names[bwe_type], kNumFlows,
        receivers[i].get()->GlobalPacketLoss());
    BWE_TEST_LOGGING_BASELINEBAR(5, bwe_names[bwe_type], kOneWayDelayMs, i);
  }
}

// Following functions are used for randomizing TCP file size and
// starting time, used on 5.7 RunMultipleShortTcpFairness.
// They are pseudo-random generators, creating always the same
// value sequence for a given Random seed.

std::vector<int> BweTest::GetFileSizesBytes(int num_files) {
  // File size chosen from uniform distribution between [100,1000] kB.
  const int kMinKbytes = 100;
  const int kMaxKbytes = 1000;

  Random random(0x12345678);
  std::vector<int> tcp_file_sizes_bytes;

  while (num_files-- > 0) {
    tcp_file_sizes_bytes.push_back(random.Rand(kMinKbytes, kMaxKbytes) * 1000);
  }

  return tcp_file_sizes_bytes;
}

std::vector<int64_t> BweTest::GetStartingTimesMs(int num_files) {
  // OFF state behaves as an exp. distribution with mean = 10 seconds.
  const float kMeanMs = 10000.0f;
  Random random(0x12345678);

  std::vector<int64_t> tcp_starting_times_ms;

  // Two TCP Flows are initialized simultaneosly at t=0 seconds.
  for (int i = 0; i < 2; ++i, --num_files) {
    tcp_starting_times_ms.push_back(0);
  }

  // Other TCP Flows are initialized in an OFF state.
  while (num_files-- > 0) {
    tcp_starting_times_ms.push_back(
        static_cast<int64_t>(random.Exponential(1.0f / kMeanMs)));
  }

  return tcp_starting_times_ms;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
