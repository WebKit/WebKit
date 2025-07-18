/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/test_utils.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/arch.h"

namespace webrtc {

void Int16FrameData::CopyFrom(const Int16FrameData& src) {
  sample_rate_hz = src.sample_rate_hz;
  view_ = InterleavedView<int16_t>(data.data(), src.samples_per_channel(),
                                   src.num_channels());
  RTC_CHECK_LE(view_.size(), kMaxDataSizeSamples);
  CopySamples(view_, src.view());
}

bool Int16FrameData::IsEqual(const Int16FrameData& frame) const {
  return samples_per_channel() == frame.samples_per_channel() &&
         num_channels() == num_channels() &&
         memcmp(data.data(), frame.data.data(),
                samples_per_channel() * num_channels() * sizeof(int16_t)) == 0;
}

void Int16FrameData::Scale(float f) {
  std::for_each(data.begin(), data.end(),
                [f](int16_t& sample) { sample = FloatS16ToS16(sample * f); });
}

void Int16FrameData::SetProperties(size_t samples_per_channel,
                                   size_t num_channels) {
  sample_rate_hz = samples_per_channel * 100;
  view_ =
      InterleavedView<int16_t>(data.data(), samples_per_channel, num_channels);
  RTC_CHECK_LE(view_.size(), kMaxDataSizeSamples);
}

void Int16FrameData::set_num_channels(size_t num_channels) {
  view_ = InterleavedView<int16_t>(data.data(), samples_per_channel(),
                                   num_channels);
  RTC_CHECK_LE(view_.size(), kMaxDataSizeSamples);
}

void Int16FrameData::FillData(int16_t value) {
  std::fill(&data[0], &data[size()], value);
}

void Int16FrameData::FillStereoData(int16_t left, int16_t right) {
  RTC_DCHECK_EQ(num_channels(), 2u);
  for (size_t i = 0; i < samples_per_channel() * 2u; i += 2u) {
    data[i] = left;
    data[i + 1] = right;
  }
}

ChannelBufferWavReader::ChannelBufferWavReader(std::unique_ptr<WavReader> file)
    : file_(std::move(file)) {}

ChannelBufferWavReader::~ChannelBufferWavReader() = default;

bool ChannelBufferWavReader::Read(ChannelBuffer<float>* buffer) {
  RTC_CHECK_EQ(file_->num_channels(), buffer->num_channels());
  interleaved_.resize(buffer->size());
  if (file_->ReadSamples(interleaved_.size(), &interleaved_[0]) !=
      interleaved_.size()) {
    return false;
  }

  FloatS16ToFloat(&interleaved_[0], interleaved_.size(), &interleaved_[0]);
  Deinterleave(&interleaved_[0], buffer->num_frames(), buffer->num_channels(),
               buffer->channels());
  return true;
}

ChannelBufferWavWriter::ChannelBufferWavWriter(std::unique_ptr<WavWriter> file)
    : file_(std::move(file)) {}

ChannelBufferWavWriter::~ChannelBufferWavWriter() = default;

void ChannelBufferWavWriter::Write(const ChannelBuffer<float>& buffer) {
  RTC_CHECK_EQ(file_->num_channels(), buffer.num_channels());
  interleaved_.resize(buffer.size());
  InterleavedView<float> view(&interleaved_[0], buffer.num_frames(),
                              buffer.num_channels());
  const float* samples = buffer.channels()[0];
  DeinterleavedView<const float> source(samples, buffer.num_frames(),
                                        buffer.num_channels());
  Interleave(source, view);
  FloatToFloatS16(&interleaved_[0], interleaved_.size(), &interleaved_[0]);
  file_->WriteSamples(&interleaved_[0], interleaved_.size());
}

ChannelBufferVectorWriter::ChannelBufferVectorWriter(std::vector<float>* output)
    : output_(output) {
  RTC_DCHECK(output_);
}

ChannelBufferVectorWriter::~ChannelBufferVectorWriter() = default;

void ChannelBufferVectorWriter::Write(const ChannelBuffer<float>& buffer) {
  // Account for sample rate changes throughout a simulation.
  interleaved_buffer_.resize(buffer.size());
  InterleavedView<float> view(&interleaved_buffer_[0], buffer.num_frames(),
                              buffer.num_channels());
  Interleave(buffer.channels(), buffer.num_frames(), buffer.num_channels(),
             view);
  size_t old_size = output_->size();
  output_->resize(old_size + interleaved_buffer_.size());
  FloatToFloatS16(interleaved_buffer_.data(), interleaved_buffer_.size(),
                  output_->data() + old_size);
}

FILE* OpenFile(absl::string_view filename, absl::string_view mode) {
  std::string filename_str(filename);
  FILE* file = fopen(filename_str.c_str(), std::string(mode).c_str());
  if (!file) {
    printf("Unable to open file %s\n", filename_str.c_str());
    exit(1);
  }
  return file;
}

}  // namespace webrtc
