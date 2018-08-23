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

#include <Functiondiscoverykeys_devpkey.h>
#include <atlbase.h>
#include <stdio.h>
#include <tchar.h>

#include <iomanip>
#include <string>
#include <utility>

#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/stringutils.h"

using ATL::CComHeapPtr;
using Microsoft::WRL::ComPtr;
using webrtc::AudioDeviceName;
using webrtc::AudioParameters;

namespace webrtc {
namespace webrtc_win {
namespace {

using core_audio_utility::ErrorToString;

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
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "CoCreateInstance failed: " << ErrorToString(error);
  }

  if (error.Error() == CO_E_NOTINITIALIZED && allow_reinitialize) {
    RTC_LOG(LS_ERROR) << "CoCreateInstance failed with CO_E_NOTINITIALIZED";
    // We have seen crashes which indicates that this method can in fact
    // fail with CO_E_NOTINITIALIZED in combination with certain 3rd party
    // modules. Calling CoInitializeEx() is an attempt to resolve the reported
    // issues. See http://crbug.com/378465 for details.
    error = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (error.Error() != S_OK) {
      error = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                 CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator));
      if (error.Error() != S_OK) {
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

// Retrieve an audio device specified by |device_id| or a default device
// specified by data-flow direction and role if |device_id| is default.
ComPtr<IMMDevice> CreateDeviceInternal(const std::string& device_id,
                                       EDataFlow data_flow,
                                       ERole role) {
  RTC_DLOG(INFO) << "CreateDeviceInternal: " << role;
  ComPtr<IMMDevice> audio_endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ComPtr<IMMDeviceEnumerator> device_enum(CreateDeviceEnumeratorInternal(true));
  if (!device_enum.Get())
    return audio_endpoint_device;

  _com_error error(S_FALSE);
  if (device_id == AudioDeviceName::kDefaultDeviceId) {
    error = device_enum->GetDefaultAudioEndpoint(
        data_flow, role, audio_endpoint_device.GetAddressOf());
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR)
          << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: "
          << ErrorToString(error);
    }
  } else {
    error = device_enum->GetDevice(rtc::ToUtf16(device_id).c_str(),
                                   audio_endpoint_device.GetAddressOf());
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR) << "IMMDeviceEnumerator::GetDevice failed: "
                        << ErrorToString(error);
    }
  }

  // Verify that the audio endpoint device is active, i.e., that the audio
  // adapter that connects to the endpoint device is present and enabled.
  if (SUCCEEDED(error.Error()) &&
      !IsDeviceActive(audio_endpoint_device.Get())) {
    RTC_LOG(LS_WARNING) << "Selected endpoint device is not active";
    audio_endpoint_device.Reset();
  }

  return audio_endpoint_device;
}

std::string GetDeviceIdInternal(IMMDevice* device) {
  // Retrieve unique name of endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
  CComHeapPtr<WCHAR> device_id;
  if (SUCCEEDED(device->GetId(&device_id))) {
    return rtc::ToUtf8(device_id, wcslen(device_id));
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
  if (error.Error() != S_OK) {
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
  if (error.Error() != S_OK) {
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
  if (error.Error() != S_OK) {
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
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IMMDevice::Activate(IAudioClient2) failed: "
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
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IMMDeviceCollection::EnumAudioEndpoints failed: "
                      << ErrorToString(error);
  }
  return collection;
}

bool GetDeviceNamesInternal(EDataFlow data_flow,
                            webrtc::AudioDeviceNames* device_names) {
  // Always add the default device in index 0 and the default communication
  // device as index 1 in the vector. The name of the default device starts
  // with "Default - " and the default communication device starts with
  // "Communication - ".
  //  Example of friendly name: "Default - Headset (SB Arena Headset)"
  ERole role[] = {eConsole, eCommunications};
  ComPtr<IMMDevice> default_device;
  AudioDeviceName default_device_name;
  for (size_t i = 0; i < arraysize(role); ++i) {
    default_device = CreateDeviceInternal(AudioDeviceName::kDefaultDeviceId,
                                          data_flow, role[i]);
    std::string device_name;
    device_name += (role[i] == eConsole ? "Default - " : "Communication - ");
    device_name += GetDeviceFriendlyNameInternal(default_device.Get());
    std::string unique_id = GetDeviceIdInternal(default_device.Get());

    default_device_name.device_name = std::move(device_name);
    default_device_name.unique_id = std::move(unique_id);
    RTC_DLOG(INFO) << "friendly name: " << default_device_name.device_name;
    RTC_DLOG(INFO) << "unique id    : " << default_device_name.unique_id;
    // Add combination of user-friendly and unique name to the output list.
    device_names->emplace_back(default_device_name);
  }

  // Next, add all active input devices on index 2 and above. Note that,
  // one device can have more than one role. Hence, if only one input device
  // is present, the output vector will contain three elements all with the
  // same unique ID but with different names.
  // Example (one capture device but three elements in device_names):
  //   0: friendly name: Default - Headset (SB Arena Headset)
  //   0: unique id    : {0.0.1.00000000}.{822d99bb-d9b0-4f6f-b2a5-cd1be220d338}
  //   1: friendly name: Communication - Headset (SB Arena Headset)
  //   1: unique id    : {0.0.1.00000000}.{822d99bb-d9b0-4f6f-b2a5-cd1be220d338}
  //   2: friendly name: Headset (SB Arena Headset)
  //   2: unique id    : {0.0.1.00000000}.{822d99bb-d9b0-4f6f-b2a5-cd1be220d338}

  // Generate a collection of active audio endpoint devices for the specified
  // direction.
  ComPtr<IMMDeviceCollection> collection = CreateCollectionInternal(data_flow);
  if (!collection.Get()) {
    return false;
  }

  // Retrieve the number of active audio devices for the specified direction.
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  if (number_of_active_devices == 0) {
    return true;
  }

  // Loop over all active devices and add friendly name and unique ID to the
  // |device_names| list which already contains two elements
  RTC_DCHECK_EQ(device_names->size(), 2);
  for (UINT i = 0; i < number_of_active_devices; ++i) {
    // Retrieve a pointer to the specified item in the device collection.
    ComPtr<IMMDevice> audio_device;
    _com_error error = collection->Item(i, audio_device.GetAddressOf());
    if (error.Error() != S_OK)
      continue;
    // Retrieve the complete device name for the given audio device endpoint.
    AudioDeviceName device_name(
        GetDeviceFriendlyNameInternal(audio_device.Get()),
        GetDeviceIdInternal(audio_device.Get()));
    RTC_DLOG(INFO) << "friendly name: " << device_name.device_name;
    RTC_DLOG(INFO) << "unique id    : " << device_name.unique_id;
    // Add combination of user-friendly and unique name to the output list.
    device_names->emplace_back(device_name);
  }

  return true;
}

HRESULT GetPreferredAudioParametersInternal(IAudioClient* client,
                                            AudioParameters* params) {
  WAVEFORMATPCMEX mix_format;
  HRESULT hr = core_audio_utility::GetSharedModeMixFormat(client, &mix_format);
  if (FAILED(hr))
    return hr;

  REFERENCE_TIME default_period = 0;
  hr = core_audio_utility::GetDevicePeriod(client, AUDCLNT_SHAREMODE_SHARED,
                                           &default_period);
  if (FAILED(hr))
    return hr;

  const int sample_rate = mix_format.Format.nSamplesPerSec;
  // TODO(henrika): utilize full mix_format.Format.wBitsPerSample.
  // const size_t bits_per_sample = AudioParameters::kBitsPerSample;
  // TODO(henrika): improve channel layout support.
  const size_t channels = mix_format.Format.nChannels;
  RTC_DCHECK_LE(channels, 2);

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
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IMMDevice::QueryInterface failed: "
                      << ErrorToString(error);
    return eAll;
  }

  EDataFlow data_flow;
  error = endpoint->GetDataFlow(&data_flow);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IMMEndpoint::GetDataFlow failed: "
                      << ErrorToString(error);
    return eAll;
  }
  return data_flow;
}

bool GetInputDeviceNames(webrtc::AudioDeviceNames* device_names) {
  RTC_DLOG(INFO) << "GetInputDeviceNames";
  RTC_DCHECK(device_names);
  return GetDeviceNamesInternal(eCapture, device_names);
}

bool GetOutputDeviceNames(webrtc::AudioDeviceNames* device_names) {
  RTC_DLOG(INFO) << "GetOutputDeviceNames";
  RTC_DCHECK(device_names);
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
  if (error.Error() != S_OK) {
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
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR) << "IAudioSessionEnumerator::GetSession failed: "
                        << ErrorToString(error);
      return 0;
    }

    // Log the display name of the audio session for debugging purposes.
    CComHeapPtr<WCHAR> display_name;
    if (SUCCEEDED(session_control->GetDisplayName(&display_name))) {
      RTC_DLOG(INFO) << "display name: "
                     << rtc::ToUtf8(display_name, wcslen(display_name));
    }

    // Get the current state and check if the state is active or not.
    AudioSessionState state;
    error = session_control->GetState(&state);
    if (error.Error() != S_OK) {
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

HRESULT SetClientProperties(IAudioClient2* client) {
  RTC_DLOG(INFO) << "SetClientProperties";
  RTC_DCHECK(client);

  AudioClientProperties properties = {0};
  properties.cbSize = sizeof(AudioClientProperties);
  properties.bIsOffload = false;
  // Real-time VoIP communication.
  // TODO(henrika): other categories?
  properties.eCategory = AudioCategory_Communications;
  // TODO(henrika): can AUDCLNT_STREAMOPTIONS_RAW be used?
  properties.Options = AUDCLNT_STREAMOPTIONS_NONE;
  _com_error error = client->SetClientProperties(&properties);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient2::SetClientProperties failed: "
                      << ErrorToString(error);
  }
  return error.Error();
}

HRESULT GetSharedModeMixFormat(IAudioClient* client,
                               WAVEFORMATEXTENSIBLE* format) {
  RTC_DLOG(INFO) << "GetSharedModeMixFormat";
  RTC_DCHECK(client);
  ScopedCoMem<WAVEFORMATEXTENSIBLE> format_ex;
  _com_error error =
      client->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&format_ex));
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetMixFormat failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  size_t bytes = sizeof(WAVEFORMATEX) + format_ex->Format.cbSize;
  RTC_DCHECK_EQ(bytes, sizeof(WAVEFORMATEXTENSIBLE));
  memcpy(format, format_ex, bytes);
  RTC_DLOG(INFO) << WaveFormatExToString(format);

  return error.Error();
}

bool IsFormatSupported(IAudioClient* client,
                       AUDCLNT_SHAREMODE share_mode,
                       const WAVEFORMATEXTENSIBLE* format) {
  RTC_DLOG(INFO) << "IsFormatSupported";
  RTC_DCHECK(client);
  ScopedCoMem<WAVEFORMATEXTENSIBLE> closest_match;
  // This method provides a way for a client to determine, before calling
  // IAudioClient::Initialize, whether the audio engine supports a particular
  // stream format or not. In shared mode, the audio engine always supports
  // the mix format (see GetSharedModeMixFormat).
  // TODO(henrika): verify support for exclusive mode as well?
  _com_error error = client->IsFormatSupported(
      share_mode, reinterpret_cast<const WAVEFORMATEX*>(format),
      reinterpret_cast<WAVEFORMATEX**>(&closest_match));
  if ((error.Error() == S_OK) && (closest_match == nullptr)) {
    RTC_DLOG(INFO)
        << "The audio endpoint device supports the specified stream format";
  } else if ((error.Error() == S_FALSE) && (closest_match != nullptr)) {
    // Call succeeded with a closest match to the specified format. This log can
    // only be triggered for shared mode.
    RTC_LOG(LS_WARNING)
        << "Exact format is not supported, but a closest match exists";
    RTC_LOG(INFO) << WaveFormatExToString(closest_match);
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
  // The |default_period| parameter specifies the default scheduling period
  // for a shared-mode stream. The |minimum_period| parameter specifies the
  // minimum scheduling period for an exclusive-mode stream.
  // The time is expressed in 100-nanosecond units.
  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME minimum_period = 0;
  _com_error error = client->GetDevicePeriod(&default_period, &minimum_period);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetDevicePeriod failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  *device_period = (share_mode == AUDCLNT_SHAREMODE_SHARED) ? default_period
                                                            : minimum_period;
  RTC_LOG(INFO) << "device_period: "
                << ReferenceTimeToTimeDelta(*device_period).ms() << " [ms]";
  return error.Error();
}

HRESULT GetPreferredAudioParameters(const std::string& device_id,
                                    bool is_output_device,
                                    AudioParameters* params) {
  RTC_DLOG(INFO) << "GetPreferredAudioParameters";
  EDataFlow data_flow = is_output_device ? eRender : eCapture;
  ComPtr<IMMDevice> device;
  if (device_id == AudioDeviceName::kDefaultCommunicationsDeviceId) {
    device = CreateDeviceInternal(AudioDeviceName::kDefaultDeviceId, data_flow,
                                  eCommunications);
  } else {
    // If |device_id| equals AudioDeviceName::kDefaultDeviceId, a default
    // device will be created.
    device = CreateDeviceInternal(device_id, data_flow, eConsole);
  }
  if (!device.Get()) {
    return E_FAIL;
  }

  ComPtr<IAudioClient> client(CreateClientInternal(device.Get()));
  if (!client.Get())
    return E_FAIL;

  return GetPreferredAudioParametersInternal(client.Get(), params);
}

HRESULT GetPreferredAudioParameters(IAudioClient* client,
                                    AudioParameters* params) {
  RTC_DCHECK(client);
  return GetPreferredAudioParametersInternal(client, params);
}

HRESULT SharedModeInitialize(IAudioClient* client,
                             const WAVEFORMATEXTENSIBLE* format,
                             HANDLE event_handle,
                             uint32_t* endpoint_buffer_size) {
  RTC_DLOG(INFO) << "SharedModeInitialize";
  RTC_DCHECK(client);
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
  RTC_DLOG(INFO) << "stream_flags: 0x" << rtc::ToHex(stream_flags);

  // Initialize the shared mode client for minimal delay.
  _com_error error = client->Initialize(
      AUDCLNT_SHAREMODE_SHARED, stream_flags, 0, 0,
      reinterpret_cast<const WAVEFORMATEX*>(format), nullptr);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Initialize failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  // If a stream is initialized to be event driven and in shared mode, the
  // associated application must also obtain a handle by making a call to
  // IAudioClient::SetEventHandle.
  if (use_event) {
    error = client->SetEventHandle(event_handle);
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR) << "IAudioClient::SetEventHandle failed: "
                        << ErrorToString(error);
      return error.Error();
    }
  }

  UINT32 buffer_size_in_frames = 0;
  // Retrieves the size (maximum capacity) of the endpoint buffer. The size is
  // expressed as the number of audio frames the buffer can hold.
  error = client->GetBufferSize(&buffer_size_in_frames);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: "
                      << ErrorToString(error);
    return error.Error();
  }

  *endpoint_buffer_size = buffer_size_in_frames;
  RTC_DLOG(INFO) << "endpoint buffer size: " << buffer_size_in_frames
                 << " [audio frames]";
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

ComPtr<IAudioRenderClient> CreateRenderClient(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateRenderClient";
  RTC_DCHECK(client);
  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  ComPtr<IAudioRenderClient> audio_render_client;
  _com_error error = client->GetService(IID_PPV_ARGS(&audio_render_client));
  if (error.Error() != S_OK) {
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
  if (error.Error() != S_OK) {
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
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetService(IID_IAudioClock) failed: "
                      << ErrorToString(error);
    return ComPtr<IAudioClock>();
  }
  return audio_clock;
}

ComPtr<ISimpleAudioVolume> CreateSimpleAudioVolume(IAudioClient* client) {
  RTC_DLOG(INFO) << "CreateSimpleAudioVolume";
  RTC_DCHECK(client);
  // Get access to the ISimpleAudioVolume interface. This interface enables a
  // client to control the master volume level of an audio session.
  ComPtr<ISimpleAudioVolume> simple_audio_volume;
  _com_error error = client->GetService(IID_PPV_ARGS(&simple_audio_volume));
  if (error.Error() != S_OK) {
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
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: "
                      << ErrorToString(error);
    return false;
  }

  UINT32 num_queued_frames = 0;
  // Get number of audio frames that are queued up to play in the endpoint
  // buffer.
  error = client->GetCurrentPadding(&num_queued_frames);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetCurrentPadding failed: "
                      << ErrorToString(error);
    return false;
  }
  RTC_DLOG(INFO) << "num_queued_frames: " << num_queued_frames;

  BYTE* data = nullptr;
  int num_frames_to_fill = endpoint_buffer_size - num_queued_frames;
  RTC_DLOG(INFO) << "num_frames_to_fill: " << num_frames_to_fill;
  error = render_client->GetBuffer(num_frames_to_fill, &data);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioRenderClient::GetBuffer failed: "
                      << ErrorToString(error);
    return false;
  }

  // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
  // explicitly write silence data to the rendering buffer.
  error = render_client->ReleaseBuffer(num_frames_to_fill,
                                       AUDCLNT_BUFFERFLAGS_SILENT);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioRenderClient::ReleaseBuffer failed: "
                      << ErrorToString(error);
    return false;
  }

  return true;
}

std::string WaveFormatExToString(const WAVEFORMATEXTENSIBLE* format) {
  RTC_DCHECK_EQ(format->Format.wFormatTag, WAVE_FORMAT_EXTENSIBLE);
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss.AppendFormat("wFormatTag: WAVE_FORMAT_EXTENSIBLE");
  ss.AppendFormat(", nChannels: %d", format->Format.nChannels);
  ss.AppendFormat(", nSamplesPerSec: %d", format->Format.nSamplesPerSec);
  ss.AppendFormat(", nAvgBytesPerSec: %d", format->Format.nAvgBytesPerSec);
  ss.AppendFormat(", nBlockAlign: %d", format->Format.nBlockAlign);
  ss.AppendFormat(", wBitsPerSample: %d", format->Format.wBitsPerSample);
  ss.AppendFormat(", cbSize: %d", format->Format.cbSize);
  ss.AppendFormat(", wValidBitsPerSample: %d",
                  format->Samples.wValidBitsPerSample);
  ss.AppendFormat(", dwChannelMask: 0x%X", format->dwChannelMask);
  if (format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
    ss << ", SubFormat: KSDATAFORMAT_SUBTYPE_PCM";
  } else if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
    ss << ", SubFormat: KSDATAFORMAT_SUBTYPE_IEEE_FLOAT";
  } else {
    ss << ", SubFormat: NOT_SUPPORTED";
  }
  return ss.str();
}

webrtc::TimeDelta ReferenceTimeToTimeDelta(REFERENCE_TIME time) {
  // Each unit of reference time is 100 nanoseconds <=> 0.1 microsecond.
  return webrtc::TimeDelta::us(0.1 * time + 0.5);
}

std::string ErrorToString(const _com_error& error) {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss.AppendFormat("%s (0x%08X)", rtc::ToUtf8(error.ErrorMessage()).c_str(),
                  error.Error());
  return ss.str();
}

}  // namespace core_audio_utility
}  // namespace webrtc_win
}  // namespace webrtc
