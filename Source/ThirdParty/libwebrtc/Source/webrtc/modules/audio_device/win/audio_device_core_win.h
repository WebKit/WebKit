/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_DEVICE_AUDIO_DEVICE_CORE_WIN_H_
#define AUDIO_DEVICE_AUDIO_DEVICE_CORE_WIN_H_

#if (_MSC_VER >= 1400)  // only include for VS 2005 and higher

#include "rtc_base/win32.h"

#include "modules/audio_device/audio_device_generic.h"

#include <wmcodecdsp.h>   // CLSID_CWMAudioAEC
                          // (must be before audioclient.h)
#include <audioclient.h>  // WASAPI
#include <audiopolicy.h>
#include <avrt.h>  // Avrt
#include <endpointvolume.h>
#include <mediaobj.h>     // IMediaObject
#include <mmdeviceapi.h>  // MMDevice

#include "rtc_base/criticalsection.h"
#include "rtc_base/scoped_ref_ptr.h"

// Use Multimedia Class Scheduler Service (MMCSS) to boost the thread priority
#pragma comment(lib, "avrt.lib")
// AVRT function pointers
typedef BOOL(WINAPI* PAvRevertMmThreadCharacteristics)(HANDLE);
typedef HANDLE(WINAPI* PAvSetMmThreadCharacteristicsA)(LPCSTR, LPDWORD);
typedef BOOL(WINAPI* PAvSetMmThreadPriority)(HANDLE, AVRT_PRIORITY);

namespace webrtc {

const float MAX_CORE_SPEAKER_VOLUME = 255.0f;
const float MIN_CORE_SPEAKER_VOLUME = 0.0f;
const float MAX_CORE_MICROPHONE_VOLUME = 255.0f;
const float MIN_CORE_MICROPHONE_VOLUME = 0.0f;
const uint16_t CORE_SPEAKER_VOLUME_STEP_SIZE = 1;
const uint16_t CORE_MICROPHONE_VOLUME_STEP_SIZE = 1;

// Utility class which initializes COM in the constructor (STA or MTA),
// and uninitializes COM in the destructor.
class ScopedCOMInitializer {
 public:
  // Enum value provided to initialize the thread as an MTA instead of STA.
  enum SelectMTA { kMTA };

  // Constructor for STA initialization.
  ScopedCOMInitializer() { Initialize(COINIT_APARTMENTTHREADED); }

  // Constructor for MTA initialization.
  explicit ScopedCOMInitializer(SelectMTA mta) {
    Initialize(COINIT_MULTITHREADED);
  }

  ~ScopedCOMInitializer() {
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

  bool succeeded() const { return SUCCEEDED(hr_); }

 private:
  void Initialize(COINIT init) { hr_ = CoInitializeEx(NULL, init); }

  HRESULT hr_;

  ScopedCOMInitializer(const ScopedCOMInitializer&);
  void operator=(const ScopedCOMInitializer&);
};

class AudioDeviceWindowsCore : public AudioDeviceGeneric {
 public:
  AudioDeviceWindowsCore();
  ~AudioDeviceWindowsCore();

  static bool CoreAudioIsSupported();

  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const;

  // Main initializaton and termination
  virtual InitStatus Init();
  virtual int32_t Terminate();
  virtual bool Initialized() const;

  // Device enumeration
  virtual int16_t PlayoutDevices();
  virtual int16_t RecordingDevices();
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]);
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]);

  // Device selection
  virtual int32_t SetPlayoutDevice(uint16_t index);
  virtual int32_t SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
  virtual int32_t SetRecordingDevice(uint16_t index);
  virtual int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device);

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool& available);
  virtual int32_t InitPlayout();
  virtual bool PlayoutIsInitialized() const;
  virtual int32_t RecordingIsAvailable(bool& available);
  virtual int32_t InitRecording();
  virtual bool RecordingIsInitialized() const;

  // Audio transport control
  virtual int32_t StartPlayout();
  virtual int32_t StopPlayout();
  virtual bool Playing() const;
  virtual int32_t StartRecording();
  virtual int32_t StopRecording();
  virtual bool Recording() const;

  // Audio mixer initialization
  virtual int32_t InitSpeaker();
  virtual bool SpeakerIsInitialized() const;
  virtual int32_t InitMicrophone();
  virtual bool MicrophoneIsInitialized() const;

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(bool& available);
  virtual int32_t SetSpeakerVolume(uint32_t volume);
  virtual int32_t SpeakerVolume(uint32_t& volume) const;
  virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;
  virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const;

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(bool& available);
  virtual int32_t SetMicrophoneVolume(uint32_t volume);
  virtual int32_t MicrophoneVolume(uint32_t& volume) const;
  virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;
  virtual int32_t MinMicrophoneVolume(uint32_t& minVolume) const;

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool& available);
  virtual int32_t SetSpeakerMute(bool enable);
  virtual int32_t SpeakerMute(bool& enabled) const;

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool& available);
  virtual int32_t SetMicrophoneMute(bool enable);
  virtual int32_t MicrophoneMute(bool& enabled) const;

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool& available);
  virtual int32_t SetStereoPlayout(bool enable);
  virtual int32_t StereoPlayout(bool& enabled) const;
  virtual int32_t StereoRecordingIsAvailable(bool& available);
  virtual int32_t SetStereoRecording(bool enable);
  virtual int32_t StereoRecording(bool& enabled) const;

  // Delay information and control
  virtual int32_t PlayoutDelay(uint16_t& delayMS) const;

  virtual bool BuiltInAECIsAvailable() const;

  virtual int32_t EnableBuiltInAEC(bool enable);

 public:
  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

 private:
  bool KeyPressed() const;

 private:  // avrt function pointers
  PAvRevertMmThreadCharacteristics _PAvRevertMmThreadCharacteristics;
  PAvSetMmThreadCharacteristicsA _PAvSetMmThreadCharacteristicsA;
  PAvSetMmThreadPriority _PAvSetMmThreadPriority;
  HMODULE _avrtLibrary;
  bool _winSupportAvrt;

 private:  // thread functions
  DWORD InitCaptureThreadPriority();
  void RevertCaptureThreadPriority();
  static DWORD WINAPI WSAPICaptureThread(LPVOID context);
  DWORD DoCaptureThread();

  static DWORD WINAPI WSAPICaptureThreadPollDMO(LPVOID context);
  DWORD DoCaptureThreadPollDMO();

  static DWORD WINAPI WSAPIRenderThread(LPVOID context);
  DWORD DoRenderThread();

  void _Lock();
  void _UnLock();

  int SetDMOProperties();

  int SetBoolProperty(IPropertyStore* ptrPS,
                      REFPROPERTYKEY key,
                      VARIANT_BOOL value);

  int SetVtI4Property(IPropertyStore* ptrPS, REFPROPERTYKEY key, LONG value);

  int32_t _EnumerateEndpointDevicesAll(EDataFlow dataFlow) const;
  void _TraceCOMError(HRESULT hr) const;

  int32_t _RefreshDeviceList(EDataFlow dir);
  int16_t _DeviceListCount(EDataFlow dir);
  int32_t _GetDefaultDeviceName(EDataFlow dir,
                                ERole role,
                                LPWSTR szBuffer,
                                int bufferLen);
  int32_t _GetListDeviceName(EDataFlow dir,
                             int index,
                             LPWSTR szBuffer,
                             int bufferLen);
  int32_t _GetDeviceName(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen);
  int32_t _GetListDeviceID(EDataFlow dir,
                           int index,
                           LPWSTR szBuffer,
                           int bufferLen);
  int32_t _GetDefaultDeviceID(EDataFlow dir,
                              ERole role,
                              LPWSTR szBuffer,
                              int bufferLen);
  int32_t _GetDefaultDeviceIndex(EDataFlow dir, ERole role, int* index);
  int32_t _GetDeviceID(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen);
  int32_t _GetDefaultDevice(EDataFlow dir, ERole role, IMMDevice** ppDevice);
  int32_t _GetListDevice(EDataFlow dir, int index, IMMDevice** ppDevice);

  // Converts from wide-char to UTF-8 if UNICODE is defined.
  // Does nothing if UNICODE is undefined.
  char* WideToUTF8(const TCHAR* src) const;

  int32_t InitRecordingDMO();

  ScopedCOMInitializer _comInit;
  AudioDeviceBuffer* _ptrAudioBuffer;
  rtc::CriticalSection _critSect;
  rtc::CriticalSection _volumeMutex;

  IMMDeviceEnumerator* _ptrEnumerator;
  IMMDeviceCollection* _ptrRenderCollection;
  IMMDeviceCollection* _ptrCaptureCollection;
  IMMDevice* _ptrDeviceOut;
  IMMDevice* _ptrDeviceIn;

  IAudioClient* _ptrClientOut;
  IAudioClient* _ptrClientIn;
  IAudioRenderClient* _ptrRenderClient;
  IAudioCaptureClient* _ptrCaptureClient;
  IAudioEndpointVolume* _ptrCaptureVolume;
  ISimpleAudioVolume* _ptrRenderSimpleVolume;

  // DirectX Media Object (DMO) for the built-in AEC.
  rtc::scoped_refptr<IMediaObject> _dmo;
  rtc::scoped_refptr<IMediaBuffer> _mediaBuffer;
  bool _builtInAecEnabled;

  HANDLE _hRenderSamplesReadyEvent;
  HANDLE _hPlayThread;
  HANDLE _hRenderStartedEvent;
  HANDLE _hShutdownRenderEvent;

  HANDLE _hCaptureSamplesReadyEvent;
  HANDLE _hRecThread;
  HANDLE _hCaptureStartedEvent;
  HANDLE _hShutdownCaptureEvent;

  HANDLE _hMmTask;

  UINT _playAudioFrameSize;
  uint32_t _playSampleRate;
  uint32_t _devicePlaySampleRate;
  uint32_t _playBlockSize;
  uint32_t _devicePlayBlockSize;
  uint32_t _playChannels;
  uint32_t _sndCardPlayDelay;
  uint32_t _sndCardRecDelay;
  UINT64 _writtenSamples;
  UINT64 _readSamples;

  UINT _recAudioFrameSize;
  uint32_t _recSampleRate;
  uint32_t _recBlockSize;
  uint32_t _recChannels;

  uint16_t _recChannelsPrioList[3];
  uint16_t _playChannelsPrioList[2];

  LARGE_INTEGER _perfCounterFreq;
  double _perfCounterFactor;

 private:
  bool _initialized;
  bool _recording;
  bool _playing;
  bool _recIsInitialized;
  bool _playIsInitialized;
  bool _speakerIsInitialized;
  bool _microphoneIsInitialized;

  bool _usingInputDeviceIndex;
  bool _usingOutputDeviceIndex;
  AudioDeviceModule::WindowsDeviceType _inputDevice;
  AudioDeviceModule::WindowsDeviceType _outputDevice;
  uint16_t _inputDeviceIndex;
  uint16_t _outputDeviceIndex;

  mutable char _str[512];
};

#endif  // #if (_MSC_VER >= 1400)

}  // namespace webrtc

#endif  // AUDIO_DEVICE_AUDIO_DEVICE_CORE_WIN_H_
