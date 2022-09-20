/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"

#include "rtc_base/cpu_time.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system_time.h"

namespace webrtc {

void DefaultVideoQualityAnalyzerCpuMeasurer::StartMeasuringCpuProcessTime() {
  MutexLock lock(&mutex_);
  cpu_time_ -= rtc::GetProcessCpuTimeNanos();
  wallclock_time_ -= rtc::SystemTimeNanos();
}

void DefaultVideoQualityAnalyzerCpuMeasurer::StopMeasuringCpuProcessTime() {
  MutexLock lock(&mutex_);
  cpu_time_ += rtc::GetProcessCpuTimeNanos();
  wallclock_time_ += rtc::SystemTimeNanos();
}

void DefaultVideoQualityAnalyzerCpuMeasurer::StartExcludingCpuThreadTime() {
  MutexLock lock(&mutex_);
  cpu_time_ += rtc::GetThreadCpuTimeNanos();
}

void DefaultVideoQualityAnalyzerCpuMeasurer::StopExcludingCpuThreadTime() {
  MutexLock lock(&mutex_);
  cpu_time_ -= rtc::GetThreadCpuTimeNanos();
}

double DefaultVideoQualityAnalyzerCpuMeasurer::GetCpuUsagePercent() {
  MutexLock lock(&mutex_);
  return static_cast<double>(cpu_time_) / wallclock_time_ * 100.0;
}

}  // namespace webrtc
