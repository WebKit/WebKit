/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/win/audio_device_wave_win.h"

#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

#include <windows.h>
#include <objbase.h>    // CoTaskMemAlloc, CoTaskMemFree
#include <strsafe.h>    // StringCchCopy(), StringCchCat(), StringCchPrintf()
#include <assert.h>

// Avoids the need of Windows 7 SDK
#ifndef WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE
#define WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE   0x0010
#endif

// Supported in Windows Vista and Windows 7.
// http://msdn.microsoft.com/en-us/library/dd370819(v=VS.85).aspx
// Taken from Mmddk.h.
#define DRV_RESERVED                      0x0800
#define DRV_QUERYFUNCTIONINSTANCEID       (DRV_RESERVED + 17)
#define DRV_QUERYFUNCTIONINSTANCEIDSIZE   (DRV_RESERVED + 18)

#define POW2(A) (2 << ((A) - 1))

namespace webrtc {

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsWave - ctor
// ----------------------------------------------------------------------------

AudioDeviceWindowsWave::AudioDeviceWindowsWave(const int32_t id) :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _timeEvent(*EventTimerWrapper::Create()),
    _recStartEvent(*EventWrapper::Create()),
    _playStartEvent(*EventWrapper::Create()),
    _hGetCaptureVolumeThread(NULL),
    _hShutdownGetVolumeEvent(NULL),
    _hSetCaptureVolumeThread(NULL),
    _hShutdownSetVolumeEvent(NULL),
    _hSetCaptureVolumeEvent(NULL),
    _critSectCb(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _mixerManager(id),
    _usingInputDeviceIndex(false),
    _usingOutputDeviceIndex(false),
    _inputDevice(AudioDeviceModule::kDefaultDevice),
    _outputDevice(AudioDeviceModule::kDefaultDevice),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _inputDeviceIsSpecified(false),
    _outputDeviceIsSpecified(false),
    _initialized(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _recording(false),
    _playing(false),
    _startRec(false),
    _stopRec(false),
    _startPlay(false),
    _stopPlay(false),
    _AGC(false),
    _hWaveIn(NULL),
    _hWaveOut(NULL),
    _recChannels(N_REC_CHANNELS),
    _playChannels(N_PLAY_CHANNELS),
    _recBufCount(0),
    _recPutBackDelay(0),
    _recDelayCount(0),
    _playBufCount(0),
    _prevPlayTime(0),
    _prevRecTime(0),
    _prevTimerCheckTime(0),
    _timesdwBytes(0),
    _timerFaults(0),
    _timerRestartAttempts(0),
    _no_of_msecleft_warnings(0),
    _MAX_minBuffer(65),
    _useHeader(0),
    _dTcheckPlayBufDelay(10),
    _playBufDelay(80),
    _playBufDelayFixed(80),
    _minPlayBufDelay(20),
    _avgCPULoad(0),
    _sndCardPlayDelay(0),
    _sndCardRecDelay(0),
    _plSampOld(0),
    _rcSampOld(0),
    _playBufType(AudioDeviceModule::kAdaptiveBufferSize),
    _recordedBytes(0),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _newMicLevel(0),
    _minMicVolume(0),
    _maxMicVolume(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created", __FUNCTION__);

    // Initialize value, set to 0 if it fails
    if (!QueryPerformanceFrequency(&_perfFreq))
    {
        _perfFreq.QuadPart = 0;
    }

    _hShutdownGetVolumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownSetVolumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hSetCaptureVolumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsWave - dtor
// ----------------------------------------------------------------------------

AudioDeviceWindowsWave::~AudioDeviceWindowsWave()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed", __FUNCTION__);

    Terminate();

    delete &_recStartEvent;
    delete &_playStartEvent;
    delete &_timeEvent;
    delete &_critSect;
    delete &_critSectCb;

    if (NULL != _hShutdownGetVolumeEvent)
    {
        CloseHandle(_hShutdownGetVolumeEvent);
        _hShutdownGetVolumeEvent = NULL;
    }

    if (NULL != _hShutdownSetVolumeEvent)
    {
        CloseHandle(_hShutdownSetVolumeEvent);
        _hShutdownSetVolumeEvent = NULL;
    }

    if (NULL != _hSetCaptureVolumeEvent)
    {
        CloseHandle(_hSetCaptureVolumeEvent);
        _hSetCaptureVolumeEvent = NULL;
    }
}

// ============================================================================
//                                     API
// ============================================================================

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{

    CriticalSectionScoped lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // inform the AudioBuffer about default settings for this implementation
    _ptrAudioBuffer->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
    _ptrAudioBuffer->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
    _ptrAudioBuffer->SetRecordingChannels(N_REC_CHANNELS);
    _ptrAudioBuffer->SetPlayoutChannels(N_PLAY_CHANNELS);
}

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const
{
    audioLayer = AudioDeviceModule::kWindowsWaveAudio;
    return 0;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

AudioDeviceGeneric::InitStatus AudioDeviceWindowsWave::Init() {
  CriticalSectionScoped lock(&_critSect);

  if (_initialized) {
    return InitStatus::OK;
  }

  const uint32_t nowTime(rtc::TimeMillis());

  _recordedBytes = 0;
  _prevRecByteCheckTime = nowTime;
  _prevRecTime = nowTime;
  _prevPlayTime = nowTime;
  _prevTimerCheckTime = nowTime;

  _playWarning = 0;
  _playError = 0;
  _recWarning = 0;
  _recError = 0;

  _mixerManager.EnumerateAll();

  if (_ptrThread) {
    // thread is already created and active
    return InitStatus::OK;
  }

  const char* threadName = "webrtc_audio_module_thread";
  _ptrThread.reset(new rtc::PlatformThread(ThreadFunc, this, threadName));
  _ptrThread->Start();
  _ptrThread->SetPriority(rtc::kRealtimePriority);

  const bool periodic(true);
  if (!_timeEvent.StartTimer(periodic, TIMER_PERIOD_MS)) {
    LOG(LS_ERROR) << "failed to start the timer event";
    _ptrThread->Stop();
    _ptrThread.reset();
    return InitStatus::OTHER_ERROR;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "periodic timer (dT=%d) is now active", TIMER_PERIOD_MS);

  _hGetCaptureVolumeThread =
      CreateThread(NULL, 0, GetCaptureVolumeThread, this, 0, NULL);
  if (_hGetCaptureVolumeThread == NULL) {
    LOG(LS_ERROR) << "  failed to create the volume getter thread";
    return InitStatus::OTHER_ERROR;
  }

  SetThreadPriority(_hGetCaptureVolumeThread, THREAD_PRIORITY_NORMAL);

  _hSetCaptureVolumeThread =
      CreateThread(NULL, 0, SetCaptureVolumeThread, this, 0, NULL);
  if (_hSetCaptureVolumeThread == NULL) {
    LOG(LS_ERROR) << "  failed to create the volume setter thread";
    return InitStatus::OTHER_ERROR;
  }

  SetThreadPriority(_hSetCaptureVolumeThread, THREAD_PRIORITY_NORMAL);

  _initialized = true;

  return InitStatus::OK;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::Terminate()
{

    if (!_initialized)
    {
        return 0;
    }

    _critSect.Enter();

    _mixerManager.Close();

    if (_ptrThread)
    {
        rtc::PlatformThread* tmpThread = _ptrThread.release();
        _critSect.Leave();

        _timeEvent.Set();

        tmpThread->Stop();
        delete tmpThread;
    }
    else
    {
        _critSect.Leave();
    }

    _critSect.Enter();
    SetEvent(_hShutdownGetVolumeEvent);
    _critSect.Leave();
    int32_t ret = WaitForSingleObject(_hGetCaptureVolumeThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  failed to close down volume getter thread");
        CloseHandle(_hGetCaptureVolumeThread);
        _hGetCaptureVolumeThread = NULL;
        return -1;
    }
    _critSect.Enter();
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "  volume getter thread is now closed");

    SetEvent(_hShutdownSetVolumeEvent);
    _critSect.Leave();
    ret = WaitForSingleObject(_hSetCaptureVolumeThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  failed to close down volume setter thread");
        CloseHandle(_hSetCaptureVolumeThread);
        _hSetCaptureVolumeThread = NULL;
        return -1;
    }
    _critSect.Enter();
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "  volume setter thread is now closed");

    CloseHandle(_hGetCaptureVolumeThread);
    _hGetCaptureVolumeThread = NULL;

    CloseHandle(_hSetCaptureVolumeThread);
    _hSetCaptureVolumeThread = NULL;

    _critSect.Leave();

    _timeEvent.StopTimer();

    _initialized = false;
    _outputDeviceIsSpecified = false;
    _inputDeviceIsSpecified = false;

    return 0;
}


DWORD WINAPI AudioDeviceWindowsWave::GetCaptureVolumeThread(LPVOID context)
{
    return(((AudioDeviceWindowsWave*)context)->DoGetCaptureVolumeThread());
}

DWORD WINAPI AudioDeviceWindowsWave::SetCaptureVolumeThread(LPVOID context)
{
    return(((AudioDeviceWindowsWave*)context)->DoSetCaptureVolumeThread());
}

DWORD AudioDeviceWindowsWave::DoGetCaptureVolumeThread()
{
    HANDLE waitObject = _hShutdownGetVolumeEvent;

    while (1)
    {
        DWORD waitResult = WaitForSingleObject(waitObject,
                                               GET_MIC_VOLUME_INTERVAL_MS);
        switch (waitResult)
        {
            case WAIT_OBJECT_0: // _hShutdownGetVolumeEvent
                return 0;
            case WAIT_TIMEOUT:	// timeout notification
                break;
            default:            // unexpected error
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                    "  unknown wait termination on get volume thread");
                return 1;
        }

        if (AGC())
        {
            uint32_t currentMicLevel = 0;
            if (MicrophoneVolume(currentMicLevel) == 0)
            {
                // This doesn't set the system volume, just stores it.
                _critSect.Enter();
                if (_ptrAudioBuffer)
                {
                    _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);
                }
                _critSect.Leave();
            }
        }
    }
}

DWORD AudioDeviceWindowsWave::DoSetCaptureVolumeThread()
{
    HANDLE waitArray[2] = {_hShutdownSetVolumeEvent, _hSetCaptureVolumeEvent};

    while (1)
    {
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
            case WAIT_OBJECT_0:     // _hShutdownSetVolumeEvent
                return 0;
            case WAIT_OBJECT_0 + 1: // _hSetCaptureVolumeEvent
                break;
            default:                // unexpected error
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                    "  unknown wait termination on set volume thread");
                return 1;
        }

        _critSect.Enter();
        uint32_t newMicLevel = _newMicLevel;
        _critSect.Leave();

        if (SetMicrophoneVolume(newMicLevel) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "  the required modification of the microphone volume failed");
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::Initialized() const
{
    return (_initialized);
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::InitSpeaker()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

    if (_mixerManager.EnumerateSpeakers() == -1)
    {
        // failed to locate any valid/controllable speaker
        return -1;
    }

    if (IsUsingOutputDeviceIndex())
    {
        if (_mixerManager.OpenSpeaker(OutputDeviceIndex()) == -1)
        {
            return -1;
        }
    }
    else
    {
        if (_mixerManager.OpenSpeaker(OutputDevice()) == -1)
        {
            return -1;
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::InitMicrophone()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_mixerManager.EnumerateMicrophones() == -1)
    {
        // failed to locate any valid/controllable microphone
        return -1;
    }

    if (IsUsingInputDeviceIndex())
    {
        if (_mixerManager.OpenMicrophone(InputDeviceIndex()) == -1)
        {
            return -1;
        }
    }
    else
    {
        if (_mixerManager.OpenMicrophone(InputDevice()) == -1)
        {
            return -1;
        }
    }

    uint32_t maxVol = 0;
    if (_mixerManager.MaxMicrophoneVolume(maxVol) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
            "  unable to retrieve max microphone volume");
    }
    _maxMicVolume = maxVol;

    uint32_t minVol = 0;
    if (_mixerManager.MinMicrophoneVolume(minVol) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
            "  unable to retrieve min microphone volume");
    }
    _minMicVolume = minVol;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::SpeakerIsInitialized() const
{
    return (_mixerManager.SpeakerIsInitialized());
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::MicrophoneIsInitialized() const
{
    return (_mixerManager.MicrophoneIsInitialized());
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SpeakerVolumeIsAvailable(bool& available)
{

    bool isAvailable(false);

    // Enumerate all avaliable speakers and make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (InitSpeaker() == -1)
    {
        // failed to find a valid speaker
        available = false;
        return 0;
    }

    // Check if the selected speaker has a volume control
    //
    _mixerManager.SpeakerVolumeIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized output mixer
    //
    _mixerManager.CloseSpeaker();

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetSpeakerVolume(uint32_t volume)
{

    return (_mixerManager.SetSpeakerVolume(volume));
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SpeakerVolume(uint32_t& volume) const
{

    uint32_t level(0);

    if (_mixerManager.SpeakerVolume(level) == -1)
    {
        return -1;
    }

    volume = level;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
//
//    The low-order word contains the left-channel volume setting, and the
//    high-order word contains the right-channel setting.
//    A value of 0xFFFF represents full volume, and a value of 0x0000 is silence.
//
//    If a device does not support both left and right volume control,
//    the low-order word of dwVolume specifies the volume level,
//    and the high-order word is ignored.
//
//    Most devices do not support the full 16 bits of volume-level control
//    and will not use the least-significant bits of the requested volume setting.
//    For example, if a device supports 4 bits of volume control, the values
//    0x4000, 0x4FFF, and 0x43BE will all be truncated to 0x4000.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight)
{

    MMRESULT res(0);
    WAVEOUTCAPS caps;

    CriticalSectionScoped lock(&_critSect);

    if (_hWaveOut == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no open playout device exists => using default");
    }

    // To determine whether the device supports volume control on both
    // the left and right channels, use the WAVECAPS_LRVOLUME flag.
    //
    res = waveOutGetDevCaps((UINT_PTR)_hWaveOut, &caps, sizeof(WAVEOUTCAPS));
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetDevCaps() failed (err=%d)", res);
        TraceWaveOutError(res);
    }
    if (!(caps.dwSupport & WAVECAPS_VOLUME))
    {
        // this device does not support volume control using the waveOutSetVolume API
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device does not support volume control using the Wave API");
        return -1;
    }
    if (!(caps.dwSupport & WAVECAPS_LRVOLUME))
    {
        // high-order word (right channel) is ignored
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "device does not support volume control on both channels");
    }

    DWORD dwVolume(0x00000000);
    dwVolume = (DWORD)(((volumeRight & 0xFFFF) << 16) | (volumeLeft & 0xFFFF));

    res = waveOutSetVolume(_hWaveOut, dwVolume);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveOutSetVolume() failed (err=%d)", res);
        TraceWaveOutError(res);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
//
//    The low-order word of this location contains the left-channel volume setting,
//    and the high-order word contains the right-channel setting.
//    A value of 0xFFFF (65535) represents full volume, and a value of 0x0000
//    is silence.
//
//    If a device does not support both left and right volume control,
//    the low-order word of the specified location contains the mono volume level.
//
//    The full 16-bit setting(s) set with the waveOutSetVolume function is returned,
//    regardless of whether the device supports the full 16 bits of volume-level
//    control.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::WaveOutVolume(uint16_t& volumeLeft, uint16_t& volumeRight) const
{

    MMRESULT res(0);
    WAVEOUTCAPS caps;

    CriticalSectionScoped lock(&_critSect);

    if (_hWaveOut == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no open playout device exists => using default");
    }

    // To determine whether the device supports volume control on both
    // the left and right channels, use the WAVECAPS_LRVOLUME flag.
    //
    res = waveOutGetDevCaps((UINT_PTR)_hWaveOut, &caps, sizeof(WAVEOUTCAPS));
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetDevCaps() failed (err=%d)", res);
        TraceWaveOutError(res);
    }
    if (!(caps.dwSupport & WAVECAPS_VOLUME))
    {
        // this device does not support volume control using the waveOutSetVolume API
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device does not support volume control using the Wave API");
        return -1;
    }
    if (!(caps.dwSupport & WAVECAPS_LRVOLUME))
    {
        // high-order word (right channel) is ignored
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "device does not support volume control on both channels");
    }

    DWORD dwVolume(0x00000000);

    res = waveOutGetVolume(_hWaveOut, &dwVolume);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveOutGetVolume() failed (err=%d)", res);
        TraceWaveOutError(res);
        return -1;
    }

    WORD wVolumeLeft = LOWORD(dwVolume);
    WORD wVolumeRight = HIWORD(dwVolume);

    volumeLeft = static_cast<uint16_t> (wVolumeLeft);
    volumeRight = static_cast<uint16_t> (wVolumeRight);

    return 0;
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MaxSpeakerVolume(uint32_t& maxVolume) const
{

    uint32_t maxVol(0);

    if (_mixerManager.MaxSpeakerVolume(maxVol) == -1)
    {
        return -1;
    }

    maxVolume = maxVol;
    return 0;
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MinSpeakerVolume(uint32_t& minVolume) const
{

    uint32_t minVol(0);

    if (_mixerManager.MinSpeakerVolume(minVol) == -1)
    {
        return -1;
    }

    minVolume = minVol;
    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SpeakerVolumeStepSize(uint16_t& stepSize) const
{

    uint16_t delta(0);

    if (_mixerManager.SpeakerVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;
    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SpeakerMuteIsAvailable(bool& available)
{

    bool isAvailable(false);

    // Enumerate all avaliable speakers and make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected speaker has a mute control
    //
    _mixerManager.SpeakerMuteIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized output mixer
    //
    _mixerManager.CloseSpeaker();

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetSpeakerMute(bool enable)
{
    return (_mixerManager.SetSpeakerMute(enable));
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SpeakerMute(bool& enabled) const
{

    bool muted(0);

    if (_mixerManager.SpeakerMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneMuteIsAvailable(bool& available)
{

    bool isAvailable(false);

    // Enumerate all avaliable microphones and make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no boost control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a mute control
    //
    _mixerManager.MicrophoneMuteIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    //
    _mixerManager.CloseMicrophone();

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetMicrophoneMute(bool enable)
{
    return (_mixerManager.SetMicrophoneMute(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneMute(bool& enabled) const
{

    bool muted(0);

    if (_mixerManager.MicrophoneMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneBoostIsAvailable(bool& available)
{

    bool isAvailable(false);

    // Enumerate all avaliable microphones and make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no boost control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a boost control
    //
    _mixerManager.MicrophoneBoostIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    //
    _mixerManager.CloseMicrophone();

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetMicrophoneBoost(bool enable)
{

    return (_mixerManager.SetMicrophoneBoost(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneBoost(bool& enabled) const
{

    bool onOff(0);

    if (_mixerManager.MicrophoneBoost(onOff) == -1)
    {
        return -1;
    }

    enabled = onOff;
    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StereoRecordingIsAvailable(bool& available)
{
    available = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetStereoRecording(bool enable)
{

    if (enable)
        _recChannels = 2;
    else
        _recChannels = 1;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StereoRecording(bool& enabled) const
{

    if (_recChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StereoPlayoutIsAvailable(bool& available)
{
    available = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
//
//  Specifies the number of output channels.
//
//  NOTE - the setting will only have an effect after InitPlayout has
//  been called.
//
//  16-bit mono:
//
//  Each sample is 2 bytes. Sample 1 is followed by samples 2, 3, 4, and so on.
//  For each sample, the first byte is the low-order byte of channel 0 and the
//  second byte is the high-order byte of channel 0.
//
//  16-bit stereo:
//
//  Each sample is 4 bytes. Sample 1 is followed by samples 2, 3, 4, and so on.
//  For each sample, the first byte is the low-order byte of channel 0 (left channel);
//  the second byte is the high-order byte of channel 0; the third byte is the
//  low-order byte of channel 1 (right channel); and the fourth byte is the
//  high-order byte of channel 1.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetStereoPlayout(bool enable)
{

    if (enable)
        _playChannels = 2;
    else
        _playChannels = 1;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StereoPlayout(bool& enabled) const
{

    if (_playChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetAGC(bool enable)
{

    _AGC = enable;

    return 0;
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::AGC() const
{
    return _AGC;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneVolumeIsAvailable(bool& available)
{

    bool isAvailable(false);

    // Enumerate all avaliable microphones and make an attempt to open up the
    // input mixer corresponding to the currently selected output device.
    //
    if (InitMicrophone() == -1)
    {
        // Failed to find valid microphone
        available = false;
        return 0;
    }

    // Check if the selected microphone has a volume control
    //
    _mixerManager.MicrophoneVolumeIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    //
    _mixerManager.CloseMicrophone();

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetMicrophoneVolume(uint32_t volume)
{
    return (_mixerManager.SetMicrophoneVolume(volume));
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneVolume(uint32_t& volume) const
{
    uint32_t level(0);

    if (_mixerManager.MicrophoneVolume(level) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to retrive current microphone level");
        return -1;
    }

    volume = level;
    return 0;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MaxMicrophoneVolume(uint32_t& maxVolume) const
{
    // _maxMicVolume can be zero in AudioMixerManager::MaxMicrophoneVolume():
    // (1) API GetLineControl() returns failure at querying the max Mic level.
    // (2) API GetLineControl() returns maxVolume as zero in rare cases.
    // Both cases show we don't have access to the mixer controls.
    // We return -1 here to indicate that.
    if (_maxMicVolume == 0)
    {
        return -1;
    }

    maxVolume = _maxMicVolume;;
    return 0;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MinMicrophoneVolume(uint32_t& minVolume) const
{
    minVolume = _minMicVolume;
    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MicrophoneVolumeStepSize(uint16_t& stepSize) const
{

    uint16_t delta(0);

    if (_mixerManager.MicrophoneVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;
    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsWave::PlayoutDevices()
{

    return (waveOutGetNumDevs());
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetPlayoutDevice(uint16_t index)
{

    if (_playIsInitialized)
    {
        return -1;
    }

    UINT nDevices = waveOutGetNumDevs();
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "number of availiable waveform-audio output devices is %u", nDevices);

    if (index < 0 || index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _usingOutputDeviceIndex = true;
    _outputDeviceIndex = index;
    _outputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device)
{
    if (_playIsInitialized)
    {
        return -1;
    }

    if (device == AudioDeviceModule::kDefaultDevice)
    {
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
    }

    _usingOutputDeviceIndex = false;
    _outputDevice = device;
    _outputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    uint16_t nDevices(PlayoutDevices());

    // Special fix for the case when the user asks for the name of the default device.
    //
    if (index == (uint16_t)(-1))
    {
        index = 0;
    }

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    WAVEOUTCAPSW caps;    // szPname member (product name (NULL terminated) is a WCHAR
    MMRESULT res;

    res = waveOutGetDevCapsW(index, &caps, sizeof(WAVEOUTCAPSW));
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetDevCapsW() failed (err=%d)", res);
        return -1;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, kAdmMaxDeviceNameSize, NULL, NULL) == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 1", GetLastError());
    }

    if (guid == NULL)
    {
        return 0;
    }

    // It is possible to get the unique endpoint ID string using the Wave API.
    // However, it is only supported on Windows Vista and Windows 7.

    size_t cbEndpointId(0);

    // Get the size (including the terminating null) of the endpoint ID string of the waveOut device.
    // Windows Vista supports the DRV_QUERYFUNCTIONINSTANCEIDSIZE and DRV_QUERYFUNCTIONINSTANCEID messages.
    res = waveOutMessage((HWAVEOUT)IntToPtr(index),
                          DRV_QUERYFUNCTIONINSTANCEIDSIZE,
                         (DWORD_PTR)&cbEndpointId, NULL);
    if (res != MMSYSERR_NOERROR)
    {
        // DRV_QUERYFUNCTIONINSTANCEIDSIZE is not supported <=> earlier version of Windows than Vista
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "waveOutMessage(DRV_QUERYFUNCTIONINSTANCEIDSIZE) failed (err=%d)", res);
        TraceWaveOutError(res);
        // Best we can do is to copy the friendly name and use it as guid
        if (WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 2", GetLastError());
        }
        return 0;
    }

    // waveOutMessage(DRV_QUERYFUNCTIONINSTANCEIDSIZE) worked => we are on a Vista or Windows 7 device

    WCHAR *pstrEndpointId = NULL;
    pstrEndpointId = (WCHAR*)CoTaskMemAlloc(cbEndpointId);

    // Get the endpoint ID string for this waveOut device.
    res = waveOutMessage((HWAVEOUT)IntToPtr(index),
                          DRV_QUERYFUNCTIONINSTANCEID,
                         (DWORD_PTR)pstrEndpointId,
                          cbEndpointId);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "waveOutMessage(DRV_QUERYFUNCTIONINSTANCEID) failed (err=%d)", res);
        TraceWaveOutError(res);
        // Best we can do is to copy the friendly name and use it as guid
        if (WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 3", GetLastError());
        }
        CoTaskMemFree(pstrEndpointId);
        return 0;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, pstrEndpointId, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 4", GetLastError());
    }
    CoTaskMemFree(pstrEndpointId);

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    uint16_t nDevices(RecordingDevices());

    // Special fix for the case when the user asks for the name of the default device.
    //
    if (index == (uint16_t)(-1))
    {
        index = 0;
    }

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    WAVEINCAPSW caps;    // szPname member (product name (NULL terminated) is a WCHAR
    MMRESULT res;

    res = waveInGetDevCapsW(index, &caps, sizeof(WAVEINCAPSW));
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInGetDevCapsW() failed (err=%d)", res);
        return -1;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, kAdmMaxDeviceNameSize, NULL, NULL) == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 1", GetLastError());
    }

    if (guid == NULL)
    {
        return 0;
    }

    // It is possible to get the unique endpoint ID string using the Wave API.
    // However, it is only supported on Windows Vista and Windows 7.

    size_t cbEndpointId(0);

    // Get the size (including the terminating null) of the endpoint ID string of the waveOut device.
    // Windows Vista supports the DRV_QUERYFUNCTIONINSTANCEIDSIZE and DRV_QUERYFUNCTIONINSTANCEID messages.
    res = waveInMessage((HWAVEIN)IntToPtr(index),
                         DRV_QUERYFUNCTIONINSTANCEIDSIZE,
                        (DWORD_PTR)&cbEndpointId, NULL);
    if (res != MMSYSERR_NOERROR)
    {
        // DRV_QUERYFUNCTIONINSTANCEIDSIZE is not supported <=> earlier version of Windows than Vista
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "waveInMessage(DRV_QUERYFUNCTIONINSTANCEIDSIZE) failed (err=%d)", res);
        TraceWaveInError(res);
        // Best we can do is to copy the friendly name and use it as guid
        if (WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 2", GetLastError());
        }
        return 0;
    }

    // waveOutMessage(DRV_QUERYFUNCTIONINSTANCEIDSIZE) worked => we are on a Vista or Windows 7 device

    WCHAR *pstrEndpointId = NULL;
    pstrEndpointId = (WCHAR*)CoTaskMemAlloc(cbEndpointId);

    // Get the endpoint ID string for this waveOut device.
    res = waveInMessage((HWAVEIN)IntToPtr(index),
                          DRV_QUERYFUNCTIONINSTANCEID,
                         (DWORD_PTR)pstrEndpointId,
                          cbEndpointId);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "waveInMessage(DRV_QUERYFUNCTIONINSTANCEID) failed (err=%d)", res);
        TraceWaveInError(res);
        // Best we can do is to copy the friendly name and use it as guid
        if (WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 3", GetLastError());
        }
        CoTaskMemFree(pstrEndpointId);
        return 0;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, pstrEndpointId, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d - 4", GetLastError());
    }
    CoTaskMemFree(pstrEndpointId);

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsWave::RecordingDevices()
{

    return (waveInGetNumDevs());
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetRecordingDevice(uint16_t index)
{

    if (_recIsInitialized)
    {
        return -1;
    }

    UINT nDevices = waveInGetNumDevs();
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "number of availiable waveform-audio input devices is %u", nDevices);

    if (index < 0 || index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _usingInputDeviceIndex = true;
    _inputDeviceIndex = index;
    _inputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device)
{
    if (device == AudioDeviceModule::kDefaultDevice)
    {
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
    }

    if (_recIsInitialized)
    {
        return -1;
    }

    _usingInputDeviceIndex = false;
    _inputDevice = device;
    _inputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::PlayoutIsAvailable(bool& available)
{

    available = false;

    // Try to initialize the playout side
    int32_t res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::RecordingIsAvailable(bool& available)
{

    available = false;

    // Try to initialize the recording side
    int32_t res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::InitPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

    if (!_outputDeviceIsSpecified)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        return 0;
    }

    // Initialize the speaker (devices might have been added or removed)
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "InitSpeaker() failed");
    }

    // Enumerate all availiable output devices
    EnumeratePlayoutDevices();

    // Start by closing any existing wave-output devices
    //
    MMRESULT res(MMSYSERR_ERROR);

    if (_hWaveOut != NULL)
    {
        res = waveOutClose(_hWaveOut);
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutClose() failed (err=%d)", res);
            TraceWaveOutError(res);
        }
    }

    // Set the output wave format
    //
    WAVEFORMATEX waveFormat;

    waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
    waveFormat.nChannels       = _playChannels;  // mono <=> 1, stereo <=> 2
    waveFormat.nSamplesPerSec  = N_PLAY_SAMPLES_PER_SEC;
    waveFormat.wBitsPerSample  = 16;
    waveFormat.nBlockAlign     = waveFormat.nChannels * (waveFormat.wBitsPerSample/8);
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0;

    // Open the given waveform-audio output device for playout
    //
    HWAVEOUT hWaveOut(NULL);

    if (IsUsingOutputDeviceIndex())
    {
        // verify settings first
        res = waveOutOpen(NULL, _outputDeviceIndex, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_FORMAT_QUERY);
        if (MMSYSERR_NOERROR == res)
        {
            // open the given waveform-audio output device for recording
            res = waveOutOpen(&hWaveOut, _outputDeviceIndex, &waveFormat, 0, 0, CALLBACK_NULL);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening output device corresponding to device ID %u", _outputDeviceIndex);
        }
    }
    else
    {
        if (_outputDevice == AudioDeviceModule::kDefaultCommunicationDevice)
        {
            // check if it is possible to open the default communication device (supported on Windows 7)
            res = waveOutOpen(NULL, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE | WAVE_FORMAT_QUERY);
            if (MMSYSERR_NOERROR == res)
            {
                // if so, open the default communication device for real
                res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL |  WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default communication device");
            }
            else
            {
                // use default device since default communication device was not avaliable
                res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "unable to open default communication device => using default instead");
            }
        }
        else if (_outputDevice == AudioDeviceModule::kDefaultDevice)
        {
            // open default device since it has been requested
            res = waveOutOpen(NULL, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_FORMAT_QUERY);
            if (MMSYSERR_NOERROR == res)
            {
                res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default output device");
            }
        }
    }

    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveOutOpen() failed (err=%d)", res);
        TraceWaveOutError(res);
        return -1;
    }

    // Log information about the aquired output device
    //
    WAVEOUTCAPS caps;

    res = waveOutGetDevCaps((UINT_PTR)hWaveOut, &caps, sizeof(WAVEOUTCAPS));
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetDevCaps() failed (err=%d)", res);
        TraceWaveOutError(res);
    }

    UINT deviceID(0);
    res = waveOutGetID(hWaveOut, &deviceID);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetID() failed (err=%d)", res);
        TraceWaveOutError(res);
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "utilized device ID : %u", deviceID);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product name       : %s", caps.szPname);

    // Store valid handle for the open waveform-audio output device
    _hWaveOut = hWaveOut;

    // Store the input wave header as well
    _waveFormatOut = waveFormat;

    // Prepare wave-out headers
    //
    const uint8_t bytesPerSample = 2*_playChannels;

    for (int n = 0; n < N_BUFFERS_OUT; n++)
    {
        // set up the output wave header
        _waveHeaderOut[n].lpData          = reinterpret_cast<LPSTR>(&_playBuffer[n]);
        _waveHeaderOut[n].dwBufferLength  = bytesPerSample*PLAY_BUF_SIZE_IN_SAMPLES;
        _waveHeaderOut[n].dwFlags         = 0;
        _waveHeaderOut[n].dwLoops         = 0;

        memset(_playBuffer[n], 0, bytesPerSample*PLAY_BUF_SIZE_IN_SAMPLES);

        // The waveOutPrepareHeader function prepares a waveform-audio data block for playback.
        // The lpData, dwBufferLength, and dwFlags members of the WAVEHDR structure must be set
        // before calling this function.
        //
        res = waveOutPrepareHeader(_hWaveOut, &_waveHeaderOut[n], sizeof(WAVEHDR));
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutPrepareHeader(%d) failed (err=%d)", n, res);
            TraceWaveOutError(res);
        }

        // perform extra check to ensure that the header is prepared
        if (_waveHeaderOut[n].dwFlags != WHDR_PREPARED)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutPrepareHeader(%d) failed (dwFlags != WHDR_PREPARED)", n);
        }
    }

    // Mark playout side as initialized
    _playIsInitialized = true;

    _dTcheckPlayBufDelay = 10;  // check playback buffer delay every 10 ms
    _playBufCount = 0;          // index of active output wave header (<=> output buffer index)
    _playBufDelay = 80;         // buffer delay/size is initialized to 80 ms and slowly decreased until er < 25
    _minPlayBufDelay = 25;      // minimum playout buffer delay
    _MAX_minBuffer = 65;        // adaptive minimum playout buffer delay cannot be larger than this value
    _intro = 1;                 // Used to make sure that adaption starts after (2000-1700)/100 seconds
    _waitCounter = 1700;        // Counter for start of adaption of playback buffer
    _erZeroCounter = 0;         // Log how many times er = 0 in consequtive calls to RecTimeProc
    _useHeader = 0;             // Counts number of "useHeader" detections. Stops at 2.

    _writtenSamples = 0;
    _writtenSamplesOld = 0;
    _playedSamplesOld = 0;
    _sndCardPlayDelay = 0;
    _sndCardRecDelay = 0;

    WEBRTC_TRACE(kTraceInfo, kTraceUtility, _id,"initial playout status: _playBufDelay=%d, _minPlayBufDelay=%d",
        _playBufDelay, _minPlayBufDelay);

    return 0;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::InitRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (!_inputDeviceIsSpecified)
    {
        return -1;
    }

    if (_recIsInitialized)
    {
        return 0;
    }

    _avgCPULoad = 0;
    _playAcc  = 0;

    // Initialize the microphone (devices might have been added or removed)
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "InitMicrophone() failed");
    }

    // Enumerate all availiable input devices
    EnumerateRecordingDevices();

    // Start by closing any existing wave-input devices
    //
    MMRESULT res(MMSYSERR_ERROR);

    if (_hWaveIn != NULL)
    {
        res = waveInClose(_hWaveIn);
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInClose() failed (err=%d)", res);
            TraceWaveInError(res);
        }
    }

    // Set the input wave format
    //
    WAVEFORMATEX waveFormat;

    waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
    waveFormat.nChannels       = _recChannels;  // mono <=> 1, stereo <=> 2
    waveFormat.nSamplesPerSec  = N_REC_SAMPLES_PER_SEC;
    waveFormat.wBitsPerSample  = 16;
    waveFormat.nBlockAlign     = waveFormat.nChannels * (waveFormat.wBitsPerSample/8);
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0;

    // Open the given waveform-audio input device for recording
    //
    HWAVEIN hWaveIn(NULL);

    if (IsUsingInputDeviceIndex())
    {
        // verify settings first
        res = waveInOpen(NULL, _inputDeviceIndex, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_FORMAT_QUERY);
        if (MMSYSERR_NOERROR == res)
        {
            // open the given waveform-audio input device for recording
            res = waveInOpen(&hWaveIn, _inputDeviceIndex, &waveFormat, 0, 0, CALLBACK_NULL);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening input device corresponding to device ID %u", _inputDeviceIndex);
        }
    }
    else
    {
        if (_inputDevice == AudioDeviceModule::kDefaultCommunicationDevice)
        {
            // check if it is possible to open the default communication device (supported on Windows 7)
            res = waveInOpen(NULL, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE | WAVE_FORMAT_QUERY);
            if (MMSYSERR_NOERROR == res)
            {
                // if so, open the default communication device for real
                res = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default communication device");
            }
            else
            {
                // use default device since default communication device was not avaliable
                res = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "unable to open default communication device => using default instead");
            }
        }
        else if (_inputDevice == AudioDeviceModule::kDefaultDevice)
        {
            // open default device since it has been requested
            res = waveInOpen(NULL, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_FORMAT_QUERY);
            if (MMSYSERR_NOERROR == res)
            {
                res = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default input device");
            }
        }
    }

    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveInOpen() failed (err=%d)", res);
        TraceWaveInError(res);
        return -1;
    }

    // Log information about the aquired input device
    //
    WAVEINCAPS caps;

    res = waveInGetDevCaps((UINT_PTR)hWaveIn, &caps, sizeof(WAVEINCAPS));
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInGetDevCaps() failed (err=%d)", res);
        TraceWaveInError(res);
    }

    UINT deviceID(0);
    res = waveInGetID(hWaveIn, &deviceID);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInGetID() failed (err=%d)", res);
        TraceWaveInError(res);
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "utilized device ID : %u", deviceID);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product name       : %s", caps.szPname);

    // Store valid handle for the open waveform-audio input device
    _hWaveIn = hWaveIn;

    // Store the input wave header as well
    _waveFormatIn = waveFormat;

    // Mark recording side as initialized
    _recIsInitialized = true;

    _recBufCount = 0;     // index of active input wave header (<=> input buffer index)
    _recDelayCount = 0;   // ensures that input buffers are returned with certain delay

    return 0;
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StartRecording()
{

    if (!_recIsInitialized)
    {
        return -1;
    }

    if (_recording)
    {
        return 0;
    }

    // set state to ensure that the recording starts from the audio thread
    _startRec = true;

    // the audio thread will signal when recording has stopped
    if (kEventTimeout == _recStartEvent.Wait(10000))
    {
        _startRec = false;
        StopRecording();
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to activate recording");
        return -1;
    }

    if (_recording)
    {
        // the recording state is set by the audio thread after recording has started
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to activate recording");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StopRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized)
    {
        return 0;
    }

    if (_hWaveIn == NULL)
    {
        return -1;
    }

    bool wasRecording = _recording;
    _recIsInitialized = false;
    _recording = false;

    MMRESULT res;

    // Stop waveform-adio input. If there are any buffers in the queue, the
    // current buffer will be marked as done (the dwBytesRecorded member in
    // the header will contain the length of data), but any empty buffers in
    // the queue will remain there.
    //
    res = waveInStop(_hWaveIn);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInStop() failed (err=%d)", res);
        TraceWaveInError(res);
    }

    // Stop input on the given waveform-audio input device and resets the current
    // position to zero. All pending buffers are marked as done and returned to
    // the application.
    //
    res = waveInReset(_hWaveIn);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInReset() failed (err=%d)", res);
        TraceWaveInError(res);
    }

    // Clean up the preparation performed by the waveInPrepareHeader function.
    // Only unprepare header if recording was ever started (and headers are prepared).
    //
    if (wasRecording)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "waveInUnprepareHeader() will be performed");
        for (int n = 0; n < N_BUFFERS_IN; n++)
        {
            res = waveInUnprepareHeader(_hWaveIn, &_waveHeaderIn[n], sizeof(WAVEHDR));
            if (MMSYSERR_NOERROR != res)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInUnprepareHeader() failed (err=%d)", res);
                TraceWaveInError(res);
            }
        }
    }

    // Close the given waveform-audio input device.
    //
    res = waveInClose(_hWaveIn);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInClose() failed (err=%d)", res);
        TraceWaveInError(res);
    }

    // Set the wave input handle to NULL
    //
    _hWaveIn = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_hWaveIn is now set to NULL");

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::RecordingIsInitialized() const
{
    return (_recIsInitialized);
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::Recording() const
{
    return (_recording);
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::PlayoutIsInitialized() const
{
    return (_playIsInitialized);
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StartPlayout()
{

    if (!_playIsInitialized)
    {
        return -1;
    }

    if (_playing)
    {
        return 0;
    }

    // set state to ensure that playout starts from the audio thread
    _startPlay = true;

    // the audio thread will signal when recording has started
    if (kEventTimeout == _playStartEvent.Wait(10000))
    {
        _startPlay = false;
        StopPlayout();
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to activate playout");
        return -1;
    }

    if (_playing)
    {
        // the playing state is set by the audio thread after playout has started
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to activate playing");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::StopPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized)
    {
        return 0;
    }

    if (_hWaveOut == NULL)
    {
        return -1;
    }

    _playIsInitialized = false;
    _playing = false;
    _sndCardPlayDelay = 0;
    _sndCardRecDelay = 0;

    MMRESULT res;

    // The waveOutReset function stops playback on the given waveform-audio
    // output device and resets the current position to zero. All pending
    // playback buffers are marked as done (WHDR_DONE) and returned to the application.
    // After this function returns, the application can send new playback buffers
    // to the device by calling waveOutWrite, or close the device by calling waveOutClose.
    //
    res = waveOutReset(_hWaveOut);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutReset() failed (err=%d)", res);
        TraceWaveOutError(res);
    }

    // The waveOutUnprepareHeader function cleans up the preparation performed
    // by the waveOutPrepareHeader function. This function must be called after
    // the device driver is finished with a data block.
    // You must call this function before freeing the buffer.
    //
    for (int n = 0; n < N_BUFFERS_OUT; n++)
    {
        res = waveOutUnprepareHeader(_hWaveOut, &_waveHeaderOut[n], sizeof(WAVEHDR));
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutUnprepareHeader() failed (err=%d)", res);
            TraceWaveOutError(res);
        }
    }

    // The waveOutClose function closes the given waveform-audio output device.
    // The close operation fails if the device is still playing a waveform-audio
    // buffer that was previously sent by calling waveOutWrite. Before calling
    // waveOutClose, the application must wait for all buffers to finish playing
    // or call the waveOutReset function to terminate playback.
    //
    res = waveOutClose(_hWaveOut);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutClose() failed (err=%d)", res);
        TraceWaveOutError(res);
    }

    _hWaveOut = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_hWaveOut is now set to NULL");

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::PlayoutDelay(uint16_t& delayMS) const
{
    CriticalSectionScoped lock(&_critSect);
    delayMS = (uint16_t)_sndCardPlayDelay;
    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::RecordingDelay(uint16_t& delayMS) const
{
    CriticalSectionScoped lock(&_critSect);
    delayMS = (uint16_t)_sndCardRecDelay;
    return 0;
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::Playing() const
{
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::SetPlayoutBuffer(const AudioDeviceModule::BufferType type, uint16_t sizeMS)
{
    CriticalSectionScoped lock(&_critSect);
    _playBufType = type;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::PlayoutBuffer(AudioDeviceModule::BufferType& type, uint16_t& sizeMS) const
{
    CriticalSectionScoped lock(&_critSect);
    type = _playBufType;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed;
    }
    else
    {
        sizeMS = _playBufDelay;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::CPULoad(uint16_t& load) const
{

    load = static_cast<uint16_t>(100*_avgCPULoad);

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::PlayoutWarning() const
{
    return ( _playWarning > 0);
}

// ----------------------------------------------------------------------------
//  PlayoutError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::PlayoutError() const
{
    return ( _playError > 0);
}

// ----------------------------------------------------------------------------
//  RecordingWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::RecordingWarning() const
{
    return ( _recWarning > 0);
}

// ----------------------------------------------------------------------------
//  RecordingError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::RecordingError() const
{
    return ( _recError > 0);
}

// ----------------------------------------------------------------------------
//  ClearPlayoutWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::ClearPlayoutWarning()
{
    _playWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearPlayoutError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::ClearPlayoutError()
{
    _playError = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::ClearRecordingWarning()
{
    _recWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::ClearRecordingError()
{
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  InputSanityCheckAfterUnlockedPeriod
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::InputSanityCheckAfterUnlockedPeriod() const
{
    if (_hWaveIn == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "input state has been modified during unlocked period");
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  OutputSanityCheckAfterUnlockedPeriod
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::OutputSanityCheckAfterUnlockedPeriod() const
{
    if (_hWaveOut == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "output state has been modified during unlocked period");
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  EnumeratePlayoutDevices
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::EnumeratePlayoutDevices()
{

    uint16_t nDevices(PlayoutDevices());
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "===============================================================");
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#output devices: %u", nDevices);

    WAVEOUTCAPS caps;
    MMRESULT res;

    for (UINT deviceID = 0; deviceID < nDevices; deviceID++)
    {
        res = waveOutGetDevCaps(deviceID, &caps, sizeof(WAVEOUTCAPS));
        if (res != MMSYSERR_NOERROR)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetDevCaps() failed (err=%d)", res);
        }

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "===============================================================");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Device ID %u:", deviceID);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "manufacturer ID      : %u", caps.wMid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product ID           : %u",caps.wPid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "version of driver    : %u.%u", HIBYTE(caps.vDriverVersion), LOBYTE(caps.vDriverVersion));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product name         : %s", caps.szPname);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "dwFormats            : 0x%x", caps.dwFormats);
        if (caps.dwFormats & WAVE_FORMAT_48S16)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "  48kHz,stereo,16bit : SUPPORTED");
        }
        else
        {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, " 48kHz,stereo,16bit  : *NOT* SUPPORTED");
        }
        if (caps.dwFormats & WAVE_FORMAT_48M16)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "  48kHz,mono,16bit   : SUPPORTED");
        }
        else
        {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, " 48kHz,mono,16bit    : *NOT* SUPPORTED");
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wChannels            : %u", caps.wChannels);
        TraceSupportFlags(caps.dwSupport);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  EnumerateRecordingDevices
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::EnumerateRecordingDevices()
{

    uint16_t nDevices(RecordingDevices());
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "===============================================================");
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#input devices: %u", nDevices);

    WAVEINCAPS caps;
    MMRESULT res;

    for (UINT deviceID = 0; deviceID < nDevices; deviceID++)
    {
        res = waveInGetDevCaps(deviceID, &caps, sizeof(WAVEINCAPS));
        if (res != MMSYSERR_NOERROR)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInGetDevCaps() failed (err=%d)", res);
        }

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "===============================================================");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Device ID %u:", deviceID);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "manufacturer ID      : %u", caps.wMid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product ID           : %u",caps.wPid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "version of driver    : %u.%u", HIBYTE(caps.vDriverVersion), LOBYTE(caps.vDriverVersion));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product name         : %s", caps.szPname);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "dwFormats            : 0x%x", caps.dwFormats);
        if (caps.dwFormats & WAVE_FORMAT_48S16)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "  48kHz,stereo,16bit : SUPPORTED");
        }
        else
        {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, " 48kHz,stereo,16bit  : *NOT* SUPPORTED");
        }
        if (caps.dwFormats & WAVE_FORMAT_48M16)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "  48kHz,mono,16bit   : SUPPORTED");
        }
        else
        {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, " 48kHz,mono,16bit    : *NOT* SUPPORTED");
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wChannels            : %u", caps.wChannels);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  TraceSupportFlags
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::TraceSupportFlags(DWORD dwSupport) const
{
    TCHAR buf[256];

    StringCchPrintf(buf, 128, TEXT("support flags        : 0x%x "), dwSupport);

    if (dwSupport & WAVECAPS_PITCH)
    {
        // supports pitch control
        StringCchCat(buf, 256, TEXT("(PITCH)"));
    }
    if (dwSupport & WAVECAPS_PLAYBACKRATE)
    {
        // supports playback rate control
        StringCchCat(buf, 256, TEXT("(PLAYBACKRATE)"));
    }
    if (dwSupport & WAVECAPS_VOLUME)
    {
        // supports volume control
        StringCchCat(buf, 256, TEXT("(VOLUME)"));
    }
    if (dwSupport & WAVECAPS_LRVOLUME)
    {
        // supports separate left and right volume control
        StringCchCat(buf, 256, TEXT("(LRVOLUME)"));
    }
    if (dwSupport & WAVECAPS_SYNC)
    {
        // the driver is synchronous and will block while playing a buffer
        StringCchCat(buf, 256, TEXT("(SYNC)"));
    }
    if (dwSupport & WAVECAPS_SAMPLEACCURATE)
    {
        // returns sample-accurate position information
        StringCchCat(buf, 256, TEXT("(SAMPLEACCURATE)"));
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%S", buf);
}

// ----------------------------------------------------------------------------
//  TraceWaveInError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::TraceWaveInError(MMRESULT error) const
{
    TCHAR buf[MAXERRORLENGTH];
    TCHAR msg[MAXERRORLENGTH];

    StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
    waveInGetErrorText(error, msg, MAXERRORLENGTH);
    StringCchCat(buf, MAXERRORLENGTH, msg);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%S", buf);
}

// ----------------------------------------------------------------------------
//  TraceWaveOutError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWave::TraceWaveOutError(MMRESULT error) const
{
    TCHAR buf[MAXERRORLENGTH];
    TCHAR msg[MAXERRORLENGTH];

    StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
    waveOutGetErrorText(error, msg, MAXERRORLENGTH);
    StringCchCat(buf, MAXERRORLENGTH, msg);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%S", buf);
}

// ----------------------------------------------------------------------------
//  PrepareStartPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::PrepareStartPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (_hWaveOut == NULL)
    {
        return -1;
    }

    // A total of 30ms of data is immediately placed in the SC buffer
    //
    int8_t zeroVec[4*PLAY_BUF_SIZE_IN_SAMPLES];  // max allocation
    memset(zeroVec, 0, 4*PLAY_BUF_SIZE_IN_SAMPLES);

    {
        Write(zeroVec, PLAY_BUF_SIZE_IN_SAMPLES);
        Write(zeroVec, PLAY_BUF_SIZE_IN_SAMPLES);
        Write(zeroVec, PLAY_BUF_SIZE_IN_SAMPLES);
    }

    _playAcc = 0;
    _playWarning = 0;
    _playError = 0;
    _dc_diff_mean = 0;
    _dc_y_prev = 0;
    _dc_penalty_counter = 20;
    _dc_prevtime = 0;
    _dc_prevplay = 0;

    return 0;
}

// ----------------------------------------------------------------------------
//  PrepareStartRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::PrepareStartRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (_hWaveIn == NULL)
    {
        return -1;
    }

    _playAcc = 0;
    _recordedBytes = 0;
    _recPutBackDelay = REC_PUT_BACK_DELAY;

    MMRESULT res;
    MMTIME mmtime;
    mmtime.wType = TIME_SAMPLES;

    res = waveInGetPosition(_hWaveIn, &mmtime, sizeof(mmtime));
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInGetPosition(TIME_SAMPLES) failed (err=%d)", res);
        TraceWaveInError(res);
    }

    _read_samples = mmtime.u.sample;
    _read_samples_old = _read_samples;
    _rec_samples_old = mmtime.u.sample;
    _wrapCounter = 0;

    for (int n = 0; n < N_BUFFERS_IN; n++)
    {
        const uint8_t nBytesPerSample = 2*_recChannels;

        // set up the input wave header
        _waveHeaderIn[n].lpData          = reinterpret_cast<LPSTR>(&_recBuffer[n]);
        _waveHeaderIn[n].dwBufferLength  = nBytesPerSample * REC_BUF_SIZE_IN_SAMPLES;
        _waveHeaderIn[n].dwFlags         = 0;
        _waveHeaderIn[n].dwBytesRecorded = 0;
        _waveHeaderIn[n].dwUser          = 0;

        memset(_recBuffer[n], 0, nBytesPerSample * REC_BUF_SIZE_IN_SAMPLES);

        // prepare a buffer for waveform-audio input
        res = waveInPrepareHeader(_hWaveIn, &_waveHeaderIn[n], sizeof(WAVEHDR));
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInPrepareHeader(%d) failed (err=%d)", n, res);
            TraceWaveInError(res);
        }

        // send an input buffer to the given waveform-audio input device
        res = waveInAddBuffer(_hWaveIn, &_waveHeaderIn[n], sizeof(WAVEHDR));
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInAddBuffer(%d) failed (err=%d)", n, res);
            TraceWaveInError(res);
        }
    }

    // start input on the given waveform-audio input device
    res = waveInStart(_hWaveIn);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInStart() failed (err=%d)", res);
        TraceWaveInError(res);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  GetPlayoutBufferDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::GetPlayoutBufferDelay(uint32_t& writtenSamples, uint32_t& playedSamples)
{
    int i;
    int ms_Header;
    long playedDifference;
    int msecInPlayoutBuffer(0);   // #milliseconds of audio in the playout buffer

    const uint16_t nSamplesPerMs = (uint16_t)(N_PLAY_SAMPLES_PER_SEC/1000);  // default is 48000/1000 = 48

    MMRESULT res;
    MMTIME mmtime;

    if (!_playing)
    {
        playedSamples = 0;
        return (0);
    }

    // Retrieve the current playback position.
    //
    mmtime.wType = TIME_SAMPLES;  // number of waveform-audio samples
    res = waveOutGetPosition(_hWaveOut, &mmtime, sizeof(mmtime));
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutGetPosition() failed (err=%d)", res);
        TraceWaveOutError(res);
    }

    writtenSamples = _writtenSamples;   // #samples written to the playout buffer
    playedSamples = mmtime.u.sample;    // current playout position in the playout buffer

    // derive remaining amount (in ms) of data in the playout buffer
    msecInPlayoutBuffer = ((writtenSamples - playedSamples)/nSamplesPerMs);

    playedDifference = (long) (_playedSamplesOld - playedSamples);

    if (playedDifference > 64000)
    {
        // If the sound cards number-of-played-out-samples variable wraps around before
        // written_sampels wraps around this needs to be adjusted. This can happen on
        // sound cards that uses less than 32 bits to keep track of number of played out
        // sampels. To avoid being fooled by sound cards that sometimes produces false
        // output we compare old value minus the new value with a large value. This is
        // neccessary because some SC:s produce an output like 153, 198, 175, 230 which
        // would trigger the wrap-around function if we didn't compare with a large value.
        // The value 64000 is chosen because 2^16=65536 so we allow wrap around at 16 bits.

        i = 31;
        while((_playedSamplesOld <= (unsigned long)POW2(i)) && (i > 14)) {
            i--;
        }

        if((i < 31) && (i > 14)) {
            // Avoid adjusting when there is 32-bit wrap-around since that is
            // something neccessary.
            //
            WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "msecleft() => wrap around occured: %d bits used by sound card)", (i+1));

            _writtenSamples = _writtenSamples - POW2(i + 1);
            writtenSamples = _writtenSamples;
            msecInPlayoutBuffer = ((writtenSamples - playedSamples)/nSamplesPerMs);
        }
    }
    else if ((_writtenSamplesOld > (unsigned long)POW2(31)) && (writtenSamples < 96000))
    {
        // Wrap around as expected after having used all 32 bits. (But we still
        // test if the wrap around happened earlier which it should not)

        i = 31;
        while (_writtenSamplesOld <= (unsigned long)POW2(i)) {
            i--;
        }

        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "  msecleft() (wrap around occured after having used all 32 bits)");

        _writtenSamplesOld = writtenSamples;
        _playedSamplesOld = playedSamples;
        msecInPlayoutBuffer = (int)((writtenSamples + POW2(i + 1) - playedSamples)/nSamplesPerMs);

    }
    else if ((writtenSamples < 96000) && (playedSamples > (unsigned long)POW2(31)))
    {
        // Wrap around has, as expected, happened for written_sampels before
        // playedSampels so we have to adjust for this until also playedSampels
        // has had wrap around.

        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "  msecleft() (wrap around occured: correction of output is done)");

        _writtenSamplesOld = writtenSamples;
        _playedSamplesOld = playedSamples;
        msecInPlayoutBuffer = (int)((writtenSamples + POW2(32) - playedSamples)/nSamplesPerMs);
    }

    _writtenSamplesOld = writtenSamples;
    _playedSamplesOld = playedSamples;


    // We use the following formaula to track that playout works as it should
    // y=playedSamples/48 - timeGetTime();
    // y represent the clock drift between system clock and sound card clock - should be fairly stable
    // When the exponential mean value of diff(y) goes away from zero something is wrong
    // The exponential formula will accept 1% clock drift but not more
    // The driver error means that we will play to little audio and have a high negative clock drift
    // We kick in our alternative method when the clock drift reaches 20%

    int diff,y;
    int unsigned time =0;

    // If we have other problems that causes playout glitches
    // we don't want to switch playout method.
    // Check if playout buffer is extremely low, or if we haven't been able to
    // exectue our code in more than 40 ms

    time = timeGetTime();

    if ((msecInPlayoutBuffer < 20) || (time - _dc_prevtime > 40))
    {
        _dc_penalty_counter = 100;
    }

    if ((playedSamples != 0))
    {
        y = playedSamples/48 - time;
        if ((_dc_y_prev != 0) && (_dc_penalty_counter == 0))
        {
            diff = y - _dc_y_prev;
            _dc_diff_mean = (990*_dc_diff_mean)/1000 + 10*diff;
        }
        _dc_y_prev = y;
    }

    if (_dc_penalty_counter)
    {
        _dc_penalty_counter--;
    }

    if (_dc_diff_mean < -200)
    {
        // Always reset the filter
        _dc_diff_mean = 0;

        // Problem is detected. Switch delay method and set min buffer to 80.
        // Reset the filter and keep monitoring the filter output.
        // If issue is detected a second time, increase min buffer to 100.
        // If that does not help, we must modify this scheme further.

        _useHeader++;
        if (_useHeader == 1)
        {
            _minPlayBufDelay = 80;
            _playWarning = 1;   // only warn first time
            WEBRTC_TRACE(kTraceInfo, kTraceUtility, -1, "Modification #1: _useHeader = %d, _minPlayBufDelay = %d", _useHeader, _minPlayBufDelay);
        }
        else if (_useHeader == 2)
        {
            _minPlayBufDelay = 100;   // add some more safety
            WEBRTC_TRACE(kTraceInfo, kTraceUtility, -1, "Modification #2: _useHeader = %d, _minPlayBufDelay = %d", _useHeader, _minPlayBufDelay);
        }
        else
        {
            // This state should not be entered... (HA)
            WEBRTC_TRACE (kTraceWarning, kTraceUtility, -1, "further actions are required!");
        }
        if (_playWarning == 1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "pending playout warning exists");
        }
        _playWarning = 1;  // triggers callback from module process thread
        WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "kPlayoutWarning message posted: switching to alternative playout delay method");
    }
    _dc_prevtime = time;
    _dc_prevplay = playedSamples;

    // Try a very rough method of looking at how many buffers are still playing
    ms_Header = 0;
    for (i = 0; i < N_BUFFERS_OUT; i++) {
        if ((_waveHeaderOut[i].dwFlags & WHDR_INQUEUE)!=0) {
            ms_Header += 10;
        }
    }

    if ((ms_Header-50) > msecInPlayoutBuffer) {
        // Test for cases when GetPosition appears to be screwed up (currently just log....)
        TCHAR infoStr[300];
        if (_no_of_msecleft_warnings%20==0)
        {
            StringCchPrintf(infoStr, 300, TEXT("writtenSamples=%i, playedSamples=%i, msecInPlayoutBuffer=%i, ms_Header=%i"), writtenSamples, playedSamples, msecInPlayoutBuffer, ms_Header);
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "%S", infoStr);
        }
        _no_of_msecleft_warnings++;
    }

    // If this is true we have had a problem with the playout
    if (_useHeader > 0)
    {
        return (ms_Header);
    }


    if (ms_Header < msecInPlayoutBuffer)
    {
        if (_no_of_msecleft_warnings % 100 == 0)
        {
            TCHAR str[300];
            StringCchPrintf(str, 300, TEXT("_no_of_msecleft_warnings=%i, msecInPlayoutBuffer=%i ms_Header=%i (minBuffer=%i buffersize=%i writtenSamples=%i playedSamples=%i)"),
                _no_of_msecleft_warnings, msecInPlayoutBuffer, ms_Header, _minPlayBufDelay, _playBufDelay, writtenSamples, playedSamples);
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "%S", str);
        }
        _no_of_msecleft_warnings++;
        ms_Header -= 6; // Round off as we only have 10ms resolution + Header info is usually slightly delayed compared to GetPosition

        if (ms_Header < 0)
            ms_Header = 0;

        return (ms_Header);
    }
    else
    {
        return (msecInPlayoutBuffer);
    }
}

// ----------------------------------------------------------------------------
//  GetRecordingBufferDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::GetRecordingBufferDelay(uint32_t& readSamples, uint32_t& recSamples)
{
    long recDifference;
    MMTIME mmtime;
    MMRESULT mmr;

    const uint16_t nSamplesPerMs = (uint16_t)(N_REC_SAMPLES_PER_SEC/1000);  // default is 48000/1000 = 48

    // Retrieve the current input position of the given waveform-audio input device
    //
    mmtime.wType = TIME_SAMPLES;
    mmr = waveInGetPosition(_hWaveIn, &mmtime, sizeof(mmtime));
    if (MMSYSERR_NOERROR != mmr)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInGetPosition() failed (err=%d)", mmr);
        TraceWaveInError(mmr);
    }

    readSamples = _read_samples;    // updated for each full fram in RecProc()
    recSamples = mmtime.u.sample;   // remaining time in input queue (recorded but not read yet)

    recDifference = (long) (_rec_samples_old - recSamples);

    if( recDifference > 64000) {
        WEBRTC_TRACE (kTraceDebug, kTraceUtility, -1,"WRAP 1 (recDifference =%d)", recDifference);
        // If the sound cards number-of-recorded-samples variable wraps around before
        // read_sampels wraps around this needs to be adjusted. This can happen on
        // sound cards that uses less than 32 bits to keep track of number of played out
        // sampels. To avoid being fooled by sound cards that sometimes produces false
        // output we compare old value minus the new value with a large value. This is
        // neccessary because some SC:s produce an output like 153, 198, 175, 230 which
        // would trigger the wrap-around function if we didn't compare with a large value.
        // The value 64000 is chosen because 2^16=65536 so we allow wrap around at 16 bits.
        //
        int i = 31;
        while((_rec_samples_old <= (unsigned long)POW2(i)) && (i > 14))
            i--;

        if((i < 31) && (i > 14)) {
            // Avoid adjusting when there is 32-bit wrap-around since that is
            // somethying neccessary.
            //
            _read_samples = _read_samples - POW2(i + 1);
            readSamples = _read_samples;
            _wrapCounter++;
        } else {
            WEBRTC_TRACE (kTraceWarning, kTraceUtility, -1,"AEC (_rec_samples_old %d recSamples %d)",_rec_samples_old, recSamples);
        }
    }

    if((_wrapCounter>200)){
        // Do nothing, handled later
    }
    else if((_rec_samples_old > (unsigned long)POW2(31)) && (recSamples < 96000)) {
        WEBRTC_TRACE (kTraceDebug, kTraceUtility, -1,"WRAP 2 (_rec_samples_old %d recSamples %d)",_rec_samples_old, recSamples);
        // Wrap around as expected after having used all 32 bits.
        _read_samples_old = readSamples;
        _rec_samples_old = recSamples;
        _wrapCounter++;
        return (int)((recSamples + POW2(32) - readSamples)/nSamplesPerMs);


    } else if((recSamples < 96000) && (readSamples > (unsigned long)POW2(31))) {
        WEBRTC_TRACE (kTraceDebug, kTraceUtility, -1,"WRAP 3 (readSamples %d recSamples %d)",readSamples, recSamples);
        // Wrap around has, as expected, happened for rec_sampels before
        // readSampels so we have to adjust for this until also readSampels
        // has had wrap around.
        _read_samples_old = readSamples;
        _rec_samples_old = recSamples;
        _wrapCounter++;
        return (int)((recSamples + POW2(32) - readSamples)/nSamplesPerMs);
    }

    _read_samples_old = _read_samples;
    _rec_samples_old = recSamples;
    int res=(((int)_rec_samples_old - (int)_read_samples_old)/nSamplesPerMs);

    if((res > 2000)||(res < 0)||(_wrapCounter>200)){
        // Reset everything
        WEBRTC_TRACE (kTraceWarning, kTraceUtility, -1,"msec_read error (res %d wrapCounter %d)",res, _wrapCounter);
        MMTIME mmtime;
        mmtime.wType = TIME_SAMPLES;

        mmr=waveInGetPosition(_hWaveIn, &mmtime, sizeof(mmtime));
        if (mmr != MMSYSERR_NOERROR) {
            WEBRTC_TRACE (kTraceWarning, kTraceUtility, -1, "waveInGetPosition failed (mmr=%d)", mmr);
        }
        _read_samples=mmtime.u.sample;
        _read_samples_old=_read_samples;
        _rec_samples_old=mmtime.u.sample;

        // Guess a decent value
        res = 20;
    }

    _wrapCounter = 0;
    return res;
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  ThreadFunc
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::ThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceWindowsWave*>(pThis)->ThreadProcess());
}

// ----------------------------------------------------------------------------
//  ThreadProcess
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWave::ThreadProcess()
{
    uint32_t time(0);
    uint32_t playDiff(0);
    uint32_t recDiff(0);

    LONGLONG playTime(0);
    LONGLONG recTime(0);

    switch (_timeEvent.Wait(1000))
    {
    case kEventSignaled:
        break;
    case kEventError:
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "EventWrapper::Wait() failed => restarting timer");
        _timeEvent.StopTimer();
        _timeEvent.StartTimer(true, TIMER_PERIOD_MS);
        return true;
    case kEventTimeout:
        return true;
    }

    time = rtc::TimeMillis();

    if (_startPlay)
    {
        if (PrepareStartPlayout() == 0)
        {
            _prevTimerCheckTime = time;
            _prevPlayTime = time;
            _startPlay = false;
            _playing = true;
            _playStartEvent.Set();
        }
    }

    if (_startRec)
    {
        if (PrepareStartRecording() == 0)
        {
            _prevTimerCheckTime = time;
            _prevRecTime = time;
            _prevRecByteCheckTime = time;
            _startRec = false;
            _recording = true;
            _recStartEvent.Set();
        }
    }

    if (_playing)
    {
        playDiff = time - _prevPlayTime;
    }

    if (_recording)
    {
        recDiff = time - _prevRecTime;
    }

    if (_playing || _recording)
    {
        RestartTimerIfNeeded(time);
    }

    if (_playing &&
        (playDiff > (uint32_t)(_dTcheckPlayBufDelay - 1)) ||
        (playDiff < 0))
    {
        Lock();
        if (_playing)
        {
            if (PlayProc(playTime) == -1)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "PlayProc() failed");
            }
            _prevPlayTime = time;
            if (playTime != 0)
                _playAcc += playTime;
        }
        UnLock();
    }

    if (_playing && (playDiff > 12))
    {
        // It has been a long time since we were able to play out, try to
        // compensate by calling PlayProc again.
        //
        Lock();
        if (_playing)
        {
            if (PlayProc(playTime))
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "PlayProc() failed");
            }
            _prevPlayTime = time;
            if (playTime != 0)
                _playAcc += playTime;
        }
        UnLock();
    }

    if (_recording &&
       (recDiff > REC_CHECK_TIME_PERIOD_MS) ||
       (recDiff < 0))
    {
        Lock();
        if (_recording)
        {
            int32_t nRecordedBytes(0);
            uint16_t maxIter(10);

            // Deliver all availiable recorded buffers and update the CPU load measurement.
            // We use a while loop here to compensate for the fact that the multi-media timer
            // can sometimed enter a "bad state" after hibernation where the resolution is
            // reduced from ~1ms to ~10-15 ms.
            //
            while ((nRecordedBytes = RecProc(recTime)) > 0)
            {
                maxIter--;
                _recordedBytes += nRecordedBytes;
                if (recTime && _perfFreq.QuadPart)
                {
                    // Measure the average CPU load:
                    // This is a simplified expression where an exponential filter is used:
                    //   _avgCPULoad = 0.99 * _avgCPULoad + 0.01 * newCPU,
                    //   newCPU = (recTime+playAcc)/f is time in seconds
                    //   newCPU / 0.01 is the fraction of a 10 ms period
                    // The two 0.01 cancels each other.
                    // NOTE - assumes 10ms audio buffers.
                    //
                    _avgCPULoad = (float)(_avgCPULoad*.99 + (recTime+_playAcc)/(double)(_perfFreq.QuadPart));
                    _playAcc = 0;
                }
                if (maxIter == 0)
                {
                    // If we get this message ofte, our compensation scheme is not sufficient.
                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "failed to compensate for reduced MM-timer resolution");
                }
            }

            if (nRecordedBytes == -1)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "RecProc() failed");
            }

            _prevRecTime = time;

            // Monitor the recording process and generate error/warning callbacks if needed
            MonitorRecording(time);
        }
        UnLock();
    }

    if (!_recording)
    {
        _prevRecByteCheckTime = time;
        _avgCPULoad = 0;
    }

    return true;
}

// ----------------------------------------------------------------------------
//  RecProc
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::RecProc(LONGLONG& consumedTime)
{
    MMRESULT res;
    uint32_t bufCount(0);
    uint32_t nBytesRecorded(0);

    consumedTime = 0;

    // count modulo N_BUFFERS_IN (0,1,2,...,(N_BUFFERS_IN-1),0,1,2,..)
    if (_recBufCount == N_BUFFERS_IN)
    {
        _recBufCount = 0;
    }

    bufCount = _recBufCount;

    // take mono/stereo mode into account when deriving size of a full buffer
    const uint16_t bytesPerSample = 2*_recChannels;
    const uint32_t fullBufferSizeInBytes = bytesPerSample * REC_BUF_SIZE_IN_SAMPLES;

    // read number of recorded bytes for the given input-buffer
    nBytesRecorded = _waveHeaderIn[bufCount].dwBytesRecorded;

    if (nBytesRecorded == fullBufferSizeInBytes ||
       (nBytesRecorded > 0))
    {
        int32_t msecOnPlaySide;
        int32_t msecOnRecordSide;
        uint32_t writtenSamples;
        uint32_t playedSamples;
        uint32_t readSamples, recSamples;
        bool send = true;

        uint32_t nSamplesRecorded = (nBytesRecorded/bytesPerSample);  // divide by 2 or 4 depending on mono or stereo

        if (nBytesRecorded == fullBufferSizeInBytes)
        {
            _timesdwBytes = 0;
        }
        else
        {
            // Test if it is stuck on this buffer
            _timesdwBytes++;
            if (_timesdwBytes < 5)
            {
                // keep trying
                return (0);
            }
            else
            {
                WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id,"nBytesRecorded=%d => don't use", nBytesRecorded);
                _timesdwBytes = 0;
                send = false;
            }
        }

        // store the recorded buffer (no action will be taken if the #recorded samples is not a full buffer)
        _ptrAudioBuffer->SetRecordedBuffer(_waveHeaderIn[bufCount].lpData, nSamplesRecorded);

        // update #samples read
        _read_samples += nSamplesRecorded;

        // Check how large the playout and recording buffers are on the sound card.
        // This info is needed by the AEC.
        //
        msecOnPlaySide = GetPlayoutBufferDelay(writtenSamples, playedSamples);
        msecOnRecordSide = GetRecordingBufferDelay(readSamples, recSamples);

        // If we use the alternative playout delay method, skip the clock drift compensation
        // since it will be an unreliable estimate and might degrade AEC performance.
        int32_t drift = (_useHeader > 0) ? 0 : GetClockDrift(playedSamples, recSamples);

        _ptrAudioBuffer->SetVQEData(msecOnPlaySide, msecOnRecordSide, drift);

        _ptrAudioBuffer->SetTypingStatus(KeyPressed());

        // Store the play and rec delay values for video synchronization
        _sndCardPlayDelay = msecOnPlaySide;
        _sndCardRecDelay = msecOnRecordSide;

        LARGE_INTEGER t1={0},t2={0};

        if (send)
        {
            QueryPerformanceCounter(&t1);

            // deliver recorded samples at specified sample rate, mic level etc. to the observer using callback
            UnLock();
            _ptrAudioBuffer->DeliverRecordedData();
            Lock();

            QueryPerformanceCounter(&t2);

            if (InputSanityCheckAfterUnlockedPeriod() == -1)
            {
                // assert(false);
                return -1;
            }
        }

        if (_AGC)
        {
            uint32_t  newMicLevel = _ptrAudioBuffer->NewMicLevel();
            if (newMicLevel != 0)
            {
                // The VQE will only deliver non-zero microphone levels when a change is needed.
                WEBRTC_TRACE(kTraceStream, kTraceUtility, _id,"AGC change of volume: => new=%u", newMicLevel);

                // We store this outside of the audio buffer to avoid
                // having it overwritten by the getter thread.
                _newMicLevel = newMicLevel;
                SetEvent(_hSetCaptureVolumeEvent);
            }
        }

        // return utilized buffer to queue after specified delay (default is 4)
        if (_recDelayCount > (_recPutBackDelay-1))
        {
            // deley buffer counter to compensate for "put-back-delay"
            bufCount = (bufCount + N_BUFFERS_IN - _recPutBackDelay) % N_BUFFERS_IN;

            // reset counter so we can make new detection
            _waveHeaderIn[bufCount].dwBytesRecorded = 0;

            // return the utilized wave-header after certain delay (given by _recPutBackDelay)
            res = waveInUnprepareHeader(_hWaveIn, &(_waveHeaderIn[bufCount]), sizeof(WAVEHDR));
            if (MMSYSERR_NOERROR != res)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInUnprepareHeader(%d) failed (err=%d)", bufCount, res);
                TraceWaveInError(res);
            }

            // ensure that the utilized header can be used again
            res = waveInPrepareHeader(_hWaveIn, &(_waveHeaderIn[bufCount]), sizeof(WAVEHDR));
            if (res != MMSYSERR_NOERROR)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveInPrepareHeader(%d) failed (err=%d)", bufCount, res);
                TraceWaveInError(res);
                return -1;
            }

            // add the utilized buffer to the queue again
            res = waveInAddBuffer(_hWaveIn, &(_waveHeaderIn[bufCount]), sizeof(WAVEHDR));
            if (res != MMSYSERR_NOERROR)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveInAddBuffer(%d) failed (err=%d)", bufCount, res);
                TraceWaveInError(res);
                if (_recPutBackDelay < 50)
                {
                    _recPutBackDelay++;
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "_recPutBackDelay increased to %d", _recPutBackDelay);
                }
                else
                {
                    if (_recError == 1)
                    {
                        WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "pending recording error exists");
                    }
                    _recError = 1;  // triggers callback from module process thread
                    WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kRecordingError message posted: _recPutBackDelay=%u", _recPutBackDelay);
                }
            }
        }  // if (_recDelayCount > (_recPutBackDelay-1))

        if (_recDelayCount < (_recPutBackDelay+1))
        {
            _recDelayCount++;
        }

        // increase main buffer count since one complete buffer has now been delivered
        _recBufCount++;

        if (send) {
            // Calculate processing time
            consumedTime = (int)(t2.QuadPart-t1.QuadPart);
            // handle wraps, time should not be higher than a second
            if ((consumedTime > _perfFreq.QuadPart) || (consumedTime < 0))
                consumedTime = 0;
        }

    }  // if ((nBytesRecorded == fullBufferSizeInBytes))

    return nBytesRecorded;
}

// ----------------------------------------------------------------------------
//  PlayProc
// ----------------------------------------------------------------------------

int AudioDeviceWindowsWave::PlayProc(LONGLONG& consumedTime)
{
    int32_t remTimeMS(0);
    int8_t playBuffer[4*PLAY_BUF_SIZE_IN_SAMPLES];
    uint32_t writtenSamples(0);
    uint32_t playedSamples(0);

    LARGE_INTEGER t1;
    LARGE_INTEGER t2;

    consumedTime = 0;
    _waitCounter++;

    // Get number of ms of sound that remains in the sound card buffer for playback.
    //
    remTimeMS = GetPlayoutBufferDelay(writtenSamples, playedSamples);

    // The threshold can be adaptive or fixed. The adaptive scheme is updated
    // also for fixed mode but the updated threshold is not utilized.
    //
    const uint16_t thresholdMS =
        (_playBufType == AudioDeviceModule::kAdaptiveBufferSize) ? _playBufDelay : _playBufDelayFixed;

    if (remTimeMS < thresholdMS + 9)
    {
        _dTcheckPlayBufDelay = 5;

        if (remTimeMS == 0)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceUtility, _id, "playout buffer is empty => we must adapt...");
            if (_waitCounter > 30)
            {
                _erZeroCounter++;
                if (_erZeroCounter == 2)
                {
                    _playBufDelay += 15;
                    _minPlayBufDelay += 20;
                    _waitCounter = 50;
                    WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "New playout states (er=0,erZero=2): minPlayBufDelay=%u, playBufDelay=%u", _minPlayBufDelay, _playBufDelay);
                }
                else if (_erZeroCounter == 3)
                {
                    _erZeroCounter = 0;
                    _playBufDelay += 30;
                    _minPlayBufDelay += 25;
                    _waitCounter = 0;
                    WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "New playout states (er=0, erZero=3): minPlayBufDelay=%u, playBufDelay=%u", _minPlayBufDelay, _playBufDelay);
                }
                else
                {
                    _minPlayBufDelay += 10;
                    _playBufDelay += 15;
                    _waitCounter = 50;
                    WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "New playout states (er=0, erZero=1): minPlayBufDelay=%u, playBufDelay=%u", _minPlayBufDelay, _playBufDelay);
                }
            }
        }
        else if (remTimeMS < _minPlayBufDelay)
        {
            // If there is less than 25 ms of audio in the play out buffer
            // increase the buffersize limit value. _waitCounter prevents
            // _playBufDelay to be increased every time this function is called.

            if (_waitCounter > 30)
            {
                _playBufDelay += 10;
                if (_intro == 0)
                    _waitCounter = 0;
                WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "Playout threshold is increased: playBufDelay=%u", _playBufDelay);
            }
        }
        else if (remTimeMS < thresholdMS - 9)
        {
            _erZeroCounter = 0;
        }
        else
        {
            _erZeroCounter = 0;
            _dTcheckPlayBufDelay = 10;
        }

        QueryPerformanceCounter(&t1);   // measure time: START

        // Ask for new PCM data to be played out using the AudioDeviceBuffer.
        // Ensure that this callback is executed without taking the audio-thread lock.
        //
        UnLock();
        uint32_t nSamples = _ptrAudioBuffer->RequestPlayoutData(PLAY_BUF_SIZE_IN_SAMPLES);
        Lock();

        if (OutputSanityCheckAfterUnlockedPeriod() == -1)
        {
            // assert(false);
            return -1;
        }

        nSamples = _ptrAudioBuffer->GetPlayoutData(playBuffer);
        if (nSamples != PLAY_BUF_SIZE_IN_SAMPLES)
        {
            WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "invalid number of output samples(%d)", nSamples);
        }

        QueryPerformanceCounter(&t2);   // measure time: STOP
        consumedTime = (int)(t2.QuadPart - t1.QuadPart);

        Write(playBuffer, PLAY_BUF_SIZE_IN_SAMPLES);

    }  // if (er < thresholdMS + 9)
    else if (thresholdMS + 9 < remTimeMS )
    {
        _erZeroCounter = 0;
        _dTcheckPlayBufDelay = 2;    // check buffer more often
        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "Need to check playout buffer more often (dT=%u, remTimeMS=%u)", _dTcheckPlayBufDelay, remTimeMS);
    }

    // If the buffersize has been stable for 20 seconds try to decrease the buffer size
    if (_waitCounter > 2000)
    {
        _intro = 0;
        _playBufDelay--;
        _waitCounter = 1990;
        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "Playout threshold is decreased: playBufDelay=%u", _playBufDelay);
    }

    // Limit the minimum sound card (playback) delay to adaptive minimum delay
    if (_playBufDelay < _minPlayBufDelay)
    {
        _playBufDelay = _minPlayBufDelay;
        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "Playout threshold is limited to %u", _minPlayBufDelay);
    }

    // Limit the maximum sound card (playback) delay to 150 ms
    if (_playBufDelay > 150)
    {
        _playBufDelay = 150;
        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "Playout threshold is limited to %d", _playBufDelay);
    }

    // Upper limit of the minimum sound card (playback) delay to 65 ms.
    // Deactivated during "useHeader mode" (_useHeader > 0).
    if (_minPlayBufDelay > _MAX_minBuffer &&
       (_useHeader == 0))
    {
        _minPlayBufDelay = _MAX_minBuffer;
        WEBRTC_TRACE(kTraceDebug, kTraceUtility, _id, "Minimum playout threshold is limited to %d", _MAX_minBuffer);
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  Write
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::Write(int8_t* data, uint16_t nSamples)
{
    if (_hWaveOut == NULL)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        MMRESULT res;

        const uint16_t bufCount(_playBufCount);

        // Place data in the memory associated with _waveHeaderOut[bufCount]
        //
        const int16_t nBytes = (2*_playChannels)*nSamples;
        memcpy(&_playBuffer[bufCount][0], &data[0], nBytes);

        // Send a data block to the given waveform-audio output device.
        //
        // When the buffer is finished, the WHDR_DONE bit is set in the dwFlags
        // member of the WAVEHDR structure. The buffer must be prepared with the
        // waveOutPrepareHeader function before it is passed to waveOutWrite.
        // Unless the device is paused by calling the waveOutPause function,
        // playback begins when the first data block is sent to the device.
        //
        res = waveOutWrite(_hWaveOut, &_waveHeaderOut[bufCount], sizeof(_waveHeaderOut[bufCount]));
        if (MMSYSERR_NOERROR != res)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "waveOutWrite(%d) failed (err=%d)", bufCount, res);
            TraceWaveOutError(res);

            _writeErrors++;
            if (_writeErrors > 10)
            {
                if (_playError == 1)
                {
                    WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "pending playout error exists");
                }
                _playError = 1;  // triggers callback from module process thread
                WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kPlayoutError message posted: _writeErrors=%u", _writeErrors);
            }

            return -1;
        }

        _playBufCount = (_playBufCount+1) % N_BUFFERS_OUT;  // increase buffer counter modulo size of total buffer
        _writtenSamples += nSamples;                        // each sample is 2 or 4 bytes
        _writeErrors = 0;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//    GetClockDrift
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::GetClockDrift(const uint32_t plSamp, const uint32_t rcSamp)
{
    int drift = 0;
    unsigned int plSampDiff = 0, rcSampDiff = 0;

    if (plSamp >= _plSampOld)
    {
        plSampDiff = plSamp - _plSampOld;
    }
    else
    {
        // Wrap
        int i = 31;
        while(_plSampOld <= (unsigned int)POW2(i))
        {
            i--;
        }

        // Add the amount remaining prior to wrapping
        plSampDiff = plSamp +  POW2(i + 1) - _plSampOld;
    }

    if (rcSamp >= _rcSampOld)
    {
        rcSampDiff = rcSamp - _rcSampOld;
    }
    else
    {   // Wrap
        int i = 31;
        while(_rcSampOld <= (unsigned int)POW2(i))
        {
            i--;
        }

        rcSampDiff = rcSamp +  POW2(i + 1) - _rcSampOld;
    }

    drift = plSampDiff - rcSampDiff;

    _plSampOld = plSamp;
    _rcSampOld = rcSamp;

    return drift;
}

// ----------------------------------------------------------------------------
//  MonitorRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::MonitorRecording(const uint32_t time)
{
    const uint16_t bytesPerSample = 2*_recChannels;
    const uint32_t nRecordedSamples = _recordedBytes/bytesPerSample;

    if (nRecordedSamples > 5*N_REC_SAMPLES_PER_SEC)
    {
        // 5 seconds of audio has been recorded...
        if ((time - _prevRecByteCheckTime) > 5700)
        {
            // ...and it was more than 5.7 seconds since we last did this check <=>
            // we have not been able to record 5 seconds of audio in 5.7 seconds,
            // hence a problem should be reported.
            // This problem can be related to USB overload.
            //
            if (_recWarning == 1)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "pending recording warning exists");
            }
            _recWarning = 1;  // triggers callback from module process thread
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "kRecordingWarning message posted: time-_prevRecByteCheckTime=%d", time - _prevRecByteCheckTime);
        }

        _recordedBytes = 0;            // restart "check again when 5 seconds are recorded"
        _prevRecByteCheckTime = time;  // reset timer to measure time for recording of 5 seconds
    }

    if ((time - _prevRecByteCheckTime) > 8000)
    {
        // It has been more than 8 seconds since we able to confirm that 5 seconds of
        // audio was recorded, hence we have not been able to record 5 seconds in
        // 8 seconds => the complete recording process is most likely dead.
        //
        if (_recError == 1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "pending recording error exists");
        }
        _recError = 1;  // triggers callback from module process thread
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kRecordingError message posted: time-_prevRecByteCheckTime=%d", time - _prevRecByteCheckTime);

        _prevRecByteCheckTime = time;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  MonitorRecording
//
//  Restart timer if needed (they seem to be messed up after a hibernate).
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWave::RestartTimerIfNeeded(const uint32_t time)
{
    const uint32_t diffMS = time - _prevTimerCheckTime;
    _prevTimerCheckTime = time;

    if (diffMS > 7)
    {
        // one timer-issue detected...
        _timerFaults++;
        if (_timerFaults > 5 && _timerRestartAttempts < 2)
        {
            // Reinitialize timer event if event fails to execute at least every 5ms.
            // On some machines it helps and the timer starts working as it should again;
            // however, not all machines (we have seen issues on e.g. IBM T60).
            // Therefore, the scheme below ensures that we do max 2 attempts to restart the timer.
            // For the cases where restart does not do the trick, we compensate for the reduced
            // resolution on both the recording and playout sides.
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, " timer issue detected => timer is restarted");
            _timeEvent.StopTimer();
            _timeEvent.StartTimer(true, TIMER_PERIOD_MS);
            // make sure timer gets time to start up and we don't kill/start timer serveral times over and over again
            _timerFaults = -20;
            _timerRestartAttempts++;
        }
    }
    else
    {
        // restart timer-check scheme since we are OK
        _timerFaults = 0;
        _timerRestartAttempts = 0;
    }

    return 0;
}


bool AudioDeviceWindowsWave::KeyPressed() const{

  int key_down = 0;
  for (int key = VK_SPACE; key < VK_NUMLOCK; key++) {
    short res = GetAsyncKeyState(key);
    key_down |= res & 0x1; // Get the LSB
  }
  return (key_down > 0);
}
}  // namespace webrtc
