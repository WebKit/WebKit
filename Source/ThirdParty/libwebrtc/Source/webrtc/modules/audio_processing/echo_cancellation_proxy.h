/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_PROXY_H_
#define MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_PROXY_H_

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

// Class for temporarily redirecting AEC2 configuration to a new API.
class EchoCancellationProxy : public EchoCancellation {
 public:
  EchoCancellationProxy(AudioProcessing* audio_processing,
                        EchoCancellation* echo_cancellation);
  ~EchoCancellationProxy() override;

  int Enable(bool enable) override;
  bool is_enabled() const override;
  int enable_drift_compensation(bool enable) override;
  bool is_drift_compensation_enabled() const override;
  void set_stream_drift_samples(int drift) override;
  int stream_drift_samples() const override;
  int set_suppression_level(SuppressionLevel level) override;
  SuppressionLevel suppression_level() const override;
  bool stream_has_echo() const override;
  int enable_metrics(bool enable) override;
  bool are_metrics_enabled() const override;
  int GetMetrics(Metrics* metrics) override;
  int enable_delay_logging(bool enable) override;
  bool is_delay_logging_enabled() const override;
  int GetDelayMetrics(int* median, int* std) override;
  int GetDelayMetrics(int* median,
                      int* std,
                      float* fraction_poor_delays) override;
  struct AecCore* aec_core() const override;

 private:
  AudioProcessing* audio_processing_;
  EchoCancellation* echo_cancellation_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoCancellationProxy);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_PROXY_H_
