/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/remix_resample.h"

#include <array>

#include "api/audio/audio_frame.h"
#include "audio/utility/audio_frame_operations.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace voe {

void RemixAndResample(const AudioFrame& src_frame,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame) {
  RemixAndResample(src_frame.data_view(), src_frame.sample_rate_hz_, resampler,
                   dst_frame);
  dst_frame->timestamp_ = src_frame.timestamp_;
  dst_frame->elapsed_time_ms_ = src_frame.elapsed_time_ms_;
  dst_frame->ntp_time_ms_ = src_frame.ntp_time_ms_;
  dst_frame->packet_infos_ = src_frame.packet_infos_;
}

void RemixAndResample(InterleavedView<const int16_t> src_data,
                      int sample_rate_hz,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame) {
  // The `samples_per_channel_` members must have been set correctly based on
  // the associated sample rate and the assumed 10ms buffer size.
  // TODO(tommi): Remove the `sample_rate_hz` param.
  RTC_DCHECK_EQ(SampleRateToDefaultChannelSize(sample_rate_hz),
                src_data.samples_per_channel());
  RTC_DCHECK_EQ(SampleRateToDefaultChannelSize(dst_frame->sample_rate_hz_),
                dst_frame->samples_per_channel());

  // Temporary buffer in case downmixing is required.
  std::array<int16_t, AudioFrame::kMaxDataSizeSamples> downmixed_audio;

  // Downmix before resampling.
  if (src_data.num_channels() > dst_frame->num_channels_) {
    RTC_DCHECK(src_data.num_channels() == 2 || src_data.num_channels() == 4)
        << "num_channels: " << src_data.num_channels();
    RTC_DCHECK(dst_frame->num_channels_ == 1 || dst_frame->num_channels_ == 2)
        << "dst_frame->num_channels_: " << dst_frame->num_channels_;

    InterleavedView<int16_t> downmixed(downmixed_audio.data(),
                                       src_data.samples_per_channel(),
                                       dst_frame->num_channels_);
    AudioFrameOperations::DownmixChannels(src_data, downmixed);
    src_data = downmixed;
  }

  // TODO(yujo): for muted input frames, don't resample. Either 1) allow
  // resampler to return output length without doing the resample, so we know
  // how much to zero here; or 2) make resampler accept a hint that the input is
  // zeroed.

  // Stash away the originally requested number of channels. Then provide
  // `dst_frame` as a target buffer with the same number of channels as the
  // source.
  auto original_dst_number_of_channels = dst_frame->num_channels_;
  int out_length = resampler->Resample(
      src_data, dst_frame->mutable_data(dst_frame->samples_per_channel_,
                                        src_data.num_channels()));
  RTC_CHECK_NE(out_length, -1) << "src_data.size=" << src_data.size();
  RTC_DCHECK_EQ(dst_frame->samples_per_channel(),
                out_length / src_data.num_channels());

  // Upmix after resampling.
  if (src_data.num_channels() == 1 && original_dst_number_of_channels == 2) {
    // The audio in dst_frame really is mono at this point; MonoToStereo will
    // set this back to stereo.
    RTC_DCHECK_EQ(dst_frame->num_channels_, 1);
    AudioFrameOperations::UpmixChannels(2, dst_frame);
  }
}

}  // namespace voe
}  // namespace webrtc
