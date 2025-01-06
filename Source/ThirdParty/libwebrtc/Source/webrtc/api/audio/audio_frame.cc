/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/audio_frame.h"

#include <string.h>

#include <cstdint>
#include <optional>

#include "api/array_view.h"
#include "api/audio/audio_view.h"
#include "api/audio/channel_layout.h"
#include "api/rtp_packet_infos.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

AudioFrame::AudioFrame() {
  // Visual Studio doesn't like this in the class definition.
  static_assert(sizeof(data_) == kMaxDataSizeBytes, "kMaxDataSizeBytes");
}

AudioFrame::AudioFrame(int sample_rate_hz,
                       size_t num_channels,
                       ChannelLayout layout /*= CHANNEL_LAYOUT_UNSUPPORTED*/)
    : samples_per_channel_(SampleRateToDefaultChannelSize(sample_rate_hz)),
      sample_rate_hz_(sample_rate_hz),
      num_channels_(num_channels),
      channel_layout_(layout == CHANNEL_LAYOUT_UNSUPPORTED
                          ? GuessChannelLayout(num_channels)
                          : layout) {
  RTC_DCHECK_LE(num_channels_, kMaxConcurrentChannels);
  RTC_DCHECK_GT(sample_rate_hz_, 0);
  RTC_DCHECK_GT(samples_per_channel_, 0u);
}

void AudioFrame::Reset() {
  ResetWithoutMuting();
  muted_ = true;
}

void AudioFrame::ResetWithoutMuting() {
  // TODO(wu): Zero is a valid value for `timestamp_`. We should initialize
  // to an invalid value, or add a new member to indicate invalidity.
  timestamp_ = 0;
  elapsed_time_ms_ = -1;
  ntp_time_ms_ = -1;
  samples_per_channel_ = 0;
  sample_rate_hz_ = 0;
  num_channels_ = 0;
  channel_layout_ = CHANNEL_LAYOUT_NONE;
  speech_type_ = kUndefined;
  vad_activity_ = kVadUnknown;
  profile_timestamp_ms_ = 0;
  packet_infos_ = RtpPacketInfos();
  absolute_capture_timestamp_ms_ = std::nullopt;
}

void AudioFrame::UpdateFrame(uint32_t timestamp,
                             const int16_t* data,
                             size_t samples_per_channel,
                             int sample_rate_hz,
                             SpeechType speech_type,
                             VADActivity vad_activity,
                             size_t num_channels) {
  RTC_CHECK_LE(num_channels, kMaxConcurrentChannels);
  timestamp_ = timestamp;
  samples_per_channel_ = samples_per_channel;
  sample_rate_hz_ = sample_rate_hz;
  speech_type_ = speech_type;
  vad_activity_ = vad_activity;
  num_channels_ = num_channels;
  channel_layout_ = GuessChannelLayout(num_channels);
  if (channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED) {
    RTC_DCHECK_EQ(num_channels, ChannelLayoutToChannelCount(channel_layout_));
  }

  const size_t length = samples_per_channel * num_channels;
  RTC_CHECK_LE(length, data_.size());
  if (data != nullptr) {
    memcpy(data_.data(), data, sizeof(int16_t) * length);
    muted_ = false;
  } else {
    muted_ = true;
  }
}

void AudioFrame::CopyFrom(const AudioFrame& src) {
  if (this == &src)
    return;

  if (muted_ && !src.muted()) {
    // TODO: bugs.webrtc.org/5647 - Since the default value for `muted_` is
    // false and `data_` may still be uninitialized (because we don't initialize
    // data_ as part of construction), we clear the full buffer here before
    // copying over new values. If we don't, msan might complain in some tests.
    // Consider locking down construction, avoiding the default constructor and
    // prefering construction that initializes all state.
    ClearSamples(data_);
  }

  timestamp_ = src.timestamp_;
  elapsed_time_ms_ = src.elapsed_time_ms_;
  ntp_time_ms_ = src.ntp_time_ms_;
  packet_infos_ = src.packet_infos_;
  muted_ = src.muted();
  samples_per_channel_ = src.samples_per_channel_;
  sample_rate_hz_ = src.sample_rate_hz_;
  speech_type_ = src.speech_type_;
  vad_activity_ = src.vad_activity_;
  num_channels_ = src.num_channels_;
  channel_layout_ = src.channel_layout_;
  absolute_capture_timestamp_ms_ = src.absolute_capture_timestamp_ms();

  auto data = src.data_view();
  RTC_CHECK_LE(data.size(), data_.size());
  if (!muted_ && !data.empty()) {
    memcpy(&data_[0], &data[0], sizeof(int16_t) * data.size());
  }
}

void AudioFrame::UpdateProfileTimeStamp() {
  profile_timestamp_ms_ = rtc::TimeMillis();
}

int64_t AudioFrame::ElapsedProfileTimeMs() const {
  if (profile_timestamp_ms_ == 0) {
    // Profiling has not been activated.
    return -1;
  }
  return rtc::TimeSince(profile_timestamp_ms_);
}

const int16_t* AudioFrame::data() const {
  return muted_ ? zeroed_data().begin() : data_.data();
}

InterleavedView<const int16_t> AudioFrame::data_view() const {
  // If you get a nullptr from `data_view()`, it's likely because the
  // samples_per_channel_ and/or num_channels_ members haven't been properly
  // set. Since `data_view()` returns an InterleavedView<> (which internally
  // uses rtc::ArrayView<>), we inherit the behavior in InterleavedView when the
  // view size is 0 that ArrayView<>::data() returns nullptr. So, even when an
  // AudioFrame is muted and we want to return `zeroed_data()`, if
  // samples_per_channel_ or  num_channels_ is 0, the view will point to
  // nullptr.
  return InterleavedView<const int16_t>(muted_ ? &zeroed_data()[0] : &data_[0],
                                        samples_per_channel_, num_channels_);
}

int16_t* AudioFrame::mutable_data() {
  // TODO: bugs.webrtc.org/5647 - Can we skip zeroing the buffer?
  // Consider instead if we should rather zero the buffer when `muted_` is set
  // to `true`.
  if (muted_) {
    ClearSamples(data_);
    muted_ = false;
  }
  return &data_[0];
}

InterleavedView<int16_t> AudioFrame::mutable_data(size_t samples_per_channel,
                                                  size_t num_channels) {
  const size_t total_samples = samples_per_channel * num_channels;
  RTC_CHECK_LE(total_samples, data_.size());
  RTC_CHECK_LE(num_channels, kMaxConcurrentChannels);
  // Sanity check for valid argument values during development.
  // If `samples_per_channel` is < `num_channels` but larger than 0,
  // then chances are the order of arguments is incorrect.
  RTC_DCHECK((samples_per_channel == 0 && num_channels == 0) ||
             num_channels <= samples_per_channel)
      << "samples_per_channel=" << samples_per_channel
      << "num_channels=" << num_channels;

  // TODO: bugs.webrtc.org/5647 - Can we skip zeroing the buffer?
  // Consider instead if we should rather zero the whole buffer when `muted_` is
  // set to `true`.
  if (muted_) {
    ClearSamples(data_, total_samples);
    muted_ = false;
  }
  samples_per_channel_ = samples_per_channel;
  num_channels_ = num_channels;
  return InterleavedView<int16_t>(&data_[0], samples_per_channel, num_channels);
}

void AudioFrame::Mute() {
  muted_ = true;
}

bool AudioFrame::muted() const {
  return muted_;
}

void AudioFrame::SetLayoutAndNumChannels(ChannelLayout layout,
                                         size_t num_channels) {
  channel_layout_ = layout;
  num_channels_ = num_channels;
#if RTC_DCHECK_IS_ON
  // Do a sanity check that the layout and num_channels match.
  // If this lookup yield 0u, then the layout is likely CHANNEL_LAYOUT_DISCRETE.
  auto expected_num_channels = ChannelLayoutToChannelCount(layout);
  if (expected_num_channels) {  // If expected_num_channels is 0
    RTC_DCHECK_EQ(expected_num_channels, num_channels_);
  }
#endif
  RTC_CHECK_LE(samples_per_channel_ * num_channels_, data_.size());
}

void AudioFrame::SetSampleRateAndChannelSize(int sample_rate) {
  sample_rate_hz_ = sample_rate;
  // We could call `AudioProcessing::GetFrameSize()` here, but that requires
  // adding a dependency on the ":audio_processing" build target, which can
  // complicate the dependency tree. Some refactoring is probably in order to
  // get some consistency around this since there are many places across the
  // code that assume this default buffer size.
  samples_per_channel_ = SampleRateToDefaultChannelSize(sample_rate_hz_);
}

// static
rtc::ArrayView<const int16_t> AudioFrame::zeroed_data() {
  static int16_t* null_data = new int16_t[kMaxDataSizeSamples]();
  return rtc::ArrayView<const int16_t>(null_data, kMaxDataSizeSamples);
}

}  // namespace webrtc
