/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/test/fake_recording_device.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kInitialMicLevel = 100;

// TODO(alessiob): Add new fake recording device kind values here as they are
// added in FakeRecordingDevice::FakeRecordingDevice.
const std::vector<int> kFakeRecDeviceKinds = {0, 1};

const std::vector<std::vector<float>> kTestMultiChannelSamples{
    std::vector<float>{-10.f, -1.f, -0.1f, 0.f, 0.1f, 1.f, 10.f}};

// Writes samples into ChannelBuffer<float>.
void WritesDataIntoChannelBuffer(const std::vector<std::vector<float>>& data,
                                 ChannelBuffer<float>* buff) {
  EXPECT_EQ(data.size(), buff->num_channels());
  EXPECT_EQ(data[0].size(), buff->num_frames());
  for (size_t c = 0; c < buff->num_channels(); ++c) {
    for (size_t f = 0; f < buff->num_frames(); ++f) {
      buff->channels()[c][f] = data[c][f];
    }
  }
}

std::unique_ptr<ChannelBuffer<float>> CreateChannelBufferWithData(
    const std::vector<std::vector<float>>& data) {
  auto buff =
      rtc::MakeUnique<ChannelBuffer<float>>(data[0].size(), data.size());
  WritesDataIntoChannelBuffer(data, buff.get());
  return buff;
}

// Checks that the samples modified using monotonic level values are also
// monotonic.
void CheckIfMonotoneSamplesModules(const ChannelBuffer<float>* prev,
                                   const ChannelBuffer<float>* curr) {
  RTC_DCHECK_EQ(prev->num_channels(), curr->num_channels());
  RTC_DCHECK_EQ(prev->num_frames(), curr->num_frames());
  bool valid = true;
  for (size_t i = 0; i < prev->num_channels(); ++i) {
    for (size_t j = 0; j < prev->num_frames(); ++j) {
      valid = std::fabs(prev->channels()[i][j]) <=
              std::fabs(curr->channels()[i][j]);
      if (!valid) {
        break;
      }
    }
    if (!valid) {
      break;
    }
  }
  EXPECT_TRUE(valid);
}

// Checks that the samples in each pair have the same sign unless the sample in
// |dst| is zero (because of zero gain).
void CheckSameSign(const ChannelBuffer<float>* src,
                   const ChannelBuffer<float>* dst) {
  RTC_DCHECK_EQ(src->num_channels(), dst->num_channels());
  RTC_DCHECK_EQ(src->num_frames(), dst->num_frames());
  const auto fsgn = [](float x) { return ((x < 0) ? -1 : (x > 0) ? 1 : 0); };
  bool valid = true;
  for (size_t i = 0; i < src->num_channels(); ++i) {
    for (size_t j = 0; j < src->num_frames(); ++j) {
      valid = dst->channels()[i][j] == 0.0f ||
              fsgn(src->channels()[i][j]) == fsgn(dst->channels()[i][j]);
      if (!valid) {
        break;
      }
    }
    if (!valid) {
      break;
    }
  }
  EXPECT_TRUE(valid);
}

std::string FakeRecordingDeviceKindToString(int fake_rec_device_kind) {
  std::ostringstream ss;
  ss << "fake recording device: " << fake_rec_device_kind;
  return ss.str();
}

std::string AnalogLevelToString(int level) {
  std::ostringstream ss;
  ss << "analog level: " << level;
  return ss.str();
}

}  // namespace

TEST(FakeRecordingDevice, CheckHelperFunctions) {
  constexpr size_t kC = 0;  // Channel index.
  constexpr size_t kS = 1;  // Sample index.

  // Check read.
  auto buff = CreateChannelBufferWithData(kTestMultiChannelSamples);
  for (size_t c = 0; c < kTestMultiChannelSamples.size(); ++c) {
    for (size_t s = 0; s < kTestMultiChannelSamples[0].size(); ++s) {
      EXPECT_EQ(kTestMultiChannelSamples[c][s], buff->channels()[c][s]);
    }
  }

  // Check write.
  buff->channels()[kC][kS] = -5.0f;
  RTC_DCHECK_NE(buff->channels()[kC][kS], kTestMultiChannelSamples[kC][kS]);

  // Check reset.
  WritesDataIntoChannelBuffer(kTestMultiChannelSamples, buff.get());
  EXPECT_EQ(buff->channels()[kC][kS], kTestMultiChannelSamples[kC][kS]);
}

// Implicitly checks that changes to the mic and undo levels are visible to the
// FakeRecordingDeviceWorker implementation are injected in FakeRecordingDevice.
TEST(FakeRecordingDevice, TestWorkerAbstractClass) {
  FakeRecordingDevice fake_recording_device(kInitialMicLevel, 1);

  auto buff1 = CreateChannelBufferWithData(kTestMultiChannelSamples);
  fake_recording_device.SetMicLevel(100);
  fake_recording_device.SimulateAnalogGain(buff1.get());

  auto buff2 = CreateChannelBufferWithData(kTestMultiChannelSamples);
  fake_recording_device.SetMicLevel(200);
  fake_recording_device.SimulateAnalogGain(buff2.get());

  for (size_t c = 0; c < kTestMultiChannelSamples.size(); ++c) {
    for (size_t s = 0; s < kTestMultiChannelSamples[0].size(); ++s) {
      EXPECT_LE(std::abs(buff1->channels()[c][s]),
                std::abs(buff2->channels()[c][s]));
    }
  }

  auto buff3 = CreateChannelBufferWithData(kTestMultiChannelSamples);
  fake_recording_device.SetMicLevel(200);
  fake_recording_device.SetUndoMicLevel(100);
  fake_recording_device.SimulateAnalogGain(buff3.get());

  for (size_t c = 0; c < kTestMultiChannelSamples.size(); ++c) {
    for (size_t s = 0; s < kTestMultiChannelSamples[0].size(); ++s) {
      EXPECT_LE(std::abs(buff1->channels()[c][s]),
                std::abs(buff3->channels()[c][s]));
      EXPECT_LE(std::abs(buff2->channels()[c][s]),
                std::abs(buff3->channels()[c][s]));
    }
  }
}

TEST(FakeRecordingDevice, GainCurveShouldBeMonotone) {
  // Create input-output buffers.
  auto buff_prev = CreateChannelBufferWithData(kTestMultiChannelSamples);
  auto buff_curr = CreateChannelBufferWithData(kTestMultiChannelSamples);

  // Test different mappings.
  for (auto fake_rec_device_kind : kFakeRecDeviceKinds) {
    SCOPED_TRACE(FakeRecordingDeviceKindToString(fake_rec_device_kind));
    FakeRecordingDevice fake_recording_device(kInitialMicLevel,
                                              fake_rec_device_kind);
    // TODO(alessiob): The test below is designed for state-less recording
    // devices. If, for instance, a device has memory, the test might need
    // to be redesigned (e.g., re-initialize fake recording device).

    // Apply lowest analog level.
    WritesDataIntoChannelBuffer(kTestMultiChannelSamples, buff_prev.get());
    fake_recording_device.SetMicLevel(0);
    fake_recording_device.SimulateAnalogGain(buff_prev.get());

    // Increment analog level to check monotonicity.
    for (int i = 1; i <= 255; ++i) {
      SCOPED_TRACE(AnalogLevelToString(i));
      WritesDataIntoChannelBuffer(kTestMultiChannelSamples, buff_curr.get());
      fake_recording_device.SetMicLevel(i);
      fake_recording_device.SimulateAnalogGain(buff_curr.get());
      CheckIfMonotoneSamplesModules(buff_prev.get(), buff_curr.get());

      // Update prev.
      buff_prev.swap(buff_curr);
    }
  }
}

TEST(FakeRecordingDevice, GainCurveShouldNotChangeSign) {
  // Create view on original samples.
  std::unique_ptr<const ChannelBuffer<float>> buff_orig =
      CreateChannelBufferWithData(kTestMultiChannelSamples);

  // Create output buffer.
  auto buff = CreateChannelBufferWithData(kTestMultiChannelSamples);

  // Test different mappings.
  for (auto fake_rec_device_kind : kFakeRecDeviceKinds) {
    SCOPED_TRACE(FakeRecordingDeviceKindToString(fake_rec_device_kind));
    FakeRecordingDevice fake_recording_device(kInitialMicLevel,
                                              fake_rec_device_kind);

    // TODO(alessiob): The test below is designed for state-less recording
    // devices. If, for instance, a device has memory, the test might need
    // to be redesigned (e.g., re-initialize fake recording device).
    for (int i = 0; i <= 255; ++i) {
      SCOPED_TRACE(AnalogLevelToString(i));
      WritesDataIntoChannelBuffer(kTestMultiChannelSamples, buff.get());
      fake_recording_device.SetMicLevel(i);
      fake_recording_device.SimulateAnalogGain(buff.get());
      CheckSameSign(buff_orig.get(), buff.get());
    }
  }
}

}  // namespace test
}  // namespace webrtc
