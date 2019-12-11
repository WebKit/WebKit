/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/copy_to_file_audio_capturer.h"

#include <utility>

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

CopyToFileAudioCapturer::CopyToFileAudioCapturer(
    std::unique_ptr<TestAudioDeviceModule::Capturer> delegate,
    std::string stream_dump_file_name)
    : delegate_(std::move(delegate)),
      wav_writer_(absl::make_unique<WavWriter>(std::move(stream_dump_file_name),
                                               delegate_->SamplingFrequency(),
                                               delegate_->NumChannels())) {}
CopyToFileAudioCapturer::~CopyToFileAudioCapturer() = default;

int CopyToFileAudioCapturer::SamplingFrequency() const {
  return delegate_->SamplingFrequency();
}

int CopyToFileAudioCapturer::NumChannels() const {
  return delegate_->NumChannels();
}

bool CopyToFileAudioCapturer::Capture(rtc::BufferT<int16_t>* buffer) {
  bool result = delegate_->Capture(buffer);
  if (result) {
    wav_writer_->WriteSamples(buffer->data(), buffer->size());
  }
  return result;
}

}  // namespace test
}  // namespace webrtc
