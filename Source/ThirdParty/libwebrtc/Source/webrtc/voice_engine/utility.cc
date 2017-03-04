/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/utility.h"

#include "webrtc/audio/utility/audio_frame_operations.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {

void RemixAndResample(const AudioFrame& src_frame,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame) {
  RemixAndResample(src_frame.data_, src_frame.samples_per_channel_,
                   src_frame.num_channels_, src_frame.sample_rate_hz_,
                   resampler, dst_frame);
  dst_frame->timestamp_ = src_frame.timestamp_;
  dst_frame->elapsed_time_ms_ = src_frame.elapsed_time_ms_;
  dst_frame->ntp_time_ms_ = src_frame.ntp_time_ms_;
}

void RemixAndResample(const int16_t* src_data,
                      size_t samples_per_channel,
                      size_t num_channels,
                      int sample_rate_hz,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame) {
  const int16_t* audio_ptr = src_data;
  size_t audio_ptr_num_channels = num_channels;
  int16_t downsmixed_audio[AudioFrame::kMaxDataSizeSamples];

  // Downmix before resampling.
  if (num_channels > dst_frame->num_channels_) {
    RTC_DCHECK(num_channels == 2 || num_channels == 4)
        << "num_channels: " << num_channels;
    RTC_DCHECK(dst_frame->num_channels_ == 1 || dst_frame->num_channels_ == 2)
        << "dst_frame->num_channels_: " << dst_frame->num_channels_;

    AudioFrameOperations::DownmixChannels(
        src_data, num_channels, samples_per_channel, dst_frame->num_channels_,
        downsmixed_audio);
    audio_ptr = downsmixed_audio;
    audio_ptr_num_channels = dst_frame->num_channels_;
  }

  if (resampler->InitializeIfNeeded(sample_rate_hz, dst_frame->sample_rate_hz_,
                                    audio_ptr_num_channels) == -1) {
    RTC_FATAL() << "InitializeIfNeeded failed: sample_rate_hz = " << sample_rate_hz
            << ", dst_frame->sample_rate_hz_ = " << dst_frame->sample_rate_hz_
            << ", audio_ptr_num_channels = " << audio_ptr_num_channels;
  }

  const size_t src_length = samples_per_channel * audio_ptr_num_channels;
  int out_length = resampler->Resample(audio_ptr, src_length, dst_frame->data_,
                                       AudioFrame::kMaxDataSizeSamples);
  if (out_length == -1) {
    RTC_FATAL() << "Resample failed: audio_ptr = " << audio_ptr
            << ", src_length = " << src_length
            << ", dst_frame->data_ = " << dst_frame->data_;
  }
  dst_frame->samples_per_channel_ = out_length / audio_ptr_num_channels;

  // Upmix after resampling.
  if (num_channels == 1 && dst_frame->num_channels_ == 2) {
    // The audio in dst_frame really is mono at this point; MonoToStereo will
    // set this back to stereo.
    dst_frame->num_channels_ = 1;
    AudioFrameOperations::MonoToStereo(dst_frame);
  }
}

void MixWithSat(int16_t target[],
                size_t target_channel,
                const int16_t source[],
                size_t source_channel,
                size_t source_len) {
  RTC_DCHECK_GE(target_channel, 1);
  RTC_DCHECK_LE(target_channel, 2);
  RTC_DCHECK_GE(source_channel, 1);
  RTC_DCHECK_LE(source_channel, 2);

  if (target_channel == 2 && source_channel == 1) {
    // Convert source from mono to stereo.
    int32_t left = 0;
    int32_t right = 0;
    for (size_t i = 0; i < source_len; ++i) {
      left = source[i] + target[i * 2];
      right = source[i] + target[i * 2 + 1];
      target[i * 2] = WebRtcSpl_SatW32ToW16(left);
      target[i * 2 + 1] = WebRtcSpl_SatW32ToW16(right);
    }
  } else if (target_channel == 1 && source_channel == 2) {
    // Convert source from stereo to mono.
    int32_t temp = 0;
    for (size_t i = 0; i < source_len / 2; ++i) {
      temp = ((source[i * 2] + source[i * 2 + 1]) >> 1) + target[i];
      target[i] = WebRtcSpl_SatW32ToW16(temp);
    }
  } else {
    int32_t temp = 0;
    for (size_t i = 0; i < source_len; ++i) {
      temp = source[i] + target[i];
      target[i] = WebRtcSpl_SatW32ToW16(temp);
    }
  }
}

}  // namespace voe
}  // namespace webrtc
