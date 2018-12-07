/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/conversational_speech/mock_wavreader_factory.h"

#include "modules/audio_processing/test/conversational_speech/mock_wavreader.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

using testing::_;
using testing::Invoke;

MockWavReaderFactory::MockWavReaderFactory(
    const Params& default_params,
    const std::map<std::string, const Params>& params)
    : default_params_(default_params), audiotrack_names_params_(params) {
  ON_CALL(*this, Create(_))
      .WillByDefault(Invoke(this, &MockWavReaderFactory::CreateMock));
}

MockWavReaderFactory::MockWavReaderFactory(const Params& default_params)
    : MockWavReaderFactory(default_params,
                           std::map<std::string, const Params>{}) {}

MockWavReaderFactory::~MockWavReaderFactory() = default;

std::unique_ptr<WavReaderInterface> MockWavReaderFactory::CreateMock(
    const std::string& filepath) {
  // Search the parameters corresponding to filepath.
  size_t delimiter = filepath.find_last_of("/\\");  // Either windows or posix
  std::string filename =
      filepath.substr(delimiter == std::string::npos ? 0 : delimiter + 1);
  const auto it = audiotrack_names_params_.find(filename);

  // If not found, use default parameters.
  if (it == audiotrack_names_params_.end()) {
    RTC_LOG(LS_VERBOSE) << "using default parameters for " << filepath;
    return std::unique_ptr<WavReaderInterface>(new MockWavReader(
        default_params_.sample_rate, default_params_.num_channels,
        default_params_.num_samples));
  }

  // Found, use the audiotrack-specific parameters.
  RTC_LOG(LS_VERBOSE) << "using ad-hoc parameters for " << filepath;
  RTC_LOG(LS_VERBOSE) << "sample_rate " << it->second.sample_rate;
  RTC_LOG(LS_VERBOSE) << "num_channels " << it->second.num_channels;
  RTC_LOG(LS_VERBOSE) << "num_samples " << it->second.num_samples;
  return std::unique_ptr<WavReaderInterface>(new MockWavReader(
      it->second.sample_rate, it->second.num_channels, it->second.num_samples));
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
