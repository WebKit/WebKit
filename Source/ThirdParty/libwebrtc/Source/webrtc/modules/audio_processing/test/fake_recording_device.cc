/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/fake_recording_device.h"

#include <algorithm>

#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {
namespace test {

namespace {

constexpr int16_t kInt16SampleMin = -32768;
constexpr int16_t kInt16SampleMax = 32767;
constexpr float kFloatSampleMin = -32768.f;
constexpr float kFloatSampleMax = 32767.0f;

}  // namespace

// Abstract class for the different fake recording devices.
class FakeRecordingDeviceWorker {
 public:
  explicit FakeRecordingDeviceWorker(const int initial_mic_level)
      : mic_level_(initial_mic_level) {}
  int mic_level() const { return mic_level_; }
  void set_mic_level(const int level) { mic_level_ = level; }
  void set_undo_mic_level(const int level) { undo_mic_level_ = level; }
  virtual ~FakeRecordingDeviceWorker() = default;
  virtual void ModifyBufferInt16(AudioFrame* buffer) = 0;
  virtual void ModifyBufferFloat(ChannelBuffer<float>* buffer) = 0;

 protected:
  // Mic level to simulate.
  int mic_level_;
  // Optional mic level to undo.
  rtc::Optional<int> undo_mic_level_;
};

namespace {

// Identity fake recording device. The samples are not modified, which is
// equivalent to a constant gain curve at 1.0 - only used for testing.
class FakeRecordingDeviceIdentity final : public FakeRecordingDeviceWorker {
 public:
  explicit FakeRecordingDeviceIdentity(const int initial_mic_level)
      : FakeRecordingDeviceWorker(initial_mic_level) {}
  ~FakeRecordingDeviceIdentity() override = default;
  void ModifyBufferInt16(AudioFrame* buffer) override {}
  void ModifyBufferFloat(ChannelBuffer<float>* buffer) override {}
};

// Linear fake recording device. The gain curve is a linear function mapping the
// mic levels range [0, 255] to [0.0, 1.0].
class FakeRecordingDeviceLinear final : public FakeRecordingDeviceWorker {
 public:
  explicit FakeRecordingDeviceLinear(const int initial_mic_level)
      : FakeRecordingDeviceWorker(initial_mic_level) {}
  ~FakeRecordingDeviceLinear() override = default;
  void ModifyBufferInt16(AudioFrame* buffer) override {
    const size_t number_of_samples =
        buffer->samples_per_channel_ * buffer->num_channels_;
    int16_t* data = buffer->mutable_data();
    // If an undo level is specified, virtually restore the unmodified
    // microphone level; otherwise simulate the mic gain only.
    const float divisor =
        (undo_mic_level_ && *undo_mic_level_ > 0) ? *undo_mic_level_ : 255.f;
    for (size_t i = 0; i < number_of_samples; ++i) {
      data[i] =
          std::max(kInt16SampleMin,
                   std::min(kInt16SampleMax,
                            static_cast<int16_t>(static_cast<float>(data[i]) *
                                                 mic_level_ / divisor)));
    }
  }
  void ModifyBufferFloat(ChannelBuffer<float>* buffer) override {
    // If an undo level is specified, virtually restore the unmodified
    // microphone level; otherwise simulate the mic gain only.
    const float divisor =
        (undo_mic_level_ && *undo_mic_level_ > 0) ? *undo_mic_level_ : 255.f;
    for (size_t c = 0; c < buffer->num_channels(); ++c) {
      for (size_t i = 0; i < buffer->num_frames(); ++i) {
        buffer->channels()[c][i] =
            std::max(kFloatSampleMin,
                     std::min(kFloatSampleMax,
                              buffer->channels()[c][i] * mic_level_ / divisor));
      }
    }
  }
};

}  // namespace

FakeRecordingDevice::FakeRecordingDevice(int initial_mic_level,
                                         int device_kind) {
  switch (device_kind) {
    case 0:
      worker_ = rtc::MakeUnique<FakeRecordingDeviceIdentity>(initial_mic_level);
      break;
    case 1:
      worker_ = rtc::MakeUnique<FakeRecordingDeviceLinear>(initial_mic_level);
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
}

FakeRecordingDevice::~FakeRecordingDevice() = default;

int FakeRecordingDevice::MicLevel() const {
  RTC_CHECK(worker_);
  return worker_->mic_level();
}

void FakeRecordingDevice::SetMicLevel(const int level) {
  RTC_CHECK(worker_);
  if (level != worker_->mic_level())
    RTC_LOG(LS_INFO) << "Simulate mic level update: " << level;
  worker_->set_mic_level(level);
}

void FakeRecordingDevice::SetUndoMicLevel(const int level) {
  RTC_DCHECK(worker_);
  // TODO(alessiob): The behavior with undo level equal to zero is not clear yet
  // and will be defined in future CLs once more FakeRecordingDeviceWorker
  // implementations need to be added.
  RTC_CHECK(level > 0) << "Zero undo mic level is unsupported";
  worker_->set_undo_mic_level(level);
}

void FakeRecordingDevice::SimulateAnalogGain(AudioFrame* buffer) {
  RTC_DCHECK(worker_);
  worker_->ModifyBufferInt16(buffer);
}

void FakeRecordingDevice::SimulateAnalogGain(ChannelBuffer<float>* buffer) {
  RTC_DCHECK(worker_);
  worker_->ModifyBufferFloat(buffer);
}

}  // namespace test
}  // namespace webrtc
