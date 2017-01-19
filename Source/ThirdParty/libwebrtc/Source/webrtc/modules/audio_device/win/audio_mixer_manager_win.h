/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_WIN_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_WIN_H

#include "webrtc/typedefs.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include <Windows.h>
#include <mmsystem.h>

namespace webrtc {

class AudioMixerManager
{
public:
    enum { MAX_NUMBER_MIXER_DEVICES = 40 };
    enum { MAX_NUMBER_OF_LINE_CONTROLS = 20 };
    enum { MAX_NUMBER_OF_MULTIPLE_ITEMS = 20 };
    struct SpeakerLineInfo
    {
        DWORD dwLineID;
        bool  speakerIsValid;
        DWORD dwVolumeControlID;
        bool  volumeControlIsValid;
        DWORD dwMuteControlID;
        bool  muteControlIsValid;
    };
    struct MicrophoneLineInfo
    {
        DWORD dwLineID;
        bool  microphoneIsValid;
        DWORD dwVolumeControlID;
        bool  volumeControlIsValid;
        DWORD dwMuteControlID;
        bool  muteControlIsValid;
        DWORD dwOnOffControlID;
        bool  onOffControlIsValid;
    };
public:
    int32_t EnumerateAll();
    int32_t EnumerateSpeakers();
    int32_t EnumerateMicrophones();
    int32_t OpenSpeaker(AudioDeviceModule::WindowsDeviceType device);
    int32_t OpenSpeaker(uint16_t index);
    int32_t OpenMicrophone(AudioDeviceModule::WindowsDeviceType device);
    int32_t OpenMicrophone(uint16_t index);
    int32_t SetSpeakerVolume(uint32_t volume);
    int32_t SpeakerVolume(uint32_t& volume) const;
    int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;
    int32_t MinSpeakerVolume(uint32_t& minVolume) const;
    int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const;
    int32_t SpeakerVolumeIsAvailable(bool& available);
    int32_t SpeakerMuteIsAvailable(bool& available);
    int32_t SetSpeakerMute(bool enable);
    int32_t SpeakerMute(bool& enabled) const;
    int32_t MicrophoneMuteIsAvailable(bool& available);
    int32_t SetMicrophoneMute(bool enable);
    int32_t MicrophoneMute(bool& enabled) const;
    int32_t MicrophoneBoostIsAvailable(bool& available);
    int32_t SetMicrophoneBoost(bool enable);
    int32_t MicrophoneBoost(bool& enabled) const;
    int32_t MicrophoneVolumeIsAvailable(bool& available);
    int32_t SetMicrophoneVolume(uint32_t volume);
    int32_t MicrophoneVolume(uint32_t& volume) const;
    int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;
    int32_t MinMicrophoneVolume(uint32_t& minVolume) const;
    int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const;
    int32_t Close();
    int32_t CloseSpeaker();
    int32_t CloseMicrophone();
    bool SpeakerIsInitialized() const;
    bool MicrophoneIsInitialized() const;
    UINT Devices() const;

private:
    UINT DestinationLines(UINT mixId) const;
    UINT SourceLines(UINT mixId, DWORD destId) const;
    bool GetCapabilities(UINT mixId, MIXERCAPS& caps, bool trace = false) const;
    bool GetDestinationLineInfo(UINT mixId, DWORD destId, MIXERLINE& line, bool trace = false) const;
    bool GetSourceLineInfo(UINT mixId, DWORD destId, DWORD srcId, MIXERLINE& line, bool trace = false) const;

    bool GetAllLineControls(UINT mixId, const MIXERLINE& line, MIXERCONTROL* controlArray, bool trace = false) const;
    bool GetLineControl(UINT mixId, DWORD dwControlID, MIXERCONTROL& control) const;
    bool GetControlDetails(UINT mixId, MIXERCONTROL& controlArray, bool trace = false) const;
    bool GetUnsignedControlValue(UINT mixId, DWORD dwControlID, DWORD& dwValue) const;
    bool SetUnsignedControlValue(UINT mixId, DWORD dwControlID, DWORD dwValue) const;
    bool SetBooleanControlValue(UINT mixId, DWORD dwControlID, bool value) const;
    bool GetBooleanControlValue(UINT mixId, DWORD dwControlID, bool& value) const;
    bool GetSelectedMuxSource(UINT mixId, DWORD dwControlID, DWORD cMultipleItems, UINT& index) const;

private:
    void ClearSpeakerState();
    void ClearSpeakerState(UINT idx);
    void ClearMicrophoneState();
    void ClearMicrophoneState(UINT idx);
    bool SpeakerIsValid(UINT idx) const;
    UINT ValidSpeakers() const;
    bool MicrophoneIsValid(UINT idx) const;
    UINT ValidMicrophones() const;

    void TraceStatusAndSupportFlags(DWORD fdwLine) const;
    void TraceTargetType(DWORD dwType) const;
    void TraceComponentType(DWORD dwComponentType) const;
    void TraceControlType(DWORD dwControlType) const;
    void TraceControlStatusAndSupportFlags(DWORD fdwControl) const;
    void TraceWaveInError(MMRESULT error) const;
    void TraceWaveOutError(MMRESULT error) const;
    // Converts from wide-char to UTF-8 if UNICODE is defined.
    // Does nothing if UNICODE is undefined.
    char* WideToUTF8(const TCHAR* src) const;

public:
    AudioMixerManager(const int32_t id);
    ~AudioMixerManager();

private:
    CriticalSectionWrapper& _critSect;
    int32_t                 _id;
    HMIXER                  _outputMixerHandle;
    UINT                    _outputMixerID;
    HMIXER                  _inputMixerHandle;
    UINT                    _inputMixerID;
    SpeakerLineInfo         _speakerState[MAX_NUMBER_MIXER_DEVICES];
    MicrophoneLineInfo      _microphoneState[MAX_NUMBER_MIXER_DEVICES];
    mutable char            _str[MAXERRORLENGTH];
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_H
