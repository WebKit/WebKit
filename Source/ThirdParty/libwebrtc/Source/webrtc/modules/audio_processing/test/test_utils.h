/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_TEST_UTILS_H_
#define MODULES_AUDIO_PROCESSING_TEST_TEST_UTILS_H_

#include <math.h>

#include <iterator>
#include <limits>
#include <memory>
#include <sstream>  // no-presubmit-check TODO(webrtc:8982)
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/audio/audio_processing.h"
#include "common_audio/channel_buffer.h"
#include "common_audio/wav_file.h"

namespace webrtc {

static const AudioProcessing::Error kNoErr = AudioProcessing::kNoError;
#define EXPECT_NOERR(expr) EXPECT_EQ(kNoErr, (expr))

// Encapsulates samples and metadata for an integer frame.
struct Int16FrameData {
  // Max data size that matches the data size of the AudioFrame class, providing
  // storage for 8 channels of 96 kHz data.
  static const int kMaxDataSizeSamples = 7680;

  Int16FrameData() {
    sample_rate_hz = 0;
    num_channels = 0;
    samples_per_channel = 0;
    data.fill(0);
  }

  void CopyFrom(const Int16FrameData& src) {
    samples_per_channel = src.samples_per_channel;
    sample_rate_hz = src.sample_rate_hz;
    num_channels = src.num_channels;

    const size_t length = samples_per_channel * num_channels;
    RTC_CHECK_LE(length, kMaxDataSizeSamples);
    memcpy(data.data(), src.data.data(), sizeof(int16_t) * length);
  }
  std::array<int16_t, kMaxDataSizeSamples> data;
  int32_t sample_rate_hz;
  size_t num_channels;
  size_t samples_per_channel;
};

// Reads ChannelBuffers from a provided WavReader.
class ChannelBufferWavReader final {
 public:
  explicit ChannelBufferWavReader(std::unique_ptr<WavReader> file);
  ~ChannelBufferWavReader();

  ChannelBufferWavReader(const ChannelBufferWavReader&) = delete;
  ChannelBufferWavReader& operator=(const ChannelBufferWavReader&) = delete;

  // Reads data from the file according to the `buffer` format. Returns false if
  // a full buffer can't be read from the file.
  bool Read(ChannelBuffer<float>* buffer);

 private:
  std::unique_ptr<WavReader> file_;
  std::vector<float> interleaved_;
};

// Writes ChannelBuffers to a provided WavWriter.
class ChannelBufferWavWriter final {
 public:
  explicit ChannelBufferWavWriter(std::unique_ptr<WavWriter> file);
  ~ChannelBufferWavWriter();

  ChannelBufferWavWriter(const ChannelBufferWavWriter&) = delete;
  ChannelBufferWavWriter& operator=(const ChannelBufferWavWriter&) = delete;

  void Write(const ChannelBuffer<float>& buffer);

 private:
  std::unique_ptr<WavWriter> file_;
  std::vector<float> interleaved_;
};

// Takes a pointer to a vector. Allows appending the samples of channel buffers
// to the given vector, by interleaving the samples and converting them to float
// S16.
class ChannelBufferVectorWriter final {
 public:
  explicit ChannelBufferVectorWriter(std::vector<float>* output);
  ChannelBufferVectorWriter(const ChannelBufferVectorWriter&) = delete;
  ChannelBufferVectorWriter& operator=(const ChannelBufferVectorWriter&) =
      delete;
  ~ChannelBufferVectorWriter();

  // Creates an interleaved copy of `buffer`, converts the samples to float S16
  // and appends the result to output_.
  void Write(const ChannelBuffer<float>& buffer);

 private:
  std::vector<float> interleaved_buffer_;
  std::vector<float>* output_;
};

// Exits on failure; do not use in unit tests.
FILE* OpenFile(absl::string_view filename, absl::string_view mode);

void SetFrameSampleRate(Int16FrameData* frame, int sample_rate_hz);

template <typename T>
void SetContainerFormat(int sample_rate_hz,
                        size_t num_channels,
                        Int16FrameData* frame,
                        std::unique_ptr<ChannelBuffer<T> >* cb) {
  SetFrameSampleRate(frame, sample_rate_hz);
  frame->num_channels = num_channels;
  cb->reset(new ChannelBuffer<T>(frame->samples_per_channel, num_channels));
}

template <typename T>
float ComputeSNR(const T* ref, const T* test, size_t length, float* variance) {
  float mse = 0;
  float mean = 0;
  *variance = 0;
  for (size_t i = 0; i < length; ++i) {
    T error = ref[i] - test[i];
    mse += error * error;
    *variance += ref[i] * ref[i];
    mean += ref[i];
  }
  mse /= length;
  *variance /= length;
  mean /= length;
  *variance -= mean * mean;

  float snr = 100;  // We assign 100 dB to the zero-error case.
  if (mse > 0)
    snr = 10 * log10(*variance / mse);
  return snr;
}

// Returns a vector<T> parsed from whitespace delimited values in to_parse,
// or an empty vector if the string could not be parsed.
template <typename T>
std::vector<T> ParseList(absl::string_view to_parse) {
  std::vector<T> values;

  std::istringstream str(  // no-presubmit-check TODO(webrtc:8982)
      std::string{to_parse});
  std::copy(
      std::istream_iterator<T>(str),  // no-presubmit-check TODO(webrtc:8982)
      std::istream_iterator<T>(),     // no-presubmit-check TODO(webrtc:8982)
      std::back_inserter(values));

  return values;
}

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_TEST_UTILS_H_
