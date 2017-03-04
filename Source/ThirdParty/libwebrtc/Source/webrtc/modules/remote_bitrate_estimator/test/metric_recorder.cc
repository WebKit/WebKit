/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/metric_recorder.h"

#include <algorithm>

#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"

namespace webrtc {
namespace testing {
namespace bwe {

namespace {
// Holder mean, Manhattan distance for p=1, EuclidianNorm/sqrt(n) for p=2.
template <typename T>
double NormLp(T sum, size_t size, double p) {
  return pow(sum / size, 1.0 / p);
}
}

const double kP = 1.0;  // Used for Norm Lp.

LinkShare::LinkShare(ChokeFilter* choke_filter)
    : choke_filter_(choke_filter), running_flows_(choke_filter->flow_ids()) {
}

void LinkShare::PauseFlow(int flow_id) {
  running_flows_.erase(flow_id);
}

void LinkShare::ResumeFlow(int flow_id) {
  running_flows_.insert(flow_id);
}

uint32_t LinkShare::TotalAvailableKbps() {
  return choke_filter_->capacity_kbps();
}

uint32_t LinkShare::AvailablePerFlowKbps(int flow_id) {
  uint32_t available_capacity_per_flow_kbps = 0;
  if (running_flows_.find(flow_id) != running_flows_.end()) {
    available_capacity_per_flow_kbps =
        TotalAvailableKbps() / static_cast<uint32_t>(running_flows_.size());
  }
  return available_capacity_per_flow_kbps;
}

MetricRecorder::MetricRecorder(const std::string algorithm_name,
                               int flow_id,
                               PacketSender* packet_sender,
                               LinkShare* link_share)
    : algorithm_name_(algorithm_name),
      flow_id_(flow_id),
      link_share_(link_share),
      now_ms_(0),
      sum_delays_ms_(0),
      delay_histogram_ms_(),
      sum_delays_square_ms2_(0),
      sum_throughput_bytes_(0),
      last_unweighted_estimate_error_(0),
      optimal_throughput_bits_(0),
      last_available_bitrate_per_flow_kbps_(0),
      start_computing_metrics_ms_(0),
      started_computing_metrics_(false),
      num_packets_received_(0) {
  std::fill_n(sum_lp_weighted_estimate_error_, 2, 0);
  if (packet_sender != nullptr)
    packet_sender->set_metric_recorder(this);
}

void MetricRecorder::SetPlotInformation(
    const std::vector<std::string>& prefixes,
    bool plot_delay,
    bool plot_loss) {
  assert(prefixes.size() == kNumMetrics);
  for (size_t i = 0; i < kNumMetrics; ++i) {
    plot_information_[i].prefix = prefixes[i];
  }
  plot_information_[kThroughput].plot_interval_ms = 100;
  plot_information_[kSendingEstimate].plot_interval_ms = 100;
  plot_information_[kDelay].plot_interval_ms = 100;
  plot_information_[kLoss].plot_interval_ms = 500;
  plot_information_[kObjective].plot_interval_ms = 1000;
  plot_information_[kTotalAvailable].plot_interval_ms = 1000;
  plot_information_[kAvailablePerFlow].plot_interval_ms = 1000;

  for (int i = kThroughput; i < kNumMetrics; ++i) {
    plot_information_[i].last_plot_ms = 0;
    switch (i) {
      case kSendingEstimate:
      case kObjective:
      case kAvailablePerFlow:
        plot_information_[i].plot = false;
        break;
      case kLoss:
        plot_information_[i].plot = plot_loss;
        break;
      case kDelay:
        plot_information_[i].plot = plot_delay;
        break;
      default:
        plot_information_[i].plot = true;
    }
  }
}

void MetricRecorder::PlotAllDynamics() {
  for (int i = kThroughput; i < kNumMetrics; ++i) {
    if (plot_information_[i].plot &&
        now_ms_ - plot_information_[i].last_plot_ms >=
            plot_information_[i].plot_interval_ms) {
      PlotDynamics(i);
    }
  }
}

void MetricRecorder::PlotDynamics(int metric) {
  if (metric == kTotalAvailable) {
    BWE_TEST_LOGGING_PLOT_WITH_NAME_AND_SSRC(
        0, plot_information_[kTotalAvailable].prefix, now_ms_,
        GetTotalAvailableKbps(), flow_id_, "Available");
  } else if (metric == kAvailablePerFlow) {
    BWE_TEST_LOGGING_PLOT_WITH_NAME_AND_SSRC(
        0, plot_information_[kAvailablePerFlow].prefix, now_ms_,
        GetAvailablePerFlowKbps(), flow_id_, "Available_per_flow");
  } else {
    PlotLine(metric, plot_information_[metric].prefix,
             plot_information_[metric].time_ms,
             plot_information_[metric].value);
  }
  plot_information_[metric].last_plot_ms = now_ms_;
}

template <typename T>
void MetricRecorder::PlotLine(int windows_id,
                              const std::string& prefix,
                              int64_t time_ms,
                              T y) {
  BWE_TEST_LOGGING_PLOT_WITH_NAME_AND_SSRC(windows_id, prefix, time_ms,
                                           static_cast<double>(y), flow_id_,
                                           algorithm_name_);
}

void MetricRecorder::UpdateTimeMs(int64_t time_ms) {
  now_ms_ = std::max(now_ms_, time_ms);
}

void MetricRecorder::UpdateThroughput(int64_t bitrate_kbps,
                                      size_t payload_size) {
  // Total throughput should be computed before updating the time.
  PushThroughputBytes(payload_size, now_ms_);
  plot_information_[kThroughput].Update(now_ms_, bitrate_kbps);
}

void MetricRecorder::UpdateSendingEstimateKbps(int64_t bitrate_kbps) {
  plot_information_[kSendingEstimate].Update(now_ms_, bitrate_kbps);
}

void MetricRecorder::UpdateDelayMs(int64_t delay_ms) {
  PushDelayMs(delay_ms, now_ms_);
  plot_information_[kDelay].Update(now_ms_, delay_ms);
}

void MetricRecorder::UpdateLoss(float loss_ratio) {
  plot_information_[kLoss].Update(now_ms_, loss_ratio);
}

void MetricRecorder::UpdateObjective() {
  plot_information_[kObjective].Update(now_ms_, ObjectiveFunction());
}

uint32_t MetricRecorder::GetTotalAvailableKbps() {
  if (link_share_ == nullptr)
    return 0;
  return link_share_->TotalAvailableKbps();
}

uint32_t MetricRecorder::GetAvailablePerFlowKbps() {
  if (link_share_ == nullptr)
    return 0;
  return link_share_->AvailablePerFlowKbps(flow_id_);
}

uint32_t MetricRecorder::GetSendingEstimateKbps() {
  return static_cast<uint32_t>(plot_information_[kSendingEstimate].value);
}

void MetricRecorder::PushDelayMs(int64_t delay_ms, int64_t arrival_time_ms) {
  if (ShouldRecord(arrival_time_ms)) {
    sum_delays_ms_ += delay_ms;
    sum_delays_square_ms2_ += delay_ms * delay_ms;
    if (delay_histogram_ms_.find(delay_ms) == delay_histogram_ms_.end()) {
      delay_histogram_ms_[delay_ms] = 0;
    }
    ++delay_histogram_ms_[delay_ms];
  }
}

void MetricRecorder::UpdateEstimateError(int64_t new_value) {
  int64_t lp_value = pow(static_cast<double>(std::abs(new_value)), kP);
  if (new_value < 0) {
    sum_lp_weighted_estimate_error_[0] += lp_value;
  } else {
    sum_lp_weighted_estimate_error_[1] += lp_value;
  }
}

void MetricRecorder::PushThroughputBytes(size_t payload_size,
                                         int64_t arrival_time_ms) {
  if (ShouldRecord(arrival_time_ms)) {
    ++num_packets_received_;
    sum_throughput_bytes_ += payload_size;

    int64_t current_available_per_flow_kbps =
        static_cast<int64_t>(GetAvailablePerFlowKbps());

    int64_t current_bitrate_diff_kbps =
        static_cast<int64_t>(GetSendingEstimateKbps()) -
        current_available_per_flow_kbps;

    int64_t weighted_estimate_error =
        (((current_bitrate_diff_kbps + last_unweighted_estimate_error_) *
          (arrival_time_ms - plot_information_[kThroughput].time_ms)) /
         2);

    UpdateEstimateError(weighted_estimate_error);

    optimal_throughput_bits_ +=
        ((current_available_per_flow_kbps +
          last_available_bitrate_per_flow_kbps_) *
         (arrival_time_ms - plot_information_[kThroughput].time_ms)) /
        2;

    last_available_bitrate_per_flow_kbps_ = current_available_per_flow_kbps;
  }
}

bool MetricRecorder::ShouldRecord(int64_t arrival_time_ms) {
  if (arrival_time_ms >= start_computing_metrics_ms_) {
    if (!started_computing_metrics_) {
      start_computing_metrics_ms_ = arrival_time_ms;
      now_ms_ = arrival_time_ms;
      started_computing_metrics_ = true;
    }
    return true;
  } else {
    return false;
  }
}

void MetricRecorder::PlotThroughputHistogram(
    const std::string& title,
    const std::string& bwe_name,
    size_t num_flows,
    int64_t extra_offset_ms,
    const std::string optimum_id) const {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
  double optimal_bitrate_per_flow_kbps = static_cast<double>(
      optimal_throughput_bits_ / RunDurationMs(extra_offset_ms));

  double neg_error = Renormalize(
      NormLp(sum_lp_weighted_estimate_error_[0], num_packets_received_, kP));
  double pos_error = Renormalize(
      NormLp(sum_lp_weighted_estimate_error_[1], num_packets_received_, kP));

  double average_bitrate_kbps = AverageBitrateKbps(extra_offset_ms);

  // Prevent the error to be too close to zero (plotting issue).
  double extra_error = average_bitrate_kbps / 500;

  std::string optimum_title =
      optimum_id.empty() ? "optimal_bitrate" : "optimal_bitrates#" + optimum_id;

  BWE_TEST_LOGGING_LABEL(4, title, "average_bitrate_(kbps)", num_flows);
  BWE_TEST_LOGGING_LIMITERRORBAR(
      4, bwe_name, average_bitrate_kbps,
      average_bitrate_kbps - neg_error - extra_error,
      average_bitrate_kbps + pos_error + extra_error, "estimate_error",
      optimal_bitrate_per_flow_kbps, optimum_title, flow_id_);

  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Channel utilization : ",
                        "%lf %%",
                        100.0 * static_cast<double>(average_bitrate_kbps) /
                            optimal_bitrate_per_flow_kbps);
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
}

void MetricRecorder::PlotThroughputHistogram(const std::string& title,
                                             const std::string& bwe_name,
                                             size_t num_flows,
                                             int64_t extra_offset_ms) const {
  PlotThroughputHistogram(title, bwe_name, num_flows, extra_offset_ms, "");
}

void MetricRecorder::PlotDelayHistogram(const std::string& title,
                                        const std::string& bwe_name,
                                        size_t num_flows,
                                        int64_t one_way_path_delay_ms) const {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
  double average_delay_ms =
      static_cast<double>(sum_delays_ms_) / num_packets_received_;
  int64_t percentile_5_ms = NthDelayPercentile(5);
  int64_t percentile_95_ms = NthDelayPercentile(95);

  BWE_TEST_LOGGING_LABEL(5, title, "average_delay_(ms)", num_flows);
  BWE_TEST_LOGGING_ERRORBAR(5, bwe_name, average_delay_ms, percentile_5_ms,
                            percentile_95_ms, "5th and 95th percentiles",
                            flow_id_);

  // Log added latency, disregard baseline path delay.
  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Delay average : ",
                        "%lf ms", average_delay_ms - one_way_path_delay_ms);
  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Delay 5th percentile : ",
                        "%ld ms", percentile_5_ms - one_way_path_delay_ms);
  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Delay 95th percentile : ",
                        "%ld ms", percentile_95_ms - one_way_path_delay_ms);
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
}

void MetricRecorder::PlotLossHistogram(const std::string& title,
                                       const std::string& bwe_name,
                                       size_t num_flows,
                                       float global_loss_ratio) const {
  BWE_TEST_LOGGING_LABEL(6, title, "packet_loss_ratio_(%)", num_flows);
  BWE_TEST_LOGGING_BAR(6, bwe_name, 100.0f * global_loss_ratio, flow_id_);

  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Loss Ratio : ", "%f %%",
                        100.0f * global_loss_ratio);
}

void MetricRecorder::PlotObjectiveHistogram(const std::string& title,
                                            const std::string& bwe_name,
                                            size_t num_flows) const {
  BWE_TEST_LOGGING_LABEL(7, title, "objective_function", num_flows);
  BWE_TEST_LOGGING_BAR(7, bwe_name, ObjectiveFunction(), flow_id_);
}

void MetricRecorder::PlotZero() {
  for (int i = kThroughput; i <= kLoss; ++i) {
    if (plot_information_[i].plot) {
      std::stringstream prefix;
      // TODO(terelius): Since this does not use the BWE_TEST_LOGGING macros,
      // it hasn't been kept up to date with the plot format. Remove or fix?
      prefix << "Receiver_" << flow_id_ << "_" + plot_information_[i].prefix;
      PlotLine(i, prefix.str(), now_ms_, 0);
      plot_information_[i].last_plot_ms = now_ms_;
    }
  }
}

void MetricRecorder::PauseFlow() {
  PlotZero();
  link_share_->PauseFlow(flow_id_);
}

void MetricRecorder::ResumeFlow(int64_t paused_time_ms) {
  UpdateTimeMs(now_ms_ + paused_time_ms);
  PlotZero();
  link_share_->ResumeFlow(flow_id_);
}

double MetricRecorder::AverageBitrateKbps(int64_t extra_offset_ms) const {
  int64_t duration_ms = RunDurationMs(extra_offset_ms);
  if (duration_ms == 0)
    return 0.0;
  return static_cast<double>(8 * sum_throughput_bytes_ / duration_ms);
}

int64_t MetricRecorder::RunDurationMs(int64_t extra_offset_ms) const {
  return now_ms_ - start_computing_metrics_ms_ - extra_offset_ms;
}

double MetricRecorder::DelayStdDev() const {
  if (num_packets_received_ == 0) {
    return 0.0;
  }
  double mean = static_cast<double>(sum_delays_ms_) / num_packets_received_;
  double mean2 =
      static_cast<double>(sum_delays_square_ms2_) / num_packets_received_;
  return sqrt(mean2 - pow(mean, 2.0));
}

// Since delay values are bounded in a subset of [0, 5000] ms,
// this function's execution time is O(1), independend of num_packets_received_.
int64_t MetricRecorder::NthDelayPercentile(int n) const {
  if (num_packets_received_ == 0) {
    return 0;
  }
  size_t num_packets_remaining = (n * num_packets_received_) / 100;
  for (auto hist : delay_histogram_ms_) {
    if (num_packets_remaining <= hist.second)
      return static_cast<int64_t>(hist.first);
    num_packets_remaining -= hist.second;
  }

  assert(false);
  return -1;
}

// The weighted_estimate_error_ was weighted based on time windows.
// This function scales back the result before plotting.
double MetricRecorder::Renormalize(double x) const {
  return (x * num_packets_received_) / now_ms_;
}

inline double U(int64_t x, double alpha) {
  if (alpha == 1.0) {
    return log(static_cast<double>(x));
  }
  return pow(static_cast<double>(x), 1.0 - alpha) / (1.0 - alpha);
}

inline double U(size_t x, double alpha) {
  return U(static_cast<int64_t>(x), alpha);
}

// TODO(magalhaesc): Update ObjectiveFunction.
double MetricRecorder::ObjectiveFunction() const {
  const double kDelta = 0.15;  // Delay penalty factor.
  const double kAlpha = 1.0;
  const double kBeta = 1.0;

  double throughput_metric = U(sum_throughput_bytes_, kAlpha);
  double delay_penalty = kDelta * U(sum_delays_ms_, kBeta);

  return throughput_metric - delay_penalty;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
