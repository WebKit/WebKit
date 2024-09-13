/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoreAudioSharedUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "CoreAudioSharedInternalUnit.h"
#include "Logging.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioUnitProperties.h>

#if PLATFORM(MAC)
#include <CoreAudio/AudioHardware.h>
#endif

namespace WebCore {

#if PLATFORM(MAC) && HAVE(VOICEACTIVITYDETECTION)
static int speechActivityListenerCallback(AudioObjectID deviceID, UInt32, const AudioObjectPropertyAddress*, void*)
{
    ASSERT(isMainRunLoop());
    CoreAudioSharedUnit::processVoiceActivityEvent(deviceID);
    return 0;
}

static void manageSpeechActivityListener(uint32_t deviceID, bool enable)
{
    const AudioObjectPropertyAddress kVoiceActivityDetectionEnable {
        kAudioDevicePropertyVoiceActivityDetectionEnable,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };
    UInt32 shouldEnable = enable;
    auto error = AudioObjectSetPropertyData(deviceID, &kVoiceActivityDetectionEnable, 0, NULL, sizeof(UInt32), &shouldEnable);
    RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioSharedUnit manageSpeechActivityListener unable to set kVoiceActivityDetectionEnable, error %d (%.4s)", (int)error, (char*)&error);

    const AudioObjectPropertyAddress kVoiceActivityDetectionState {
        kAudioDevicePropertyVoiceActivityDetectionState,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };

    if (!enable) {
        error = AudioObjectRemovePropertyListener(deviceID, &kVoiceActivityDetectionState, (AudioObjectPropertyListenerProc)speechActivityListenerCallback, NULL);
        RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioSharedUnit manageSpeechActivityListener unable to remove kVoiceActivityDetectionEnable listener, error %d (%.4s)", (int)error, (char*)&error);
        return;
    }

    error = AudioObjectAddPropertyListener(deviceID, &kVoiceActivityDetectionState, (AudioObjectPropertyListenerProc)speechActivityListenerCallback, NULL);
    RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioSharedUnit manageSpeechActivityListener unable to set kVoiceActivityDetectionEnable listener, error %d (%.4s)", (int)error, (char*)&error);
}

void CoreAudioSharedUnit::processVoiceActivityEvent(AudioObjectID deviceID)
{
    UInt32 voiceDetected = 0;
    UInt32 propertySize = sizeof(UInt32);

    const AudioObjectPropertyAddress kVoiceActivityDetectionState {
        kAudioDevicePropertyVoiceActivityDetectionState,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };
    OSStatus status = AudioObjectGetPropertyData(deviceID, &kVoiceActivityDetectionState, 0, NULL, &propertySize, &voiceDetected);
    if (status != kAudioHardwareNoError)
        return;

    if (voiceDetected != 1)
        return;

    CoreAudioSharedUnit::unit().voiceActivityDetected();
}
#endif // PLATFORM(MAC) && HAVE(VOICEACTIVITYDETECTION)

void CoreAudioSharedInternalUnit::setVoiceActivityDetection(bool shouldEnable)
{
#if HAVE(VOICEACTIVITYDETECTION)
#if PLATFORM(MAC)
    auto deviceID = CoreAudioSharedUnit::unit().captureDeviceID();
    if (!deviceID) {
        if (auto err = defaultInputDevice(&deviceID))
            return;
    }
    manageSpeechActivityListener(deviceID, shouldEnable);
#else
    const UInt32 outputBus = 0;
    AUVoiceIOMutedSpeechActivityEventListener listener = ^(AUVoiceIOSpeechActivityEvent event) {
        if (event == kAUVoiceIOSpeechActivityHasStarted)
            CoreAudioSharedUnit::unit().voiceActivityDetected();
    };

    set(kAUVoiceIOProperty_MutedSpeechActivityEventListener, kAudioUnitScope_Global, outputBus, shouldEnable ? &listener : nullptr, sizeof(AUVoiceIOMutedSpeechActivityEventListener));
#endif
#else
    UNUSED_PARAM(shouldEnable);
#endif // HAVE(VOICEACTIVITYDETECTION)
}

void CoreAudioSharedUnit::enableMutedSpeechActivityEventListener(Function<void()>&& callback)
{
    setVoiceActivityListenerCallback(WTFMove(callback));

    if (!m_ioUnit) {
        m_shouldSetVoiceActivityListener = true;
        return;
    }

    m_ioUnit->setVoiceActivityDetection(true);
}

void CoreAudioSharedUnit::disableMutedSpeechActivityEventListener()
{
    setVoiceActivityListenerCallback({ });
    if (!m_ioUnit) {
        m_shouldSetVoiceActivityListener = false;
        return;
    }

    m_ioUnit->setVoiceActivityDetection(false);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
