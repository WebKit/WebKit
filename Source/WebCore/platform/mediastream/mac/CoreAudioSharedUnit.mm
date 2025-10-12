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

#include <pal/cocoa/AVFAudioSoftLink.h>

#if HAVE(AVAUDIOAPPLICATION)

OBJC_CLASS WebCoreAudioInputMuteChangeListener;

namespace WebCore {
void registerAudioInputMuteChangeListener(WebCoreAudioInputMuteChangeListener*);
void unregisterAudioInputMuteChangeListener(WebCoreAudioInputMuteChangeListener*);
}

@interface WebCoreAudioInputMuteChangeListener : NSObject {
}

- (void)start;
- (void)stop;
- (void)handleMuteStatusChangedNotification:(NSNotification*)notification;
@end

@implementation WebCoreAudioInputMuteChangeListener
- (void)start
{
    WebCore::registerAudioInputMuteChangeListener(self);
}

- (void)stop
{
    WebCore::unregisterAudioInputMuteChangeListener(self);
}

- (void)handleMuteStatusChangedNotification:(NSNotification*)notification
{
    NSNumber* newMuteState = [notification.userInfo valueForKey:AVAudioApplicationMuteStateKey];
    WebCore::CoreAudioSharedUnit::singleton().handleMuteStatusChangedNotification(newMuteState.boolValue);
}

@end
#endif // HAVE(AVAUDIOAPPLICATION)

namespace WebCore {

#if HAVE(AVAUDIOAPPLICATION)
static AVAudioApplication *getSharedAVAudioApplication()
{
    return PAL::isAVFAudioFrameworkAvailable() ? (AVAudioApplication *)[PAL::getAVAudioApplicationClassSingleton() sharedInstance] : nil;
}

#if PLATFORM(MAC)
static void setNoopInputMuteStateChangeHandler(AVAudioApplication *audioApplication, bool shouldAddHandler)
{
    @try {
        NSError *error = nil;
        if (shouldAddHandler) {
            // We set the handler to enable receiving AVAudioApplicationInputMuteStateChangeNotification notifications.
            [audioApplication setInputMuteStateChangeHandler:^(BOOL) {
                return YES;
            } error:&error];
        } else
            [audioApplication setInputMuteStateChangeHandler:nil error:&error];
        RELEASE_LOG_ERROR_IF(error, WebRTC, "WebCoreAudioInputMuteChangeListener failed to set mute state change handler due to error: %@, shouldAddHandler: %d.", error.localizedDescription, shouldAddHandler);
    } @catch (NSException *exception) {
        RELEASE_LOG_ERROR(WebRTC, "WebCoreAudioInputMuteChangeListener failed to set mute state change handler due to exception: %@, shouldAddHandler: %d.", exception, shouldAddHandler);
    }
}
#endif

void registerAudioInputMuteChangeListener(WebCoreAudioInputMuteChangeListener *listener)
{
    auto *audioApplication = getSharedAVAudioApplication();
    if (!audioApplication)
        return;

#if PLATFORM(MAC)
    setNoopInputMuteStateChangeHandler(audioApplication, true);
#endif

    [[NSNotificationCenter defaultCenter] addObserver:listener selector:@selector(handleMuteStatusChangedNotification:) name:AVAudioApplicationInputMuteStateChangeNotification object:audioApplication];

}

void unregisterAudioInputMuteChangeListener(WebCoreAudioInputMuteChangeListener *listener)
{
    auto *audioApplication = getSharedAVAudioApplication();
    if (!audioApplication)
        return;

#if PLATFORM(MAC)
    setNoopInputMuteStateChangeHandler(audioApplication, false);
#endif

    [[NSNotificationCenter defaultCenter] removeObserver:listener];
}
#endif // HAVE(AVAUDIOAPPLICATION)

#if PLATFORM(MAC) && HAVE(VOICEACTIVITYDETECTION)
static int speechActivityListenerCallback(AudioObjectID deviceID, UInt32, const AudioObjectPropertyAddress*, void*)
{
    CoreAudioSharedUnit::processVoiceActivityEvent(deviceID);
    return 0;
}

static bool manageSpeechActivityListener(uint32_t deviceID, bool enable)
{
    const AudioObjectPropertyAddress kVoiceActivityDetectionEnable {
        kAudioDevicePropertyVoiceActivityDetectionEnable,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };
    UInt32 shouldEnable = enable;
    auto error = AudioObjectSetPropertyData(deviceID, &kVoiceActivityDetectionEnable, 0, NULL, sizeof(UInt32), &shouldEnable);
    if (error) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit manageSpeechActivityListener unable to set kVoiceActivityDetectionEnable, error %d (%.4s)", (int)error, (char*)&error);
        return false;
    }

    const AudioObjectPropertyAddress kVoiceActivityDetectionState {
        kAudioDevicePropertyVoiceActivityDetectionState,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };

    if (!enable) {
        error = AudioObjectRemovePropertyListener(deviceID, &kVoiceActivityDetectionState, (AudioObjectPropertyListenerProc)speechActivityListenerCallback, NULL);
        RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioSharedUnit manageSpeechActivityListener unable to remove kVoiceActivityDetectionEnable listener, error %d (%.4s)", (int)error, (char*)&error);
        return !error;
    }

    error = AudioObjectAddPropertyListener(deviceID, &kVoiceActivityDetectionState, (AudioObjectPropertyListenerProc)speechActivityListenerCallback, NULL);
    RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioSharedUnit manageSpeechActivityListener unable to set kVoiceActivityDetectionEnable listener, error %d (%.4s)", (int)error, (char*)&error);
    return !error;
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

    callOnMainRunLoop([] {
        CoreAudioSharedUnit::singleton().voiceActivityDetected();
    });
}
#endif // PLATFORM(MAC) && HAVE(VOICEACTIVITYDETECTION)

bool CoreAudioSharedInternalUnit::setVoiceActivityDetection(bool shouldEnable)
{
#if HAVE(VOICEACTIVITYDETECTION)
#if PLATFORM(MAC)
    auto deviceID = CoreAudioSharedUnit::singleton().captureDeviceID();
    if (!deviceID && defaultInputDevice(&deviceID))
        return false;
    return manageSpeechActivityListener(deviceID, shouldEnable);
#else
    const UInt32 outputBus = 0;
    AUVoiceIOMutedSpeechActivityEventListener listener = ^(AUVoiceIOSpeechActivityEvent event) {
        if (event == kAUVoiceIOSpeechActivityHasStarted) {
            callOnMainThread([] {
                CoreAudioSharedUnit::singleton().voiceActivityDetected();
            });
        }
    };

    auto err = set(kAUVoiceIOProperty_MutedSpeechActivityEventListener, kAudioUnitScope_Global, outputBus, shouldEnable ? &listener : nullptr, shouldEnable ? sizeof(AUVoiceIOMutedSpeechActivityEventListener) : 0);
    RELEASE_LOG_ERROR_IF(err, WebRTC, "CoreAudioSharedInternalUnit::setVoiceActivityDetection failed activation, error %d (%.4s)", (int)err, (char*)&err);
    return !err;
#endif
#else
    UNUSED_PARAM(shouldEnable);
    return false;
#endif // HAVE(VOICEACTIVITYDETECTION)
}

void CoreAudioSharedUnit::setMuteStatusChangedCallback(Function<void(bool)>&& callback)
{
    if (!m_muteStatusChangedCallback && !callback)
        return;

    ASSERT(!!m_muteStatusChangedCallback != !!callback);
    m_muteStatusChangedCallback = WTFMove(callback);

#if HAVE(AVAUDIOAPPLICATION)
    if (!m_muteStatusChangedCallback) {
        [m_inputMuteChangeListener stop];
        m_inputMuteChangeListener = nullptr;
        return;
    }

    m_inputMuteChangeListener = adoptNS([[WebCoreAudioInputMuteChangeListener alloc] init]);
    [m_inputMuteChangeListener start];
#endif
}

void CoreAudioSharedUnit::setMutedState(bool isMuted)
{
#if HAVE(AVAUDIOAPPLICATION)
    auto *audioApplication = getSharedAVAudioApplication();
    if (!audioApplication)
        return;

    NSError *error = nil;
    [audioApplication setInputMuted:isMuted error:&error];
    RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioSharedUnit::setMutedState failed due to error: %@.", error.localizedDescription);
#else
    UNUSED_PARAM(isMuted);
#endif
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
