/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ADAPTIVE_FIR_FILTER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ADAPTIVE_FIR_FILTER_H_

#include <array>
#include <memory>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"
#include "webrtc/modules/audio_processing/aec3/fft_data.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace aec3 {
// Adapts the filter partitions.
void AdaptPartitions(const FftBuffer& X_buffer,
                     const FftData& G,
                     rtc::ArrayView<FftData> H);
#if defined(WEBRTC_ARCH_X86_FAMILY)
void AdaptPartitions_SSE2(const FftBuffer& X_buffer,
                          const FftData& G,
                          rtc::ArrayView<FftData> H);
#endif

// Produces the filter output.
void ApplyFilter(const FftBuffer& X_buffer,
                 rtc::ArrayView<const FftData> H,
                 FftData* S);
#if defined(WEBRTC_ARCH_X86_FAMILY)
void ApplyFilter_SSE2(const FftBuffer& X_buffer,
                      rtc::ArrayView<const FftData> H,
                      FftData* S);
#endif

}  // namespace aec3

// Provides a frequency domain adaptive filter functionality.
class AdaptiveFirFilter {
 public:
  AdaptiveFirFilter(size_t size_partitions,
                    bool use_filter_statistics,
                    Aec3Optimization optimization,
                    ApmDataDumper* data_dumper);

  ~AdaptiveFirFilter();

  // Produces the output of the filter.
  void Filter(const FftBuffer& X_buffer, FftData* S) const;

  // Adapts the filter.
  void Adapt(const FftBuffer& X_buffer, const FftData& G);

  // Receives reports that known echo path changes have occured and adjusts
  // the filter adaptation accordingly.
  void HandleEchoPathChange();

  // Returns the filter size.
  size_t SizePartitions() const { return H_.size(); }

  // Returns the filter based echo return loss. This method can only be used if
  // the usage of filter statistics has been specified during the creation of
  // the adaptive filter.
  const std::array<float, kFftLengthBy2Plus1>& Erl() const {
    RTC_DCHECK(erl_) << "The filter must be created with use_filter_statistics "
                        "set to true in order to be able to call retrieve the "
                        "ERL.";
    return *erl_;
  }

  // Returns the frequency responses for the filter partitions. This method can
  // only be used if the usage of filter statistics has been specified during
  // the creation of the adaptive filter.
  const std::vector<std::array<float, kFftLengthBy2Plus1>>&
  FilterFrequencyResponse() const {
    RTC_DCHECK(H2_) << "The filter must be created with use_filter_statistics "
                       "set to true in order to be able to call retrieve the "
                       "filter frequency responde.";
    return *H2_;
  }

  void DumpFilter(const char* name) {
    for (auto& H : H_) {
      data_dumper_->DumpRaw(name, H.re);
      data_dumper_->DumpRaw(name, H.im);
    }
  }

 private:
  ApmDataDumper* const data_dumper_;
  const Aec3Fft fft_;
  const Aec3Optimization optimization_;
  std::vector<FftData> H_;
  std::unique_ptr<std::vector<std::array<float, kFftLengthBy2Plus1>>> H2_;
  std::unique_ptr<std::array<float, kFftLengthBy2Plus1>> erl_;
  size_t partition_to_constrain_ = 0;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveFirFilter);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ADAPTIVE_FIR_FILTER_H_
