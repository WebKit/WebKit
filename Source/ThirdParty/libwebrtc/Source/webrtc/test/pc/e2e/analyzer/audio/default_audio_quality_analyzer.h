/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_

#include <map>
#include <string>

#include "api/stats_types.h"
#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/track_id_stream_label_map.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {

struct AudioStreamStats {
  SamplesStatsCounter expand_rate;
  SamplesStatsCounter accelerate_rate;
  SamplesStatsCounter preemptive_rate;
  SamplesStatsCounter speech_expand_rate;
  SamplesStatsCounter preferred_buffer_size_ms;
};

// TODO(bugs.webrtc.org/10430): Migrate to the new GetStats as soon as
// bugs.webrtc.org/10428 is fixed.
class DefaultAudioQualityAnalyzer : public AudioQualityAnalyzerInterface {
 public:
  void Start(std::string test_case_name,
             TrackIdStreamLabelMap* analyzer_helper) override;
  void OnStatsReports(const std::string& pc_label,
                      const StatsReports& stats_reports) override;
  void Stop() override;

  // Returns audio quality stats per stream label.
  std::map<std::string, AudioStreamStats> GetAudioStreamsStats() const;

 private:
  const std::string& GetStreamLabelFromStatsReport(
      const StatsReport* stats_report) const;
  std::string GetTestCaseName(const std::string& stream_label) const;
  void ReportResult(const std::string& metric_name,
                    const std::string& stream_label,
                    const SamplesStatsCounter& counter,
                    const std::string& unit,
                    webrtc::test::ImproveDirection improve_direction) const;

  std::string test_case_name_;
  TrackIdStreamLabelMap* analyzer_helper_;

  rtc::CriticalSection lock_;
  std::map<std::string, AudioStreamStats> streams_stats_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
