/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/win/core_audio_utility_win.h"

#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <tchar.h>

#include <iomanip>
#include <string>
#include <utility>

#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/win/windows_version.h"

using Microsoft::WRL::ComPtr;
using webrtc::AudioDeviceName;
using webrtc::AudioParameters;

namespace webrtc {
namespace webrtc_win {
namespace {

using core_audio_utility::ErrorToString;

// Converts from channel mask to list of included channels.
// Each audio data format contains channels for one or more of the positions
// listed below. The number of channels simply equals the number of nonzero
// flag bits in the `channel_mask`. The relative positions of the channels
// within each block of audio data always follow the same relative ordering
// as the flag bits in the table below. For example, if `channel_mask` contains
// the value 0x00000033, the format defines four audio channels that are
// assigned for playback to the front-left, front-right, back-left,
// and back-right speakers, respectively. The channel data should be interleaved
// in that order within each block.
std::string ChannelMaskToString(DWORD channel_mask) {
  std::string ss;
  int n = 0;
  if (channel_mask & SPEAKER_FRONT_LEFT) {
    ss += "FRONT_LEFT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_FRONT_RIGHT) {
    ss += "FRONT_RIGHT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_FRONT_CENTER) {
    ss += "FRONT_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_LOW_FREQUENCY) {
    ss += "LOW_FREQUENCY | ";
    ++n;
  }
  if (channel_mask & SPEAKER_BACK_LEFT) {
    ss += "BACK_LEFT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_BACK_RIGHT) {
    ss += "BACK_RIGHT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_FRONT_LEFT_OF_CENTER) {
    ss += "FRONT_LEFT_OF_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_FRONT_RIGHT_OF_CENTER) {
    ss += "RIGHT_OF_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_BACK_CENTER) {
    ss += "BACK_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_SIDE_LEFT) {
    ss += "SIDE_LEFT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_SIDE_RIGHT) {
    ss += "SIDE_RIGHT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_CENTER) {
    ss += "TOP_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_FRONT_LEFT) {
    ss += "TOP_FRONT_LEFT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_FRONT_CENTER) {
    ss += "TOP_FRONT_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_FRONT_RIGHT) {
    ss += "TOP_FRONT_RIGHT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_BACK_LEFT) {
    ss += "TOP_BACK_LEFT | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_BACK_CENTER) {
    ss += "TOP_BACK_CENTER | ";
    ++n;
  }
  if (channel_mask & SPEAKER_TOP_BACK_RIGHT) {
    ss += "TOP_BACK_RIGHT | ";
    ++n;
  }

  if (!ss.empty()) {
    // Delete last appended " | " substring.
    ss.erase(ss.end() - 3, ss.end());
  }
  ss += " (";
  ss += std::to_string(n);
  ss += ")";
  return ss;
}

#if !defined(KSAUDIO_SPEAKER_1POINT1)
// These values are only defined in ksmedia.h after a certain version, to build
// cleanly for older windows versions this just defines the ones that are
// missing.
#define KSAUDIO_SPEAKER_1POINT1 (SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_2POINT1 \
  (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_3POINT0 \
  (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER)
#define KSAUDIO_SPEAKER_3POINT1                                      \
  (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | \
   SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_5POINT0                                      \
  (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | \
   SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)
#define KSAUDIO_SPEAKER_7POINT0                                      \
  (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | \
   SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT |      \
   SPEAKER_SIDE_RIGHT)
#endif

#if !defined(AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY)
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

// Converts the most common format tags defined in mmreg.h into string
// equivalents. Mainly intended for log messages.
const char* WaveFormatTagToString(WORD format_tag) {
  switch (format_tag) {
    case WAVE_FORMAT_UNKNOWN:
      return "WAVE_FORMAT_UNKNOWN";
    case WAVE_FORMAT_PCM:
      return "WAVE_FORMAT_PCM";
    case WAVE_FORMAT_IEEE_FLOAT:
      return "WAVE_FORMAT_IEEE_FLOAT";
    case WAVE_FORMAT_EXTENSIBLE:
      return "WAVE_FORMAT_EXTENSIBLE";
    default:
      return "UNKNOWN";
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

const char* FlowToString(const EDataFlow flow) {
  switch (flow) {
    case eRender:
      return "Render";
    case eCapture:
      return "Capture";
    case eAll:
      return "Render or Capture";
    default:
      return "Unsupported";
  }
}

bool LoadAudiosesDll() {
  static const wchar_t* const kAudiosesDLL =
      L"%WINDIR%\\system32\\audioses.dll";
  wchar_t path[MAX_PATH] = {0};
  ExpandEnvironmentStringsW(kAudiosesDLL, path, arraysize(path));
  RTC_DLOG(INFO) << rtc::ToUtf8(path);
  return (LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) !=
          nullptr);
}

bool LoadAvrtDll() {
  static const wchar_t* const kAvrtDLL = L"%WINDIR%\\system32\\Avrt.dll";
  wchar_t path[MAX_PATH] = {0};
  ExpandEnvironmentStringsW(kAvrtDLL, path, arraysize(path));
  RTC_DLOG(INFO) << rtc::ToUtf8(path);
  return (LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) !=
          nullptr);
}

ComPtr<IMMDeviceEnumerator> CreateDeviceEnumeratorInternal(
    bool allow_reinitialize) {
  ComPtr<IMMDeviceEnumerator> device_enumerator;
  _com_error error =
      ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&device_enumerator));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "CoCreateInstance failed: " << ErrorToString(error);
  }

  if (error.Error() == CO_E_NOTINITIALIZED && allow_reinitialize) {
    RTC_LOG(LS_ERROR) << "CoCreateInstance failed with CO_E_NOTINITIALIZED";
    // We have seen crashes which indicates that this method can in fact
    // fail with CO_E_NOTINITIALIZED in combination with certain 3rd party
    // modules. Calling CoInitializeEx() is an attempt to resolve the reported
    // issues. See http://crbug.com/378465 for details.
    error = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(error.Error())) {
      error = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                 CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator));
      if (FAILED(error.Error())) {
        RTC_LOG(LS_ERROR) << "CoCreateInstance failed: "
                          << ErrorToString(error);
      }
    }
  }
  return device_enumerator;
}

bool IsSupportedInternal() {
  // The Core Audio APIs are implemented in the user-mode system components
  // Audioses.dll and Mmdevapi.dll. Dependency Walker shows that it is
  // enough to verify possibility to load the Audioses DLL since it depends
  // on Mmdevapi.dll. See http://crbug.com/166397 why this extra step is
  // required to guarantee Core Audio support.
  if (!LoadAudiosesDll())
    return false;

  // Being able to load the Audioses.dll does not seem to be sufficient for
  // all devices to guarantee Core Audio support. To be 100%, we also verify
  // that it is possible to a create the IMMDeviceEnumerator interface. If
  // this works as well we should be home free.
  ComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumeratorInternal(false);
  if (!device_enumerator) {
    RTC_LOG(LS_ERROR)
        << "Failed to create Core Audio device enumerator on thread with ID "
        << rtc::CurrentThreadId();
    return false;
  }

  return true;
}

bool IsDeviceActive(IMMDevice* device) {
  DWORD state = DEVICE_STATE_DISABLED;
  return SUCCEEDED(device->GetState(&state)) && (state & DEVICE_STATE_ACTIVE);
}

// Retrieve an audio device specified by `device_id` or a default device
// specified by data-flow direction and role if `device_id` is default.
ComPtr<IMMDevice> CreateDeviceInternal(const std::string& device_id,
                                       EDataFlow data_flow,
                                       ERole role) {
  RTC_DLOG(INFO) << "CreateDeviceInternal: "
                    "id="
                 << device_id << ", flow=" << FlowToString(data_flow)
                 << ", role=" << RoleToString(role);
  ComPtr<IMMDevice> audio_endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ComPtr<IMMDeviceEnumerator> device_enum(CreateDeviceEnumeratorInternal(true));
  if (!device_enum.Get())
    return audio_endpoint_device;

  _com_error error(S_FALSE);
  if (device_id == AudioDeviceName::kDefaultDeviceId) {
    // Get the default audio endpoint for the specified data-flow direction and
    // role. Note that, if only a single rendering or capture device is
    // available, the system always assigns all three rendering or capture roles
    // to that device. If the method fails to find a rendering or capture device
    // for the specified role, this means that no rendering or capture device is
    // available at all. If no device is available, the method sets the output
    // pointer to NULL and returns ERROR_NOT_FOUND.
    error = device_enum->GetDefaultAudioEndpoint(
        data_flow, role, audio_endpoint_device.GetAddressOf());
    if (FAILED(error.Error())) {
      RTC_LOG(LS_ERROR)
          << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: "
          << ErrorToString(error);
    }
  } else {
    // Ask for an audio endpoint device that is identified by an endpoint ID
    // string.
    error = device_enum->GetDevice(rtc::ToUtf16(device_id).c_str(),
                                   audio_endpoint_device.GetAddressOf());
    if (FAILED(error.Error())) {
      RTC_LOG(LS_ERROR) << "IMMDeviceEnumerator::GetDevice failed: "
                        << ErrorToString(error);
    }
  }

  // Verify that the audio endpoint device is active, i.e., that the audio
  // adapter that connects to the endpoint device is present and enabled.
  if (SUCCEEDED(error.Error()) && audio_endpoint_device.Get() &&
      !IsDeviceActive(audio_endpoint_device.Get())) {
    RTC_LOG(LS_WARNING) << "Selected endpoint device is not active";
    audio_endpoint_device.Reset();
  }

  return audio_endpoint_device;
}

std::string GetDeviceIdInternal(IMMDevice* device) {
  // Retrieve unique name of endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
  LPWSTR device_id;
  if (SUCCEEDED(device->GetId(&device_id))) {
    std::string device_id_utf8 = rtc::ToUtf8(device_id, wcslen(device_id));
    CoTaskMemFree(device_id);
    return device_id_utf8;
  } else {
    return std::string();
  }
}

std::string GetDeviceFriendlyNameInternal(IMMDevice* device) {
  // Retrieve user-friendly name of endpoint device.
  // Example: "Microphone (Realtek High Definition Audio)".
  ComPtr<IPropertyStore> properties;
  HRESULT hr = device->OpenPropertyStore(STGM_READ, properties.GetAddressOf());
  if (FAILED(hr))
    return std::string();

  ScopedPropVariant friendly_name_pv;
  hr = properties->GetValue(PKEY_Device_FriendlyName,
                            friendly_name_pv.Receive());
  if (FAILED(hr))
    return std::string();

  if (friendly_name_pv.get().vt == VT_LPWSTR &&
      friendly_name_pv.get().pwszVal) {
    return rtc::ToUtf8(friendly_name_pv.get().pwszVal,
                       wcslen(friendly_name_pv.get().pwszVal));
  } else {
    return std::string();
  }
}

ComPtr<IAudioSessionManager2> CreateSessionManager2Internal(
    IMMDevice* audio_device) {
  if (!audio_device)
    return ComPtr<IAudioSessionManager2>();

  ComPtr<IAudioSessionManager2> audio_session_manager;
  _com_error error =
      audio_device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                             nullptr, &audio_session_manager);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDevice::Activate(IAudioSessionManager2) failed: "
                      << ErrorToString(error);
  }
  return audio_session_manager;
}

ComPtr<IAudioSessionEnumerator> CreateSessionEnumeratorInternal(
    IMMDevice* audio_device) {
  if (!audio_device) {
    return ComPtr<IAudioSessionEnumerator>();
  }

  ComPtr<IAudioSessionEnumerator> audio_session_enumerator;
  ComPtr<IAudioSessionManager2> audio_session_manager =
      CreateSessionManager2Internal(audio_device);
  if (!audio_session_manager.Get()) {
    return audio_session_enumerator;
  }
  _com_error error =
      audio_session_manager->GetSessionEnumerator(&audio_session_enumerator);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR)
        << "IAudioSessionEnumerator::IAudioSessionEnumerator failed: "
        << ErrorToString(error);
    return ComPtr<IAudioSessionEnumerator>();
  }
  return audio_session_enumerator;
}

// Creates and activates an IAudioClient COM object given the selected
// endpoint device.
ComPtr<IAudioClient> CreateClientInternal(IMMDevice* audio_device) {
  if (!audio_device)
    return ComPtr<IAudioClient>();

  ComPtr<IAudioClient> audio_client;
  _com_error error = audio_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                            nullptr, &audio_client);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDevice::Activate(IAudioClient) failed: "
                      << ErrorToString(error);
  }
  return audio_client;
}

ComPtr<IAudioClient2> CreateClient2Internal(IMMDevice* audio_device) {
  if (!audio_device)
    return ComPtr<IAudioClient2>();

  ComPtr<IAudioClient2> audio_client;
  _com_error error = audio_device->Activate(__uuidof(IAudioClient2), CLSCTX_ALL,
                                            nullptr, &audio_client);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDevice::Activate(IAudioClient2) failed: "
                      << ErrorToString(error);
  }
  return audio_client;
}

ComPtr<IAudioClient3> CreateClient3Internal(IMMDevice* audio_device) {
  if (!audio_device)
    return ComPtr<IAudioClient3>();

  ComPtr<IAudioClient3> audio_client;
  _com_error error = audio_device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL,
                                            nullptr, &audio_client);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDevice::Activate(IAudioClient3) failed: "
                      << ErrorToString(error);
  }
  return audio_client;
}

ComPtr<IMMDeviceCollection> CreateCollectionInternal(EDataFlow data_flow) {
  ComPtr<IMMDeviceEnumerator> device_enumerator(
      CreateDeviceEnumeratorInternal(true));
  if (!device_enumerator) {
    return ComPtr<IMMDeviceCollection>();
  }

  // Generate a collection of active (present and not disabled) audio endpoint
  // devices for the specified data-flow direction.
  // This method will succeed even if all devices are disabled.
  ComPtr<IMMDeviceCollection> collection;
  _com_error error = device_enumerator->EnumAudioEndpoints(
      data_flow, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDeviceCollection::EnumAudioEndpoints failed: "
                      << ErrorToString(error);
  }
  return collection;
}

bool GetDeviceNamesInternal(EDataFlow data_flow,
                            webrtc::AudioDeviceNames* device_names) {
  RTC_DLOG(LS_INFO) << "GetDeviceNamesInternal: flow="
                    << FlowToString(data_flow);

  // Generate a collection of active audio endpoint devices for the specified
  // direction.
  ComPtr<IMMDeviceCollection> collection = CreateCollectionInternal(data_flow);
  if (!collection.Get()) {
    RTC_LOG(LS_ERROR) << "Failed to create a collection of active devices";
    return false;
  }

  // Retrieve the number of active (present, not disabled and plugged in) audio
  // devices for the specified direction.
  UINT number_of_active_devices = 0;
  _com_error error = collection->GetCount(&number_of_active_devices);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDeviceCollection::GetCount failed: "
                      << ErrorToString(error);
    return false;
  }

  if (number_of_active_devices == 0) {
    RTC_DLOG(LS_WARNING) << "Found no active devices";
    return false;
  }

  // Loop over all active devices and add friendly name and unique id to the
  // `device_names` queue. For now, devices are added at indexes 0, 1, ..., N-1
  // but they will be moved to 2,3,..., N+1 at the next stage when default and
  // default communication devices are added at index 0 and 1.
  ComPtr<IMMDevice> audio_device;
  for (UINT i = 0; i < number_of_active_devices; ++i) {
    // Retrieve a pointer to the specified item in the device collection.
    error = collection->Item(i, audio_device.GetAddressOf());
    if (FAILED(error.Error())) {
      // Skip this item and try to get the next item instead; will result in an
      // incomplete list of devices.
      RTC_LOG(LS_WARNING) << "IMMDeviceCollection::Item failed: "
                          << ErrorToString(error);
      continue;
    }
    if (!audio_device.Get()) {
      RTC_LOG(LS_WARNING) << "Invalid audio device";
      continue;
    }

    // Retrieve the complete device name for the given audio device endpoint.
    AudioDeviceName device_name(
        GetDeviceFriendlyNameInternal(audio_device.Get()),
        GetDeviceIdInternal(audio_device.Get()));
    // Add combination of user-friendly and unique name to the output list.
    device_names->push_back(device_name);
  }

  // Log a warning of the list of device is not complete but let's keep on
  // trying to add default and default communications device at the front.
  if (device_names->size() != number_of_active_devices) {
    RTC_DLOG(LS_WARNING)
        << "List of device names does not contain all active devices";
  }

  // Avoid adding default and default communication devices if no active device
  // could be added to the queue. We might as well break here and return false
  // since no active devices were identified.
  if (device_names->empty()) {
    RTC_DLOG(LS_ERROR) << "List of active devices is empty";
    return false;
  }

  // Prepend the queue with two more elements: one for the default device and
  // one for the default communication device (can correspond to the same unique
  // id if only one active device exists). The first element (index 0) is the
  // default device and the second element (index 1) is the default
  // communication device.
  ERole role[] = {eCommunications, eConsole};
  ComPtr<IMMDevice> default_device;
  AudioDeviceName default_device_name;
  for (size_t i = 0; i < arraysize(role); ++i) {
    default_device = CreateDeviceInternal(AudioDeviceName::kDefaultDeviceId,
                                          data_flow, role[i]);
    if (!default_device.Get()) {
      // Add empty strings to device name if the device could not be created.
      RTC_DLOG(LS_WARNING) << "Failed to add device with role: "
                           << RoleToString(role[i]);
      default_device_name.device_name = std::string();
      default_device_name.unique_id = std::string();
    } else {
      // Populate the device name with friendly name and unique id.
      std::string device_name;
      device_name += (role[i] == eConsole ? "Default - " : "Communication - ");
      device_name += GetDeviceFriendlyNameInternal(default_device.Get());
      std::string unique_id = GetDeviceIdInternal(default_device.Get());
      default_device_name.device_name = std::move(device_name);
      default_device_name.unique_id = std::move(unique_id);
    }

    // Add combination of user-friendly and unique name to the output queue.
    // The last element (<=> eConsole) will be at the front of the queue, hence
    // at index 0. Empty strings will be added for cases where no default
    // devices were found.
    device_names->push_front(default_device_name);
  }

  // Example of log output when only one device is active. Note that the queue
  // contains two extra elements at index 0 (Default) and 1 (Communication) to
  // allow selection of device by role instead of id. All elements corresponds
  // the same unique id.
  // [0] friendly name: Default - Headset Microphone (2- Arctis 7 Chat)
  // [0] unique id    : {0.0.1.00000000}.{ff9eed76-196e-467a-b295-26986e69451c}
  // [1] friendly name: Communication - Headset Microphone (2- Arctis 7 Chat)
  // [1] unique id    : {0.0.1.00000000}.{ff9eed76-196e-467a-b295-26986e69451c}
  // [2] friendly name: Headset Microphone (2- Arctis 7 Chat)
  // [2] unique id    : {0.0.1.00000000}.{ff9eed76-196e-467a-b295-26986e69451c}
  for (size_t i = 0; i < device_names->size(); ++i) {
    RTC_DLOG(INFO) << "[" << i
                   << "] friendly name: " << (*device_names)[i].device_name;
    RTC_DLOG(INFO) << "[" << i
                   << "] unique id    : " << (*device_names)[i].unique_id;
  }

  return true;
}

HRESULT GetPreferredAudioParametersInternal(IAudioClient* client,
                                            AudioParameters* params,
                                            int fixed_sample_rate) {
  WAVEFORMATPCMEX mix_format;
  HRESULT hr = core_audio_utility::GetSharedModeMixFormat(client, &mix_format);
  if (FAILED(hr))
    return hr;

  REFERENCE_TIME default_period = 0;
  hr = core_audio_utility::GetDevicePeriod(client, AUDCLNT_SHAREMODE_SHARED,
                                           &default_period);
  if (FAILED(hr))
    return hr;

  int sample_rate = mix_format.Format.nSamplesPerSec;
  // Override default sample rate if `fixed_sample_rate` is set and different
  // from the default rate.
  if (fixed_sample_rate > 0 && fixed_sample_rate != sample_rate) {
    RTC_DLOG(INFO) << "Using fixed sample rate instead of the preferred: "
                   << sample_rate << " is replaced by " << fixed_sample_rate;
    sample_rate = fixed_sample_rate;
  }
  // TODO(henrika): utilize full mix_format.Format.wBitsPerSample.
  // const size_t bits_per_sample = AudioParameters::kBitsPerSample;
  // TODO(henrika): improve channel layout support.
  const size_t channels = mix_format.Format.nChannels;

  // Use the native device period to derive the smallest possible buffer size
  // in shared mode.
  double device_period_in_seconds =
      static_cast<double>(
          core_audio_utility::ReferenceTimeToTimeDelta(default_period).ms()) /
      1000.0L;
  const size_t frames_per_buffer =
      static_cast<size_t>(sample_rate * device_period_in_seconds + 0.5);

  AudioParameters audio_params(sample_rate, channels, frames_per_buffer);
  *params = audio_params;
  RTC_DLOG(INFO) << audio_params.ToString();

  return hr;
}

}  // namespace

namespace core_audio_utility {

// core_audio_utility::WaveFormatWrapper implementation.
WAVEFORMATEXTENSIBLE* WaveFormatWrapper::GetExtensible() const {
  RTC_CHECK(IsExtensible());
  return reinterpret_cast<WAVEFORMATEXTENSIBLE*>(ptr_);
}

bool WaveFormatWrapper::IsExtensible() const {
  return ptr_->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ptr_->cbSize >= 22;
}

bool WaveFormatWrapper::IsPcm() const {
  return IsExtensible() ? GetExtensible()->SubFormat == KSDATAFORMAT_SUBTYPE_PCM
                        : ptr_->wFormatTag == WAVE_FORMAT_PCM;
}

bool WaveFormatWrapper::IsFloat() const {
  return IsExtensible()
             ? GetExtensible()->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
             : ptr_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
}

size_t WaveFormatWrapper::size() const {
  return sizeof(*ptr_) + ptr_->cbSize;
}

bool IsSupported() {
  RTC_DLOG(INFO) << "IsSupported";
  static bool g_is_supported = IsSupportedInternal();
  return g_is_supported;
}

bool IsMMCSSSupported() {
  RTC_DLOG(INFO) << "IsMMCSSSupported";
  return LoadAvrtDll();
}

int NumberOfActiveDevices(EDataFlow data_flow) {
  // Generate a collection of active audio endpoint devices for the specified
  // data-flow direction.
  ComPtr<IMMDeviceCollection> collection = CreateCollectionInternal(data_flow);
  if (!collection.Get()) {
    return 0;
  }

  // Retrieve the number of active audio devices for the specified direction.
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  std::string str;
  if (data_flow == eCapture) {
    str = "Number of capture devices: ";
  } else if (data_flow == eRender) {
    str = "Number of render devices: ";
  } else if (data_flow == eAll) {
    str = "Total number of devices: ";
  }
  RTC_DLOG(INFO) << str << number_of_active_devices;
  return static_cast<int>(number_of_active_devices);
}

uint32_t GetAudioClientVersion() {
  uint32_t version = 1;
  if (rtc::rtc_win::GetVersion() >= rtc::rtc_win::VERSION_WIN10) {
    version = 3;
  } else if (rtc::rtc_win::GetVersion() >= rtc::rtc_win::VERSION_WIN8) {
    version = 2;
  }
  return version;
}

ComPtr<IMMDeviceEnumerator> CreateDeviceEnumerator() {
  RTC_DLOG(INFO) << "CreateDeviceEnumerator";
  return CreateDeviceEnumeratorInternal(true);
}

std::string GetDefaultInputDeviceID() {
  RTC_DLOG(INFO) << "GetDefaultInputDeviceID";
  ComPtr<IMMDevice> device(
      CreateDevice(AudioDeviceName::kDefaultDeviceId, eCapture, eConsole));
  return device.Get() ? GetDeviceIdInternal(device.Get()) : std::string();
}

std::string GetDefaultOutputDeviceID() {
  RTC_DLOG(INFO) << "GetDefaultOutputDeviceID";
  ComPtr<IMMDevice> device(
      CreateDevice(AudioDeviceName::kDefaultDeviceId, eRender, eConsole));
  return device.Get() ? GetDeviceIdInternal(device.Get()) : std::string();
}

std::string GetCommunicationsInputDeviceID() {
  RTC_DLOG(INFO) << "GetCommunicationsInputDeviceID";
  ComPtr<IMMDevice> device(CreateDevice(AudioDeviceName::kDefaultDeviceId,
                                        eCapture, eCommunications));
  return device.Get() ? GetDeviceIdInternal(device.Get()) : std::string();
}

std::string GetCommunicationsOutputDeviceID() {
  RTC_DLOG(INFO) << "GetCommunicationsOutputDeviceID";
  ComPtr<IMMDevice> device(CreateDevice(AudioDeviceName::kDefaultDeviceId,
                                        eRender, eCommunications));
  return device.Get() ? GetDeviceIdInternal(device.Get()) : std::string();
}

ComPtr<IMMDevice> CreateDevice(const std::string& device_id,
                               EDataFlow data_flow,
                               ERole role) {
  RTC_DLOG(INFO) << "CreateDevice";
  return CreateDeviceInternal(device_id, data_flow, role);
}

AudioDeviceName GetDeviceName(IMMDevice* device) {
  RTC_DLOG(INFO) << "GetDeviceName";
  RTC_DCHECK(device);
  AudioDeviceName device_name(GetDeviceFriendlyNameInternal(device),
                              GetDeviceIdInternal(device));
  RTC_DLOG(INFO) << "friendly name: " << device_name.device_name;
  RTC_DLOG(INFO) << "unique id    : " << device_name.unique_id;
  return device_name;
}

std::string GetFriendlyName(const std::string& device_id,
                            EDataFlow data_flow,
                            ERole role) {
  RTC_DLOG(INFO) << "GetFriendlyName";
  ComPtr<IMMDevice> audio_device = CreateDevice(device_id, data_flow, role);
  if (!audio_device.Get())
    return std::string();

  AudioDeviceName device_name = GetDeviceName(audio_device.Get());
  return device_name.device_name;
}

EDataFlow GetDataFlow(IMMDevice* device) {
  RTC_DLOG(INFO) << "GetDataFlow";
  RTC_DCHECK(device);
  ComPtr<IMMEndpoint> endpoint;
  _com_error error = device->QueryInterface(endpoint.GetAddressOf());
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMDevice::QueryInterface failed: "
                      << ErrorToString(error);
    return eAll;
  }

  EDataFlow data_flow;
  error = endpoint->GetDataFlow(&data_flow);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IMMEndpoint::GetDataFlow failed: "
                      << ErrorToString(error);
    return eAll;
  }
  return data_flow;
}

bool GetInputDeviceNames(webrtc::AudioDeviceNames* device_names) {
  RTC_DLOG(INFO) << "GetInputDeviceNames";
  RTC_DCHECK(device_names);
  RTC_DCHECK(device_names->empty());
  return GetDeviceNamesInternal(eCapture, device_names);
}

bool GetOutputDeviceNames(webrtc::AudioDeviceNames* device_names) {
  RTC_DLOG(INFO) << "GetOutputDeviceNames";
  RTC_DCHECK(device_names);
  RTC_DCHECK(device_names->empty());
  return GetDeviceNamesInternal(eRender, device_names);
}

ComPtr<IAudioSessionManager2> CreateSessionManager2(IMMDevice* device) {
  RTC_DLOG(INFO) << "CreateSessionManager2";
  return CreateSessionManager2Internal(device);
}

Microsoft::WRL::ComPtr<IAudioSessionEnumerator> CreateSessionEnumerator(
    IMMDevice* device) {
  RTC_DLOG(INFO) << "CreateSessionEnumerator";
  return CreateSessionEnumeratorInternal(device);
}

int NumberOfActiveSessions(IMMDevice* device) {
  RTC_DLOG(INFO) << "NumberOfActiveSessions";
  ComPtr<IAudioSessionEnumerator> session_enumerator =
      CreateSessionEnumerator(device);

  // Iterate over all audio sessions for the given device.
  int session_count = 0;
  _com_error error = session_enumerator->GetCount(&session_count);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioSessionEnumerator::GetCount failed: "
                      << ErrorToString(error);
    return 0;
  }
  RTC_DLOG(INFO) << "Total number of audio sessions: " << session_count;

  int num_active = 0;
  for (int session = 0; session < session_count; session++) {
    // Acquire the session control interface.
    ComPtr<IAudioSessionControl> session_control;
    error = session_enumerator->GetSession(session, &session_control);
    if (FAILED(error.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioSessionEnumerator::GetSession failed: "
                        << ErrorToString(error);
      return 0;
    }

    // Log the display name of the audio session for debugging purposes.
    LPWSTR display_name;
    if (SUCCEEDED(session_control->GetDisplayName(&display_name))) {
      RTC_DLOG(INFO) << "display name: "
                     << rtc::ToUtf8(display_name, wcslen(display_name));
      CoTaskMemFree(display_name);
    }

    // Get the current state and check if the state is active or not.
    AudioSessionState state;
    error = session_control->GetState(&state);
    if (FAILED(error.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioSessionControl::GetState failed: "
                        << ErrorToString(error);
      return 0;
    }
    if (state == AudioSessionStateActive) {
      ++num_active;
    }
  }

  RTC_DLOG(INFO) << "Number of active audio sessions: " << num_active;
  return num_active;
}

ComPtr<IAudioClient> CreateClient(const std::string& device_id,
                                  EDataFlow data_flow,
                                  ERole role) {
  RTC_DLOG(INFO) << "CreateClient";
  ComPtr<IMMDevice> device(CreateDevice(device_id, data_flow, role));
  return CreateClientInternal(device.Get());
}

ComPtr<IAudioClient2> CreateClient2(const std::string& device_id,
                                    EDataFlow data_flow,
                                    ERole role) {
  RTC_DLOG(INFO) << "CreateClient2";
  ComPtr<IMMDevice> device(CreateDevice(device_id, data_flow, role));
  return CreateClient2Internal(device.Get());
}

ComPtr<IAudioClient3> CreateClient3(const std::string& device_id,
                                    EDataFlow data_flow,
                                    ERole role) {
  RTC_DLOG(INFO) << "CreateClient3";
  ComPtr<IMMDevice> device(CreateDevice(device_id, data_flow, role));
  return CreateClient3Internal(device.Get());
}

HRESULT SetClientProperties(IAudioClient2* client) {
  RTC_DLOG(INFO) << "SetClientProperties";
  RTC_DCHECK(client);
  if (GetAudioClientVersion() < 2) {
    RTC_LOG(LS_WARNING) << "Requires IAudioClient2 or higher";
    return AUDCLNT_E_UNSUPPORTED_FORMAT;
  }
  AudioClientProperties props = {0};
  props.cbSize = sizeof(AudioClientProperties);
  // Real-time VoIP communication.
  // TODO(henrika): other categories?
  props.eCategory = AudioCategory_Communications;
  // Hardware-offloaded audio processing allows the main audio processing tasks
  // to be performed outside the computer's main CPU. Check support and log the
  // result but hard-code `bIsOffload` to FALSE for now.
  // TODO(henrika): evaluate hardware-offloading. Might complicate usage of
  // IAudioClient::GetMixFormat().
  BOOL supports_offload = FALSE;
  _com_error error =
      client->IsOffloadCapable(props.eCategory, &supports_offload);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient2::IsOffloadCapable failed: "
                      << ErrorToString(error);
  }
  RTC_DLOG(INFO) << "supports_offload: " << supports_offload;
  props.bIsOffload = false;
#if (NTDDI_VERSION < NTDDI_WINBLUE)
  RTC_DLOG(INFO) << "options: Not supported in this build";
#else
  // TODO(henrika): pros and cons compared with AUDCLNT_STREAMOPTIONS_NONE?
  props.Options |= AUDCLNT_STREAMOPTIONS_NONE;
  // Requires System.Devices.AudioDevice.RawProcessingSupported.
  // The application can choose to *always ignore* the OEM AEC/AGC by setting
  // the AUDCLNT_STREAMOPTIONS_RAW flag in the call to SetClientProperties.
  // This flag will preserve the user experience aspect of Communications
  // streams, but will not insert any OEM provided communications specific
  // processing in the audio signal path.
  // props.Options |= AUDCLNT_STREAMOPTIONS_RAW;

  // If it is important to avoid resampling in the audio engine, set this flag.
  // AUDCLNT_STREAMOPTIONS_MATCH_FORMAT (or anything in IAudioClient3) is not
  // an appropriate interface to use for communications scenarios.
  // This interface is mainly meant for pro audio scenarios.
  // props.Options |= AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;
  RTC_DLOG(INFO) << "options: 0x" << rtc::ToHex(props.Options);
#endif
  error = client->SetClientProperties(&props);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient2::SetClientProperties failed: "
                      << ErrorToString(error);
  }
  return error.Error();
}

HRESULT GetBufferSizeLimits(IAudioClient2* client,
                            const WAVEFORMATEXTENSIBLE* format,
                            REFERENCE_TIME* min_buffer_duration,
                            REFERENCE_TIME* max_buffer_duration) {
  RTC_DLOG(INFO) << "GetBufferSizeLimits";
  RTC_DCHECK(client);
  if (GetAudioClientVersion() < 2) {
    RTC_LOG(LS_WARNING) << "Requires IAudioClient2 or higher";
    return AUDCLNT_E_UNSUPPORTED_FORMAT;
  }
  REFERENCE_TIME min_duration = 0;
  REFERENCE_TIME max_duration = 0;
  _com_error error =
      client->GetBufferSizeLimits(reinterpret_cast<const WAVEFORMATEX*>(format),
                                  TRUE, &min_duration, &max_duration);
  if (error.Error() == AUDCLNT_E_OFFLOAD_MODE_ONLY) {
    // This API seems to be supported in off-load mode only but it is not
    // documented as a valid error code. Making a special note about it here.
    RTC_LOG(LS_ERROR) << "IAudioClient2::GetBufferSizeLimits failed: "
                         "AUDCLNT_E_OFFLOAD_MODE_ONLY";
  } else if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient2::GetBufferSizeLimits failed: "
                      << ErrorToString(error);
  } else {
    *min_buffer_duration = min_duration;
    *max_buffer_duration = max_duration;
    RTC_DLOG(INFO) << "min_buffer_duration: " << min_buffer_duration;
    RTC_DLOG(INFO) << "max_buffer_duration: " << max_buffer_duration;
  }
  return error.Error();
}

HRESULT GetSharedModeMixFormat(IAudioClient* client,
                               WAVEFORMATEXTENSIBLE* format) {
  RTC_DLOG(INFO) << "GetSharedModeMixFormat";
  RTC_DCHECK(client);

  // The GetMixFormat method retrieves the stream format that the audio engine
  // uses for its internal processing of shared-mode streams. The method
  // allocates the storage for the structure and this memory will be released
  // when `mix_format` goes out of scope. The GetMixFormat method retrieves a
  // format descriptor that is in the form of a WAVEFORMATEXTENSIBLE structure
  // instead of a standalone WAVEFORMATEX structure. The method outputs a
  // pointer to the WAVEFORMATEX structure that is embedded at the start of
  // this WAVEFORMATEXTENSIBLE structure.
  // Note that, crbug/803056 indicates that some devices can return a format
  // where only the WAVEFORMATEX parts is initialized and we must be able to
  // account for that.
  ScopedCoMem<WAVEFORMATEXTENSIBLE> mix_format;
  _com_error error =
      client->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&mix_format));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetMixFormat failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  // Use a wave format wrapper to make things simpler.
  WaveFormatWrapper wrapped_format(mix_format.Get());

  // Verify that the reported format can be mixed by the audio engine in
  // shared mode.
  if (!wrapped_format.IsPcm() && !wrapped_format.IsFloat()) {
    RTC_DLOG(LS_ERROR)
        << "Only pure PCM or float audio streams can be mixed in shared mode";
    return AUDCLNT_E_UNSUPPORTED_FORMAT;
  }

  // Log a warning for the rare case where `mix_format` only contains a
  // stand-alone WAVEFORMATEX structure but don't return.
  if (!wrapped_format.IsExtensible()) {
    RTC_DLOG(WARNING)
        << "The returned format contains no extended information. "
           "The size is "
        << wrapped_format.size() << " bytes.";
  }

  // Copy the correct number of bytes into |*format| taking into account if
  // the returned structure is correctly extended or not.
  RTC_CHECK_LE(wrapped_format.size(), sizeof(WAVEFORMATEXTENSIBLE));
  memcpy(format, wrapped_format.get(), wrapped_format.size());
  RTC_DLOG(INFO) << WaveFormatToString(format);

  return error.Error();
}

bool IsFormatSupported(IAudioClient* client,
                       AUDCLNT_SHAREMODE share_mode,
                       const WAVEFORMATEXTENSIBLE* format) {
  RTC_DLOG(INFO) << "IsFormatSupported";
  RTC_DCHECK(client);
  ScopedCoMem<WAVEFORMATEX> closest_match;
  // This method provides a way for a client to determine, before calling
  // IAudioClient::Initialize, whether the audio engine supports a particular
  // stream format or not. In shared mode, the audio engine always supports
  // the mix format (see GetSharedModeMixFormat).
  // TODO(henrika): verify support for exclusive mode as well?
  _com_error error = client->IsFormatSupported(
      share_mode, reinterpret_cast<const WAVEFORMATEX*>(format),
      &closest_match);
  RTC_LOG(INFO) << WaveFormatToString(
      const_cast<WAVEFORMATEXTENSIBLE*>(format));
  if ((error.Error() == S_OK) && (closest_match == nullptr)) {
    RTC_DLOG(INFO)
        << "The audio endpoint device supports the specified stream format";
  } else if ((error.Error() == S_FALSE) && (closest_match != nullptr)) {
    // Call succeeded with a closest match to the specified format. This log can
    // only be triggered for shared mode.
    RTC_LOG(LS_WARNING)
        << "Exact format is not supported, but a closest match exists";
    RTC_LOG(INFO) << WaveFormatToString(closest_match.Get());
  } else if ((error.Error() == AUDCLNT_E_UNSUPPORTED_FORMAT) &&
             (closest_match == nullptr)) {
    // The audio engine does not support the caller-specified format or any
    // similar format.
    RTC_DLOG(INFO) << "The audio endpoint device does not support the "
                      "specified stream format";
  } else {
    RTC_LOG(LS_ERROR) << "IAudioClient::IsFormatSupported failed: "
                      << ErrorToString(error);
  }

  return (error.Error() == S_OK);
}

HRESULT GetDevicePeriod(IAudioClient* client,
                        AUDCLNT_SHAREMODE share_mode,
                        REFERENCE_TIME* device_period) {
  RTC_DLOG(INFO) << "GetDevicePeriod";
  RTC_DCHECK(client);
  // The `default_period` parameter specifies the default scheduling period
  // for a shared-mode stream. The `minimum_period` parameter specifies the
  // minimum scheduling period for an exclusive-mode stream.
  // The time is expressed in 100-nanosecond units.
  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME minimum_period = 0;
  _com_error error = client->GetDevicePeriod(&default_period, &minimum_period);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetDevicePeriod failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  *device_period = (share_mode == AUDCLNT_SHAREMODE_SHARED) ? default_period
                                                            : minimum_period;
  RTC_LOG(INFO) << "device_period: "
                << ReferenceTimeToTimeDelta(*device_period).ms() << " [ms]";
  RTC_LOG(INFO) << "minimum_period: "
                << ReferenceTimeToTimeDelta(minimum_period).ms() << " [ms]";
  return error.Error();
}

HRESULT GetSharedModeEnginePeriod(IAudioClient3* client3,
                                  const WAVEFORMATEXTENSIBLE* format,
                                  uint32_t* default_period_in_frames,
                                  uint32_t* fundamental_period_in_frames,
                                  uint32_t* min_period_in_frames,
                                  uint32_t* max_period_in_frames) {
  RTC_DLOG(INFO) << "GetSharedModeEnginePeriod";
  RTC_DCHECK(client3);

  UINT32 default_period = 0;
  UINT32 fundamental_period = 0;
  UINT32 min_period = 0;
  UINT32 max_period = 0;
  _com_error error = client3->GetSharedModeEnginePeriod(
      reinterpret_cast<const WAVEFORMATEX*>(format), &default_period,
      &fundamental_period, &min_period, &max_period);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient3::GetSharedModeEnginePeriod failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  WAVEFORMATEX format_ex = format->Format;
  const WORD sample_rate = format_ex.nSamplesPerSec;
  RTC_LOG(INFO) << "default_period_in_frames: " << default_period << " ("
                << FramesToMilliseconds(default_period, sample_rate) << " ms)";
  RTC_LOG(INFO) << "fundamental_period_in_frames: " << fundamental_period
                << " (" << FramesToMilliseconds(fundamental_period, sample_rate)
                << " ms)";
  RTC_LOG(INFO) << "min_period_in_frames: " << min_period << " ("
                << FramesToMilliseconds(min_period, sample_rate) << " ms)";
  RTC_LOG(INFO) << "max_period_in_frames: " << max_period << " ("
                << FramesToMilliseconds(max_period, sample_rate) << " ms)";
  *default_period_in_frames = default_period;
  *fundamental_period_in_frames = fundamental_period;
  *min_period_in_frames = min_period;
  *max_period_in_frames = max_period;
  return error.Error();
}

HRESULT GetPreferredAudioParameters(IAudioClient* client,
                                    AudioParameters* params) {
  RTC_DLOG(INFO) << "GetPreferredAudioParameters";
  RTC_DCHECK(client);
  return GetPreferredAudioParametersInternal(client, params, -1);
}

HRESULT GetPreferredAudioParameters(IAudioClient* client,
                                    webrtc::AudioParameters* params,
                                    uint32_t sample_rate) {
  RTC_DLOG(INFO) << "GetPreferredAudioParameters: " << sample_rate;
  RTC_DCHECK(client);
  return GetPreferredAudioParametersInternal(client, params, sample_rate);
}

HRESULT SharedModeInitialize(IAudioClient* client,
                             const WAVEFORMATEXTENSIBLE* format,
                             HANDLE event_handle,
                             REFERENCE_TIME buffer_duration,
                             bool auto_convert_pcm,
                             uint32_t* endpoint_buffer_size) {
  RTC_DLOG(INFO) << "SharedModeInitialize: buffer_duration=" << buffer_duration
                 << ", auto_convert_pcm=" << auto_convert_pcm;
  RTC_DCHECK(client);
  RTC_DCHECK_GE(buffer_duration, 0);
  if (buffer_duration != 0) {
    RTC_DLOG(LS_WARNING) << "Non-default buffer size is used";
  }
  if (auto_convert_pcm) {
    RTC_DLOG(LS_WARNING) << "Sample rate converter can be utilized";
  }
  // The AUDCLNT_STREAMFLAGS_NOPERSIST flag disables persistence of the volume
  // and mute settings for a session that contains rendering streams.
  // By default, the volume level and muting state for a rendering session are
  // persistent across system restarts. The volume level and muting state for a
  // capture session are never persistent.
  DWORD stream_flags = AUDCLNT_STREAMFLAGS_NOPERSIST;

  // Enable event-driven streaming if a valid event handle is provided.
  // After the stream starts, the audio engine will signal the event handle
  // to notify the client each time a buffer becomes ready to process.
  // Event-driven buffering is supported for both rendering and capturing.
  // Both shared-mode and exclusive-mode streams can use event-driven buffering.
  bool use_event =
      (event_handle != nullptr && event_handle != INVALID_HANDLE_VALUE);
  if (use_event) {
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    RTC_DLOG(INFO) << "The stream is initialized to be event driven";
  }

  // Check if sample-rate conversion is requested.
  if (auto_convert_pcm) {
    // Add channel matrixer (not utilized here) and rate converter to convert
    // from our (the client's) format to the audio engine mix format.
    // Currently only supported for testing, i.e., not possible to enable using
    // public APIs.
    RTC_DLOG(INFO) << "The stream is initialized to support rate conversion";
    stream_flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    stream_flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
  }
  RTC_DLOG(INFO) << "stream_flags: 0x" << rtc::ToHex(stream_flags);

  // Initialize the shared mode client for minimal delay if `buffer_duration`
  // is 0 or possibly a higher delay (more robust) if `buffer_duration` is
  // larger than 0. The actual size is given by IAudioClient::GetBufferSize().
  _com_error error = client->Initialize(
      AUDCLNT_SHAREMODE_SHARED, stream_flags, buffer_duration, 0,
      reinterpret_cast<const WAVEFORMATEX*>(format), nullptr);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Initialize failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  // If a stream is initialized to be event driven and in shared mode, the
  // associated application must also obtain a handle by making a call to
  // IAudioClient::SetEventHandle.
  if (use_event) {
    error = client->SetEventHandle(event_handle);
    if (FAILED(error.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioClient::SetEventHandle failed: "
                        << ErrorToString(error);
      return error.Error();
    }
  }

  UINT32 buffer_size_in_frames = 0;
  // Retrieves the size (maximum capacity) of the endpoint buffer. The size is
  // expressed as the number of audio frames the buffer can hold.
  // For rendering clients, the buffer length determines the maximum amount of
  // rendering data that the application can write to the endpoint buffer
  // during a single processing pass. For capture clients, the buffer length
  // determines the maximum amount of capture data that the audio engine can
  // read from the endpoint buffer during a single processing pass.
  error = client->GetBufferSize(&buffer_size_in_frames);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  *endpoint_buffer_size = buffer_size_in_frames;
  RTC_DLOG(INFO) << "endpoint buffer size: " << buffer_size_in_frames
                 << " [audio frames]";
  const double size_in_ms = static_cast<double>(buffer_size_in_frames) /
                            (format->Format.nSamplesPerSec / 1000.0);
  RTC_DLOG(INFO) << "endpoint buffer size: "
                 << static_cast<int>(size_in_ms + 0.5) << " [ms]";
  RTC_DLOG(INFO) << "bytes per audio frame: " << format->Format.nBlockAlign;
  RTC_DLOG(INFO) << "endpoint buffer size: "
                 << buffer_size_in_frames * format->Format.nChannels *
                        (format->Format.wBitsPerSample / 8)
                 << " [bytes]";

  // TODO(henrika): utilize when delay measurements are added.
  REFERENCE_TIME latency = 0;
  error = client->GetStreamLatency(&latency);
  RTC_DLOG(INFO) << "stream latency: " << ReferenceTimeToTimeDelta(latency).ms()
                 << " [ms]";
  return error.Error();
}

HRESULT SharedModeInitializeLowLatency(IAudioClient3* client,
                                       const WAVEFORMATEXTENSIBLE* format,
                                       HANDLE event_handle,
                                       uint32_t period_in_frames,
                                       bool auto_convert_pcm,
                                       uint32_t* endpoint_buffer_size) {
  RTC_DLOG(INFO) << "SharedModeInitializeLowLatency: period_in_frames="
                 << period_in_frames
                 << ", auto_convert_pcm=" << auto_convert_pcm;
  RTC_DCHECK(client);
  RTC_DCHECK_GT(period_in_frames, 0);
  if (auto_convert_pcm) {
    RTC_DLOG(LS_WARNING) << "Sample rate converter is enabled";
  }

  // Define stream flags.
  DWORD stream_flags = AUDCLNT_STREAMFLAGS_NOPERSIST;
  bool use_event =
      (event_handle != nullptr && event_handle != INVALID_HANDLE_VALUE);
  if (use_event) {
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    RTC_DLOG(INFO) << "The stream is initialized to be event driven";
  }
  if (auto_convert_pcm) {
    stream_flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    stream_flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
  }
  RTC_DLOG(INFO) << "stream_flags: 0x" << rtc::ToHex(stream_flags);

  // Initialize the shared mode client for lowest possible latency.
  // It is assumed that GetSharedModeEnginePeriod() has been used to query the
  // smallest possible engine period and that it is given by `period_in_frames`.
  _com_error error = client->InitializeSharedAudioStream(
      stream_flags, period_in_frames,
      reinterpret_cast<const WAVEFORMATEX*>(format), nullptr);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient3::InitializeSharedAudioStream failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  // Set the event handle.
  if (use_event) {
    error = client->SetEventHandle(event_handle);
    if (FAILED(error.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioClient::SetEventHandle failed: "
                        << ErrorToString(error);
      return error.Error();
    }
  }

  UINT32 buffer_size_in_frames = 0;
  // Retrieve the size (maximum capacity) of the endpoint buffer.
  error = client->GetBufferSize(&buffer_size_in_frames);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  *endpoint_buffer_size = buffer_size_in_frames;
  RTC_DLOG(INFO) << "endpoint buffer size: " << buffer_size_in_frames
                 << " [audio frames]";
  const double size_in_ms = static_cast<double>(buffer_size_in_frames) /
                            (format->Format.nSamplesPerSec / 1000.0);
  RTC_DLOG(INFO) << "endpoint buffer size: "
                 << static_cast<int>(size_in_ms + 0.5) << " [ms]";
  RTC_DLOG(INFO) << "bytes per audio frame: " << format->Format.nBlockAlign;
  RTC_DLOG(INFO) << "endpoint buffer size: "
                 << buffer_size_in_frames * format->Format.nChannels *
                        (format->Format.wBitsPerSample / 8)
                 << " [bytes]";

  // TODO(henrika): utilize when delay measurements are added.
  REFERENCE_TIME latency = 0;
  error = client->GetStreamLatency(&latency);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_WARNING) << "IAudioClient::GetStreamLatency failed: "
                        << ErrorToString(error);
  } else {
    RTC_DLOG(INFO) << "stream latency: "
                   << ReferenceTimeToTimeDelta(latency).ms() << " [ms]";
  }
  return error.Error();
}

ComPtr<IAudioRenderClient> CreateRenderClient(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateRenderClient";
  RTC_DCHECK(client);
  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  ComPtr<IAudioRenderClient> audio_render_client;
  _com_error error = client->GetService(IID_PPV_ARGS(&audio_render_client));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR)
        << "IAudioClient::GetService(IID_IAudioRenderClient) failed: "
        << ErrorToString(error);
    return ComPtr<IAudioRenderClient>();
  }
  return audio_render_client;
}

ComPtr<IAudioCaptureClient> CreateCaptureClient(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateCaptureClient";
  RTC_DCHECK(client);
  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from a capturing endpoint buffer.
  ComPtr<IAudioCaptureClient> audio_capture_client;
  _com_error error = client->GetService(IID_PPV_ARGS(&audio_capture_client));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR)
        << "IAudioClient::GetService(IID_IAudioCaptureClient) failed: "
        << ErrorToString(error);
    return ComPtr<IAudioCaptureClient>();
  }
  return audio_capture_client;
}

ComPtr<IAudioClock> CreateAudioClock(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateAudioClock";
  RTC_DCHECK(client);
  // Get access to the IAudioClock interface. This interface enables us to
  // monitor a stream's data rate and the current position in the stream.
  ComPtr<IAudioClock> audio_clock;
  _com_error error = client->GetService(IID_PPV_ARGS(&audio_clock));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetService(IID_IAudioClock) failed: "
                      << ErrorToString(error);
    return ComPtr<IAudioClock>();
  }
  return audio_clock;
}

ComPtr<IAudioSessionControl> CreateAudioSessionControl(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateAudioSessionControl";
  RTC_DCHECK(client);
  ComPtr<IAudioSessionControl> audio_session_control;
  _com_error error = client->GetService(IID_PPV_ARGS(&audio_session_control));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetService(IID_IAudioControl) failed: "
                      << ErrorToString(error);
    return ComPtr<IAudioSessionControl>();
  }
  return audio_session_control;
}

ComPtr<ISimpleAudioVolume> CreateSimpleAudioVolume(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateSimpleAudioVolume";
  RTC_DCHECK(client);
  // Get access to the ISimpleAudioVolume interface. This interface enables a
  // client to control the master volume level of an audio session.
  ComPtr<ISimpleAudioVolume> simple_audio_volume;
  _com_error error = client->GetService(IID_PPV_ARGS(&simple_audio_volume));
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR)
        << "IAudioClient::GetService(IID_ISimpleAudioVolume) failed: "
        << ErrorToString(error);
    return ComPtr<ISimpleAudioVolume>();
  }
  return simple_audio_volume;
}

bool FillRenderEndpointBufferWithSilence(IAudioClient* client,
                                         IAudioRenderClient* render_client) {
  RTC_DLOG(INFO) << "FillRenderEndpointBufferWithSilence";
  RTC_DCHECK(client);
  RTC_DCHECK(render_client);
  UINT32 endpoint_buffer_size = 0;
  _com_error error = client->GetBufferSize(&endpoint_buffer_size);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: "
                      << ErrorToString(error);
    return false;
  }

  UINT32 num_queued_frames = 0;
  // Get number of audio frames that are queued up to play in the endpoint
  // buffer.
  error = client->GetCurrentPadding(&num_queued_frames);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetCurrentPadding failed: "
                      << ErrorToString(error);
    return false;
  }
  RTC_DLOG(INFO) << "num_queued_frames: " << num_queued_frames;

  BYTE* data = nullptr;
  int num_frames_to_fill = endpoint_buffer_size - num_queued_frames;
  RTC_DLOG(INFO) << "num_frames_to_fill: " << num_frames_to_fill;
  error = render_client->GetBuffer(num_frames_to_fill, &data);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioRenderClient::GetBuffer failed: "
                      << ErrorToString(error);
    return false;
  }

  // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
  // explicitly write silence data to the rendering buffer.
  error = render_client->ReleaseBuffer(num_frames_to_fill,
                                       AUDCLNT_BUFFERFLAGS_SILENT);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioRenderClient::ReleaseBuffer failed: "
                      << ErrorToString(error);
    return false;
  }

  return true;
}

std::string WaveFormatToString(const WaveFormatWrapper format) {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  // Start with the WAVEFORMATEX part (which always exists).
  ss.AppendFormat("wFormatTag: %s (0x%X)",
                  WaveFormatTagToString(format->wFormatTag),
                  format->wFormatTag);
  ss.AppendFormat(", nChannels: %d", format->nChannels);
  ss.AppendFormat(", nSamplesPerSec: %d", format->nSamplesPerSec);
  ss.AppendFormat(", nAvgBytesPerSec: %d", format->nAvgBytesPerSec);
  ss.AppendFormat(", nBlockAlign: %d", format->nBlockAlign);
  ss.AppendFormat(", wBitsPerSample: %d", format->wBitsPerSample);
  ss.AppendFormat(", cbSize: %d", format->cbSize);
  if (!format.IsExtensible())
    return ss.str();

  // Append the WAVEFORMATEXTENSIBLE part (which we know exists).
  ss.AppendFormat(
      " [+] wValidBitsPerSample: %d, dwChannelMask: %s",
      format.GetExtensible()->Samples.wValidBitsPerSample,
      ChannelMaskToString(format.GetExtensible()->dwChannelMask).c_str());
  if (format.IsPcm()) {
    ss.AppendFormat("%s", ", SubFormat: KSDATAFORMAT_SUBTYPE_PCM");
  } else if (format.IsFloat()) {
    ss.AppendFormat("%s", ", SubFormat: KSDATAFORMAT_SUBTYPE_IEEE_FLOAT");
  } else {
    ss.AppendFormat("%s", ", SubFormat: NOT_SUPPORTED");
  }
  return ss.str();
}

webrtc::TimeDelta ReferenceTimeToTimeDelta(REFERENCE_TIME time) {
  // Each unit of reference time is 100 nanoseconds <=> 0.1 microsecond.
  return webrtc::TimeDelta::Micros(0.1 * time + 0.5);
}

double FramesToMilliseconds(uint32_t num_frames, uint16_t sample_rate) {
  // Convert the current period in frames into milliseconds.
  return static_cast<double>(num_frames) / (sample_rate / 1000.0);
}

std::string ErrorToString(const _com_error& error) {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss.AppendFormat("(HRESULT: 0x%08X)", error.Error());
  return ss.str();
}

}  // namespace core_audio_utility
}  // namespace webrtc_win
}  // namespace webrtc
