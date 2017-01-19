/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/modules/audio_device/linux/audio_mixer_manager_alsa_linux.h"
#include "webrtc/system_wrappers/include/trace.h"

extern webrtc_adm_linux_alsa::AlsaSymbolTable AlsaSymbolTable;

// Accesses ALSA functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libalsa, which means our binary
// will work on systems that don't have it.
#define LATE(sym) \
  LATESYM_GET(webrtc_adm_linux_alsa::AlsaSymbolTable, &AlsaSymbolTable, sym)

namespace webrtc
{

AudioMixerManagerLinuxALSA::AudioMixerManagerLinuxALSA(const int32_t id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _outputMixerHandle(NULL),
    _inputMixerHandle(NULL),
    _outputMixerElement(NULL),
    _inputMixerElement(NULL)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s constructed", __FUNCTION__);

    memset(_outputMixerStr, 0, kAdmMaxDeviceNameSize);
    memset(_inputMixerStr, 0, kAdmMaxDeviceNameSize);
}

AudioMixerManagerLinuxALSA::~AudioMixerManagerLinuxALSA()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destructed", __FUNCTION__);

    Close();

    delete &_critSect;
}

// ============================================================================
//                                    PUBLIC METHODS
// ============================================================================

int32_t AudioMixerManagerLinuxALSA::Close()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s",
                 __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    CloseSpeaker();
    CloseMicrophone();

    return 0;

}

int32_t AudioMixerManagerLinuxALSA::CloseSpeaker()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s",
                 __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    int errVal = 0;

    if (_outputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing playout mixer");
        LATE(snd_mixer_free)(_outputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error freeing playout mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        errVal = LATE(snd_mixer_detach)(_outputMixerHandle, _outputMixerStr);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error detachinging playout mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        errVal = LATE(snd_mixer_close)(_outputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error snd_mixer_close(handleMixer) errVal=%d",
                         errVal);
        }
        _outputMixerHandle = NULL;
        _outputMixerElement = NULL;
    }
    memset(_outputMixerStr, 0, kAdmMaxDeviceNameSize);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::CloseMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    int errVal = 0;

    if (_inputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer");

        LATE(snd_mixer_free)(_inputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error freeing record mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer 2");

        errVal = LATE(snd_mixer_detach)(_inputMixerHandle, _inputMixerStr);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error detachinging record mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer 3");

        errVal = LATE(snd_mixer_close)(_inputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error snd_mixer_close(handleMixer) errVal=%d",
                         errVal);
        }

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer 4");
        _inputMixerHandle = NULL;
        _inputMixerElement = NULL;
    }
    memset(_inputMixerStr, 0, kAdmMaxDeviceNameSize);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::OpenSpeaker(char* deviceName)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::OpenSpeaker(name=%s)", deviceName);

    CriticalSectionScoped lock(&_critSect);

    int errVal = 0;

    // Close any existing output mixer handle
    //
    if (_outputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing playout mixer");

        LATE(snd_mixer_free)(_outputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error freeing playout mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        errVal = LATE(snd_mixer_detach)(_outputMixerHandle, _outputMixerStr);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error detachinging playout mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        errVal = LATE(snd_mixer_close)(_outputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error snd_mixer_close(handleMixer) errVal=%d",
                         errVal);
        }
    }
    _outputMixerHandle = NULL;
    _outputMixerElement = NULL;

    errVal = LATE(snd_mixer_open)(&_outputMixerHandle, 0);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "snd_mixer_open(&_outputMixerHandle, 0) - error");
        return -1;
    }

    char controlName[kAdmMaxDeviceNameSize] = { 0 };
    GetControlName(controlName, deviceName);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     snd_mixer_attach(_outputMixerHandle, %s)", controlName);

    errVal = LATE(snd_mixer_attach)(_outputMixerHandle, controlName);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     snd_mixer_attach(_outputMixerHandle, %s) error: %s",
                     controlName, LATE(snd_strerror)(errVal));
        _outputMixerHandle = NULL;
        return -1;
    }
    strcpy(_outputMixerStr, controlName);

    errVal = LATE(snd_mixer_selem_register)(_outputMixerHandle, NULL, NULL);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     snd_mixer_selem_register(_outputMixerHandle,"
                     " NULL, NULL), error: %s",
                     LATE(snd_strerror)(errVal));
        _outputMixerHandle = NULL;
        return -1;
    }

    // Load and find the proper mixer element
    if (LoadSpeakerMixerElement() < 0)
    {
        return -1;
    }

    if (_outputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  the output mixer device is now open (0x%x)",
                     _outputMixerHandle);
    }

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::OpenMicrophone(char *deviceName)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::OpenMicrophone(name=%s)",
                 deviceName);

    CriticalSectionScoped lock(&_critSect);

    int errVal = 0;

    // Close any existing input mixer handle
    //
    if (_inputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer");

        LATE(snd_mixer_free)(_inputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error freeing record mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer");

        errVal = LATE(snd_mixer_detach)(_inputMixerHandle, _inputMixerStr);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error detachinging record mixer: %s",
                         LATE(snd_strerror)(errVal));
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer");

        errVal = LATE(snd_mixer_close)(_inputMixerHandle);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error snd_mixer_close(handleMixer) errVal=%d",
                         errVal);
        }
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer");
    }
    _inputMixerHandle = NULL;
    _inputMixerElement = NULL;

    errVal = LATE(snd_mixer_open)(&_inputMixerHandle, 0);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     snd_mixer_open(&_inputMixerHandle, 0) - error");
        return -1;
    }

    char controlName[kAdmMaxDeviceNameSize] = { 0 };
    GetControlName(controlName, deviceName);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     snd_mixer_attach(_inputMixerHandle, %s)", controlName);

    errVal = LATE(snd_mixer_attach)(_inputMixerHandle, controlName);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     snd_mixer_attach(_inputMixerHandle, %s) error: %s",
                     controlName, LATE(snd_strerror)(errVal));

        _inputMixerHandle = NULL;
        return -1;
    }
    strcpy(_inputMixerStr, controlName);

    errVal = LATE(snd_mixer_selem_register)(_inputMixerHandle, NULL, NULL);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     snd_mixer_selem_register(_inputMixerHandle,"
                     " NULL, NULL), error: %s",
                     LATE(snd_strerror)(errVal));

        _inputMixerHandle = NULL;
        return -1;
    }
    // Load and find the proper mixer element
    if (LoadMicMixerElement() < 0)
    {
        return -1;
    }

    if (_inputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  the input mixer device is now open (0x%x)",
                     _inputMixerHandle);
    }

    return 0;
}

bool AudioMixerManagerLinuxALSA::SpeakerIsInitialized() const
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (_outputMixerHandle != NULL);
}

bool AudioMixerManagerLinuxALSA::MicrophoneIsInitialized() const
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s",
                 __FUNCTION__);

    return (_inputMixerHandle != NULL);
}

int32_t AudioMixerManagerLinuxALSA::SetSpeakerVolume(
    uint32_t volume)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetSpeakerVolume(volume=%u)",
                 volume);

    CriticalSectionScoped lock(&_critSect);

    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer element exists");
        return -1;
    }

    int errVal =
        LATE(snd_mixer_selem_set_playback_volume_all)(_outputMixerElement,
                                                      volume);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error changing master volume: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    return (0);
}

int32_t AudioMixerManagerLinuxALSA::SpeakerVolume(
    uint32_t& volume) const
{

    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer element exists");
        return -1;
    }

    long int vol(0);

    int
        errVal = LATE(snd_mixer_selem_get_playback_volume)(
            _outputMixerElement,
            (snd_mixer_selem_channel_id_t) 0,
            &vol);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "Error getting outputvolume: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     AudioMixerManagerLinuxALSA::SpeakerVolume() => vol=%i",
                 vol);

    volume = static_cast<uint32_t> (vol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MaxSpeakerVolume(
    uint32_t& maxVolume) const
{

    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avilable output mixer element exists");
        return -1;
    }

    long int minVol(0);
    long int maxVol(0);

    int errVal =
        LATE(snd_mixer_selem_get_playback_volume_range)(_outputMixerElement,
                                                        &minVol, &maxVol);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     Playout hardware volume range, min: %d, max: %d",
                 minVol, maxVol);

    if (maxVol <= minVol)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error getting get_playback_volume_range: %s",
                     LATE(snd_strerror)(errVal));
    }

    maxVolume = static_cast<uint32_t> (maxVol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MinSpeakerVolume(
    uint32_t& minVolume) const
{

    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer element exists");
        return -1;
    }

    long int minVol(0);
    long int maxVol(0);

    int errVal =
        LATE(snd_mixer_selem_get_playback_volume_range)(_outputMixerElement,
                                                        &minVol, &maxVol);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     Playout hardware volume range, min: %d, max: %d",
                 minVol, maxVol);

    if (maxVol <= minVol)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error getting get_playback_volume_range: %s",
                     LATE(snd_strerror)(errVal));
    }

    minVolume = static_cast<uint32_t> (minVol);

    return 0;
}

// TL: Have done testnig with these but they don't seem reliable and
// they were therefore not added
/*
 // ----------------------------------------------------------------------------
 //    SetMaxSpeakerVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMaxSpeakerVolume(
     uint32_t maxVolume)
 {

 if (_outputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
 "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_playback_volume_range(
 _outputMixerElement, &minVol, &maxVol);
 if ((maxVol <= minVol) || (errVal != 0))
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting playback volume range: %s", snd_strerror(errVal));
 }

 maxVol = maxVolume;
 errVal = snd_mixer_selem_set_playback_volume_range(
 _outputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
  "     Playout hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
  "     Error setting playback volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }

 // ----------------------------------------------------------------------------
 //    SetMinSpeakerVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMinSpeakerVolume(
     uint32_t minVolume)
 {

 if (_outputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
 "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_playback_volume_range(
 _outputMixerElement, &minVol, &maxVol);
 if ((maxVol <= minVol) || (errVal != 0))
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting playback volume range: %s", snd_strerror(errVal));
 }

 minVol = minVolume;
 errVal = snd_mixer_selem_set_playback_volume_range(
 _outputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
 "     Playout hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
 "     Error setting playback volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }
 */

int32_t AudioMixerManagerLinuxALSA::SpeakerVolumeStepSize(
    uint16_t& stepSize) const
{

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer exists");
        return -1;
    }

    // The step size is always 1 for ALSA
    stepSize = 1;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SpeakerVolumeIsAvailable(
    bool& available)
{
    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer element exists");
        return -1;
    }

    available = LATE(snd_mixer_selem_has_playback_volume)(_outputMixerElement);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SpeakerMuteIsAvailable(
    bool& available)
{
    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer element exists");
        return -1;
    }

    available = LATE(snd_mixer_selem_has_playback_switch)(_outputMixerElement);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SetSpeakerMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetSpeakerMute(enable=%u)",
                 enable);

    CriticalSectionScoped lock(&_critSect);

    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer element exists");
        return -1;
    }

    // Ensure that the selected speaker destination has a valid mute control.
    bool available(false);
    SpeakerMuteIsAvailable(available);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to mute the speaker");
        return -1;
    }

    // Note value = 0 (off) means muted
    int errVal =
        LATE(snd_mixer_selem_set_playback_switch_all)(_outputMixerElement,
                                                      !enable);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error setting playback switch: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    return (0);
}

int32_t AudioMixerManagerLinuxALSA::SpeakerMute(bool& enabled) const
{

    if (_outputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable output mixer exists");
        return -1;
    }

    // Ensure that the selected speaker destination has a valid mute control.
    bool available =
        LATE(snd_mixer_selem_has_playback_switch)(_outputMixerElement);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to mute the speaker");
        return -1;
    }

    int value(false);

    // Retrieve one boolean control value for a specified mute-control
    //
    int
        errVal = LATE(snd_mixer_selem_get_playback_switch)(
            _outputMixerElement,
            (snd_mixer_selem_channel_id_t) 0,
            &value);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error getting playback switch: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    // Note value = 0 (off) means muted
    enabled = (bool) !value;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneMuteIsAvailable(
    bool& available)
{
    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    available = LATE(snd_mixer_selem_has_capture_switch)(_inputMixerElement);
    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SetMicrophoneMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetMicrophoneMute(enable=%u)",
                 enable);

    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    // Ensure that the selected microphone destination has a valid mute control.
    bool available(false);
    MicrophoneMuteIsAvailable(available);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to mute the microphone");
        return -1;
    }

    // Note value = 0 (off) means muted
    int errVal =
        LATE(snd_mixer_selem_set_capture_switch_all)(_inputMixerElement,
                                                     !enable);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error setting capture switch: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    return (0);
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneMute(bool& enabled) const
{

    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer exists");
        return -1;
    }

    // Ensure that the selected microphone destination has a valid mute control.
    bool available =
        LATE(snd_mixer_selem_has_capture_switch)(_inputMixerElement);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to mute the microphone");
        return -1;
    }

    int value(false);

    // Retrieve one boolean control value for a specified mute-control
    //
    int
        errVal = LATE(snd_mixer_selem_get_capture_switch)(
            _inputMixerElement,
            (snd_mixer_selem_channel_id_t) 0,
            &value);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error getting capture switch: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    // Note value = 0 (off) means muted
    enabled = (bool) !value;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneBoostIsAvailable(
    bool& available)
{
    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer exists");
        return -1;
    }

    // Microphone boost cannot be enabled through ALSA Simple Mixer Interface
    available = false;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SetMicrophoneBoost(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetMicrophoneBoost(enable=%u)",
                 enable);

    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer exists");
        return -1;
    }

    // Ensure that the selected microphone destination has a valid mute control.
    bool available(false);
    MicrophoneMuteIsAvailable(available);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to enable microphone boost");
        return -1;
    }

    // It is assumed that the call above fails!

    return (0);
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneBoost(bool& enabled) const
{

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer exists");
        return -1;
    }

    // Microphone boost cannot be enabled on this platform!
    enabled = false;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneVolumeIsAvailable(
    bool& available)
{
    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    available = LATE(snd_mixer_selem_has_capture_volume)(_inputMixerElement);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SetMicrophoneVolume(
    uint32_t volume)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetMicrophoneVolume(volume=%u)",
                 volume);

    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    int
        errVal =
            LATE(snd_mixer_selem_set_capture_volume_all)(_inputMixerElement,
                                                         volume);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error changing microphone volume: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    return (0);
}

// TL: Have done testnig with these but they don't seem reliable and
// they were therefore not added
/*
 // ----------------------------------------------------------------------------
 //    SetMaxMicrophoneVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMaxMicrophoneVolume(
     uint32_t maxVolume)
 {

 if (_inputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_capture_volume_range(_inputMixerElement,
  &minVol, &maxVol);
 if ((maxVol <= minVol) || (errVal != 0))
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting capture volume range: %s", snd_strerror(errVal));
 }

 maxVol = (long int)maxVolume;
 printf("min %d max %d", minVol, maxVol);
 errVal = snd_mixer_selem_set_capture_volume_range(_inputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
 "     Capture hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
  "     Error setting capture volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }

 // ----------------------------------------------------------------------------
 //    SetMinMicrophoneVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMinMicrophoneVolume(
 uint32_t minVolume)
 {

 if (_inputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_capture_volume_range(
 _inputMixerElement, &minVol, &maxVol);
 if (maxVol <= minVol)
 {
 //maxVol = 255;
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting capture volume range: %s", snd_strerror(errVal));
 }

 printf("min %d max %d", minVol, maxVol);
 minVol = (long int)minVolume;
 errVal = snd_mixer_selem_set_capture_volume_range(
 _inputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
  "     Capture hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
  "     Error setting capture volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }
 */

int32_t AudioMixerManagerLinuxALSA::MicrophoneVolume(
    uint32_t& volume) const
{

    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    long int vol(0);

    int
        errVal =
            LATE(snd_mixer_selem_get_capture_volume)(
                _inputMixerElement,
                (snd_mixer_selem_channel_id_t) 0,
                &vol);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "Error getting inputvolume: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     AudioMixerManagerLinuxALSA::MicrophoneVolume() => vol=%i",
                 vol);

    volume = static_cast<uint32_t> (vol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MaxMicrophoneVolume(
    uint32_t& maxVolume) const
{

    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    long int minVol(0);
    long int maxVol(0);

    // check if we have mic volume at all
    if (!LATE(snd_mixer_selem_has_capture_volume)(_inputMixerElement))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     No microphone volume available");
        return -1;
    }

    int errVal =
        LATE(snd_mixer_selem_get_capture_volume_range)(_inputMixerElement,
                                                       &minVol, &maxVol);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     Microphone hardware volume range, min: %d, max: %d",
                 minVol, maxVol);
    if (maxVol <= minVol)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error getting microphone volume range: %s",
                     LATE(snd_strerror)(errVal));
    }

    maxVolume = static_cast<uint32_t> (maxVol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MinMicrophoneVolume(
    uint32_t& minVolume) const
{

    if (_inputMixerElement == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer element exists");
        return -1;
    }

    long int minVol(0);
    long int maxVol(0);

    int errVal =
        LATE(snd_mixer_selem_get_capture_volume_range)(_inputMixerElement,
                                                       &minVol, &maxVol);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     Microphone hardware volume range, min: %d, max: %d",
                 minVol, maxVol);
    if (maxVol <= minVol)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error getting microphone volume range: %s",
                     LATE(snd_strerror)(errVal));
    }

    minVolume = static_cast<uint32_t> (minVol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const
{

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  no avaliable input mixer exists");
        return -1;
    }

    // The step size is always 1 for ALSA
    stepSize = 1;

    return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

int32_t AudioMixerManagerLinuxALSA::LoadMicMixerElement() const
{
    int errVal = LATE(snd_mixer_load)(_inputMixerHandle);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "snd_mixer_load(_inputMixerHandle), error: %s",
                     LATE(snd_strerror)(errVal));
        _inputMixerHandle = NULL;
        return -1;
    }

    snd_mixer_elem_t *elem = NULL;
    snd_mixer_elem_t *micElem = NULL;
    unsigned mixerIdx = 0;
    const char *selemName = NULL;

    // Find and store handles to the right mixer elements
    for (elem = LATE(snd_mixer_first_elem)(_inputMixerHandle); elem; elem
        = LATE(snd_mixer_elem_next)(elem), mixerIdx++)
    {
        if (LATE(snd_mixer_selem_is_active)(elem))
        {
            selemName = LATE(snd_mixer_selem_get_name)(elem);
            if (strcmp(selemName, "Capture") == 0) // "Capture", "Mic"
            {
                _inputMixerElement = elem;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                             _id, "     Capture element set");
            } else if (strcmp(selemName, "Mic") == 0)
            {
                micElem = elem;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                             _id, "     Mic element found");
            }
        }

        if (_inputMixerElement)
        {
            // Use the first Capture element that is found
            // The second one may not work
            break;
        }
    }

    if (_inputMixerElement == NULL)
    {
        // We didn't find a Capture handle, use Mic.
        if (micElem != NULL)
        {
            _inputMixerElement = micElem;
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "     Using Mic as capture volume.");
        } else
        {
            _inputMixerElement = NULL;
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "Could not find capture volume on the mixer.");

            return -1;
        }
    }

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::LoadSpeakerMixerElement() const
{
    int errVal = LATE(snd_mixer_load)(_outputMixerHandle);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     snd_mixer_load(_outputMixerHandle), error: %s",
                     LATE(snd_strerror)(errVal));
        _outputMixerHandle = NULL;
        return -1;
    }

    snd_mixer_elem_t *elem = NULL;
    snd_mixer_elem_t *masterElem = NULL;
    snd_mixer_elem_t *speakerElem = NULL;
    unsigned mixerIdx = 0;
    const char *selemName = NULL;

    // Find and store handles to the right mixer elements
    for (elem = LATE(snd_mixer_first_elem)(_outputMixerHandle); elem; elem
        = LATE(snd_mixer_elem_next)(elem), mixerIdx++)
    {
        if (LATE(snd_mixer_selem_is_active)(elem))
        {
            selemName = LATE(snd_mixer_selem_get_name)(elem);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "snd_mixer_selem_get_name %d: %s =%x", mixerIdx,
                         selemName, elem);

            // "Master", "PCM", "Wave", "Master Mono", "PC Speaker", "PCM", "Wave"
            if (strcmp(selemName, "PCM") == 0)
            {
                _outputMixerElement = elem;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                             _id, "     PCM element set");
            } else if (strcmp(selemName, "Master") == 0)
            {
                masterElem = elem;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                             _id, "     Master element found");
            } else if (strcmp(selemName, "Speaker") == 0)
            {
                speakerElem = elem;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                             _id, "     Speaker element found");
            }
        }

        if (_outputMixerElement)
        {
            // We have found the element we want
            break;
        }
    }

    // If we didn't find a PCM Handle, use Master or Speaker
    if (_outputMixerElement == NULL)
    {
        if (masterElem != NULL)
        {
            _outputMixerElement = masterElem;
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "     Using Master as output volume.");
        } else if (speakerElem != NULL)
        {
            _outputMixerElement = speakerElem;
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "     Using Speaker as output volume.");
        } else
        {
            _outputMixerElement = NULL;
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "Could not find output volume in the mixer.");
            return -1;
        }
    }

    return 0;
}

void AudioMixerManagerLinuxALSA::GetControlName(char* controlName,
                                                char* deviceName) const
{
    // Example
    // deviceName: "front:CARD=Intel,DEV=0"
    // controlName: "hw:CARD=Intel"
    char* pos1 = strchr(deviceName, ':');
    char* pos2 = strchr(deviceName, ',');
    if (!pos2)
    {
        // Can also be default:CARD=Intel
        pos2 = &deviceName[strlen(deviceName)];
    }
    if (pos1 && pos2)
    {
        strcpy(controlName, "hw");
        int nChar = (int) (pos2 - pos1);
        strncpy(&controlName[2], pos1, nChar);
        controlName[2 + nChar] = '\0';
    } else
    {
        strcpy(controlName, deviceName);
    }

}

}
