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
#include "modules/audio_device/audio_device_buffer.h"

#include <memory>
#include <string>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/win/windows_version.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {
namespace webrtc_win {
namespace {

// Even if the device supports low latency and even if IAudioClient3 can be
// used (requires Win10 or higher), we currently disable any attempts to
// initialize the client for low-latency.
// TODO(henrika): more research is needed before we can enable low-latency.
const bool kEnableLowLatencyIfSupported = false;

// Each unit of reference time is 100 nanoseconds, hence |kReftimesPerSec|
// corresponds to one second.
// TODO(henrika): possibly add usage in Init().
// const REFERENCE_TIME kReferenceTimesPerSecond = 10000000;

enum DefaultDeviceType {
  kUndefined = -1,
  kDefault = 0,
  kDefaultCommunications = 1,
  kDefaultDeviceTypeMaxCount = kDefaultCommunications + 1,
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

const char* RoleToString(const ERole role) {
  switch (role) {
    case eConsole:
      return "Console";
    case eMultimedia:
      return "Multimedia";
    case eCommunications:
      return "Communications";
    default:
      return "Unsupported";
  }
}

std::string IndexToString(int index) {
  std::string ss = std::to_string(index);
  switch (index) {
    case kDefault:
      ss += " (Default)";
      break;
    case kDefaultCommunications:
      ss += " (Communications)";
      break;
    default:
      break;
  }
  return ss;
}

const char* SessionStateToString(AudioSessionState state) {
  switch (state) {
    case AudioSessionStateActive:
      return "Active";
    case AudioSessionStateInactive:
      return "Inactive";
    case AudioSessionStateExpired:
      return "Expired";
    default:
      return "Invalid";
  }
}

const char* SessionDisconnectReasonToString(
    AudioSessionDisconnectReason reason) {
  switch (reason) {
    case DisconnectReasonDeviceRemoval:
      return "DeviceRemoval";
    case DisconnectReasonServerShutdown:
      return "ServerShutdown";
    case DisconnectReasonFormatChanged:
      return "FormatChanged";
    case DisconnectReasonSessionLogoff:
      return "SessionLogoff";
    case DisconnectReasonSessionDisconnected:
      return "Disconnected";
    case DisconnectReasonExclusiveModeOverride:
      return "ExclusiveModeOverride";
    default:
      return "Invalid";
  }
}

void Run(void* obj) {
  RTC_DCHECK(obj);
  reinterpret_cast<CoreAudioBase*>(obj)->ThreadRun();
}

// Returns true if the selected audio device supports low latency, i.e, if it
// is possible to initialize the engine using periods less than the default
// period (10ms).
bool IsLowLatencySupported(IAudioClient3* client3,
                           const WAVEFORMATEXTENSIBLE* format,
                           uint32_t* min_period_in_frames) {
  RTC_DLOG(INFO) << __FUNCTION__;

  // Get the range of periodicities supported by the engine for the specified
  // stream format.
  uint32_t default_period = 0;
  uint32_t fundamental_period = 0;
  uint32_t min_period = 0;
  uint32_t max_period = 0;
  if (FAILED(core_audio_utility::GetSharedModeEnginePeriod(
          client3, format, &default_period, &fundamental_period, &min_period,
          &max_period))) {
    return false;
  }

  // Low latency is supported if the shortest allowed period is less than the
  // default engine period.
  // TODO(henrika): verify that this assumption is correct.
  const bool low_latency = min_period < default_period;
  RTC_LOG(INFO) << "low_latency: " << low_latency;
  *min_period_in_frames = low_latency ? min_period : 0;
  return low_latency;
}

}  // namespace

CoreAudioBase::CoreAudioBase(Direction direction,
                             bool automatic_restart,
                             OnDataCallback data_callback,
                             OnErrorCallback error_callback)
    : format_(),
      direction_(direction),
      automatic_restart_(automatic_restart),
      on_data_callback_(data_callback),
      on_error_callback_(error_callback),
      device_index_(kUndefined),
      is_restarting_(false) {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction) << "]";
  RTC_DLOG(INFO) << "Automatic restart: " << automatic_restart;
  RTC_DLOG(INFO) << "Windows version: " << rtc::rtc_win::GetVersion();

  // Create the event which the audio engine will signal each time a buffer
  // becomes ready to be processed by the client.
  audio_samples_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(audio_samples_event_.IsValid());

  // Event to be set in Stop() when rendering/capturing shall stop.
  stop_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(stop_event_.IsValid());

  // Event to be set when it has been detected that an active device has been
  // invalidated or the stream format has changed.
  restart_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(restart_event_.IsValid());
}

CoreAudioBase::~CoreAudioBase() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_EQ(ref_count_, 1);
}

EDataFlow CoreAudioBase::GetDataFlow() const {
  return direction_ == CoreAudioBase::Direction::kOutput ? eRender : eCapture;
}

bool CoreAudioBase::IsRestarting() const {
  return is_restarting_;
}

int64_t CoreAudioBase::TimeSinceStart() const {
  return rtc::TimeSince(start_time_);
}

int CoreAudioBase::NumberOfActiveDevices() const {
  return core_audio_utility::NumberOfActiveDevices(GetDataFlow());
}

int CoreAudioBase::NumberOfEnumeratedDevices() const {
  const int num_active = NumberOfActiveDevices();
  return num_active > 0 ? num_active + kDefaultDeviceTypeMaxCount : 0;
}

void CoreAudioBase::ReleaseCOMObjects() {
  RTC_DLOG(INFO) << __FUNCTION__;
  // ComPtr::Reset() sets the ComPtr to nullptr releasing any previous
  // reference.
  if (audio_client_) {
    audio_client_.Reset();
  }
  if (audio_clock_.Get()) {
    audio_clock_.Reset();
  }
  if (audio_session_control_.Get()) {
    audio_session_control_.Reset();
  }
}

bool CoreAudioBase::IsDefaultDevice(int index) const {
  return index == kDefault;
}

bool CoreAudioBase::IsDefaultCommunicationsDevice(int index) const {
  return index == kDefaultCommunications;
}

bool CoreAudioBase::IsDefaultDeviceId(const std::string& device_id) const {
  // Returns true if |device_id| corresponds to the id of the default
  // device. Note that, if only one device is available (or if the user has not
  // explicitly set a default device), |device_id| will also math
  // IsDefaultCommunicationsDeviceId().
  return (IsInput() &&
          (device_id == core_audio_utility::GetDefaultInputDeviceID())) ||
         (IsOutput() &&
          (device_id == core_audio_utility::GetDefaultOutputDeviceID()));
}

bool CoreAudioBase::IsDefaultCommunicationsDeviceId(
    const std::string& device_id) const {
  // Returns true if |device_id| corresponds to the id of the default
  // communication device. Note that, if only one device is available (or if
  // the user has not explicitly set a communication device), |device_id| will
  // also math IsDefaultDeviceId().
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

int CoreAudioBase::SetDevice(int index) {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]: index=" << IndexToString(index);
  if (initialized_) {
    return -1;
  }

  std::string device_id = GetDeviceID(index);
  RTC_DLOG(INFO) << "index=" << IndexToString(index)
                 << " => device_id: " << device_id;
  device_index_ = index;
  device_id_ = device_id;

  return device_id_.empty() ? -1 : 0;
}

int CoreAudioBase::DeviceName(int index,
                              std::string* name,
                              std::string* guid) const {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]: index=" << IndexToString(index);
  if (index > NumberOfEnumeratedDevices() - 1) {
    RTC_LOG(LS_ERROR) << "Invalid device index";
    return -1;
  }

  AudioDeviceNames device_names;
  bool ok = IsInput() ? core_audio_utility::GetInputDeviceNames(&device_names)
                      : core_audio_utility::GetOutputDeviceNames(&device_names);
  // Validate the index one extra time in-case the size of the generated list
  // did not match NumberOfEnumeratedDevices().
  if (!ok || static_cast<int>(device_names.size()) <= index) {
    RTC_LOG(LS_ERROR) << "Failed to get the device name";
    return -1;
  }

  *name = device_names[index].device_name;
  RTC_DLOG(INFO) << "name: " << *name;
  if (guid != nullptr) {
    *guid = device_names[index].unique_id;
    RTC_DLOG(INFO) << "guid: " << *guid;
  }
  return 0;
}

bool CoreAudioBase::Init() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DCHECK_GE(device_index_, 0);
  RTC_DCHECK(!device_id_.empty());
  RTC_DCHECK(audio_device_buffer_);
  RTC_DCHECK(!audio_client_);
  RTC_DCHECK(!audio_session_control_.Get());

  // Use an existing combination of |device_index_| and |device_id_| to set
  // parameters which are required to create an audio client. It is up to the
  // parent class to set |device_index_| and |device_id_|.
  std::string device_id = AudioDeviceName::kDefaultDeviceId;
  ERole role = ERole();
  if (IsDefaultDevice(device_index_)) {
    role = eConsole;
  } else if (IsDefaultCommunicationsDevice(device_index_)) {
    role = eCommunications;
  } else {
    device_id = device_id_;
  }
  RTC_LOG(LS_INFO) << "Unique device identifier: device_id=" << device_id
                   << ", role=" << RoleToString(role);

  // Create an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  ComPtr<IAudioClient> audio_client;
  if (core_audio_utility::GetAudioClientVersion() == 3) {
    RTC_DLOG(INFO) << "Using IAudioClient3";
    audio_client =
        core_audio_utility::CreateClient3(device_id, GetDataFlow(), role);
  } else if (core_audio_utility::GetAudioClientVersion() == 2) {
    RTC_DLOG(INFO) << "Using IAudioClient2";
    audio_client =
        core_audio_utility::CreateClient2(device_id, GetDataFlow(), role);
  } else {
    RTC_DLOG(INFO) << "Using IAudioClient";
    audio_client =
        core_audio_utility::CreateClient(device_id, GetDataFlow(), role);
  }
  if (!audio_client) {
    return false;
  }

  // Set extra client properties before initialization if the audio client
  // supports it.
  // TODO(henrika): evaluate effect(s) of making these changes. Also, perhaps
  // these types of settings belongs to the client and not the utility parts.
  if (core_audio_utility::GetAudioClientVersion() >= 2) {
    if (FAILED(core_audio_utility::SetClientProperties(
            static_cast<IAudioClient2*>(audio_client.Get())))) {
      return false;
    }
  }

  // Retrieve preferred audio input or output parameters for the given client
  // and the specified client properties. Override the preferred rate if sample
  // rate has been defined by the user. Rate conversion will be performed by
  // the audio engine to match the client if needed.
  AudioParameters params;
  HRESULT res = sample_rate_ ? core_audio_utility::GetPreferredAudioParameters(
                                   audio_client.Get(), &params, *sample_rate_)
                             : core_audio_utility::GetPreferredAudioParameters(
                                   audio_client.Get(), &params);
  if (FAILED(res)) {
    return false;
  }

  // Define the output WAVEFORMATEXTENSIBLE format in |format_|.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  // Check the preferred channel configuration and request implicit channel
  // upmixing (audio engine extends from 2 to N channels internally) if the
  // preferred number of channels is larger than two; i.e., initialize the
  // stream in stereo even if the preferred configuration is multi-channel.
  if (params.channels() <= 2) {
    format->nChannels = rtc::dchecked_cast<WORD>(params.channels());
  } else {
    // TODO(henrika): ensure that this approach works on different multi-channel
    // devices. Verified on:
    // - Corsair VOID PRO Surround USB Adapter (supports 7.1)
    RTC_LOG(LS_WARNING)
        << "Using channel upmixing in WASAPI audio engine (2 => "
        << params.channels() << ")";
    format->nChannels = 2;
  }
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = rtc::dchecked_cast<WORD>(params.bits_per_sample());
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  // Add the parts which are unique for the WAVE_FORMAT_EXTENSIBLE structure.
  format_.Samples.wValidBitsPerSample =
      rtc::dchecked_cast<WORD>(params.bits_per_sample());
  format_.dwChannelMask =
      format->nChannels == 1 ? KSAUDIO_SPEAKER_MONO : KSAUDIO_SPEAKER_STEREO;
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  RTC_DLOG(INFO) << core_audio_utility::WaveFormatToString(&format_);

  // Verify that the format is supported but exclude the test if the default
  // sample rate has been overridden. If so, the WASAPI audio engine will do
  // any necessary conversions between the client format we have given it and
  // the playback mix format or recording split format.
  if (!sample_rate_) {
    if (!core_audio_utility::IsFormatSupported(
            audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &format_)) {
      return false;
    }
  }

  // Check if low-latency is supported and use special initialization if it is.
  // Low-latency initialization requires these things:
  // - IAudioClient3 (>= Win10)
  // - HDAudio driver
  // - kEnableLowLatencyIfSupported changed from false (default) to true.
  // TODO(henrika): IsLowLatencySupported() returns AUDCLNT_E_UNSUPPORTED_FORMAT
  // when |sample_rate_.has_value()| returns true if rate conversion is
  // actually required (i.e., client asks for other than the default rate).
  bool low_latency_support = false;
  uint32_t min_period_in_frames = 0;
  if (kEnableLowLatencyIfSupported &&
      core_audio_utility::GetAudioClientVersion() >= 3) {
    low_latency_support =
        IsLowLatencySupported(static_cast<IAudioClient3*>(audio_client.Get()),
                              &format_, &min_period_in_frames);
  }

  if (low_latency_support) {
    RTC_DCHECK_GE(core_audio_utility::GetAudioClientVersion(), 3);
    // Use IAudioClient3::InitializeSharedAudioStream() API to initialize a
    // low-latency event-driven client. Request the smallest possible
    // periodicity.
    // TODO(henrika): evaluate this scheme in terms of CPU etc.
    if (FAILED(core_audio_utility::SharedModeInitializeLowLatency(
            static_cast<IAudioClient3*>(audio_client.Get()), &format_,
            audio_samples_event_, min_period_in_frames,
            sample_rate_.has_value(), &endpoint_buffer_size_frames_))) {
      return false;
    }
  } else {
    // Initialize the audio stream between the client and the device in shared
    // mode using event-driven buffer handling. Also, using 0 as requested
    // buffer size results in a default (minimum) endpoint buffer size.
    // TODO(henrika): possibly increase |requested_buffer_size| to add
    // robustness.
    const REFERENCE_TIME requested_buffer_size = 0;
    if (FAILED(core_audio_utility::SharedModeInitialize(
            audio_client.Get(), &format_, audio_samples_event_,
            requested_buffer_size, sample_rate_.has_value(),
            &endpoint_buffer_size_frames_))) {
      return false;
    }
  }

  // Check device period and the preferred buffer size and log a warning if
  // WebRTC's buffer size is not an even divisor of the preferred buffer size
  // in Core Audio.
  // TODO(henrika): sort out if a non-perfect match really is an issue.
  // TODO(henrika): compare with IAudioClient3::GetSharedModeEnginePeriod().
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

  // Create an AudioSessionControl interface given the initialized client.
  // The IAudioControl interface enables a client to configure the control
  // parameters for an audio session and to monitor events in the session.
  ComPtr<IAudioSessionControl> audio_session_control =
      core_audio_utility::CreateAudioSessionControl(audio_client.Get());
  if (!audio_session_control.Get()) {
    return false;
  }

  // The Sndvol program displays volume and mute controls for sessions that
  // are in the active and inactive states.
  AudioSessionState state;
  if (FAILED(audio_session_control->GetState(&state))) {
    return false;
  }
  RTC_DLOG(INFO) << "audio session state: " << SessionStateToString(state);
  RTC_DCHECK_EQ(state, AudioSessionStateInactive);

  // Register the client to receive notifications of session events, including
  // changes in the stream state.
  if (FAILED(audio_session_control->RegisterAudioSessionNotification(this))) {
    return false;
  }

  // Store valid COM interfaces.
  audio_client_ = audio_client;
  audio_session_control_ = audio_session_control;

  return true;
}

bool CoreAudioBase::Start() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  if (IsRestarting()) {
    // Audio thread should be alive during internal restart since the restart
    // callback is triggered on that thread and it also makes the restart
    // sequence less complex.
    RTC_DCHECK(audio_thread_);
  }

  // Start an audio thread but only if one does not already exist (which is the
  // case during restart).
  if (!audio_thread_) {
    audio_thread_ = std::make_unique<rtc::PlatformThread>(
        Run, this, IsInput() ? "wasapi_capture_thread" : "wasapi_render_thread",
        rtc::kRealtimePriority);
    RTC_DCHECK(audio_thread_);
    audio_thread_->Start();
    if (!audio_thread_->IsRunning()) {
      StopThread();
      RTC_LOG(LS_ERROR) << "Failed to start audio thread";
      return false;
    }
    RTC_DLOG(INFO) << "Started thread with name: " << audio_thread_->name()
                   << " and id: " << audio_thread_->GetThreadRef();
  }

  // Start streaming data between the endpoint buffer and the audio engine.
  _com_error error = audio_client_->Start();
  if (FAILED(error.Error())) {
    StopThread();
    RTC_LOG(LS_ERROR) << "IAudioClient::Start failed: "
                      << core_audio_utility::ErrorToString(error);
    return false;
  }

  start_time_ = rtc::TimeMillis();
  num_data_callbacks_ = 0;

  return true;
}

bool CoreAudioBase::Stop() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DLOG(INFO) << "total activity time: " << TimeSinceStart();

  // Stop audio streaming.
  _com_error error = audio_client_->Stop();
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Stop failed: "
                      << core_audio_utility::ErrorToString(error);
  }
  // Stop and destroy the audio thread but only when a restart attempt is not
  // ongoing.
  if (!IsRestarting()) {
    StopThread();
  }

  // Flush all pending data and reset the audio clock stream position to 0.
  error = audio_client_->Reset();
  if (FAILED(error.Error())) {
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

  // Delete the previous registration by the client to receive notifications
  // about audio session events.
  RTC_DLOG(INFO) << "audio session state: "
                 << SessionStateToString(GetAudioSessionState());
  error = audio_session_control_->UnregisterAudioSessionNotification(this);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR)
        << "IAudioSessionControl::UnregisterAudioSessionNotification failed: "
        << core_audio_utility::ErrorToString(error);
  }

  // To ensure that the restart process is as simple as possible, the audio
  // thread is not destroyed during restart attempts triggered by internal
  // error callbacks.
  if (!IsRestarting()) {
    thread_checker_audio_.Detach();
  }

  // Release all allocated COM interfaces to allow for a restart without
  // intermediate destruction.
  ReleaseCOMObjects();

  return true;
}

bool CoreAudioBase::IsVolumeControlAvailable(bool* available) const {
  // A valid IAudioClient is required to access the ISimpleAudioVolume interface
  // properly. It is possible to use IAudioSessionManager::GetSimpleAudioVolume
  // as well but we use the audio client here to ensure that the initialized
  // audio session is visible under group box labeled "Applications" in
  // Sndvol.exe.
  if (!audio_client_) {
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

// Internal test method which can be used in tests to emulate a restart signal.
// It simply sets the same event which is normally triggered by session and
// device notifications. Hence, the emulated restart sequence covers most parts
// of a real sequence expect the actual device switch.
bool CoreAudioBase::Restart() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  if (!automatic_restart()) {
    return false;
  }
  is_restarting_ = true;
  SetEvent(restart_event_.Get());
  return true;
}

void CoreAudioBase::StopThread() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK(!IsRestarting());
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
    ResetEvent(restart_event_.Get());
  }
}

bool CoreAudioBase::HandleRestartEvent() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DCHECK_RUN_ON(&thread_checker_audio_);
  RTC_DCHECK(audio_thread_);
  RTC_DCHECK(IsRestarting());
  // Let each client (input and/or output) take care of its own restart
  // sequence since each side might need unique actions.
  // TODO(henrika): revisit and investigate if one common base implementation
  // is possible
  bool restart_ok = on_error_callback_(ErrorType::kStreamDisconnected);
  is_restarting_ = false;
  return restart_ok;
}

bool CoreAudioBase::SwitchDeviceIfNeeded() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DCHECK_RUN_ON(&thread_checker_audio_);
  RTC_DCHECK(IsRestarting());

  RTC_DLOG(INFO) << "device_index=" << device_index_
                 << " => device_id: " << device_id_;

  // Ensure that at least one device exists and can be utilized. The most
  // probable cause for ending up here is that a device has been removed.
  if (core_audio_utility::NumberOfActiveDevices(IsInput() ? eCapture
                                                          : eRender) < 1) {
    RTC_DLOG(LS_ERROR) << "All devices are disabled or removed";
    return false;
  }

  // Get the unique device ID for the index which is currently used. It seems
  // safe to assume that if the ID is the same as the existing device ID, then
  // the device configuration is the same as before.
  std::string device_id = GetDeviceID(device_index_);
  if (device_id != device_id_) {
    RTC_LOG(LS_WARNING)
        << "Device configuration has changed => changing device selection...";
    // TODO(henrika): depending on the current state and how we got here, we
    // must select a new device here.
    if (SetDevice(kDefault) == -1) {
      RTC_LOG(LS_WARNING) << "Failed to set new audio device";
      return false;
    }
  } else {
    RTC_LOG(INFO)
        << "Device configuration has not changed => keeping selected device";
  }
  return true;
}

AudioSessionState CoreAudioBase::GetAudioSessionState() const {
  AudioSessionState state = AudioSessionStateInactive;
  RTC_DCHECK(audio_session_control_.Get());
  _com_error error = audio_session_control_->GetState(&state);
  if (FAILED(error.Error())) {
    RTC_DLOG(LS_ERROR) << "IAudioSessionControl::GetState failed: "
                       << core_audio_utility::ErrorToString(error);
  }
  return state;
}

// TODO(henrika): only used for debugging purposes currently.
ULONG CoreAudioBase::AddRef() {
  ULONG new_ref = InterlockedIncrement(&ref_count_);
  // RTC_DLOG(INFO) << "__AddRef => " << new_ref;
  return new_ref;
}

// TODO(henrika): does not call delete this.
ULONG CoreAudioBase::Release() {
  ULONG new_ref = InterlockedDecrement(&ref_count_);
  // RTC_DLOG(INFO) << "__Release => " << new_ref;
  return new_ref;
}

// TODO(henrika): can probably be replaced by "return S_OK" only.
HRESULT CoreAudioBase::QueryInterface(REFIID iid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }
  if (iid == IID_IUnknown || iid == __uuidof(IAudioSessionEvents)) {
    *object = static_cast<IAudioSessionEvents*>(this);
    return S_OK;
  };
  *object = nullptr;
  return E_NOINTERFACE;
}

// IAudioSessionEvents::OnStateChanged.
HRESULT CoreAudioBase::OnStateChanged(AudioSessionState new_state) {
  RTC_DLOG(INFO) << "___" << __FUNCTION__ << "["
                 << DirectionToString(direction())
                 << "] new_state: " << SessionStateToString(new_state);
  return S_OK;
}

// When a session is disconnected because of a device removal or format change
// event, we want to inform the audio thread about the lost audio session and
// trigger an attempt to restart audio using a new (default) device.
// This method is called on separate threads owned by the session manager and
// it can happen that the same type of callback is called more than once for the
// same event.
HRESULT CoreAudioBase::OnSessionDisconnected(
    AudioSessionDisconnectReason disconnect_reason) {
  RTC_DLOG(INFO) << "___" << __FUNCTION__ << "["
                 << DirectionToString(direction()) << "] reason: "
                 << SessionDisconnectReasonToString(disconnect_reason);
  // Ignore changes in the audio session (don't try to restart) if the user
  // has explicitly asked for this type of ADM during construction.
  if (!automatic_restart()) {
    RTC_DLOG(LS_WARNING) << "___Automatic restart is disabled";
    return S_OK;
  }

  if (IsRestarting()) {
    RTC_DLOG(LS_WARNING) << "___Ignoring since restart is already active";
    return S_OK;
  }

  // By default, automatic restart is enabled and the restart event will be set
  // below if the device was removed or the format was changed.
  if (disconnect_reason == DisconnectReasonDeviceRemoval ||
      disconnect_reason == DisconnectReasonFormatChanged) {
    is_restarting_ = true;
    SetEvent(restart_event_.Get());
  }
  return S_OK;
}

// IAudioSessionEvents::OnDisplayNameChanged
HRESULT CoreAudioBase::OnDisplayNameChanged(LPCWSTR new_display_name,
                                            LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnIconPathChanged
HRESULT CoreAudioBase::OnIconPathChanged(LPCWSTR new_icon_path,
                                         LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnSimpleVolumeChanged
HRESULT CoreAudioBase::OnSimpleVolumeChanged(float new_simple_volume,
                                             BOOL new_mute,
                                             LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnChannelVolumeChanged
HRESULT CoreAudioBase::OnChannelVolumeChanged(DWORD channel_count,
                                              float new_channel_volumes[],
                                              DWORD changed_channel,
                                              LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnGroupingParamChanged
HRESULT CoreAudioBase::OnGroupingParamChanged(LPCGUID new_grouping_param,
                                              LPCGUID event_context) {
  return S_OK;
}

void CoreAudioBase::ThreadRun() {
  if (!core_audio_utility::IsMMCSSSupported()) {
    RTC_LOG(LS_ERROR) << "MMCSS is not supported";
    return;
  }
  RTC_DLOG(INFO) << "[" << DirectionToString(direction())
                 << "] ThreadRun starts...";
  // TODO(henrika): difference between "Pro Audio" and "Audio"?
  ScopedMMCSSRegistration mmcss_registration(L"Pro Audio");
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  RTC_DCHECK(mmcss_registration.Succeeded());
  RTC_DCHECK(com_initializer.Succeeded());
  RTC_DCHECK(stop_event_.IsValid());
  RTC_DCHECK(audio_samples_event_.IsValid());

  bool streaming = true;
  bool error = false;
  HANDLE wait_array[] = {stop_event_.Get(), restart_event_.Get(),
                         audio_samples_event_.Get()};

  // The device frequency is the frequency generated by the hardware clock in
  // the audio device. The GetFrequency() method reports a constant frequency.
  UINT64 device_frequency = 0;
  _com_error result(S_FALSE);
  if (audio_clock_) {
    RTC_DCHECK(IsOutput());
    result = audio_clock_->GetFrequency(&device_frequency);
    if (FAILED(result.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioClock::GetFrequency failed: "
                        << core_audio_utility::ErrorToString(result);
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
        // |restart_event_| has been set.
        error = !HandleRestartEvent();
        break;
      case WAIT_OBJECT_0 + 2:
        // |audio_samples_event_| has been set.
        error = !on_data_callback_(device_frequency);
        break;
      default:
        error = true;
        break;
    }
  }

  if (streaming && error) {
    RTC_LOG(LS_ERROR) << "[" << DirectionToString(direction())
                      << "] WASAPI streaming failed.";
    // Stop audio streaming since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    result = audio_client_->Stop();
    if (FAILED(result.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioClient::Stop failed: "
                        << core_audio_utility::ErrorToString(result);
    }

    // TODO(henrika): notify clients that something has gone wrong and that
    // this stream should be destroyed instead of reused in the future.
  }

  RTC_DLOG(INFO) << "[" << DirectionToString(direction())
                 << "] ...ThreadRun stops";
}

}  // namespace webrtc_win
}  // namespace webrtc
