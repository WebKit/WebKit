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
#include <memory>
#include <optional>

#include "modules/audio_processing/agc2/gain_map_internal.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {

namespace {

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
  virtual void ModifyBufferInt16(rtc::ArrayView<int16_t> buffer) = 0;
  virtual void ModifyBufferFloat(ChannelBuffer<float>* buffer) = 0;

 protected:
  // Mic level to simulate.
  int mic_level_;
  // Optional mic level to undo.
  std::optional<int> undo_mic_level_;
};

namespace {

// Identity fake recording device. The samples are not modified, which is
// equivalent to a constant gain curve at 1.0 - only used for testing.
class FakeRecordingDeviceIdentity final : public FakeRecordingDeviceWorker {
 public:
  explicit FakeRecordingDeviceIdentity(const int initial_mic_level)
      : FakeRecordingDeviceWorker(initial_mic_level) {}
  ~FakeRecordingDeviceIdentity() override = default;
  void ModifyBufferInt16(rtc::ArrayView<int16_t> /* buffer */) override {}
  void ModifyBufferFloat(ChannelBuffer<float>* /* buffer */) override {}
};

// Linear fake recording device. The gain curve is a linear function mapping the
// mic levels range [0, 255] to [0.0, 1.0].
class FakeRecordingDeviceLinear final : public FakeRecordingDeviceWorker {
 public:
  explicit FakeRecordingDeviceLinear(const int initial_mic_level)
      : FakeRecordingDeviceWorker(initial_mic_level) {}
  ~FakeRecordingDeviceLinear() override = default;
  void ModifyBufferInt16(rtc::ArrayView<int16_t> buffer) override {
    const size_t number_of_samples = buffer.size();
    int16_t* data = buffer.data();
    // If an undo level is specified, virtually restore the unmodified
    // microphone level; otherwise simulate the mic gain only.
    const float divisor =
        (undo_mic_level_ && *undo_mic_level_ > 0) ? *undo_mic_level_ : 255.f;
    for (size_t i = 0; i < number_of_samples; ++i) {
      data[i] = rtc::saturated_cast<int16_t>(data[i] * mic_level_ / divisor);
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
            rtc::SafeClamp(buffer->channels()[c][i] * mic_level_ / divisor,
                           kFloatSampleMin, kFloatSampleMax);
      }
    }
  }
};

float ComputeAgcLinearFactor(const std::optional<int>& undo_mic_level,
                             int mic_level) {
  // If an undo level is specified, virtually restore the unmodified
  // microphone level; otherwise simulate the mic gain only.
  const int undo_level =
      (undo_mic_level && *undo_mic_level > 0) ? *undo_mic_level : 100;
  return DbToRatio(kGainMap[mic_level] - kGainMap[undo_level]);
}

// Roughly dB-scale fake recording device. Valid levels are [0, 255]. The mic
// applies a gain from kGainMap in agc/gain_map_internal.h.
class FakeRecordingDeviceAgc final : public FakeRecordingDeviceWorker {
 public:
  explicit FakeRecordingDeviceAgc(const int initial_mic_level)
      : FakeRecordingDeviceWorker(initial_mic_level) {}
  ~FakeRecordingDeviceAgc() override = default;
  void ModifyBufferInt16(rtc::ArrayView<int16_t> buffer) override {
    const float scaling_factor =
        ComputeAgcLinearFactor(undo_mic_level_, mic_level_);
    const size_t number_of_samples = buffer.size();
    int16_t* data = buffer.data();
    for (size_t i = 0; i < number_of_samples; ++i) {
      data[i] = rtc::saturated_cast<int16_t>(data[i] * scaling_factor);
    }
  }
  void ModifyBufferFloat(ChannelBuffer<float>* buffer) override {
    const float scaling_factor =
        ComputeAgcLinearFactor(undo_mic_level_, mic_level_);
    for (size_t c = 0; c < buffer->num_channels(); ++c) {
      for (size_t i = 0; i < buffer->num_frames(); ++i) {
        buffer->channels()[c][i] =
            rtc::SafeClamp(buffer->channels()[c][i] * scaling_factor,
                           kFloatSampleMin, kFloatSampleMax);
      }
    }
  }
};

}  // namespace

FakeRecordingDevice::FakeRecordingDevice(int initial_mic_level,
                                         int device_kind) {
  switch (device_kind) {
    case 0:
      worker_ =
          std::make_unique<FakeRecordingDeviceIdentity>(initial_mic_level);
      break;
    case 1:
      worker_ = std::make_unique<FakeRecordingDeviceLinear>(initial_mic_level);
      break;
    case 2:
      worker_ = std::make_unique<FakeRecordingDeviceAgc>(initial_mic_level);
      break;
    default:
      RTC_DCHECK_NOTREACHED();
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

void FakeRecordingDevice::SimulateAnalogGain(rtc::ArrayView<int16_t> buffer) {
  RTC_DCHECK(worker_);
  worker_->ModifyBufferInt16(buffer);
}

void FakeRecordingDevice::SimulateAnalogGain(ChannelBuffer<float>* buffer) {
  RTC_DCHECK(worker_);
  worker_->ModifyBufferFloat(buffer);
}

}  // namespace test
}  // namespace webrtc
