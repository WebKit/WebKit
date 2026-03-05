/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/numerics/sample_stats.h"

#include <cmath>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

double SampleStats<double>::Max() {
  if (IsEmpty())
    return INFINITY;
  return GetMax();
}

double SampleStats<double>::Mean() {
  if (IsEmpty())
    return 0;
  return GetAverage();
}

double SampleStats<double>::Median() {
  return Quantile(0.5);
}

double SampleStats<double>::Quantile(double quantile) {
  if (IsEmpty())
    return 0;
  return GetPercentile(quantile);
}

double SampleStats<double>::Min() {
  if (IsEmpty())
    return -INFINITY;
  return GetMin();
}

double SampleStats<double>::Variance() {
  if (IsEmpty())
    return 0;
  return GetVariance();
}

double SampleStats<double>::StandardDeviation() {
  return sqrt(Variance());
}

int SampleStats<double>::Count() {
  return static_cast<int>(GetSamples().size());
}

void SampleStats<TimeDelta>::AddSample(TimeDelta delta, Timestamp time) {
  RTC_DCHECK(delta.IsFinite());
  stats_.AddSample({.value = delta.seconds<double>(), .time = time});
}

void SampleStats<TimeDelta>::AddSample(TimeDelta delta) {
  RTC_DCHECK(delta.IsFinite());
  AddSample(delta, Clock::GetRealTimeClock()->CurrentTime());
}

void SampleStats<TimeDelta>::AddSampleMs(double delta_ms) {
  AddSample(TimeDelta::Millis(delta_ms));
}
void SampleStats<TimeDelta>::AddSamples(const SampleStats<TimeDelta>& other) {
  stats_.AddSamples(other.stats_);
}

bool SampleStats<TimeDelta>::IsEmpty() {
  return stats_.IsEmpty();
}

TimeDelta SampleStats<TimeDelta>::Max() {
  return TimeDelta::Seconds(stats_.Max());
}

TimeDelta SampleStats<TimeDelta>::Mean() {
  return TimeDelta::Seconds(stats_.Mean());
}

TimeDelta SampleStats<TimeDelta>::Median() {
  return Quantile(0.5);
}

TimeDelta SampleStats<TimeDelta>::Quantile(double quantile) {
  return TimeDelta::Seconds(stats_.Quantile(quantile));
}

TimeDelta SampleStats<TimeDelta>::Min() {
  return TimeDelta::Seconds(stats_.Min());
}

TimeDelta SampleStats<TimeDelta>::Variance() {
  return TimeDelta::Seconds(stats_.Variance());
}

TimeDelta SampleStats<TimeDelta>::StandardDeviation() {
  return TimeDelta::Seconds(stats_.StandardDeviation());
}

int SampleStats<TimeDelta>::Count() {
  return stats_.Count();
}

void SampleStats<DataRate>::AddSample(DataRate rate, Timestamp time) {
  stats_.AddSample({.value = rate.bps<double>(), .time = time});
}

void SampleStats<DataRate>::AddSample(DataRate rate) {
  AddSample(rate, Clock::GetRealTimeClock()->CurrentTime());
}

void SampleStats<DataRate>::AddSampleBps(double rate_bps) {
  AddSample(DataRate::BitsPerSec(rate_bps));
}

void SampleStats<DataRate>::AddSamples(const SampleStats<DataRate>& other) {
  stats_.AddSamples(other.stats_);
}

bool SampleStats<DataRate>::IsEmpty() {
  return stats_.IsEmpty();
}

DataRate SampleStats<DataRate>::Max() {
  return DataRate::BitsPerSec(stats_.Max());
}

DataRate SampleStats<DataRate>::Mean() {
  return DataRate::BitsPerSec(stats_.Mean());
}

DataRate SampleStats<DataRate>::Median() {
  return Quantile(0.5);
}

DataRate SampleStats<DataRate>::Quantile(double quantile) {
  return DataRate::BitsPerSec(stats_.Quantile(quantile));
}

DataRate SampleStats<DataRate>::Min() {
  return DataRate::BitsPerSec(stats_.Min());
}

DataRate SampleStats<DataRate>::Variance() {
  return DataRate::BitsPerSec(stats_.Variance());
}

DataRate SampleStats<DataRate>::StandardDeviation() {
  return DataRate::BitsPerSec(stats_.StandardDeviation());
}

int SampleStats<DataRate>::Count() {
  return stats_.Count();
}

}  // namespace webrtc
