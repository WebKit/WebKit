/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_resampler.h"

#include <string.h>

#include "api/audio/audio_frame.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace acm2 {

ACMResampler::ACMResampler() {}

ACMResampler::~ACMResampler() {}

int ACMResampler::Resample10Msec(const int16_t* in_audio,
                                 int in_freq_hz,
                                 int out_freq_hz,
                                 size_t num_audio_channels,
                                 size_t out_capacity_samples,
                                 int16_t* out_audio) {
  InterleavedView<const int16_t> src(
      in_audio, SampleRateToDefaultChannelSize(in_freq_hz), num_audio_channels);
  InterleavedView<int16_t> dst(out_audio,
                               SampleRateToDefaultChannelSize(out_freq_hz),
                               num_audio_channels);
  RTC_DCHECK_GE(out_capacity_samples, dst.size());
  if (in_freq_hz == out_freq_hz) {
    if (out_capacity_samples < src.data().size()) {
      RTC_DCHECK_NOTREACHED();
      return -1;
    }
    CopySamples(dst, src);
    RTC_DCHECK_EQ(dst.samples_per_channel(), src.samples_per_channel());
    return static_cast<int>(dst.samples_per_channel());
  }

  int out_length = resampler_.Resample(src, dst);
  if (out_length == -1) {
    RTC_LOG(LS_ERROR) << "Resample(" << in_audio << ", " << src.data().size()
                      << ", " << out_audio << ", " << out_capacity_samples
                      << ") failed.";
    return -1;
  }
  RTC_DCHECK_EQ(out_length, dst.size());
  RTC_DCHECK_EQ(out_length / num_audio_channels, dst.samples_per_channel());
  return static_cast<int>(dst.samples_per_channel());
}

}  // namespace acm2
}  // namespace webrtc
