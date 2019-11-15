/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/performance_stats.h"

#include <algorithm>

namespace webrtc {
namespace test {
void EventRateCounter::AddEvent(Timestamp event_time) {
  if (first_time_.IsFinite())
    interval_.AddSample(event_time - last_time_);
  first_time_ = std::min(first_time_, event_time);
  last_time_ = std::max(last_time_, event_time);
  event_count_++;
}

void EventRateCounter::AddEvents(EventRateCounter other) {
  first_time_ = std::min(first_time_, other.first_time_);
  last_time_ = std::max(last_time_, other.last_time_);
  event_count_ += other.event_count_;
  interval_.AddSamples(other.interval_);
}

bool EventRateCounter::IsEmpty() const {
  return first_time_ == last_time_;
}

double EventRateCounter::Rate() const {
  if (event_count_ == 0)
    return 0;
  if (event_count_ == 1)
    return NAN;
  return (event_count_ - 1) / (last_time_ - first_time_).seconds<double>();
}

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

void SampleStats<TimeDelta>::AddSample(TimeDelta delta) {
  RTC_DCHECK(delta.IsFinite());
  stats_.AddSample(delta.seconds<double>());
}

void SampleStats<TimeDelta>::AddSampleMs(double delta_ms) {
  AddSample(TimeDelta::ms(delta_ms));
}
void SampleStats<TimeDelta>::AddSamples(const SampleStats<TimeDelta>& other) {
  stats_.AddSamples(other.stats_);
}

bool SampleStats<TimeDelta>::IsEmpty() {
  return stats_.IsEmpty();
}

TimeDelta SampleStats<TimeDelta>::Max() {
  return TimeDelta::seconds(stats_.Max());
}

TimeDelta SampleStats<TimeDelta>::Mean() {
  return TimeDelta::seconds(stats_.Mean());
}

TimeDelta SampleStats<TimeDelta>::Median() {
  return Quantile(0.5);
}

TimeDelta SampleStats<TimeDelta>::Quantile(double quantile) {
  return TimeDelta::seconds(stats_.Quantile(quantile));
}

TimeDelta SampleStats<TimeDelta>::Min() {
  return TimeDelta::seconds(stats_.Min());
}

TimeDelta SampleStats<TimeDelta>::Variance() {
  return TimeDelta::seconds(stats_.Variance());
}

TimeDelta SampleStats<TimeDelta>::StandardDeviation() {
  return TimeDelta::seconds(stats_.StandardDeviation());
}

int SampleStats<TimeDelta>::Count() {
  return stats_.Count();
}

void SampleStats<DataRate>::AddSample(DataRate sample) {
  stats_.AddSample(sample.bps<double>());
}

void SampleStats<DataRate>::AddSampleBps(double rate_bps) {
  stats_.AddSample(rate_bps);
}

void SampleStats<DataRate>::AddSamples(const SampleStats<DataRate>& other) {
  stats_.AddSamples(other.stats_);
}

bool SampleStats<DataRate>::IsEmpty() {
  return stats_.IsEmpty();
}

DataRate SampleStats<DataRate>::Max() {
  return DataRate::bps(stats_.Max());
}

DataRate SampleStats<DataRate>::Mean() {
  return DataRate::bps(stats_.Mean());
}

DataRate SampleStats<DataRate>::Median() {
  return Quantile(0.5);
}

DataRate SampleStats<DataRate>::Quantile(double quantile) {
  return DataRate::bps(stats_.Quantile(quantile));
}

DataRate SampleStats<DataRate>::Min() {
  return DataRate::bps(stats_.Min());
}

DataRate SampleStats<DataRate>::Variance() {
  return DataRate::bps(stats_.Variance());
}

DataRate SampleStats<DataRate>::StandardDeviation() {
  return DataRate::bps(stats_.StandardDeviation());
}

int SampleStats<DataRate>::Count() {
  return stats_.Count();
}

void VideoFramesStats::AddFrameInfo(const VideoFrameBuffer& frame,
                                    Timestamp at_time) {
  ++count;
  RTC_DCHECK(at_time.IsFinite());
  pixels.AddSample(frame.width() * frame.height());
  resolution.AddSample(std::max(frame.width(), frame.height()));
  frames.AddEvent(at_time);
}

void VideoFramesStats::AddStats(const VideoFramesStats& other) {
  count += other.count;
  pixels.AddSamples(other.pixels);
  resolution.AddSamples(other.resolution);
  frames.AddEvents(other.frames);
}

void VideoQualityStats::AddStats(const VideoQualityStats& other) {
  capture.AddStats(other.capture);
  render.AddStats(other.render);
  lost_count += other.lost_count;
  freeze_count += other.freeze_count;
  capture_to_decoded_delay.AddSamples(other.capture_to_decoded_delay);
  end_to_end_delay.AddSamples(other.end_to_end_delay);
  psnr.AddSamples(other.psnr);
  psnr_with_freeze.AddSamples(other.psnr_with_freeze);
  skipped_between_rendered.AddSamples(other.skipped_between_rendered);
  freeze_duration.AddSamples(other.freeze_duration);
  time_between_freezes.AddSamples(other.time_between_freezes);
}

}  // namespace test
}  // namespace webrtc
