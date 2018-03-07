/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/call_stats.h"

#include <algorithm>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
// Time interval for updating the observers.
const int64_t kUpdateIntervalMs = 1000;
// Weight factor to apply to the average rtt.
const float kWeightFactor = 0.3f;

void RemoveOldReports(int64_t now, std::list<CallStats::RttTime>* reports) {
  // A rtt report is considered valid for this long.
  const int64_t kRttTimeoutMs = 1500;
  while (!reports->empty() &&
         (now - reports->front().time) > kRttTimeoutMs) {
    reports->pop_front();
  }
}

int64_t GetMaxRttMs(std::list<CallStats::RttTime>* reports) {
  if (reports->empty())
    return -1;
  int64_t max_rtt_ms = 0;
  for (const CallStats::RttTime& rtt_time : *reports)
    max_rtt_ms = std::max(rtt_time.rtt, max_rtt_ms);
  return max_rtt_ms;
}

int64_t GetAvgRttMs(std::list<CallStats::RttTime>* reports) {
  if (reports->empty()) {
    return -1;
  }
  int64_t sum = 0;
  for (std::list<CallStats::RttTime>::const_iterator it = reports->begin();
       it != reports->end(); ++it) {
    sum += it->rtt;
  }
  return sum / reports->size();
}

void UpdateAvgRttMs(std::list<CallStats::RttTime>* reports, int64_t* avg_rtt) {
  int64_t cur_rtt_ms = GetAvgRttMs(reports);
  if (cur_rtt_ms == -1) {
    // Reset.
    *avg_rtt = -1;
    return;
  }
  if (*avg_rtt == -1) {
    // Initialize.
    *avg_rtt = cur_rtt_ms;
    return;
  }
  *avg_rtt = *avg_rtt * (1.0f - kWeightFactor) + cur_rtt_ms * kWeightFactor;
}
}  // namespace

class RtcpObserver : public RtcpRttStats {
 public:
  explicit RtcpObserver(CallStats* owner) : owner_(owner) {}
  virtual ~RtcpObserver() {}

  virtual void OnRttUpdate(int64_t rtt) {
    owner_->OnRttUpdate(rtt);
  }

  // Returns the average RTT.
  virtual int64_t LastProcessedRtt() const {
    return owner_->avg_rtt_ms();
  }

 private:
  CallStats* owner_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcpObserver);
};

CallStats::CallStats(Clock* clock)
    : clock_(clock),
      rtcp_rtt_stats_(new RtcpObserver(this)),
      last_process_time_(clock_->TimeInMilliseconds()),
      max_rtt_ms_(-1),
      avg_rtt_ms_(-1),
      sum_avg_rtt_ms_(0),
      num_avg_rtt_(0),
      time_of_first_rtt_ms_(-1) {}

CallStats::~CallStats() {
  RTC_DCHECK(observers_.empty());
  UpdateHistograms();
}

int64_t CallStats::TimeUntilNextProcess() {
  return last_process_time_ + kUpdateIntervalMs - clock_->TimeInMilliseconds();
}

void CallStats::Process() {
  rtc::CritScope cs(&crit_);
  int64_t now = clock_->TimeInMilliseconds();
  if (now < last_process_time_ + kUpdateIntervalMs)
    return;

  last_process_time_ = now;

  RemoveOldReports(now, &reports_);
  max_rtt_ms_ = GetMaxRttMs(&reports_);
  UpdateAvgRttMs(&reports_, &avg_rtt_ms_);

  // If there is a valid rtt, update all observers with the max rtt.
  if (max_rtt_ms_ >= 0) {
    RTC_DCHECK_GE(avg_rtt_ms_, 0);
    for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnRttUpdate(avg_rtt_ms_, max_rtt_ms_);
    }
    // Sum for Histogram of average RTT reported over the entire call.
    sum_avg_rtt_ms_ += avg_rtt_ms_;
    ++num_avg_rtt_;
  }
}

int64_t CallStats::avg_rtt_ms() const {
  rtc::CritScope cs(&crit_);
  return avg_rtt_ms_;
}

RtcpRttStats* CallStats::rtcp_rtt_stats() const {
  return rtcp_rtt_stats_.get();
}

void CallStats::RegisterStatsObserver(CallStatsObserver* observer) {
  rtc::CritScope cs(&crit_);
  for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer)
      return;
  }
  observers_.push_back(observer);
}

void CallStats::DeregisterStatsObserver(CallStatsObserver* observer) {
  rtc::CritScope cs(&crit_);
  for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      return;
    }
  }
}

void CallStats::OnRttUpdate(int64_t rtt) {
  rtc::CritScope cs(&crit_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  reports_.push_back(RttTime(rtt, now_ms));
  if (time_of_first_rtt_ms_ == -1)
    time_of_first_rtt_ms_ = now_ms;
}

void CallStats::UpdateHistograms() {
  rtc::CritScope cs(&crit_);
  if (time_of_first_rtt_ms_ == -1 || num_avg_rtt_ < 1)
    return;

  int64_t elapsed_sec =
      (clock_->TimeInMilliseconds() - time_of_first_rtt_ms_) / 1000;
  if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
    int64_t avg_rtt_ms = (sum_avg_rtt_ms_ + num_avg_rtt_ / 2) / num_avg_rtt_;
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.AverageRoundTripTimeInMilliseconds", avg_rtt_ms);
  }
}

}  // namespace webrtc
