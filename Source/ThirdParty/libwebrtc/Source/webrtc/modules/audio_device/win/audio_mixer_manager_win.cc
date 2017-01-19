/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/win/audio_mixer_manager_win.h"
#include "webrtc/system_wrappers/include/trace.h"

#include <assert.h>      // assert()
#include <strsafe.h>    // StringCchCopy(), StringCchCat(), StringCchPrintf()

#ifdef _WIN32
// removes warning: "reinterpret_cast: conversion from 'UINT' to 'HMIXEROBJ'
//                of greater size"
#pragma warning(disable:4312)
#endif

// Avoids the need of Windows 7 SDK
#ifndef WAVE_MAPPED_kDefaultCommunicationDevice
#define  WAVE_MAPPED_kDefaultCommunicationDevice   0x0010
#endif

namespace webrtc {

// ============================================================================
//                             CONSTRUCTION/DESTRUCTION
// ============================================================================

AudioMixerManager::AudioMixerManager(const int32_t id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _inputMixerHandle(NULL),
    _outputMixerHandle(NULL)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s constructed", __FUNCTION__);
    ClearSpeakerState();
    ClearMicrophoneState();
}

AudioMixerManager::~AudioMixerManager()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destructed", __FUNCTION__);

    Close();

    delete &_critSect;
}

// ============================================================================
//                                 PUBLIC METHODS
// ============================================================================

// ----------------------------------------------------------------------------
//  Close
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::Close()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (_outputMixerHandle != NULL)
    {
        mixerClose(_outputMixerHandle);
        _outputMixerHandle = NULL;
    }
    if (_inputMixerHandle != NULL)
    {
        mixerClose(_inputMixerHandle);
        _inputMixerHandle = NULL;
    }
    return 0;

}

// ----------------------------------------------------------------------------
//  CloseSpeaker
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::CloseSpeaker()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (_outputMixerHandle == NULL)
    {
        return -1;
    }

    ClearSpeakerState(_outputMixerID);

    mixerClose(_outputMixerHandle);
    _outputMixerHandle = NULL;

    return 0;
}

// ----------------------------------------------------------------------------
//  CloseMicrophone
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::CloseMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerHandle == NULL)
    {
        return -1;
    }

    ClearMicrophoneState(_inputMixerID);

    mixerClose(_inputMixerHandle);
    _inputMixerHandle = NULL;

    return 0;
}

// ----------------------------------------------------------------------------
//  EnumerateAll
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::EnumerateAll()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    UINT nDevices = mixerGetNumDevs();
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#mixer devices: %u", nDevices);

    MIXERCAPS    caps;
    MIXERLINE    destLine;
    MIXERLINE    sourceLine;
    MIXERCONTROL controlArray[MAX_NUMBER_OF_LINE_CONTROLS];

    UINT mixId(0);
    UINT destId(0);
    UINT sourceId(0);

    for (mixId = 0; mixId < nDevices; mixId++)
    {
        if (!GetCapabilities(mixId, caps, true))
            continue;

        for (destId = 0; destId < caps.cDestinations; destId++)
        {
            GetDestinationLineInfo(mixId, destId, destLine, true);
            GetAllLineControls(mixId, destLine, controlArray, true);

            for (sourceId = 0; sourceId < destLine.cConnections; sourceId++)
            {
                GetSourceLineInfo(mixId, destId, sourceId, sourceLine, true);
                GetAllLineControls(mixId, sourceLine, controlArray, true);
            }
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  EnumerateSpeakers
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::EnumerateSpeakers()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    UINT nDevices = mixerGetNumDevs();
    if (nDevices > MAX_NUMBER_MIXER_DEVICES)
    {
        assert(false);
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#mixer devices: %u", nDevices);

    MIXERCAPS    caps;
    MIXERLINE    destLine;
    MIXERCONTROL controlArray[MAX_NUMBER_OF_LINE_CONTROLS];

    UINT mixId(0);
    UINT destId(0);

    ClearSpeakerState();

    // scan all avaliable mixer devices
    for (mixId = 0; mixId < nDevices; mixId++)
    {
        // get capabilities for the specified mixer ID
        if (!GetCapabilities(mixId, caps))
            continue;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[mixerID=%d] %s: ", mixId, WideToUTF8(caps.szPname));
        // scan all avaliable destinations for this mixer
        for (destId = 0; destId < caps.cDestinations; destId++)
        {
            GetDestinationLineInfo(mixId, destId, destLine);
            if ((destLine.cControls == 0)                         ||    // no controls or
                (destLine.cConnections == 0)                      ||    // no source lines or
                (destLine.fdwLine & MIXERLINE_LINEF_DISCONNECTED) ||    // disconnected or
                !(destLine.fdwLine & MIXERLINE_LINEF_ACTIVE))           // inactive
            {
                // don't store this line ID since it will not be possible to control
                continue;
            }
            if ((destLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS) ||
                (destLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_HEADPHONES))
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found valid speaker/headphone (name: %s, ID: %u)", WideToUTF8(destLine.szName), destLine.dwLineID);
                _speakerState[mixId].dwLineID = destLine.dwLineID;
                _speakerState[mixId].speakerIsValid = true;
                // retrieve all controls for the speaker component
                GetAllLineControls(mixId, destLine, controlArray);
                for (UINT c = 0; c < destLine.cControls; c++)
                {
                    if (controlArray[c].dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME)
                    {
                        _speakerState[mixId].dwVolumeControlID = controlArray[c].dwControlID;
                        _speakerState[mixId].volumeControlIsValid = true;
                        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found volume control (name: %s, ID: %u)", WideToUTF8(controlArray[c].szName), controlArray[c].dwControlID);
                    }
                    else if (controlArray[c].dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE)
                    {
                        _speakerState[mixId].dwMuteControlID = controlArray[c].dwControlID;
                        _speakerState[mixId].muteControlIsValid = true;
                        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found mute control (name: %s, ID: %u)", WideToUTF8(controlArray[c].szName), controlArray[c].dwControlID);
                    }
                }
                break;
            }
        }
        if (!SpeakerIsValid(mixId))
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "unable to find a valid speaker destination line", mixId);
        }
    }

    if (ValidSpeakers() == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to locate any valid speaker line");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  EnumerateMicrophones
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::EnumerateMicrophones()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    UINT nDevices = mixerGetNumDevs();
    if (nDevices > MAX_NUMBER_MIXER_DEVICES)
    {
        assert(false);
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#mixer devices: %u", nDevices);

    MIXERCAPS    caps;
    MIXERLINE    destLine;
    MIXERLINE    sourceLine;
    MIXERCONTROL controlArray[MAX_NUMBER_OF_LINE_CONTROLS];

    UINT mixId(0);
    UINT destId(0);

    ClearMicrophoneState();

    // scan all avaliable mixer devices
    for (mixId = 0; mixId < nDevices; mixId++)
    {
        // get capabilities for the specified mixer ID
        if (!GetCapabilities(mixId, caps))
            continue;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[mixerID=%d] %s: ", mixId, WideToUTF8(caps.szPname));
        // scan all avaliable destinations for this mixer
        for (destId = 0; destId < caps.cDestinations; destId++)
        {
            GetDestinationLineInfo(mixId, destId, destLine);

            if ((destLine.cConnections == 0)                      ||    // no source lines or
                (destLine.fdwLine & MIXERLINE_LINEF_DISCONNECTED) ||    // disconnected or
               !(destLine.fdwLine & MIXERLINE_LINEF_ACTIVE))            // inactive
            {
                // Don't store this line ID since there are no sources connected to this destination.
                // Compare with the speaker side where we also exclude lines with no controls.
                continue;
            }

            if (destLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found valid Wave In destination (name: %s, ID: %u)", WideToUTF8(destLine.szName), destLine.dwLineID);
                _microphoneState[mixId].dwLineID = destLine.dwLineID;
                _microphoneState[mixId].microphoneIsValid = true;

                // retrieve all controls for the identified wave-in destination
                if (!GetAllLineControls(mixId, destLine, controlArray))
                {
                    // This destination has no controls. We must try to control
                    // one of its sources instead. 
                    // This is a rare state but has been found for some
                    // Logitech USB headsets.

                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
                    "this destination has no controls => must control source");
                    for (DWORD sourceId = 0; sourceId < destLine.cConnections; sourceId++)
                    {
                        GetSourceLineInfo(mixId, destId, sourceId, sourceLine, false); 
                        if (sourceLine.dwComponentType == 
                            MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE)
                        {
                            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
                            "found microphone source ( name: %s, ID: %u)", 
                            WideToUTF8(sourceLine.szName), sourceId);
                            GetAllLineControls(mixId, sourceLine, controlArray, false);
                            // scan the controls for this source and search for volume, 
                            // mute and on/off (<=> boost) controls
                            for (UINT sc = 0; sc < sourceLine.cControls; sc++)
                            {
                                if (controlArray[sc].dwControlType == 
                                    MIXERCONTROL_CONTROLTYPE_VOLUME)
                                {
                                    // store this volume control
                                    _microphoneState[mixId].dwVolumeControlID = 
                                    controlArray[sc].dwControlID;
                                    _microphoneState[mixId].volumeControlIsValid = true;
                                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
                                    "found volume control (name: %s, ID: %u)", 
                                    WideToUTF8(controlArray[sc].szName), 
                                    controlArray[sc].dwControlID);
                                }
                                else if (controlArray[sc].dwControlType == 
                                         MIXERCONTROL_CONTROLTYPE_MUTE)
                                {
                                    // store this mute control
                                    _microphoneState[mixId].dwMuteControlID =
                                    controlArray[sc].dwControlID;
                                    _microphoneState[mixId].muteControlIsValid = true;
                                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
                                    "found mute control (name: %s, ID: %u)", 
                                    WideToUTF8(controlArray[sc].szName), 
                                    controlArray[sc].dwControlID);
                                }
                                else if (controlArray[sc].dwControlType == 
                                         MIXERCONTROL_CONTROLTYPE_ONOFF ||
                                         controlArray[sc].dwControlType == 
                                         MIXERCONTROL_CONTROLTYPE_LOUDNESS)
                                {
                                    // store this on/off control (most likely a Boost control)
                                    _microphoneState[mixId].dwOnOffControlID = 
                                    controlArray[sc].dwControlID;
                                    _microphoneState[mixId].onOffControlIsValid = true;
                                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
                                    "found on/off control (name: %s, ID: %u)", 
                                    WideToUTF8(controlArray[sc].szName), 
                                    controlArray[sc].dwControlID);
                                 }
                             }
                         }
                    }

                    break;
                }

                // It seems like there are three different configurations we can find in this state:
                //
                // (1) The Wave-in destination contains one MUX control only
                // (2) The Wave-in destination contains one or more controls where one is a volume control
                // (3) On Vista and Win 7, it seems like case 2 above is extended.
                //     It is common that a Wave-in destination has two master controls (volume and mute),
                //     AND a microphone source as well with its own volume and mute controls with unique
                //     identifiers. Initial tests have shown that it is sufficient to modify the master
                //     controls only. The source controls will "follow" the master settings, hence the
                //     source controls seem to be redundant.
                //
                // For case 1, we should locate the selected source and its controls. The MUX setting will
                // give us the selected source. NOTE - the selecion might not be a microphone.
                //
                // For case 2, the volume control works as a master level control and we should use that one.
                //
                // For case 3, we use the master controls only and assume that the source control will "follow".
                //
                // Examples of case 1: - SigmaTel Audio (built-in)
                //                     - add more..
                //
                // Examples of case 2: - Plantronics USB Headset
                //                      - Eutectics IPP 200 USB phone
                //                      - add more...
                //
                // Examples of case 3: - Realtek High Definition on Vista (TL)
                //                     - add more...

                if ((destLine.cControls == 1) &&
                    (controlArray[0].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX))
                {
                    // Case 1: MUX control detected  => locate the selected source and its volume control
                    //         Note that, the selecion might not be a microphone. A warning is given for
                    //         this case only, i.e., it is OK to control a selected Line In source as long
                    //         as it is connected to the wave-in destination.

                    UINT selection(0);
                    const DWORD nItemsInMux(controlArray[0].cMultipleItems);

                    // decide which source line that is selected in the mux
                    if (GetSelectedMuxSource(mixId, controlArray[0].dwControlID, nItemsInMux, selection))
                    {
                        // selection now contains the index of the selected source =>
                        // read the line information for this source
                        // if conditions listed below
                        // condition 1: invalid source
                        // condition 2: no controls
                        // condition 3: disconnected
                        // condition 4: inactive
                        if (!GetSourceLineInfo(mixId, destId, selection, sourceLine)  ||
                           (sourceLine.cControls == 0)                                ||
                           (sourceLine.fdwLine & MIXERLINE_LINEF_DISCONNECTED)        ||
                          !(sourceLine.fdwLine & MIXERLINE_LINEF_ACTIVE))               
                        {
                            continue;
                        }

                        if (sourceLine.dwComponentType != MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE)
                        {
                            // add more details about the selected source (not a microphone)
                            TraceComponentType(sourceLine.dwComponentType);
                            // send a warning just to inform about the fact that a non-microphone source will be controlled
                            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "the selected (to be controlled) source is not a microphone type");
                        }

                        // retrieve all controls for the selected source
                        GetAllLineControls(mixId, sourceLine, controlArray);
                        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "MUX selection is %u [0,%u]", selection, nItemsInMux-1);

                        // scan the controls for this source and search for volume, mute and on/off (<=> boost) controls
                        for (UINT sc = 0; sc < sourceLine.cControls; sc++)
                        {
                            if (controlArray[sc].dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME)
                            {
                                // store this volume control
                                _microphoneState[mixId].dwVolumeControlID = controlArray[sc].dwControlID;
                                _microphoneState[mixId].volumeControlIsValid = true;
                                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found volume control (name: %s, ID: %u)", WideToUTF8(controlArray[sc].szName), controlArray[sc].dwControlID);
                            }
                            else if (controlArray[sc].dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE)
                            {
                                // store this mute control
                                _microphoneState[mixId].dwMuteControlID = controlArray[sc].dwControlID;
                                _microphoneState[mixId].muteControlIsValid = true;
                                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found mute control (name: %s, ID: %u)", WideToUTF8(controlArray[sc].szName), controlArray[sc].dwControlID);
                            }
                            else if (controlArray[sc].dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF ||
                                     controlArray[sc].dwControlType == MIXERCONTROL_CONTROLTYPE_LOUDNESS)
                            {
                                // store this on/off control (most likely a Boost control)
                                _microphoneState[mixId].dwOnOffControlID = controlArray[sc].dwControlID;
                                _microphoneState[mixId].onOffControlIsValid = true;
                                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found on/off control (name: %s, ID: %u)", WideToUTF8(controlArray[sc].szName), controlArray[sc].dwControlID);
                            }
                        }
                    }
                    else
                    {
                        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to detect which source to control");
                    }

                }
                else if (destLine.cConnections == 1)
                {
                    // Case 2 or Case 3:

                    GetSourceLineInfo(mixId, destId, 0, sourceLine);
                    if ((sourceLine.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE) &&
                        (sourceLine.cControls > 0))
                    {
                        // Case 3: same as Case 2 below but we have also detected a Microphone source
                        //         with its own controls. So far, I have not been able to find any device
                        //         where it is required to modify these controls. Until I have found such
                        //         a device, this case will be handled as a Case 2 (see below).

                        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "microphone source controls will not be controlled");
                    }
                    else if ((sourceLine.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE) &&
                             (sourceLine.cControls == 0))
                    {
                        // default state on non Vista/Win 7 machines
                        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "microphone source has no controls => use master controls instead");
                    }
                    else
                    {
                        // add more details about the selected source (not a microphone)
                        TraceComponentType(sourceLine.dwComponentType);
                        // send a warning just to inform about the fact that a non-microphone source will be controlled
                        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "the connected (to be controlled) source is not a microphone type");
                    }

                    // Case 2 : one source only and no MUX control detected =>
                    //          locate the master volume control (and mute + boost controls if possible)

                    // scan the controls for this wave-in destination and search for volume, mute and on/off (<=> boost) controls
                    for (UINT dc = 0; dc < destLine.cControls; dc++)
                    {
                        if (controlArray[dc].dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME)
                        {
                            // store this volume control
                            _microphoneState[mixId].dwVolumeControlID = controlArray[dc].dwControlID;
                            _microphoneState[mixId].volumeControlIsValid = true;
                            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found volume control (name: %s, ID: %u)", WideToUTF8(controlArray[dc].szName), controlArray[dc].dwControlID);
                        }
                        else if (controlArray[dc].dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE)
                        {
                            // store this mute control
                            _microphoneState[mixId].dwMuteControlID = controlArray[dc].dwControlID;
                            _microphoneState[mixId].muteControlIsValid = true;
                            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found mute control (name: %s, ID: %u)", WideToUTF8(controlArray[dc].szName), controlArray[dc].dwControlID);
                        }
                        else if (controlArray[dc].dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF ||
                                 controlArray[dc].dwControlType == MIXERCONTROL_CONTROLTYPE_LOUDNESS ||
                                 controlArray[dc].dwControlType == MIXERCONTROL_CONTROLTYPE_BOOLEAN)
                        {
                            // store this on/off control
                            _microphoneState[mixId].dwOnOffControlID = controlArray[dc].dwControlID;
                            _microphoneState[mixId].onOffControlIsValid = true;
                            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "found on/off control (name: %s, ID: %u)", WideToUTF8(controlArray[dc].szName), controlArray[dc].dwControlID);
                        }
                    }
                }
                else
                {
                    // We are in a state where more than one source is connected to the wave-in destination.
                    // I am bailing out here for now until I understand this case better.
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to locate valid microphone controls for this mixer");
                }
                break;
            }
        }  // for (destId = 0; destId < caps.cDestinations; destId++)

        if (!MicrophoneIsValid(mixId))
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "unable to find a valid microphone destination line", mixId);
        }
    }  // for (mixId = 0; mixId < nDevices; mixId++)

    if (ValidMicrophones() == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to locate any valid microphone line");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  OpenSpeaker I(II)
//
//  Verifies that the mixer contains a valid speaker destination line.
//  Avoids opening the mixer if valid control has not been found.
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::OpenSpeaker(AudioDeviceModule::WindowsDeviceType device)
{
    if (device == AudioDeviceModule::kDefaultDevice)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::OpenSpeaker(kDefaultDevice)");
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::OpenSpeaker(kDefaultCommunicationDevice)");
    }

    CriticalSectionScoped lock(&_critSect);

    // Close any existing output mixer handle
    //
    if (_outputMixerHandle != NULL)
    {
        mixerClose(_outputMixerHandle);
        _outputMixerHandle = NULL;
    }

    MMRESULT     res = MMSYSERR_NOERROR;
    WAVEFORMATEX waveFormat;
    HWAVEOUT     hWaveOut(NULL);

    waveFormat.wFormatTag      = WAVE_FORMAT_PCM ;
    waveFormat.nChannels       = 2;
    waveFormat.nSamplesPerSec  = 48000;
    waveFormat.wBitsPerSample  = 16;
    waveFormat.nBlockAlign     = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0;

    // We need a waveform-audio output handle for the currently selected output device.
    // This handle will then give us the corresponding mixer identifier. Once the mixer
    // ID is known, it is possible to open the output mixer.
    //
    if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        // check if it is possible to open the default communication device (supported on Windows 7)
        res = waveOutOpen(NULL, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL |
            WAVE_MAPPED_kDefaultCommunicationDevice | WAVE_FORMAT_QUERY);
        if (MMSYSERR_NOERROR == res)
        {
            // if so, open the default communication device for real
            res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_MAPPED_kDefaultCommunicationDevice);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default communication device");
        }
        else
        {
            // use default device since default communication device was not avaliable
            res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                "unable to open default communication device => using default instead");
        }
    }
    else if (device == AudioDeviceModule::kDefaultDevice)
    {
        // open default device since it has been requested
        res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default output device");
    }

    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutOpen() failed (err=%d)", res);
        TraceWaveOutError(res);
    }

    UINT   mixerId(0);
    HMIXER hMixer(NULL);

    // Retrieve the device identifier for a mixer device associated with the
    // aquired waveform-audio output handle.
    //
    res = mixerGetID((HMIXEROBJ)hWaveOut, &mixerId, MIXER_OBJECTF_HWAVEOUT);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetID(MIXER_OBJECTF_HWAVEOUT) failed (err=%d)", res);
        // identification failed => use default mixer identifier (=0)
        mixerId = 0;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "specified output device <=> mixer ID %u", mixerId);

    // The waveform-audio output handle is no longer needed.
    //
    waveOutClose(hWaveOut);

    // Verify that the mixer contains a valid speaker destination line.
    // Avoid opening the mixer if valid control has not been found.
    //
    if (!SpeakerIsValid(mixerId))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to control the speaker volume for this mixer device");
        return -1;
    }

    // Open the specified mixer device and ensure that the device will not
    // be removed until the application closes the handle.
    //
    res = mixerOpen(&hMixer, mixerId, 0, 0, MIXER_OBJECTF_MIXER);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerOpen() failed (err=%d)", res);
    }

    // Store the output mixer handle and active mixer identifier
    //
    _outputMixerHandle = hMixer;
    _outputMixerID = mixerId;

    if (_outputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "the output mixer device is now open (0x%x)", _outputMixerHandle);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  OpenSpeaker II(II)
//
//  Verifies that the mixer contains a valid speaker destination line.
//  Avoids opening the mixer if valid control has not been found.
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::OpenSpeaker(uint16_t index)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::OpenSpeaker(index=%d)", index);

    CriticalSectionScoped lock(&_critSect);

    // Close any existing output mixer handle
    //
    if (_outputMixerHandle != NULL)
    {
        mixerClose(_outputMixerHandle);
        _outputMixerHandle = NULL;
    }

    MMRESULT     res;
    WAVEFORMATEX waveFormat;
    HWAVEOUT     hWaveOut(NULL);

    const UINT   deviceID(index);  // use index parameter as device identifier

    waveFormat.wFormatTag      = WAVE_FORMAT_PCM ;
    waveFormat.nChannels       = 2;
    waveFormat.nSamplesPerSec  = 48000;
    waveFormat.wBitsPerSample  = 16;
    waveFormat.nBlockAlign     = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0;

    // We need a waveform-audio output handle for the currently selected output device.
    // This handle will then give us the corresponding mixer identifier. Once the mixer
    // ID is known, it is possible to open the output mixer.
    //
    res = waveOutOpen(&hWaveOut, deviceID, &waveFormat, 0, 0, CALLBACK_NULL);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveOutOpen(deviceID=%u) failed (err=%d)", index, res);
        TraceWaveOutError(res);
    }

    UINT   mixerId(0);
    HMIXER hMixer(NULL);

    // Retrieve the device identifier for a mixer device associated with the
    // aquired waveform-audio output handle.
    //
    res = mixerGetID((HMIXEROBJ)hWaveOut, &mixerId, MIXER_OBJECTF_HWAVEOUT);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetID(MIXER_OBJECTF_HWAVEOUT) failed (err=%d)", res);
        // identification failed => use default mixer identifier (=0)
        mixerId = 0;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "specified output device <=> mixer ID %u", mixerId);

    // The waveform-audio output handle is no longer needed.
    //
    waveOutClose(hWaveOut);

    // Verify that the mixer contains a valid speaker destination line.
    // Avoid opening the mixer if valid control has not been found.
    //
    if (!SpeakerIsValid(mixerId))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to control the speaker volume for this mixer device");
        return -1;
    }

    // Open the specified mixer device and ensure that the device will not
    // be removed until the application closes the handle.
    //
    res = mixerOpen(&hMixer, mixerId, 0, 0, MIXER_OBJECTF_MIXER);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerOpen() failed (err=%d)", res);
    }

    // Store the output mixer handle and active mixer identifier
    //
    _outputMixerHandle = hMixer;
    _outputMixerID = mixerId;

    if (_outputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "the output mixer device is now open (0x%x)", _outputMixerHandle);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  OpenMicrophone I(II)
//
//  Verifies that the mixer contains a valid wave-in destination line.
//  Avoids opening the mixer if valid control has not been found.
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::OpenMicrophone(AudioDeviceModule::WindowsDeviceType device)
{
    if (device == AudioDeviceModule::kDefaultDevice)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::OpenMicrophone(kDefaultDevice)");
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::OpenMicrophone(kDefaultCommunicationDevice)");
    }

    CriticalSectionScoped lock(&_critSect);

    // Close any existing output mixer handle
    //
    if (_inputMixerHandle != NULL)
    {
        mixerClose(_inputMixerHandle);
        _inputMixerHandle = NULL;
    }

    MMRESULT     res = MMSYSERR_NOERROR;
    WAVEFORMATEX waveFormat;
    HWAVEIN         hWaveIn(NULL);

    waveFormat.wFormatTag      = WAVE_FORMAT_PCM ;
    waveFormat.nChannels       = 1;
    waveFormat.nSamplesPerSec  = 48000;
    waveFormat.wBitsPerSample  = 16;
    waveFormat.nBlockAlign     = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0 ;

    // We need a waveform-audio input handle for the currently selected input device.
    // This handle will then give us the corresponding mixer identifier. Once the mixer
    // ID is known, it is possible to open the input mixer.
    //
    if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        // check if it is possible to open the default communication device (supported on Windows 7)
        res = waveInOpen(NULL, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL |
            WAVE_MAPPED_kDefaultCommunicationDevice | WAVE_FORMAT_QUERY);
        if (MMSYSERR_NOERROR == res)
        {
            // if so, open the default communication device for real
            res = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_MAPPED_kDefaultCommunicationDevice);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default communication device");
        }
        else
        {
            // use default device since default communication device was not avaliable
            res = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                "unable to open default communication device => using default instead");
        }
    }
    else if (device == AudioDeviceModule::kDefaultDevice)
    {
        // open default device since it has been requested
        res = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "opening default input device");
    }

    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInOpen() failed (err=%d)", res);
        TraceWaveInError(res);
    }

    UINT   mixerId(0);
    HMIXER hMixer(NULL);

    // Retrieve the device identifier for a mixer device associated with the
    // aquired waveform-audio input handle.
    //
    res = mixerGetID((HMIXEROBJ)hWaveIn, &mixerId, MIXER_OBJECTF_HWAVEIN);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetID(MIXER_OBJECTF_HWAVEIN) failed (err=%d)", res);
        // identification failed => use default mixer identifier (=0)
        mixerId = 0;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "specified input device <=> mixer ID %u", mixerId);

    // The waveform-audio input handle is no longer needed.
    //
    waveInClose(hWaveIn);

    // Verify that the mixer contains a valid wave-in destination line and a volume control.
    // Avoid opening the mixer if valid control has not been found.
    //
    if (!MicrophoneIsValid(mixerId))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to control the microphone volume for this mixer device");
        return -1;
    }

    // Open the specified mixer device and ensure that the device will not
    // be removed until the application closes the handle.
    //
    res = mixerOpen(&hMixer, mixerId, 0, 0, MIXER_OBJECTF_MIXER);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerOpen() failed (err=%d)", res);
    }

    // Store the input mixer handle and active mixer identifier
    //
    _inputMixerHandle = hMixer;
    _inputMixerID = mixerId;

    if (_inputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "the input mixer device is now open (0x%x)", _inputMixerHandle);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  OpenMicrophone II(II)
//
//  Verifies that the mixer contains a valid wave-in destination line.
//  Avoids opening the mixer if valid control has not been found.
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::OpenMicrophone(uint16_t index)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::OpenMicrophone(index=%d)", index);

    CriticalSectionScoped lock(&_critSect);

    // Close any existing input mixer handle
    //
    if (_inputMixerHandle != NULL)
    {
        mixerClose(_inputMixerHandle);
        _inputMixerHandle = NULL;
    }

    MMRESULT     res;
    WAVEFORMATEX waveFormat;
    HWAVEIN         hWaveIn(NULL);

    const UINT   deviceID(index);  // use index parameter as device identifier

    waveFormat.wFormatTag      = WAVE_FORMAT_PCM ;
    waveFormat.nChannels       = 1;
    waveFormat.nSamplesPerSec  = 48000;
    waveFormat.wBitsPerSample  = 16;
    waveFormat.nBlockAlign     = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0;

    // We need a waveform-audio input handle for the currently selected input device.
    // This handle will then give us the corresponding mixer identifier. Once the mixer
    // ID is known, it is possible to open the input mixer.
    //
    res = waveInOpen(&hWaveIn, deviceID, &waveFormat, 0, 0, CALLBACK_NULL);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "waveInOpen(deviceID=%u) failed (err=%d)", index, res);
        TraceWaveInError(res);
    }

    UINT   mixerId(0);
    HMIXER hMixer(NULL);

    // Retrieve the device identifier for a mixer device associated with the
    // aquired waveform-audio input handle.
    //
    res = mixerGetID((HMIXEROBJ)hWaveIn, &mixerId, MIXER_OBJECTF_HWAVEIN);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetID(MIXER_OBJECTF_HWAVEIN) failed (err=%d)", res);
        // identification failed => use default mixer identifier (=0)
        mixerId = 0;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "specified input device <=> mixer ID %u", mixerId);

    // The waveform-audio input handle is no longer needed.
    //
    waveInClose(hWaveIn);

    // Verify that the mixer contains a valid wave-in destination line.
    // Avoid opening the mixer if valid control has not been found.
    //
    if (!MicrophoneIsValid(mixerId))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to control the microphone volume for this mixer device");
        return -1;
    }

    // Open the specified mixer device and ensure that the device will not
    // be removed until the application closes the handle.
    //
    res = mixerOpen(&hMixer, mixerId, 0, 0, MIXER_OBJECTF_MIXER);
    if (MMSYSERR_NOERROR != res)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerOpen() failed (err=%d)", res);
    }

    // Store the input mixer handle and active mixer identifier
    //
    _inputMixerHandle = hMixer;
    _inputMixerID = mixerId;

    if (_inputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "the input mixer device is now open (0x%x)", _inputMixerHandle);
    }

    return 0;
}

// ----------------------------------------------------------------------------
// SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioMixerManager::SpeakerIsInitialized() const
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (_outputMixerHandle != NULL);
}

// ----------------------------------------------------------------------------
// MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioMixerManager::MicrophoneIsInitialized() const
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (_inputMixerHandle != NULL);
}

// ----------------------------------------------------------------------------
// SetSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SetSpeakerVolume(uint32_t volume)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::SetSpeakerVolume(volume=%u)", volume);

    CriticalSectionScoped lock(&_critSect);

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    const UINT mixerID(_outputMixerID);
    const DWORD dwControlID(_speakerState[_outputMixerID].dwVolumeControlID);
    DWORD dwValue(volume);

    // Set one unsigned control value for a specified volume-control identifier
    //
    if (!SetUnsignedControlValue(mixerID, dwControlID, dwValue))
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
//
//  Note that (MIXERCONTROL_CONTROLTYPE_VOLUME & MIXERCONTROL_CT_UNITS_MASK)
//  always equals MIXERCONTROL_CT_UNITS_UNSIGNED;
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SpeakerVolume(uint32_t& volume) const
{

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    const UINT mixerID(_outputMixerID);
    const DWORD dwControlID(_speakerState[_outputMixerID].dwVolumeControlID);
    DWORD dwValue(0);

    // Retrieve one unsigned control value for a specified volume-control identifier
    //
    if (!GetUnsignedControlValue(mixerID, dwControlID, dwValue))
    {
        return -1;
    }

    volume = dwValue;

    return 0;
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
//
//  Note that (MIXERCONTROL_CONTROLTYPE_VOLUME & MIXERCONTROL_CT_UNITS_MASK)
//  always equals MIXERCONTROL_CT_UNITS_UNSIGNED
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MaxSpeakerVolume(uint32_t& maxVolume) const
{

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    const UINT mixerID(_outputMixerID);
    const DWORD dwControlID(_speakerState[_outputMixerID].dwVolumeControlID);
    MIXERCONTROL mixerControl;

    // Retrieve one control line for a specified volume-control identifier
    //
    if (!GetLineControl(mixerID, dwControlID, mixerControl))
    {
        return -1;
    }

    maxVolume = mixerControl.Bounds.dwMaximum;

    return 0;
}

// ----------------------------------------------------------------------------
// MinSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MinSpeakerVolume(uint32_t& minVolume) const
{

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    const UINT mixerID(_outputMixerID);
    const DWORD dwControlID(_speakerState[_outputMixerID].dwVolumeControlID);
    MIXERCONTROL mixerControl;

    // Retrieve one control line for a specified volume-control identifier
    //
    if (!GetLineControl(mixerID, dwControlID, mixerControl))
    {
        return -1;
    }

    minVolume = mixerControl.Bounds.dwMinimum;

    return 0;
}

// ----------------------------------------------------------------------------
// SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SpeakerVolumeStepSize(uint16_t& stepSize) const
{

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    const UINT mixerID(_outputMixerID);
    MIXERCONTROL mixerControl;

    // Retrieve one control line for a specified volume-control identifier
    //
    if (!GetLineControl(mixerID, _speakerState[mixerID].dwVolumeControlID, mixerControl))
    {
        return -1;
    }

    stepSize = static_cast<uint16_t> (mixerControl.Metrics.cSteps);

    return 0;
}

// ----------------------------------------------------------------------------
// SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SpeakerVolumeIsAvailable(bool& available)
{
    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    available = _speakerState[_outputMixerID].volumeControlIsValid;

    return 0;
}

// ----------------------------------------------------------------------------
// SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SpeakerMuteIsAvailable(bool& available)
{
    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    available = _speakerState[_outputMixerID].muteControlIsValid;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
//
//  This mute function works a master mute for the output speaker.
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SetSpeakerMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::SetSpeakerMute(enable=%u)", enable);

    CriticalSectionScoped lock(&_critSect);

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    // Ensure that the selected speaker destination has a valid mute control.
    // If so, its identifier was stored during the enumeration phase which must
    // have taken place since the output mixer handle exists.
    //
    if (!_speakerState[_outputMixerID].muteControlIsValid)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to mute this speaker line");
        return -1;
    }

    const DWORD dwControlID(_speakerState[_outputMixerID].dwMuteControlID);

    // Set one boolean control value for the specified mute-control
    //
    if (!SetBooleanControlValue(_outputMixerID, dwControlID, enable))
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SpeakerMute(bool& enabled) const
{

    if (_outputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable output mixer exists");
        return -1;
    }

    // Ensure that the selected speaker destination has a valid mute control.
    // If so, its identifier was stored during the enumeration phase which must
    // have taken place since the output mixer handle exists.
    //
    if (!_speakerState[_outputMixerID].muteControlIsValid)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to mute this speaker line");
        return -1;
    }

    const DWORD dwControlID(_speakerState[_outputMixerID].dwMuteControlID);
    bool value(false);

    // Retrieve one boolean control value for a specified mute-control identifier
    //
    if (!GetBooleanControlValue(_outputMixerID, dwControlID, value))
    {
        return -1;
    }

    enabled = value;

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneMuteIsAvailable(bool& available)
{
    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    available = _microphoneState[_inputMixerID].muteControlIsValid;

    return 0;
}

// ----------------------------------------------------------------------------
// SetMicrophoneMute
//
//  This mute function works a master mute for the input microphone.
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SetMicrophoneMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::SetMicrophoneMute(enable=%u)", enable);

    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    // Ensure that the selected wave-in destinationhas a valid mute control.
    // If so, its identifier was stored during the enumeration phase which must
    // have taken place since the input mixer handle exists.
    //
    if (!_microphoneState[_inputMixerID].muteControlIsValid)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to mute this microphone line");
        return -1;
    }

    const DWORD dwControlID(_microphoneState[_inputMixerID].dwMuteControlID);

    // Set one boolean control value for the specified mute-control
    //
    if (!SetBooleanControlValue(_inputMixerID, dwControlID, enable))
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneMute(bool& enabled) const
{

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    // Ensure that the selected wave-in destinationhas a valid mute control.
    // If so, its identifier was stored during the enumeration phase which must
    // have taken place since the input mixer handle exists.
    //
    if (!_microphoneState[_inputMixerID].muteControlIsValid)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "it is not possible to mute this microphone line");
        return -1;
    }

    const DWORD dwControlID(_microphoneState[_inputMixerID].dwMuteControlID);
    bool value(false);

    // Retrieve one boolean control value for a specified mute-control identifier
    //
    if (!GetBooleanControlValue(_inputMixerID, dwControlID, value))
    {
        return -1;
    }

    enabled = value;

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneBoostIsAvailable(bool& available)
{
    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    available = _microphoneState[_inputMixerID].onOffControlIsValid;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SetMicrophoneBoost(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::SetMicrophoneBoost(enable=%u)", enable);

    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    // Ensure that the selected wave-in destination has a valid boost (on/off) control.
    // If so, its identifier was stored during the enumeration phase which must
    // have taken place since the input mixer handle exists.
    //
    if (!_microphoneState[_inputMixerID].onOffControlIsValid)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no boost control exists for this wave-in line");
        return -1;
    }

    const DWORD dwControlID(_microphoneState[_inputMixerID].dwOnOffControlID);

    // Set one boolean control value for the specified boost (on/off) control
    //
    if (!SetBooleanControlValue(_inputMixerID, dwControlID, enable))
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneBoost(bool& enabled) const
{

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    // Ensure that the selected wave-in destination has a valid boost (on/off) control.
    // If so, its identifier was stored during the enumeration phase which must
    // have taken place since the input mixer handle exists.
    //
    if (!_microphoneState[_inputMixerID].onOffControlIsValid)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no boost control exists for this wave-in line");
        return -1;
    }

    const DWORD dwControlID(_microphoneState[_inputMixerID].dwOnOffControlID);
    bool value(false);

    // Retrieve one boolean control value for a specified boost-control identifier
    //
    if (!GetBooleanControlValue(_inputMixerID, dwControlID, value))
    {
        return -1;
    }

    enabled = value;

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneVolumeIsAvailable(bool& available)
{
    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    available = _microphoneState[_inputMixerID].volumeControlIsValid;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::SetMicrophoneVolume(uint32_t volume)
{
    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    const UINT mixerID(_inputMixerID);
    const DWORD dwControlID(_microphoneState[_inputMixerID].dwVolumeControlID);
    DWORD dwValue(volume);

    // Set one unsigned control value for a specified volume-control identifier
    //
    if (!SetUnsignedControlValue(mixerID, dwControlID, dwValue))
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneVolume(uint32_t& volume) const
{
    CriticalSectionScoped lock(&_critSect);

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    const UINT mixerID(_inputMixerID);
    const DWORD dwControlID(_microphoneState[_inputMixerID].dwVolumeControlID);
    DWORD dwValue(0);

    // Retrieve one unsigned control value for a specified volume-control identifier
    //
    if (!GetUnsignedControlValue(mixerID, dwControlID, dwValue))
    {
        return -1;
    }

    volume = dwValue;

    return 0;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MaxMicrophoneVolume(uint32_t& maxVolume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    const UINT mixerID(_inputMixerID);
    const DWORD dwControlID(_microphoneState[_inputMixerID].dwVolumeControlID);
    MIXERCONTROL mixerControl;

    // Retrieve one control line for a specified volume-control identifier
    //
    if (!GetLineControl(mixerID, dwControlID, mixerControl))
    {
        return -1;
    }

    maxVolume = mixerControl.Bounds.dwMaximum;

    return 0;
}

// ----------------------------------------------------------------------------
// MinMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MinMicrophoneVolume(uint32_t& minVolume) const
{

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    const UINT mixerID(_inputMixerID);
    const DWORD dwControlID(_microphoneState[_inputMixerID].dwVolumeControlID);
    MIXERCONTROL mixerControl;

    // Retrieve one control line for a specified volume-control identifier
    //
    if (!GetLineControl(mixerID, dwControlID, mixerControl))
    {
        return -1;
    }

    minVolume = mixerControl.Bounds.dwMinimum;

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioMixerManager::MicrophoneVolumeStepSize(uint16_t& stepSize) const
{

    if (_inputMixerHandle == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "no avaliable input mixer exists");
        return -1;
    }

    const UINT mixerID(_inputMixerID);
    const DWORD dwControlID(_microphoneState[_inputMixerID].dwVolumeControlID);
    MIXERCONTROL mixerControl;

    // Retrieve one control line for a specified volume-control identifier
    //
    if (!GetLineControl(mixerID, dwControlID, mixerControl))
    {
        return -1;
    }

    stepSize = static_cast<uint16_t> (mixerControl.Metrics.cSteps);

    return 0;
}

// ============================================================================
//                              PRIVATE METHODS
// ============================================================================

// ----------------------------------------------------------------------------
//  Devices
//
//  A given audio card has one Mixer device associated with it. All of the
//  various components on that card are controlled through that card's one
//  Mixer device.
// ----------------------------------------------------------------------------

UINT AudioMixerManager::Devices() const
{
    UINT nDevs = mixerGetNumDevs();
    return nDevs;
}

// ----------------------------------------------------------------------------
//  DestinationLines
//
//  # destination lines given mixer ID.
// ----------------------------------------------------------------------------

UINT AudioMixerManager::DestinationLines(UINT mixId) const
{
    MIXERCAPS caps;
    if (!GetCapabilities(mixId, caps))
    {
        return 0;
    }
    return (caps.cDestinations);
}
// ----------------------------------------------------------------------------
//  DestinationLines
//
//  # source lines given mixer ID and destination ID.
// ----------------------------------------------------------------------------

UINT AudioMixerManager::SourceLines(UINT mixId, DWORD destId) const
{
    MIXERLINE dline;
    if (!GetDestinationLineInfo(mixId, destId, dline))
    {
        return 0;
    }
    return (dline.cConnections);
}

// ----------------------------------------------------------------------------
//  GetCapabilities
//
//  Queries a specified mixer device to determine its capabilities.
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetCapabilities(UINT mixId, MIXERCAPS& caps, bool trace) const
{
    MMRESULT res;
    MIXERCAPS mcaps;

    res = mixerGetDevCaps(mixId, &mcaps, sizeof(MIXERCAPS));
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetDevCaps() failed (err=%d)", res);
        return false;
    }

    memcpy(&caps, &mcaps, sizeof(MIXERCAPS));

    if (trace)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "===============================================================");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Mixer ID %u:", mixId);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "manufacturer ID      : %u", caps.wMid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product ID           : %u", caps.wPid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "version of driver    : %u", caps.vDriverVersion);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product name         : %s", WideToUTF8(caps.szPname));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "misc. support bits   : %u", caps.fdwSupport);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "count of destinations: %u (+)", caps.cDestinations);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "===============================================================");
    }

    if (caps.cDestinations == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "invalid number of mixer destinations");
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//  GetDestinationLineInfo
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetDestinationLineInfo(UINT mixId, DWORD destId, MIXERLINE& line, bool trace) const
{
    MMRESULT  res;
    MIXERLINE mline;

    mline.cbStruct = sizeof(MIXERLINE);
    mline.dwDestination = destId;   // max destination index is cDestinations-1
    mline.dwSource = 0;             // not set for MIXER_GETLINEINFOF_DESTINATION

    // Retrieve information about the specified destination line of a mixer device.
    // Note that we use the mixer ID here and not a handle to an opened mixer.
    // It is not required to open the mixer for enumeration purposes only.
    //
    res = mixerGetLineInfo(reinterpret_cast<HMIXEROBJ>(mixId), &mline, MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_DESTINATION);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetLineInfo(MIXER_GETLINEINFOF_DESTINATION) failed (err=%d)", res);
        return false;
    }

    memcpy(&line, &mline, sizeof(MIXERLINE));

    if (trace)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "> Destination Line ID %u:", destId);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "destination line index : %u", mline.dwDestination);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "dwLineID               : %lu (unique)", mline.dwLineID);
        TraceStatusAndSupportFlags(mline.fdwLine);
        TraceComponentType(mline.dwComponentType);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "count of channels      : %u", mline.cChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "# audio source lines   : %u (+)", mline.cConnections);    // valid only for destinations
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "# controls             : %u (*)", mline.cControls);       // can be zero
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "short name             : %s", WideToUTF8(mline.szShortName));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "full name              : %s", WideToUTF8(mline.szName));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
        TraceTargetType(mline.Target.dwType);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "target device ID       : %lu", mline.Target.dwDeviceID);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "manufacturer ID        : %u", mline.Target.wMid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product ID             : %u", mline.Target.wPid);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "driver version         : %u", mline.Target.vDriverVersion);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "product name           : %s", WideToUTF8(mline.Target.szPname));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "---------------------------------------------------------------");
    }

    return true;
}

// ----------------------------------------------------------------------------
//  GetSourceLineInfo
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetSourceLineInfo(UINT mixId, DWORD destId, DWORD srcId, MIXERLINE& line, bool trace) const
{
    MMRESULT  res;
    MIXERLINE mline;

    mline.cbStruct = sizeof(MIXERLINE);
    mline.dwDestination = destId;   // we want the source info for this destination
    mline.dwSource = srcId;         // source index (enumerate over these)

    // Retrieve information about the specified source line of a mixer device.
    // Note that we use the mixer ID here and not a handle to an opened mixer.
    // It is not required to open the mixer for enumeration purposes only.
    //
    res = mixerGetLineInfo(reinterpret_cast<HMIXEROBJ>(mixId), &mline, MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_SOURCE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetLineInfo(MIXER_GETLINEINFOF_SOURCE) failed (err=%d)", res);
        return false;
    }

    memcpy(&line, &mline, sizeof(MIXERLINE));

    if (trace)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, " >> Source Line ID %u:", srcId);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "destination line index : %u", mline.dwDestination);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "dwSource               : %u", mline.dwSource);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "dwLineID               : %lu (unique)", mline.dwLineID);
        TraceStatusAndSupportFlags(mline.fdwLine);
        TraceComponentType(mline.dwComponentType);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "# controls             : %u (*)", mline.cControls);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "full name              : %s", WideToUTF8(mline.szName));
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
        TraceTargetType(mline.Target.dwType);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "---------------------------------------------------------------");
    }

    return true;
}

// ----------------------------------------------------------------------------
// GetAllLineControls
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetAllLineControls(UINT mixId, const MIXERLINE& line, MIXERCONTROL* controlArray, bool trace) const
{
    // Ensure that we don't try to aquire information if there are no controls for this line
    //
    if (line.cControls == 0)
        return false;

    MMRESULT          res;
    MIXERLINECONTROLS mlineControls;            // contains information about the controls of an audio line

    mlineControls.dwLineID  = line.dwLineID;    // unique audio line identifier
    mlineControls.cControls = line.cControls;   // number of controls associated with the line
    mlineControls.pamxctrl  = controlArray;     // points to the first MIXERCONTROL structure to be filled
    mlineControls.cbStruct  = sizeof(MIXERLINECONTROLS);
    mlineControls.cbmxctrl  = sizeof(MIXERCONTROL);

    // Get information on ALL controls associated with the specified audio line
    //
    res = mixerGetLineControls(reinterpret_cast<HMIXEROBJ>(mixId), &mlineControls, MIXER_OBJECTF_MIXER | MIXER_GETLINECONTROLSF_ALL);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetLineControls(MIXER_GETLINECONTROLSF_ALL) failed  (err=%d)", res);
        return false;
    }

    if (trace)
    {
        for (UINT c = 0; c < line.cControls; c++)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, " >> Control ID %u:", c);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "dwControlID            : %u (unique)", controlArray[c].dwControlID);
            TraceControlType(controlArray[c].dwControlType);
            TraceControlStatusAndSupportFlags(controlArray[c].fdwControl);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cMultipleItems         : %u", controlArray[c].cMultipleItems);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "short name             : %s", WideToUTF8(controlArray[c].szShortName));
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "full name              : %s", WideToUTF8(controlArray[c].szName));
            if ((controlArray[c].dwControlType & MIXERCONTROL_CT_UNITS_MASK) == MIXERCONTROL_CT_UNITS_SIGNED)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "min signed value       : %d", controlArray[c].Bounds.lMinimum);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "max signed value       : %d", controlArray[c].Bounds.lMaximum);
            }
            else if ((controlArray[c].dwControlType & MIXERCONTROL_CT_UNITS_MASK) == MIXERCONTROL_CT_UNITS_UNSIGNED ||
                     (controlArray[c].dwControlType & MIXERCONTROL_CT_UNITS_MASK) == MIXERCONTROL_CT_UNITS_BOOLEAN)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "min unsigned value     : %u",  controlArray[c].Bounds.dwMinimum);
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "max unsigned value     : %u", controlArray[c].Bounds.dwMaximum);
            }
            if (controlArray[c].dwControlType  != MIXERCONTROL_CONTROLTYPE_CUSTOM)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cSteps                 : %u",  controlArray[c].Metrics.cSteps);
            }
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "...............................................................");
            GetControlDetails(mixId, controlArray[c], true);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "...............................................................");

        }
    }

    return true;
}

// ----------------------------------------------------------------------------
//  GetLineControls
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetLineControl(UINT mixId, DWORD dwControlID, MIXERCONTROL& control) const
{
    MMRESULT          res;
    MIXERLINECONTROLS mlineControl;

    mlineControl.dwControlID = dwControlID;
    mlineControl.cControls   = 1;
    mlineControl.pamxctrl    = &control;
    mlineControl.cbStruct    = sizeof(MIXERLINECONTROLS);
    mlineControl.cbmxctrl    = sizeof(MIXERCONTROL);

    // Get information on one controls associated with the specified conrol identifier
    //
    res = mixerGetLineControls(reinterpret_cast<HMIXEROBJ>(mixId), &mlineControl, MIXER_OBJECTF_MIXER | MIXER_GETLINECONTROLSF_ONEBYID);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetLineControls(MIXER_GETLINECONTROLSF_ONEBYID) failed (err=%d)", res);
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//  GetControlDetails
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetControlDetails(UINT mixId, MIXERCONTROL& controlArray, bool trace) const
{
    assert(controlArray.cMultipleItems <= MAX_NUMBER_OF_MULTIPLE_ITEMS);

    MMRESULT                     res;
    MIXERCONTROLDETAILS          controlDetails;

    MIXERCONTROLDETAILS_UNSIGNED valueUnsigned[MAX_NUMBER_OF_MULTIPLE_ITEMS];
    MIXERCONTROLDETAILS_SIGNED   valueSigned[MAX_NUMBER_OF_MULTIPLE_ITEMS];
    MIXERCONTROLDETAILS_BOOLEAN  valueBoolean[MAX_NUMBER_OF_MULTIPLE_ITEMS];

    enum ControlType
    {
        CT_UNITS_UNSIGNED,
        CT_UNITS_SIGNED,
        CT_UNITS_BOOLEAN
    };

    ControlType ctype(CT_UNITS_UNSIGNED);

    controlDetails.cbStruct       = sizeof(MIXERCONTROLDETAILS);
    controlDetails.dwControlID    = controlArray.dwControlID;       // control identifier
    controlDetails.cChannels      = 1;                              // we need to set values as if they were uniform
    controlDetails.cMultipleItems = controlArray.cMultipleItems;    // only nonzero for CONTROLF_MULTIPLE controls
                                                                    // can e.g. happen for CONTROLTYPE_MUX
    if (controlDetails.cMultipleItems > MAX_NUMBER_OF_MULTIPLE_ITEMS)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "cMultipleItems > %d", MAX_NUMBER_OF_MULTIPLE_ITEMS);
        controlDetails.cMultipleItems = MAX_NUMBER_OF_MULTIPLE_ITEMS;
    }

    if ((controlArray.dwControlType & MIXERCONTROL_CT_UNITS_MASK) == MIXERCONTROL_CT_UNITS_SIGNED)
    {
        ctype = CT_UNITS_SIGNED;
        controlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_SIGNED);
        controlDetails.paDetails = &valueSigned[0];
    }
    else if ((controlArray.dwControlType & MIXERCONTROL_CT_UNITS_MASK) == MIXERCONTROL_CT_UNITS_UNSIGNED)
    {
        ctype = CT_UNITS_UNSIGNED;
        controlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
        controlDetails.paDetails = &valueUnsigned[0];
    }
    else if ((controlArray.dwControlType & MIXERCONTROL_CT_UNITS_MASK) == MIXERCONTROL_CT_UNITS_BOOLEAN)
    {
        ctype = CT_UNITS_BOOLEAN;
        controlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
        controlDetails.paDetails = &valueBoolean[0];
    }

    // Retrieve a control's value
    //
    res = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(mixId), &controlDetails, MIXER_OBJECTF_MIXER | MIXER_GETCONTROLDETAILSF_VALUE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetControlDetails(MIXER_GETCONTROLDETAILSF_VALUE) failed (err=%d)", res);
        return false;
    }

    if (trace)
    {
        UINT nItems(1);
        nItems = (controlDetails.cMultipleItems > 0 ? controlDetails.cMultipleItems : 1);
        for (UINT i = 0; i < nItems; i++)
        {
            if (ctype == CT_UNITS_SIGNED)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "signed value           : %d", valueSigned[i].lValue);
            }
            else if (ctype == CT_UNITS_UNSIGNED)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "unsigned value         : %u", valueUnsigned[i].dwValue);
            }
            else if (ctype == CT_UNITS_BOOLEAN)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "boolean value          : %u", valueBoolean[i].fValue);
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
//  GetUnsignedControlValue
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetUnsignedControlValue(UINT mixId, DWORD dwControlID, DWORD& dwValue) const
{
    MMRESULT                     res;
    MIXERCONTROLDETAILS          controlDetails;
    MIXERCONTROLDETAILS_UNSIGNED valueUnsigned;

    controlDetails.dwControlID    = dwControlID;
    controlDetails.cbStruct       = sizeof(MIXERCONTROLDETAILS);
    controlDetails.cChannels      = 1;
    controlDetails.cMultipleItems = 0;
    controlDetails.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
    controlDetails.paDetails      = &valueUnsigned;

    // Retrieve the unsigned value
    //
    res = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(mixId), &controlDetails, MIXER_OBJECTF_MIXER | MIXER_GETCONTROLDETAILSF_VALUE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetControlDetails(MIXER_GETCONTROLDETAILSF_VALUE) failed (err=%d)", res);
        return false;
    }

    // Deliver the retrieved value
    //
    dwValue = valueUnsigned.dwValue;

    return true;
}

// ----------------------------------------------------------------------------
//  SetUnsignedControlValue
// ----------------------------------------------------------------------------

bool AudioMixerManager::SetUnsignedControlValue(UINT mixId, DWORD dwControlID, DWORD dwValue) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "AudioMixerManager::SetUnsignedControlValue(mixId=%u, dwControlID=%d, dwValue=%d)", mixId, dwControlID, dwValue);

    MMRESULT                     res;
    MIXERCONTROLDETAILS          controlDetails;
    MIXERCONTROLDETAILS_UNSIGNED valueUnsigned;

    controlDetails.dwControlID    = dwControlID;
    controlDetails.cbStruct       = sizeof(MIXERCONTROLDETAILS);
    controlDetails.cChannels      = 1;
    controlDetails.cMultipleItems = 0;
    controlDetails.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
    controlDetails.paDetails      = &valueUnsigned;

    valueUnsigned.dwValue         = dwValue;

    // Set the unsigned value
    //
    res = mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(mixId), &controlDetails, MIXER_OBJECTF_MIXER | MIXER_GETCONTROLDETAILSF_VALUE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerSetControlDetails(MIXER_GETCONTROLDETAILSF_VALUE) failed (err=%d)", res);
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//  SetBooleanControlValue
// ----------------------------------------------------------------------------

bool AudioMixerManager::SetBooleanControlValue(UINT mixId, DWORD dwControlID, bool value) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioMixerManager::SetBooleanControlValue(mixId=%u, dwControlID=%d, value=%d)", mixId, dwControlID, value);

    MMRESULT                    res;
    MIXERCONTROLDETAILS         controlDetails;
    MIXERCONTROLDETAILS_BOOLEAN valueBoolean;

    controlDetails.dwControlID    = dwControlID;
    controlDetails.cbStruct       = sizeof(MIXERCONTROLDETAILS);
    controlDetails.cChannels      = 1;
    controlDetails.cMultipleItems = 0;
    controlDetails.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
    controlDetails.paDetails      = &valueBoolean;

    if (value == true)
        valueBoolean.fValue = TRUE;
    else
        valueBoolean.fValue = FALSE;

    // Set the boolean value
    //
    res = mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(mixId), &controlDetails, MIXER_OBJECTF_MIXER | MIXER_GETCONTROLDETAILSF_VALUE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerSetControlDetails(MIXER_GETCONTROLDETAILSF_VALUE) failed (err=%d)", res);
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//  GetBooleanControlValue
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetBooleanControlValue(UINT mixId, DWORD dwControlID, bool& value) const
{
    MMRESULT                    res;
    MIXERCONTROLDETAILS         controlDetails;
    MIXERCONTROLDETAILS_BOOLEAN valueBoolean;

    controlDetails.dwControlID    = dwControlID;
    controlDetails.cbStruct       = sizeof(MIXERCONTROLDETAILS);
    controlDetails.cChannels      = 1;
    controlDetails.cMultipleItems = 0;
    controlDetails.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
    controlDetails.paDetails      = &valueBoolean;

    // Retrieve the boolean value
    //
    res = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(mixId), &controlDetails, MIXER_OBJECTF_MIXER | MIXER_GETCONTROLDETAILSF_VALUE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetControlDetails(MIXER_GETCONTROLDETAILSF_VALUE) failed (err=%d)", res);
        return false;
    }

    // Deliver the retrieved value
    //
    if (valueBoolean.fValue == 0)
        value = false;
    else
        value = true;

    return true;
}

// ----------------------------------------------------------------------------
//  GetSelectedMuxSource
// ----------------------------------------------------------------------------

bool AudioMixerManager::GetSelectedMuxSource(UINT mixId, DWORD dwControlID, DWORD cMultipleItems, UINT& index) const
{
    assert(cMultipleItems <= MAX_NUMBER_OF_MULTIPLE_ITEMS);

    MMRESULT                    res;
    MIXERCONTROLDETAILS         controlDetails;
    MIXERCONTROLDETAILS_BOOLEAN valueBoolean[MAX_NUMBER_OF_MULTIPLE_ITEMS];
    memset(&valueBoolean, 0, sizeof(valueBoolean));

    controlDetails.dwControlID    = dwControlID;
    controlDetails.cbStruct       = sizeof(MIXERCONTROLDETAILS);
    controlDetails.cChannels      = 1;
    controlDetails.cMultipleItems = cMultipleItems;
    controlDetails.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
    controlDetails.paDetails      = &valueBoolean;

    // Retrieve the boolean values
    //
    res = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(mixId), &controlDetails, MIXER_OBJECTF_MIXER | MIXER_GETCONTROLDETAILSF_VALUE);
    if (res != MMSYSERR_NOERROR)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "mixerGetControlDetails(MIXER_GETCONTROLDETAILSF_VALUE) failed (err=%d)", res);
        return false;
    }

    // Map the current MUX setting to an index corresponding to a source index.
    // e.g. with cMultipleItems = 3,
    //  valueBoolean[] = {1,0,0} => index = 2
    //  valueBoolean[] = {0,1,0} => index = 1
    //  valueBoolean[] = {0,0,1} => index = 0
    //
    // If there is no "1" in the array, we assume index should be 0.
    index = 0;
    for (DWORD i = 0; i < cMultipleItems; i++)
    {
        if (valueBoolean[i].fValue > 0)
        {
            index = (cMultipleItems - 1) - i;
            break;
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
//  TraceStatusAndSupportFlags
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceStatusAndSupportFlags(DWORD fdwLine) const
{
    TCHAR buf[128];

    StringCchPrintf(buf, 128, TEXT("status & support flags : 0x%x "), fdwLine);

    switch (fdwLine)
    {
    case MIXERLINE_LINEF_ACTIVE:
        StringCchCat(buf, 128, TEXT("(ACTIVE DESTINATION)"));
        break;
    case MIXERLINE_LINEF_DISCONNECTED:
        StringCchCat(buf, 128, TEXT("(DISCONNECTED)"));
        break;
    case MIXERLINE_LINEF_SOURCE:
        StringCchCat(buf, 128, TEXT("(INACTIVE SOURCE)"));
        break;
    case MIXERLINE_LINEF_SOURCE | MIXERLINE_LINEF_ACTIVE:
        StringCchCat(buf, 128, TEXT("(ACTIVE SOURCE)"));
        break;
    default:
        StringCchCat(buf, 128, TEXT("(INVALID)"));
        break;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  TraceComponentType
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceComponentType(DWORD dwComponentType) const
{
    TCHAR buf[128];

    StringCchPrintf(buf, 128, TEXT("component type         : 0x%x "), dwComponentType);

    switch (dwComponentType)
    {
    // Destination
    case MIXERLINE_COMPONENTTYPE_DST_UNDEFINED:
        StringCchCat(buf, 128, TEXT("(DST_UNDEFINED)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_DIGITAL:
        StringCchCat(buf, 128, TEXT("(DST_DIGITAL)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_LINE:
        StringCchCat(buf, 128, TEXT("(DST_LINE)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_MONITOR:
        StringCchCat(buf, 128, TEXT("(DST_MONITOR)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_SPEAKERS:
        StringCchCat(buf, 128, TEXT("(DST_SPEAKERS)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_HEADPHONES:
        StringCchCat(buf, 128, TEXT("(DST_HEADPHONES)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_TELEPHONE:
        StringCchCat(buf, 128, TEXT("(DST_TELEPHONE)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_WAVEIN:
        StringCchCat(buf, 128, TEXT("(DST_WAVEIN)"));
        break;
    case MIXERLINE_COMPONENTTYPE_DST_VOICEIN:
        StringCchCat(buf, 128, TEXT("(DST_VOICEIN)"));
        break;
    // Source
    case MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED:
        StringCchCat(buf, 128, TEXT("(SRC_UNDEFINED)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_DIGITAL:
        StringCchCat(buf, 128, TEXT("(SRC_DIGITAL)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_LINE:
        StringCchCat(buf, 128, TEXT("(SRC_LINE)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE:
        StringCchCat(buf, 128, TEXT("(SRC_MICROPHONE)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER:
        StringCchCat(buf, 128, TEXT("(SRC_SYNTHESIZER)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC:
        StringCchCat(buf, 128, TEXT("(SRC_COMPACTDISC)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_TELEPHONE:
        StringCchCat(buf, 128, TEXT("(SRC_TELEPHONE)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER:
        StringCchCat(buf, 128, TEXT("(SRC_PCSPEAKER)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT:
        StringCchCat(buf, 128, TEXT("(SRC_WAVEOUT)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY:
        StringCchCat(buf, 128, TEXT("(SRC_AUXILIARY)"));
        break;
    case MIXERLINE_COMPONENTTYPE_SRC_ANALOG:
        StringCchCat(buf, 128, TEXT("(SRC_ANALOG)"));
        break;
    default:
        StringCchCat(buf, 128, TEXT("(INVALID)"));
        break;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  TraceTargetType
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceTargetType(DWORD dwType) const
{
    TCHAR buf[128];

    StringCchPrintf(buf, 128, TEXT("media device type      : 0x%x "), dwType);

    switch (dwType)
    {
    case MIXERLINE_TARGETTYPE_UNDEFINED:
        StringCchCat(buf, 128, TEXT("(UNDEFINED)"));
        break;
    case MIXERLINE_TARGETTYPE_WAVEOUT:
        StringCchCat(buf, 128, TEXT("(WAVEOUT)"));
        break;
    case MIXERLINE_TARGETTYPE_WAVEIN:
        StringCchCat(buf, 128, TEXT("(WAVEIN)"));
        break;
    case MIXERLINE_TARGETTYPE_MIDIOUT:
        StringCchCat(buf, 128, TEXT("(MIDIOUT)"));
        break;
    case MIXERLINE_TARGETTYPE_MIDIIN:
        StringCchCat(buf, 128, TEXT("(MIDIIN)"));
        break;
    default:
        StringCchCat(buf, 128, TEXT("(INVALID)"));
        break;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  TraceControlType
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceControlType(DWORD dwControlType) const
{
    TCHAR buf[128];

    // Class type classification
    //
    StringCchPrintf(buf, 128, TEXT("class type             : 0x%x "), dwControlType);

    switch (dwControlType & MIXERCONTROL_CT_CLASS_MASK)
    {
    case MIXERCONTROL_CT_CLASS_CUSTOM:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_CUSTOM)"));
        break;
    case MIXERCONTROL_CT_CLASS_METER:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_METER)"));
        break;
    case MIXERCONTROL_CT_CLASS_SWITCH:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_SWITCH)"));
        break;
    case MIXERCONTROL_CT_CLASS_NUMBER:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_NUMBER)"));
        break;
    case MIXERCONTROL_CT_CLASS_SLIDER:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_SLIDER)"));
        break;
    case MIXERCONTROL_CT_CLASS_FADER:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_FADER)"));
        break;
    case MIXERCONTROL_CT_CLASS_TIME:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_TIME)"));
        break;
    case MIXERCONTROL_CT_CLASS_LIST:
        StringCchCat(buf, 128, TEXT("(CT_CLASS_LIST)"));
        break;
    default:
        StringCchCat(buf, 128, TEXT("(INVALID)"));
        break;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));

    // Control type (for each class)
    //
    StringCchPrintf(buf, 128, TEXT("control type           : 0x%x "), dwControlType);

    switch (dwControlType)
    {
    case MIXERCONTROL_CONTROLTYPE_CUSTOM:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_CUSTOM)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_BOOLEANMETER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_BOOLEANMETER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_SIGNEDMETER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_SIGNEDMETER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_PEAKMETER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_PEAKMETER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_UNSIGNEDMETER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_UNSIGNEDMETER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_BOOLEAN:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_BOOLEAN)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_ONOFF:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_ONOFF)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MUTE:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MUTE)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MONO:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MONO)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_LOUDNESS:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_LOUDNESS)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_STEREOENH:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_STEREOENH)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_BASS_BOOST:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_BASS_BOOST)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_BUTTON:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_BUTTON)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_DECIBELS:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_DECIBELS)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_SIGNED:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_SIGNED)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_UNSIGNED:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_UNSIGNED)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_PERCENT:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_PERCENT)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_SLIDER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_SLIDER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_PAN:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_PAN)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_QSOUNDPAN:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_QSOUNDPAN)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_FADER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_FADER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_VOLUME:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_VOLUME)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_BASS:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_BASS)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_TREBLE:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_TREBLE)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_EQUALIZER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_EQUALIZER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_SINGLESELECT:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_SINGLESELECT)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MUX:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MUX)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MULTIPLESELECT:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MULTIPLESELECT)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MIXER:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MIXER)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MICROTIME:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MICROTIME)"));
        break;
    case MIXERCONTROL_CONTROLTYPE_MILLITIME:
        StringCchCat(buf, 128, TEXT("(CONTROLTYPE_MILLITIME)"));
        break;
    default:
        StringCchCat(buf, 128, TEXT("(INVALID)"));
        break;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  TraceControlStatusAndSupportFlags
//
//  fdwControl
//
//  Status and support flags for the audio line control. The following values
//  are defined:
//
//  MIXERCONTROL_CONTROLF_DISABLED
//
//  The control is disabled, perhaps due to other settings for the mixer hardware,
//  and cannot be used. An application can read current settings from a
//  disabled control, but it cannot apply settings.
//
//  MIXERCONTROL_CONTROLF_MULTIPLE
//
//  The control has two or more settings per channel. An equalizer, for example,
//  requires this flag because each frequency band can be set to a different value.
//  An equalizer that affects both channels of a stereo line in a uniform fashion
//  will also specify the MIXERCONTROL_CONTROLF_UNIFORM flag.
//
//  MIXERCONTROL_CONTROLF_UNIFORM
//
//  The control acts on all channels of a multichannel line in a uniform fashion.
//  For example, a control that mutes both channels of a stereo line would set
//  this flag. Most MIXERCONTROL_CONTROLTYPE_MUX and
//  MIXERCONTROL_CONTROLTYPE_MIXER controls also specify the
//  MIXERCONTROL_CONTROLF_UNIFORM flag.
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceControlStatusAndSupportFlags(DWORD fdwControl) const
{
    TCHAR buf[128];

    StringCchPrintf(buf, 128, TEXT("control support flags  : 0x%x "), fdwControl);

    if (fdwControl & MIXERCONTROL_CONTROLF_DISABLED)
    {
        // The control is disabled, perhaps due to other settings for the mixer hardware,
        // and cannot be used. An application can read current settings from a disabled
        // control, but it cannot apply settings.
        StringCchCat(buf, 128, TEXT("(CONTROLF_DISABLED)"));
    }

    if (fdwControl & MIXERCONTROL_CONTROLF_MULTIPLE)
    {
        // The control has two or more settings per channel. An equalizer, for example,
        // requires this flag because each frequency band can be set to a different
        // value. An equalizer that affects both channels of a stereo line in a
        // uniform fashion will also specify the MIXERCONTROL_CONTROLF_UNIFORM flag.
        StringCchCat(buf, 128, TEXT("(CONTROLF_MULTIPLE)"));
    }

    if (fdwControl & MIXERCONTROL_CONTROLF_UNIFORM)
    {
        // The control acts on all channels of a multichannel line in a uniform
        // fashion. For example, a control that mutes both channels of a stereo
        // line would set this flag. Most MIXERCONTROL_CONTROLTYPE_MUX and
        // MIXERCONTROL_CONTROLTYPE_MIXER controls also specify the
        // MIXERCONTROL_CONTROLF_UNIFORM flag.
        StringCchCat(buf, 128, TEXT("(CONTROLF_UNIFORM)"));
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  ClearSpeakerState I (II)
// ----------------------------------------------------------------------------

void AudioMixerManager::ClearSpeakerState(UINT idx)
{
    _speakerState[idx].dwLineID = 0L;
    _speakerState[idx].dwVolumeControlID = 0L;
    _speakerState[idx].dwMuteControlID = 0L;
    _speakerState[idx].speakerIsValid = false;
    _speakerState[idx].muteControlIsValid = false;
    _speakerState[idx].volumeControlIsValid = false;
}

// ----------------------------------------------------------------------------
//  ClearSpeakerState II (II)
// ----------------------------------------------------------------------------

void AudioMixerManager::ClearSpeakerState()
{
    for (int i = 0; i < MAX_NUMBER_MIXER_DEVICES; i++)
    {
        ClearSpeakerState(i);
    }
}

// ----------------------------------------------------------------------------
//  SpeakerIsValid
// ----------------------------------------------------------------------------

bool AudioMixerManager::SpeakerIsValid(UINT idx) const
{
    return (_speakerState[idx].speakerIsValid);
}

// ----------------------------------------------------------------------------
//  ValidSpeakers
//
//  Counts number of valid speaker destinations for all mixer devices.
// ----------------------------------------------------------------------------

UINT AudioMixerManager::ValidSpeakers() const
{
    UINT nSpeakers(0);
    for (int i = 0; i < MAX_NUMBER_MIXER_DEVICES; i++)
    {
        if (SpeakerIsValid(i))
            nSpeakers++;
    }
    return nSpeakers;
}

// ----------------------------------------------------------------------------
//  ClearMicrophoneState I (II)
// ----------------------------------------------------------------------------

void AudioMixerManager::ClearMicrophoneState(UINT idx)
{
    _microphoneState[idx].dwLineID = 0L;
    _microphoneState[idx].dwVolumeControlID = 0L;
    _microphoneState[idx].dwMuteControlID = 0L;
    _microphoneState[idx].dwOnOffControlID = 0L;
    _microphoneState[idx].microphoneIsValid = false;
    _microphoneState[idx].muteControlIsValid = false;
    _microphoneState[idx].volumeControlIsValid = false;
    _microphoneState[idx].onOffControlIsValid = false;
}

// ----------------------------------------------------------------------------
//  ClearMicrophoneState II (II)
// ----------------------------------------------------------------------------

void AudioMixerManager::ClearMicrophoneState()
{
    for (int i = 0; i < MAX_NUMBER_MIXER_DEVICES; i++)
    {
        ClearMicrophoneState(i);
    }
}

// ----------------------------------------------------------------------------
//  MicrophoneIsValid
// ----------------------------------------------------------------------------

bool AudioMixerManager::MicrophoneIsValid(UINT idx) const
{
    return (_microphoneState[idx].microphoneIsValid);

}

// ----------------------------------------------------------------------------
//  ValidMicrophones
//
//  Counts number of valid speaker destinations for all mixer devices.
//  To be valid, a speaker destination line must exist.
// ----------------------------------------------------------------------------

UINT AudioMixerManager::ValidMicrophones() const
{
    UINT nMicrophones(0);
    for (int i = 0; i < MAX_NUMBER_MIXER_DEVICES; i++)
    {
        if (MicrophoneIsValid(i))
            nMicrophones++;
    }
    return nMicrophones;
}

// ----------------------------------------------------------------------------
//  TraceWaveInError
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceWaveInError(MMRESULT error) const
{
    TCHAR buf[MAXERRORLENGTH];
    TCHAR msg[MAXERRORLENGTH];

    StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
    waveInGetErrorText(error, msg, MAXERRORLENGTH);
    StringCchCat(buf, MAXERRORLENGTH, msg);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  TraceWaveOutError
// ----------------------------------------------------------------------------

void AudioMixerManager::TraceWaveOutError(MMRESULT error) const
{
    TCHAR buf[MAXERRORLENGTH];
    TCHAR msg[MAXERRORLENGTH];

    StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
    waveOutGetErrorText(error, msg, MAXERRORLENGTH);
    StringCchCat(buf, MAXERRORLENGTH, msg);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  WideToUTF8
// ----------------------------------------------------------------------------

char* AudioMixerManager::WideToUTF8(const TCHAR* src) const {
#ifdef UNICODE
    const size_t kStrLen = sizeof(_str);
    memset(_str, 0, kStrLen);
    // Get required size (in bytes) to be able to complete the conversion.
    unsigned int required_size = (unsigned int)WideCharToMultiByte(CP_UTF8, 0, src, -1, _str, 0, 0, 0);
    if (required_size <= kStrLen)
    {
        // Process the entire input string, including the terminating null char.
        if (WideCharToMultiByte(CP_UTF8, 0, src, -1, _str, kStrLen, 0, 0) == 0)
            memset(_str, 0, kStrLen);
    }
    return _str;
#else
    return const_cast<char*>(src);
#endif
}

}  // namespace webrtc
