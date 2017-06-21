/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_factory.h"

#include <cstddef>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {
namespace {

using conversational_speech::WavReaderInterface;

class WavReaderAdaptor final : public WavReaderInterface {
 public:
  explicit WavReaderAdaptor(const std::string& filepath)
      : wav_reader_(filepath) {}
  ~WavReaderAdaptor() override = default;

  size_t ReadFloatSamples(rtc::ArrayView<float> samples) override {
    return wav_reader_.ReadSamples(samples.size(), samples.begin());
  }

  size_t ReadInt16Samples(rtc::ArrayView<int16_t> samples) override {
    return wav_reader_.ReadSamples(samples.size(), samples.begin());
  }

  int SampleRate() const override {
    return wav_reader_.sample_rate();
  }

  size_t NumChannels() const override {
    return wav_reader_.num_channels();
  }

  size_t NumSamples() const override {
    return wav_reader_.num_samples();
  }

 private:
  WavReader wav_reader_;
};

}  // namespace

namespace conversational_speech {

WavReaderFactory::WavReaderFactory() = default;

WavReaderFactory::~WavReaderFactory() = default;

std::unique_ptr<WavReaderInterface> WavReaderFactory::Create(
    const std::string& filepath) const {
  return std::unique_ptr<WavReaderAdaptor>(new WavReaderAdaptor(filepath));
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
