/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/win/core_audio_base_win.h"

#include <string>

#include "absl/memory/memory.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/win/windows_version.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {
namespace webrtc_win {
namespace {

enum DefaultDeviceType {
  kDefault,
  kDefaultCommunications,
  kDefaultDeviceTypeMaxCount,
};

const char* DirectionToString(CoreAudioBase::Direction direction) {
  switch (direction) {
    case CoreAudioBase::Direction::kOutput:
      return "Output";
    case CoreAudioBase::Direction::kInput:
      return "Input";
    default:
      return "Unkown";
  }
}

void Run(void* obj) {
  RTC_DCHECK(obj);
  reinterpret_cast<CoreAudioBase*>(obj)->ThreadRun();
}

}  // namespace

CoreAudioBase::CoreAudioBase(Direction direction, OnDataCallback callback)
    : direction_(direction), on_data_callback_(callback), format_() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction) << "]";

  // Create the event which the audio engine will signal each time a buffer
  // becomes ready to be processed by the client.
  audio_samples_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(audio_samples_event_.IsValid());

  // Event to be be set in Stop() when rendering/capturing shall stop.
  stop_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(stop_event_.IsValid());
}

CoreAudioBase::~CoreAudioBase() {
  RTC_DLOG(INFO) << __FUNCTION__;
}

EDataFlow CoreAudioBase::GetDataFlow() const {
  return direction_ == CoreAudioBase::Direction::kOutput ? eRender : eCapture;
}

int CoreAudioBase::NumberOfActiveDevices() const {
  return core_audio_utility::NumberOfActiveDevices(GetDataFlow());
}

int CoreAudioBase::NumberOfEnumeratedDevices() const {
  const int num_active = NumberOfActiveDevices();
  return num_active > 0 ? num_active + kDefaultDeviceTypeMaxCount : 0;
}

bool CoreAudioBase::IsDefaultDevice(int index) const {
  return index == kDefault;
}

bool CoreAudioBase::IsDefaultCommunicationsDevice(int index) const {
  return index == kDefaultCommunications;
}

bool CoreAudioBase::IsDefaultDevice(const std::string& device_id) const {
  return (IsInput() &&
          (device_id == core_audio_utility::GetDefaultInputDeviceID())) ||
         (IsOutput() &&
          (device_id == core_audio_utility::GetDefaultOutputDeviceID()));
}

bool CoreAudioBase::IsDefaultCommunicationsDevice(
    const std::string& device_id) const {
  return (IsInput() &&
          (device_id ==
           core_audio_utility::GetCommunicationsInputDeviceID())) ||
         (IsOutput() &&
          (device_id == core_audio_utility::GetCommunicationsOutputDeviceID()));
}

bool CoreAudioBase::IsInput() const {
  return direction_ == CoreAudioBase::Direction::kInput;
}

bool CoreAudioBase::IsOutput() const {
  return direction_ == CoreAudioBase::Direction::kOutput;
}

std::string CoreAudioBase::GetDeviceID(int index) const {
  if (index >= NumberOfEnumeratedDevices()) {
    RTC_LOG(LS_ERROR) << "Invalid device index";
    return std::string();
  }

  std::string device_id;
  if (IsDefaultDevice(index)) {
    device_id = IsInput() ? core_audio_utility::GetDefaultInputDeviceID()
                          : core_audio_utility::GetDefaultOutputDeviceID();
  } else if (IsDefaultCommunicationsDevice(index)) {
    device_id = IsInput()
                    ? core_audio_utility::GetCommunicationsInputDeviceID()
                    : core_audio_utility::GetCommunicationsOutputDeviceID();
  } else {
    AudioDeviceNames device_names;
    bool ok = IsInput()
                  ? core_audio_utility::GetInputDeviceNames(&device_names)
                  : core_audio_utility::GetOutputDeviceNames(&device_names);
    if (ok) {
      device_id = device_names[index].unique_id;
    }
  }
  return device_id;
}

int CoreAudioBase::DeviceName(int index,
                              std::string* name,
                              std::string* guid) const {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  if (index > NumberOfEnumeratedDevices() - 1) {
    RTC_LOG(LS_ERROR) << "Invalid device index";
    return -1;
  }

  AudioDeviceNames device_names;
  bool ok = IsInput() ? core_audio_utility::GetInputDeviceNames(&device_names)
                      : core_audio_utility::GetOutputDeviceNames(&device_names);
  if (!ok) {
    RTC_LOG(LS_ERROR) << "Failed to get the device name";
    return -1;
  }

  *name = device_names[index].device_name;
  RTC_DLOG(INFO) << "name: " << *name;
  if (guid != nullptr) {
    *guid = device_names[index].unique_id;
    RTC_DLOG(INFO) << "guid: " << guid;
  }
  return 0;
}

bool CoreAudioBase::Init() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DCHECK(!device_id_.empty());
  RTC_DCHECK(audio_device_buffer_);
  RTC_DCHECK(!audio_client_.Get());

  // Use an existing |device_id_| and set parameters which are required to
  // create an audio client. It is up to the parent class to set |device_id_|.
  // TODO(henrika): improve device notification.
  std::string device_id = device_id_;
  ERole role = eConsole;
  if (IsDefaultDevice(device_id)) {
    device_id = AudioDeviceName::kDefaultDeviceId;
    role = eConsole;
  } else if (IsDefaultCommunicationsDevice(device_id)) {
    device_id = AudioDeviceName::kDefaultCommunicationsDeviceId;
    role = eCommunications;
  }

  // Create an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  ComPtr<IAudioClient> audio_client =
      core_audio_utility::CreateClient(device_id, GetDataFlow(), role);
  if (!audio_client.Get()) {
    return false;
  }

  // Retrieve preferred audio input or output parameters for the given client.
  AudioParameters params;
  if (FAILED(core_audio_utility::GetPreferredAudioParameters(audio_client.Get(),
                                                             &params))) {
    return false;
  }

  // Define the output WAVEFORMATEXTENSIBLE format in |format_|.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = rtc::dchecked_cast<WORD>(params.channels());
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = rtc::dchecked_cast<WORD>(params.bits_per_sample());
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  // Add the parts which are unique for the WAVE_FORMAT_EXTENSIBLE structure.
  format_.Samples.wValidBitsPerSample =
      rtc::dchecked_cast<WORD>(params.bits_per_sample());
  // TODO(henrika): improve (common for input and output?)
  format_.dwChannelMask = params.channels() == 1
                              ? SPEAKER_FRONT_CENTER
                              : SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  RTC_DLOG(INFO) << core_audio_utility::WaveFormatExToString(&format_);

  // Verify that the format is supported.
  if (!core_audio_utility::IsFormatSupported(
          audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &format_)) {
    return false;
  }

  // Initialize the audio stream between the client and the device in shared
  // mode using event-driven buffer handling.
  if (FAILED(core_audio_utility::SharedModeInitialize(
          audio_client.Get(), &format_, audio_samples_event_,
          &endpoint_buffer_size_frames_))) {
    return false;
  }

  // Check device period and the preferred buffer size and log a warning if
  // WebRTC's buffer size is not an even divisor of the preferred buffer size
  // in Core Audio.
  // TODO(henrik): sort out if a non-perfect match really is an issue.
  REFERENCE_TIME device_period;
  if (FAILED(core_audio_utility::GetDevicePeriod(
          audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &device_period))) {
    return false;
  }
  const double device_period_in_seconds =
      static_cast<double>(
          core_audio_utility::ReferenceTimeToTimeDelta(device_period).ms()) /
      1000.0L;
  const int preferred_frames_per_buffer =
      static_cast<int>(params.sample_rate() * device_period_in_seconds + 0.5);
  RTC_DLOG(INFO) << "preferred_frames_per_buffer: "
                 << preferred_frames_per_buffer;
  if (preferred_frames_per_buffer % params.frames_per_buffer()) {
    RTC_LOG(WARNING) << "Buffer size of " << params.frames_per_buffer()
                     << " is not an even divisor of "
                     << preferred_frames_per_buffer;
  }

  // Store valid COM interfaces.
  audio_client_ = audio_client;

  return true;
}

bool CoreAudioBase::Start() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";

  audio_thread_ = absl::make_unique<rtc::PlatformThread>(
      Run, this, IsInput() ? "wasapi_capture_thread" : "wasapi_render_thread",
      rtc::kRealtimePriority);
  audio_thread_->Start();
  if (!audio_thread_->IsRunning()) {
    StopThread();
    RTC_LOG(LS_ERROR) << "Failed to start audio thread";
    return false;
  }
  RTC_DLOG(INFO) << "Started thread with name: " << audio_thread_->name();

  // Start streaming data between the endpoint buffer and the audio engine.
  _com_error error = audio_client_->Start();
  if (error.Error() != S_OK) {
    StopThread();
    RTC_LOG(LS_ERROR) << "IAudioClient::Start failed: "
                      << core_audio_utility::ErrorToString(error);
    return false;
  }

  return true;
}

bool CoreAudioBase::Stop() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";

  // Stop streaming and the internal audio thread.
  _com_error error = audio_client_->Stop();
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Stop failed: "
                      << core_audio_utility::ErrorToString(error);
  }
  StopThread();

  // Flush all pending data and reset the audio clock stream position to 0.
  error = audio_client_->Reset();
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Reset failed: "
                      << core_audio_utility::ErrorToString(error);
  }

  if (IsOutput()) {
    // Extra safety check to ensure that the buffers are cleared.
    // If the buffers are not cleared correctly, the next call to Start()
    // would fail with AUDCLNT_E_BUFFER_ERROR at
    // IAudioRenderClient::GetBuffer().
    UINT32 num_queued_frames = 0;
    audio_client_->GetCurrentPadding(&num_queued_frames);
    RTC_DCHECK_EQ(0u, num_queued_frames);
  }

  return true;
}

bool CoreAudioBase::IsVolumeControlAvailable(bool* available) const {
  // A valid IAudioClient is required to access the ISimpleAudioVolume interface
  // properly. It is possible to use IAudioSessionManager::GetSimpleAudioVolume
  // as well but we use the audio client here to ensure that the initialized
  // audio session is visible under group box labeled "Applications" in
  // Sndvol.exe.
  if (!audio_client_.Get()) {
    return false;
  }

  // Try to create an ISimpleAudioVolume instance.
  ComPtr<ISimpleAudioVolume> audio_volume =
      core_audio_utility::CreateSimpleAudioVolume(audio_client_.Get());
  if (!audio_volume.Get()) {
    RTC_DLOG(LS_ERROR) << "Volume control is not supported";
    return false;
  }

  // Try to use the valid volume control.
  float volume = 0.0;
  _com_error error = audio_volume->GetMasterVolume(&volume);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "ISimpleAudioVolume::GetMasterVolume failed: "
                      << core_audio_utility::ErrorToString(error);
    *available = false;
  }
  RTC_DLOG(INFO) << "master volume for output audio session: " << volume;

  *available = true;
  return false;
}

void CoreAudioBase::StopThread() {
  RTC_DLOG(INFO) << __FUNCTION__;
  if (audio_thread_) {
    if (audio_thread_->IsRunning()) {
      RTC_DLOG(INFO) << "Sets stop_event...";
      SetEvent(stop_event_.Get());
      RTC_DLOG(INFO) << "PlatformThread::Stop...";
      audio_thread_->Stop();
    }
    audio_thread_.reset();

    // Ensure that we don't quit the main thread loop immediately next
    // time Start() is called.
    ResetEvent(stop_event_.Get());
  }
}

void CoreAudioBase::ThreadRun() {
  if (!core_audio_utility::IsMMCSSSupported()) {
    RTC_LOG(LS_ERROR) << "MMCSS is not supported";
    return;
  }
  RTC_DLOG(INFO) << "ThreadRun starts...";
  // TODO(henrika): difference between "Pro Audio" and "Audio"?
  ScopedMMCSSRegistration mmcss_registration(L"Pro Audio");
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  RTC_DCHECK(mmcss_registration.Succeeded());
  RTC_DCHECK(com_initializer.Succeeded());
  RTC_DCHECK(stop_event_.IsValid());
  RTC_DCHECK(audio_samples_event_.IsValid());

  bool streaming = true;
  bool error = false;
  HANDLE wait_array[] = {stop_event_.Get(), audio_samples_event_.Get()};

  // The device frequency is the frequency generated by the hardware clock in
  // the audio device. The GetFrequency() method reports a constant frequency.
  UINT64 device_frequency = 0;
  if (audio_clock_.Get()) {
    RTC_DCHECK(IsOutput());
    _com_error result = audio_clock_->GetFrequency(&device_frequency);
    if ((error = result.Error()) != S_OK) {
      RTC_LOG(LS_ERROR) << "IAudioClock::GetFrequency failed: "
                        << core_audio_utility::ErrorToString(error);
    }
  }

  // Keep streaming audio until the stop event or the stream-switch event
  // is signaled. An error event can also break the main thread loop.
  while (streaming && !error) {
    // Wait for a close-down event, stream-switch event or a new render event.
    DWORD wait_result = WaitForMultipleObjects(arraysize(wait_array),
                                               wait_array, false, INFINITE);
    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_event_| has been set.
        streaming = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |audio_samples_event_| has been set.
        error = !on_data_callback_(device_frequency);
        break;
      default:
        error = true;
        break;
    }
  }

  if (streaming && error) {
    RTC_LOG(LS_ERROR) << "WASAPI streaming failed.";
    // Stop audio streaming since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    audio_client_->Stop();

    // TODO(henrika): notify clients that something has gone wrong and that
    // this stream should be destroyed instead of reused in the future.
  }

  RTC_DLOG(INFO) << "...ThreadRun stops";
}

}  // namespace webrtc_win
}  // namespace webrtc
