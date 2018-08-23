/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FRAMEWORK_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FRAMEWORK_H_

#include <assert.h>
#include <math.h>

#include <algorithm>
#include <list>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/paced_sender.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "modules/remote_bitrate_estimator/test/packet.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class RtcpBandwidthObserver;

namespace testing {
namespace bwe {

class DelayCapHelper;

class RateCounter {
 public:
  explicit RateCounter(int64_t window_size_ms);
  RateCounter();
  ~RateCounter();

  void UpdateRates(int64_t send_time_us, uint32_t payload_size);

  int64_t window_size_ms() const { return (window_size_us_ + 500) / 1000; }
  uint32_t packets_per_second() const;
  uint32_t bits_per_second() const;

  double BitrateWindowS() const;

 private:
  typedef std::pair<int64_t, uint32_t> TimeSizePair;

  int64_t window_size_us_;
  uint32_t recently_received_packets_;
  uint32_t recently_received_bytes_;
  int64_t last_accumulated_us_;
  std::list<TimeSizePair> window_;
};

typedef std::set<int> FlowIds;
const FlowIds CreateFlowIds(const int* flow_ids_array, size_t num_flow_ids);
const FlowIds CreateFlowIdRange(int initial_value, int last_value);

template <typename T>
bool DereferencingComparator(const T* const& a, const T* const& b) {
  assert(a != NULL);
  assert(b != NULL);
  return *a < *b;
}

template <typename T>
class Stats {
 public:
  Stats()
      : data_(),
        last_mean_count_(0),
        last_variance_count_(0),
        last_minmax_count_(0),
        mean_(0),
        variance_(0),
        min_(0),
        max_(0) {}

  void Push(T data_point) { data_.push_back(data_point); }

  T GetMean() {
    if (last_mean_count_ != data_.size()) {
      last_mean_count_ = data_.size();
      mean_ = std::accumulate(data_.begin(), data_.end(), static_cast<T>(0));
      assert(last_mean_count_ != 0);
      mean_ /= static_cast<T>(last_mean_count_);
    }
    return mean_;
  }
  T GetVariance() {
    if (last_variance_count_ != data_.size()) {
      last_variance_count_ = data_.size();
      T mean = GetMean();
      variance_ = 0;
      for (const auto& sample : data_) {
        T diff = (sample - mean);
        variance_ += diff * diff;
      }
      assert(last_variance_count_ != 0);
      variance_ /= static_cast<T>(last_variance_count_);
    }
    return variance_;
  }
  T GetStdDev() { return sqrt(static_cast<double>(GetVariance())); }
  T GetMin() {
    RefreshMinMax();
    return min_;
  }
  T GetMax() {
    RefreshMinMax();
    return max_;
  }

  std::string AsString() {
    std::stringstream ss;
    ss << (GetMean() >= 0 ? GetMean() : -1) << ", "
       << (GetStdDev() >= 0 ? GetStdDev() : -1);
    return ss.str();
  }

  void Log(const std::string& units) {
    BWE_TEST_LOGGING_LOG5("", "%f %s\t+/-%f\t[%f,%f]", GetMean(), units.c_str(),
                          GetStdDev(), GetMin(), GetMax());
  }

 private:
  void RefreshMinMax() {
    if (last_minmax_count_ != data_.size()) {
      last_minmax_count_ = data_.size();
      min_ = max_ = 0;
      if (data_.empty()) {
        return;
      }
      typename std::vector<T>::const_iterator it = data_.begin();
      min_ = max_ = *it;
      while (++it != data_.end()) {
        min_ = std::min(min_, *it);
        max_ = std::max(max_, *it);
      }
    }
  }

  std::vector<T> data_;
  typename std::vector<T>::size_type last_mean_count_;
  typename std::vector<T>::size_type last_variance_count_;
  typename std::vector<T>::size_type last_minmax_count_;
  T mean_;
  T variance_;
  T min_;
  T max_;
};

bool IsTimeSorted(const Packets& packets);

class PacketProcessor;

enum ProcessorType { kSender, kReceiver, kRegular };

class PacketProcessorListener {
 public:
  virtual ~PacketProcessorListener() {}

  virtual void AddPacketProcessor(PacketProcessor* processor,
                                  ProcessorType type) = 0;
  virtual void RemovePacketProcessor(PacketProcessor* processor) = 0;
};

class PacketProcessor {
 public:
  PacketProcessor(PacketProcessorListener* listener,
                  int flow_id,
                  ProcessorType type);
  PacketProcessor(PacketProcessorListener* listener,
                  const FlowIds& flow_ids,
                  ProcessorType type);
  virtual ~PacketProcessor();

  // Called after each simulation batch to allow the processor to plot any
  // internal data.
  virtual void Plot(int64_t timestamp_ms) {}

  // Run simulation for |time_ms| milliseconds, consuming packets from, and
  // producing packets into in_out. The outgoing packet list must be sorted on
  // |send_time_us_|. The simulation time |time_ms| is optional to use.
  virtual void RunFor(int64_t time_ms, Packets* in_out) = 0;

  const FlowIds& flow_ids() const { return flow_ids_; }

  uint32_t packets_per_second() const;
  uint32_t bits_per_second() const;

 protected:
  RateCounter rate_counter_;

 private:
  PacketProcessorListener* listener_;
  const FlowIds flow_ids_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PacketProcessor);
};

class RateCounterFilter : public PacketProcessor {
 public:
  RateCounterFilter(PacketProcessorListener* listener,
                    int flow_id,
                    const char* name,
                    const std::string& algorithm_name);
  RateCounterFilter(PacketProcessorListener* listener,
                    const FlowIds& flow_ids,
                    const char* name,
                    const std::string& algorithm_name);
  RateCounterFilter(PacketProcessorListener* listener,
                    const FlowIds& flow_ids,
                    const char* name,
                    int64_t start_plotting_time_ms,
                    const std::string& algorithm_name);
  ~RateCounterFilter() override;

  void LogStats();
  Stats<double> GetBitrateStats() const;
  void Plot(int64_t timestamp_ms) override;
  void RunFor(int64_t time_ms, Packets* in_out) override;

 private:
  Stats<double> packets_per_second_stats_;
  Stats<double> kbps_stats_;
  int64_t start_plotting_time_ms_;
  int flow_id_ = 0;
  std::string name_;
  // Algorithm name if single flow, Total link utilization if all flows.
  std::string algorithm_name_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RateCounterFilter);
};

class LossFilter : public PacketProcessor {
 public:
  LossFilter(PacketProcessorListener* listener, int flow_id);
  LossFilter(PacketProcessorListener* listener, const FlowIds& flow_ids);
  ~LossFilter() override {}

  void SetLoss(float loss_percent);
  void RunFor(int64_t time_ms, Packets* in_out) override;

 private:
  Random random_;
  float loss_fraction_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(LossFilter);
};

class DelayFilter : public PacketProcessor {
 public:
  DelayFilter(PacketProcessorListener* listener, int flow_id);
  DelayFilter(PacketProcessorListener* listener, const FlowIds& flow_ids);
  ~DelayFilter() override {}

  void SetOneWayDelayMs(int64_t one_way_delay_ms);
  void RunFor(int64_t time_ms, Packets* in_out) override;

 private:
  int64_t one_way_delay_us_;
  int64_t last_send_time_us_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DelayFilter);
};

class JitterFilter : public PacketProcessor {
 public:
  JitterFilter(PacketProcessorListener* listener, int flow_id);
  JitterFilter(PacketProcessorListener* listener, const FlowIds& flow_ids);
  ~JitterFilter() override {}

  void SetMaxJitter(int64_t stddev_jitter_ms);
  void RunFor(int64_t time_ms, Packets* in_out) override;
  void set_reorderdering(bool reordering) { reordering_ = reordering; }
  int64_t MeanUs();

 private:
  Random random_;
  int64_t stddev_jitter_us_;
  int64_t last_send_time_us_;
  bool reordering_;  // False by default.

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(JitterFilter);
};

// Reorders two consecutive packets with a probability of reorder_percent.
class ReorderFilter : public PacketProcessor {
 public:
  ReorderFilter(PacketProcessorListener* listener, int flow_id);
  ReorderFilter(PacketProcessorListener* listener, const FlowIds& flow_ids);
  ~ReorderFilter() override {}

  void SetReorder(float reorder_percent);
  void RunFor(int64_t time_ms, Packets* in_out) override;

 private:
  Random random_;
  float reorder_fraction_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ReorderFilter);
};

// Apply a bitrate choke with an infinite queue on the packet stream.
class ChokeFilter : public PacketProcessor {
 public:
  ChokeFilter(PacketProcessorListener* listener, int flow_id);
  ChokeFilter(PacketProcessorListener* listener, const FlowIds& flow_ids);
  ~ChokeFilter() override;

  void set_capacity_kbps(uint32_t kbps);
  void set_max_delay_ms(int64_t max_queueing_delay_ms);

  uint32_t capacity_kbps();

  void RunFor(int64_t time_ms, Packets* in_out) override;

  Stats<double> GetDelayStats() const;

 private:
  uint32_t capacity_kbps_;
  int64_t last_send_time_us_;
  std::unique_ptr<DelayCapHelper> delay_cap_helper_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ChokeFilter);
};

class TraceBasedDeliveryFilter : public PacketProcessor {
 public:
  TraceBasedDeliveryFilter(PacketProcessorListener* listener, int flow_id);
  TraceBasedDeliveryFilter(PacketProcessorListener* listener,
                           const FlowIds& flow_ids);
  TraceBasedDeliveryFilter(PacketProcessorListener* listener,
                           int flow_id,
                           const char* name);
  ~TraceBasedDeliveryFilter() override;

  // The file should contain nanosecond timestamps corresponding to the time
  // when the network can accept another packet. The timestamps should be
  // separated by new lines, e.g., "100000000\n125000000\n321000000\n..."
  bool Init(const std::string& filename);
  void Plot(int64_t timestamp_ms) override;
  void RunFor(int64_t time_ms, Packets* in_out) override;

  void set_max_delay_ms(int64_t max_delay_ms);
  Stats<double> GetDelayStats() const;
  Stats<double> GetBitrateStats() const;

 private:
  void ProceedToNextSlot();

  typedef std::vector<int64_t> TimeList;
  int64_t current_offset_us_;
  TimeList delivery_times_us_;
  TimeList::const_iterator next_delivery_it_;
  int64_t local_time_us_;
  std::unique_ptr<RateCounter> rate_counter_;
  std::string name_;
  std::unique_ptr<DelayCapHelper> delay_cap_helper_;
  Stats<double> packets_per_second_stats_;
  Stats<double> kbps_stats_;

  RTC_DISALLOW_COPY_AND_ASSIGN(TraceBasedDeliveryFilter);
};

class VideoSource {
 public:
  VideoSource(int flow_id,
              float fps,
              uint32_t kbps,
              uint32_t ssrc,
              int64_t first_frame_offset_ms);
  virtual ~VideoSource() {}

  virtual void RunFor(int64_t time_ms, Packets* in_out);

  int flow_id() const;
  virtual void SetBitrateBps(int bitrate_bps) {}
  uint32_t bits_per_second() const { return bits_per_second_; }
  uint32_t max_payload_size_bytes() const { return kMaxPayloadSizeBytes; }
  int64_t GetTimeUntilNextFrameMs() const;

 protected:
  virtual uint32_t NextFrameSize();
  virtual uint32_t NextPacketSize(uint32_t frame_size,
                                  uint32_t remaining_payload);

  const uint32_t kMaxPayloadSizeBytes;
  const uint32_t kTimestampBase;
  const double frame_period_ms_;
  uint32_t bits_per_second_;
  uint32_t frame_size_bytes_;

 private:
  Random random_;
  const int flow_id_;
  int64_t next_frame_ms_;
  int64_t next_frame_rand_ms_;
  int64_t now_ms_;
  RTPHeader prototype_header_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(VideoSource);
};

class AdaptiveVideoSource : public VideoSource {
 public:
  AdaptiveVideoSource(int flow_id,
                      float fps,
                      uint32_t kbps,
                      uint32_t ssrc,
                      int64_t first_frame_offset_ms);
  ~AdaptiveVideoSource() override {}

  void SetBitrateBps(int bitrate_bps) override;

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveVideoSource);
};

class PeriodicKeyFrameSource : public AdaptiveVideoSource {
 public:
  PeriodicKeyFrameSource(int flow_id,
                         float fps,
                         uint32_t kbps,
                         uint32_t ssrc,
                         int64_t first_frame_offset_ms,
                         int key_frame_interval);
  ~PeriodicKeyFrameSource() override {}

 protected:
  uint32_t NextFrameSize() override;
  uint32_t NextPacketSize(uint32_t frame_size,
                          uint32_t remaining_payload) override;

 private:
  int key_frame_interval_;
  uint32_t frame_counter_;
  int compensation_bytes_;
  int compensation_per_frame_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PeriodicKeyFrameSource);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FRAMEWORK_H_
