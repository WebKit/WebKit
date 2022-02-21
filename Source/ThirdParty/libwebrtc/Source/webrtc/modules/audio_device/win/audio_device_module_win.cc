/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/win/audio_device_module_win.h"

#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/string_utils.h"

namespace webrtc {
namespace webrtc_win {
namespace {

#define RETURN_IF_OUTPUT_RESTARTS(...) \
  do {                                 \
    if (output_->Restarting()) {       \
      return __VA_ARGS__;              \
    }                                  \
  } while (0)

#define RETURN_IF_INPUT_RESTARTS(...) \
  do {                                \
    if (input_->Restarting()) {       \
      return __VA_ARGS__;             \
    }                                 \
  } while (0)

#define RETURN_IF_OUTPUT_IS_INITIALIZED(...) \
  do {                                       \
    if (output_->PlayoutIsInitialized()) {   \
      return __VA_ARGS__;                    \
    }                                        \
  } while (0)

#define RETURN_IF_INPUT_IS_INITIALIZED(...) \
  do {                                      \
    if (input_->RecordingIsInitialized()) { \
      return __VA_ARGS__;                   \
    }                                       \
  } while (0)

#define RETURN_IF_OUTPUT_IS_ACTIVE(...) \
  do {                                  \
    if (output_->Playing()) {           \
      return __VA_ARGS__;               \
    }                                   \
  } while (0)

#define RETURN_IF_INPUT_IS_ACTIVE(...) \
  do {                                 \
    if (input_->Recording()) {         \
      return __VA_ARGS__;              \
    }                                  \
  } while (0)

// This class combines a generic instance of an AudioInput and a generic
// instance of an AudioOutput to create an AudioDeviceModule. This is mostly
// done by delegating to the audio input/output with some glue code. This class
// also directly implements some of the AudioDeviceModule methods with dummy
// implementations.
//
// An instance must be created, destroyed and used on one and the same thread,
// i.e., all public methods must also be called on the same thread. A thread
// checker will RTC_DCHECK if any method is called on an invalid thread.
// TODO(henrika): is thread checking needed in AudioInput and AudioOutput?
class WindowsAudioDeviceModule : public AudioDeviceModuleForTest {
 public:
  enum class InitStatus {
    OK = 0,
    PLAYOUT_ERROR = 1,
    RECORDING_ERROR = 2,
    OTHER_ERROR = 3,
    NUM_STATUSES = 4
  };

  WindowsAudioDeviceModule(std::unique_ptr<AudioInput> audio_input,
                           std::unique_ptr<AudioOutput> audio_output,
                           TaskQueueFactory* task_queue_factory)
      : input_(std::move(audio_input)),
        output_(std::move(audio_output)),
        task_queue_factory_(task_queue_factory) {
    RTC_CHECK(input_);
    RTC_CHECK(output_);
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
  }

  ~WindowsAudioDeviceModule() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    Terminate();
  }

  WindowsAudioDeviceModule(const WindowsAudioDeviceModule&) = delete;
  WindowsAudioDeviceModule& operator=(const WindowsAudioDeviceModule&) = delete;

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer* audioLayer) const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    // TODO(henrika): it might be possible to remove this unique signature.
    *audioLayer = AudioDeviceModule::kWindowsCoreAudio2;
    return 0;
  }

  int32_t RegisterAudioCallback(AudioTransport* audioCallback) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK(audio_device_buffer_);
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return audio_device_buffer_->RegisterAudioCallback(audioCallback);
  }

  int32_t Init() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    RETURN_IF_INPUT_RESTARTS(0);
    if (initialized_) {
      return 0;
    }
    audio_device_buffer_ =
        std::make_unique<AudioDeviceBuffer>(task_queue_factory_);
    AttachAudioBuffer();
    InitStatus status;
    if (output_->Init() != 0) {
      status = InitStatus::PLAYOUT_ERROR;
    } else if (input_->Init() != 0) {
      output_->Terminate();
      status = InitStatus::RECORDING_ERROR;
    } else {
      initialized_ = true;
      status = InitStatus::OK;
    }
    if (status != InitStatus::OK) {
      RTC_LOG(LS_ERROR) << "Audio device initialization failed";
      return -1;
    }
    return 0;
  }

  int32_t Terminate() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    RETURN_IF_INPUT_RESTARTS(0);
    if (!initialized_)
      return 0;
    int32_t err = input_->Terminate();
    err |= output_->Terminate();
    initialized_ = false;
    RTC_DCHECK_EQ(err, 0);
    return err;
  }

  bool Initialized() const override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return initialized_;
  }

  int16_t PlayoutDevices() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    return output_->NumDevices();
  }

  int16_t RecordingDevices() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_INPUT_RESTARTS(0);
    return input_->NumDevices();
  }

  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    std::string name_str, guid_str;
    int ret = -1;
    if (guid != nullptr) {
      ret = output_->DeviceName(index, &name_str, &guid_str);
      rtc::strcpyn(guid, kAdmMaxGuidSize, guid_str.c_str());
    } else {
      ret = output_->DeviceName(index, &name_str, nullptr);
    }
    rtc::strcpyn(name, kAdmMaxDeviceNameSize, name_str.c_str());
    return ret;
  }
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_INPUT_RESTARTS(0);
    std::string name_str, guid_str;
    int ret = -1;
    if (guid != nullptr) {
      ret = input_->DeviceName(index, &name_str, &guid_str);
      rtc::strcpyn(guid, kAdmMaxGuidSize, guid_str.c_str());
    } else {
      ret = input_->DeviceName(index, &name_str, nullptr);
    }
    rtc::strcpyn(name, kAdmMaxDeviceNameSize, name_str.c_str());
    return ret;
  }

  int32_t SetPlayoutDevice(uint16_t index) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    return output_->SetDevice(index);
  }

  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    return output_->SetDevice(device);
  }
  int32_t SetRecordingDevice(uint16_t index) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->SetDevice(index);
  }

  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->SetDevice(device);
  }

  int32_t PlayoutIsAvailable(bool* available) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t InitPlayout() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    RETURN_IF_OUTPUT_IS_INITIALIZED(0);
    return output_->InitPlayout();
  }

  bool PlayoutIsInitialized() const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(true);
    return output_->PlayoutIsInitialized();
  }

  int32_t RecordingIsAvailable(bool* available) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t InitRecording() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_INPUT_RESTARTS(0);
    RETURN_IF_INPUT_IS_INITIALIZED(0);
    return input_->InitRecording();
  }

  bool RecordingIsInitialized() const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_INPUT_RESTARTS(true);
    return input_->RecordingIsInitialized();
  }

  int32_t StartPlayout() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    RETURN_IF_OUTPUT_IS_ACTIVE(0);
    return output_->StartPlayout();
  }

  int32_t StopPlayout() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(-1);
    return output_->StopPlayout();
  }

  bool Playing() const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(true);
    return output_->Playing();
  }

  int32_t StartRecording() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_INPUT_RESTARTS(0);
    RETURN_IF_INPUT_IS_ACTIVE(0);
    return input_->StartRecording();
  }

  int32_t StopRecording() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_INPUT_RESTARTS(-1);
    return input_->StopRecording();
  }

  bool Recording() const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RETURN_IF_INPUT_RESTARTS(true);
    return input_->Recording();
  }

  int32_t InitSpeaker() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_ ? 0 : -1;
  }

  bool SpeakerIsInitialized() const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_;
  }

  int32_t InitMicrophone() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_ ? 0 : -1;
  }

  bool MicrophoneIsInitialized() const override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_;
  }

  int32_t SpeakerVolumeIsAvailable(bool* available) override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = false;
    return 0;
  }

  int32_t SetSpeakerVolume(uint32_t volume) override { return 0; }
  int32_t SpeakerVolume(uint32_t* volume) const override { return 0; }
  int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override { return 0; }
  int32_t MinSpeakerVolume(uint32_t* minVolume) const override { return 0; }

  int32_t MicrophoneVolumeIsAvailable(bool* available) override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = false;
    return 0;
  }

  int32_t SetMicrophoneVolume(uint32_t volume) override { return 0; }
  int32_t MicrophoneVolume(uint32_t* volume) const override { return 0; }
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override { return 0; }
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override { return 0; }

  int32_t SpeakerMuteIsAvailable(bool* available) override { return 0; }
  int32_t SetSpeakerMute(bool enable) override { return 0; }
  int32_t SpeakerMute(bool* enabled) const override { return 0; }

  int32_t MicrophoneMuteIsAvailable(bool* available) override { return 0; }
  int32_t SetMicrophoneMute(bool enable) override { return 0; }
  int32_t MicrophoneMute(bool* enabled) const override { return 0; }

  int32_t StereoPlayoutIsAvailable(bool* available) const override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t SetStereoPlayout(bool enable) override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return 0;
  }

  int32_t StereoPlayout(bool* enabled) const override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *enabled = true;
    return 0;
  }

  int32_t StereoRecordingIsAvailable(bool* available) const override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return 0;
  }

  int32_t StereoRecording(bool* enabled) const override {
    // TODO(henrika): improve support.
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *enabled = true;
    return 0;
  }

  int32_t PlayoutDelay(uint16_t* delayMS) const override { return 0; }

  bool BuiltInAECIsAvailable() const override { return false; }
  bool BuiltInAGCIsAvailable() const override { return false; }
  bool BuiltInNSIsAvailable() const override { return false; }

  int32_t EnableBuiltInAEC(bool enable) override { return 0; }
  int32_t EnableBuiltInAGC(bool enable) override { return 0; }
  int32_t EnableBuiltInNS(bool enable) override { return 0; }

  int32_t AttachAudioBuffer() {
    RTC_DLOG(INFO) << __FUNCTION__;
    output_->AttachAudioBuffer(audio_device_buffer_.get());
    input_->AttachAudioBuffer(audio_device_buffer_.get());
    return 0;
  }

  int RestartPlayoutInternally() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RETURN_IF_OUTPUT_RESTARTS(0);
    return output_->RestartPlayout();
  }

  int RestartRecordingInternally() override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->RestartRecording();
  }

  int SetPlayoutSampleRate(uint32_t sample_rate) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->SetSampleRate(sample_rate);
  }

  int SetRecordingSampleRate(uint32_t sample_rate) override {
    RTC_DLOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->SetSampleRate(sample_rate);
  }

 private:
  // Ensures that the class is used on the same thread as it is constructed
  // and destroyed on.
  SequenceChecker thread_checker_;

  // Implements the AudioInput interface and deals with audio capturing parts.
  const std::unique_ptr<AudioInput> input_;

  // Implements the AudioOutput interface and deals with audio rendering parts.
  const std::unique_ptr<AudioOutput> output_;

  TaskQueueFactory* const task_queue_factory_;

  // The AudioDeviceBuffer (ADB) instance is needed for sending/receiving audio
  // to/from the WebRTC layer. Created and owned by this object. Used by
  // both `input_` and `output_` but they use orthogonal parts of the ADB.
  std::unique_ptr<AudioDeviceBuffer> audio_device_buffer_;

  // Set to true after a successful call to Init(). Cleared by Terminate().
  bool initialized_ = false;
};

}  // namespace

rtc::scoped_refptr<AudioDeviceModuleForTest>
CreateWindowsCoreAudioAudioDeviceModuleFromInputAndOutput(
    std::unique_ptr<AudioInput> audio_input,
    std::unique_ptr<AudioOutput> audio_output,
    TaskQueueFactory* task_queue_factory) {
  RTC_DLOG(INFO) << __FUNCTION__;
  return rtc::make_ref_counted<WindowsAudioDeviceModule>(
      std::move(audio_input), std::move(audio_output), task_queue_factory);
}

}  // namespace webrtc_win
}  // namespace webrtc
