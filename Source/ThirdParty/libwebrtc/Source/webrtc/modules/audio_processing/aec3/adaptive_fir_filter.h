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
#include "webrtc/modules/audio_processing/aec3/fft_data.h"
#include "webrtc/modules/audio_processing/aec3/render_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace aec3 {
// Computes and stores the frequency response of the filter.
void UpdateFrequencyResponse(
    rtc::ArrayView<const FftData> H,
    std::vector<std::array<float, kFftLengthBy2Plus1>>* H2);
#if defined(WEBRTC_HAS_NEON)
void UpdateFrequencyResponse_NEON(
    rtc::ArrayView<const FftData> H,
    std::vector<std::array<float, kFftLengthBy2Plus1>>* H2);
#endif
#if defined(WEBRTC_ARCH_X86_FAMILY)
void UpdateFrequencyResponse_SSE2(
    rtc::ArrayView<const FftData> H,
    std::vector<std::array<float, kFftLengthBy2Plus1>>* H2);
#endif

// Computes and stores the echo return loss estimate of the filter, which is the
// sum of the partition frequency responses.
void UpdateErlEstimator(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>& H2,
    std::array<float, kFftLengthBy2Plus1>* erl);
#if defined(WEBRTC_HAS_NEON)
void UpdateErlEstimator_NEON(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>& H2,
    std::array<float, kFftLengthBy2Plus1>* erl);
#endif
#if defined(WEBRTC_ARCH_X86_FAMILY)
void UpdateErlEstimator_SSE2(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>& H2,
    std::array<float, kFftLengthBy2Plus1>* erl);
#endif

// Adapts the filter partitions.
void AdaptPartitions(const RenderBuffer& render_buffer,
                     const FftData& G,
                     rtc::ArrayView<FftData> H);
#if defined(WEBRTC_HAS_NEON)
void AdaptPartitions_NEON(const RenderBuffer& render_buffer,
                          const FftData& G,
                          rtc::ArrayView<FftData> H);
#endif
#if defined(WEBRTC_ARCH_X86_FAMILY)
void AdaptPartitions_SSE2(const RenderBuffer& render_buffer,
                          const FftData& G,
                          rtc::ArrayView<FftData> H);
#endif

// Produces the filter output.
void ApplyFilter(const RenderBuffer& render_buffer,
                 rtc::ArrayView<const FftData> H,
                 FftData* S);
#if defined(WEBRTC_HAS_NEON)
void ApplyFilter_NEON(const RenderBuffer& render_buffer,
                      rtc::ArrayView<const FftData> H,
                      FftData* S);
#endif
#if defined(WEBRTC_ARCH_X86_FAMILY)
void ApplyFilter_SSE2(const RenderBuffer& render_buffer,
                      rtc::ArrayView<const FftData> H,
                      FftData* S);
#endif

}  // namespace aec3

// Provides a frequency domain adaptive filter functionality.
class AdaptiveFirFilter {
 public:
  AdaptiveFirFilter(size_t size_partitions,
                    Aec3Optimization optimization,
                    ApmDataDumper* data_dumper);

  ~AdaptiveFirFilter();

  // Produces the output of the filter.
  void Filter(const RenderBuffer& render_buffer, FftData* S) const;

  // Adapts the filter.
  void Adapt(const RenderBuffer& render_buffer, const FftData& G);

  // Receives reports that known echo path changes have occured and adjusts
  // the filter adaptation accordingly.
  void HandleEchoPathChange();

  // Returns the filter size.
  size_t SizePartitions() const { return H_.size(); }

  // Returns the filter based echo return loss.
  const std::array<float, kFftLengthBy2Plus1>& Erl() const { return erl_; }

  // Returns the frequency responses for the filter partitions.
  const std::vector<std::array<float, kFftLengthBy2Plus1>>&
  FilterFrequencyResponse() const {
    return H2_;
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
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2_;
  std::array<float, kFftLengthBy2Plus1> erl_;
  size_t partition_to_constrain_ = 0;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveFirFilter);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ADAPTIVE_FIR_FILTER_H_
