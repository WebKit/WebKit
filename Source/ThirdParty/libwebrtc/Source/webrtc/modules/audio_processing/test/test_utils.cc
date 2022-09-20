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

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/arch.h"

namespace webrtc {

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
  Interleave(buffer.channels(), buffer.num_frames(), buffer.num_channels(),
             &interleaved_[0]);
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
  Interleave(buffer.channels(), buffer.num_frames(), buffer.num_channels(),
             interleaved_buffer_.data());
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

void SetFrameSampleRate(Int16FrameData* frame, int sample_rate_hz) {
  frame->sample_rate_hz = sample_rate_hz;
  frame->samples_per_channel =
      AudioProcessing::kChunkSizeMs * sample_rate_hz / 1000;
}

}  // namespace webrtc
