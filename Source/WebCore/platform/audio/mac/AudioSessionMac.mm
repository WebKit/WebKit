/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioSession.h"

#if USE(AUDIO_SESSION) && PLATFORM(MAC)

#include "FloatConversion.h"
#include "Logging.h"
#include "NotImplemented.h"
#include <CoreAudio/AudioHardware.h>
#include <wtf/BlockPtr.h>
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

static AudioDeviceID defaultDevice()
{
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    UInt32 infoSize = sizeof(deviceID);

    AudioObjectPropertyAddress defaultOutputDeviceAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };
    OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputDeviceAddress, 0, 0, &infoSize, (void*)&deviceID);
    if (result)
        return 0; // error
    return deviceID;
}

class AudioSessionPrivate {
public:
    AudioSessionPrivate(AudioSession& session, bool mutedState)
        : weakPtrFactory(&session)
        , lastMutedState(mutedState) { }
    WeakPtrFactory<AudioSession> weakPtrFactory;
    bool lastMutedState;
    HashSet<AudioSession::Observer*> observers;
    BlockPtr<void(UInt32, const AudioObjectPropertyAddress*)> muteChangedBlock;
    BlockPtr<void(UInt32, const AudioObjectPropertyAddress*)> audioInputDeviceChangedBlock;
    BlockPtr<void(UInt32, const AudioObjectPropertyAddress*)> outputDeviceChangedBlock;
    mutable std::optional<bool> outputDeviceSupportsLowPowerMode;
};

AudioSession::AudioSession()
    : m_private(makeUniqueRef<AudioSessionPrivate>(*this, isMuted()))
{
    m_private->outputDeviceChangedBlock = (AudioObjectPropertyListenerBlock)[this, weakThis = m_private->weakPtrFactory.createWeakPtr()] (UInt32, const AudioObjectPropertyAddress*) {
        if (!weakThis)
            return;

        m_private->outputDeviceSupportsLowPowerMode = std::nullopt;

        for (auto* observer : m_private->observers)
            observer->currentAudioOutputDeviceChanged();
    };
}

AudioSession::~AudioSession()
{
    AudioObjectPropertyAddress outputDeviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    AudioObjectRemovePropertyListenerBlock(defaultDevice(), &outputDeviceAddress, dispatch_get_main_queue(), m_private->outputDeviceChangedBlock.get());
}

AudioSession::CategoryType AudioSession::category() const
{
    notImplemented();
    return None;
}

void AudioSession::setCategory(CategoryType)
{
    notImplemented();
}

AudioSession::CategoryType AudioSession::categoryOverride() const
{
    notImplemented();
    return None;
}

void AudioSession::setCategoryOverride(CategoryType)
{
    notImplemented();
}

float AudioSession::sampleRate() const
{
    Float64 nominalSampleRate;
    UInt32 nominalSampleRateSize = sizeof(Float64);

    AudioObjectPropertyAddress nominalSampleRateAddress = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &nominalSampleRateAddress, 0, 0, &nominalSampleRateSize, (void*)&nominalSampleRate);
    if (result)
        return 0;

    return narrowPrecisionToFloat(nominalSampleRate);
}

size_t AudioSession::bufferSize() const
{
    UInt32 bufferSize;
    UInt32 bufferSizeSize = sizeof(bufferSize);

    AudioObjectPropertyAddress bufferSizeAddress = {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &bufferSizeAddress, 0, 0, &bufferSizeSize, &bufferSize);

    if (result)
        return 0;
    return bufferSize;
}

size_t AudioSession::numberOfOutputChannels() const
{
    notImplemented();
    return 0;
}

bool AudioSession::tryToSetActive(bool)
{
    notImplemented();
    return true;
}

size_t AudioSession::preferredBufferSize() const
{
    UInt32 bufferSize;
    UInt32 bufferSizeSize = sizeof(bufferSize);

    AudioObjectPropertyAddress preferredBufferSizeAddress = {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &preferredBufferSizeAddress, 0, 0, &bufferSizeSize, &bufferSize);

    if (result)
        return 0;
    return bufferSize;
}

void AudioSession::setPreferredBufferSize(size_t bufferSize)
{
    AudioValueRange bufferSizeRange = {0, 0};
    UInt32 bufferSizeRangeSize = sizeof(AudioValueRange);
    AudioObjectPropertyAddress bufferSizeRangeAddress = {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &bufferSizeRangeAddress, 0, 0, &bufferSizeRangeSize, &bufferSizeRange);
    if (result)
        return;

    size_t minBufferSize = static_cast<size_t>(bufferSizeRange.mMinimum);
    size_t maxBufferSize = static_cast<size_t>(bufferSizeRange.mMaximum);
    UInt32 bufferSizeOut = std::min(maxBufferSize, std::max(minBufferSize, bufferSize));

    AudioObjectPropertyAddress preferredBufferSizeAddress = {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };

    result = AudioObjectSetPropertyData(defaultDevice(), &preferredBufferSizeAddress, 0, 0, sizeof(bufferSizeOut), (void*)&bufferSizeOut);

#if LOG_DISABLED
    UNUSED_PARAM(result);
#else
    if (result)
        LOG(Media, "AudioSession::setPreferredBufferSize(%zu) - failed with error %d", bufferSize, static_cast<int>(result));
    else
        LOG(Media, "AudioSession::setPreferredBufferSize(%zu)", bufferSize);
#endif
}

bool AudioSession::isMuted() const
{
    UInt32 mute = 0;
    UInt32 muteSize = sizeof(mute);
    AudioObjectPropertyAddress muteAddress = { kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    AudioObjectGetPropertyData(defaultDevice(), &muteAddress, 0, nullptr, &muteSize, &mute);
    
    switch (mute) {
    case 0:
        return false;
    case 1:
        return true;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static bool currentDeviceSupportsLowPowerBufferSize()
{
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    UInt32 descriptorSize = sizeof(deviceID);
    AudioObjectPropertyAddress defaultOutputDeviceDescriptor = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputDeviceDescriptor, 0, 0, &descriptorSize, (void*)&deviceID))
        return false;

    UInt32 transportType = kAudioDeviceTransportTypeUnknown;
    descriptorSize = sizeof(transportType);
    AudioObjectPropertyAddress tranportTypeDescriptor = {
        kAudioDevicePropertyTransportType,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster,
    };

    if (AudioObjectGetPropertyData(deviceID, &tranportTypeDescriptor, 0, 0, &descriptorSize, &transportType))
        return false;

    // Only allow low-power buffer size when using built-in output device, many external devices perform
    // poorly with a large output buffer.
    return kAudioDeviceTransportTypeBuiltIn == transportType;
}

bool AudioSession::outputDeviceSupportsLowPowerMode() const
{
    if (!m_private->outputDeviceSupportsLowPowerMode)
        m_private->outputDeviceSupportsLowPowerMode = currentDeviceSupportsLowPowerBufferSize();
    return m_private->outputDeviceSupportsLowPowerMode.value();
}

void AudioSession::addObserver(Observer& observer)
{
    ASSERT(!m_private->observers.contains(&observer));
    m_private->observers.add(&observer);

    if (m_private->observers.size() > 1)
        return;

    auto weakThis = m_private->weakPtrFactory.createWeakPtr();
    m_private->muteChangedBlock = (AudioObjectPropertyListenerBlock)[this, weakThis] (UInt32, const AudioObjectPropertyAddress*) {
        if (!weakThis)
            return;

        bool isCurrentlyMuted = isMuted();
        if (m_private->lastMutedState == isCurrentlyMuted)
            return;
        m_private->lastMutedState = isCurrentlyMuted;

        for (auto* observer : m_private->observers)
            observer->hardwareMutedStateDidChange(this);
    };

    m_private->audioInputDeviceChangedBlock = (AudioObjectPropertyListenerBlock)[this, weakThis] (UInt32, const AudioObjectPropertyAddress*) {
        if (!weakThis)
            return;

        for (auto* observer : m_private->observers)
            observer->currentAudioInputDeviceChanged();
    };

    AudioObjectPropertyAddress muteAddress = { kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    AudioObjectAddPropertyListenerBlock(defaultDevice(), &muteAddress, dispatch_get_main_queue(), m_private->muteChangedBlock.get());

    AudioObjectPropertyAddress inputDeviceAddress = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    AudioObjectAddPropertyListenerBlock(defaultDevice(), &inputDeviceAddress, dispatch_get_main_queue(), m_private->audioInputDeviceChangedBlock.get());
}

void AudioSession::removeObserver(Observer& observer)
{
    ASSERT(m_private->observers.contains(&observer));
    m_private->observers.remove(&observer);

    if (!m_private->observers.isEmpty())
        return;

    AudioObjectPropertyAddress muteAddress = { kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    AudioObjectRemovePropertyListenerBlock(defaultDevice(), &muteAddress, dispatch_get_main_queue(), m_private->muteChangedBlock.get());

    AudioObjectPropertyAddress inputDeviceAddress = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    AudioObjectRemovePropertyListenerBlock(defaultDevice(), &inputDeviceAddress, dispatch_get_main_queue(), m_private->audioInputDeviceChangedBlock.get());
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(MAC)
