/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AudioSession.h"

#if USE(AUDIO_SESSION) && PLATFORM(MAC)

#import "FloatConversion.h"
#import "Logging.h"
#import "NotImplemented.h"
#import <CoreAudio/AudioHardware.h>
#import <wtf/MainThread.h>
#import <wtf/UniqueArray.h>
#import <wtf/text/WTFString.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

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

#if ENABLE(ROUTING_ARBITRATION)
static Optional<bool> isPlayingToBluetoothOverride;

static float defaultDeviceTransportIsBluetooth()
{
    if (isPlayingToBluetoothOverride)
        return *isPlayingToBluetoothOverride;

    static const AudioObjectPropertyAddress audioDeviceTransportTypeProperty = {
        kAudioDevicePropertyTransportType,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster,
    };
    UInt32 transportType = kAudioDeviceTransportTypeUnknown;
    UInt32 transportSize = sizeof(transportType);
    if (AudioObjectGetPropertyData(defaultDevice(), &audioDeviceTransportTypeProperty, 0, 0, &transportSize, &transportType))
        return false;

    return transportType == kAudioDeviceTransportTypeBluetooth || transportType == kAudioDeviceTransportTypeBluetoothLE;
}
#endif

class AudioSessionPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AudioSessionPrivate() = default;
    Optional<bool> lastMutedState;
    AudioSession::CategoryType category { AudioSession::None };
#if ENABLE(ROUTING_ARBITRATION)
    bool setupArbitrationOngoing { false };
    Optional<bool> playingToBluetooth;
    Optional<bool> playingToBluetoothOverride;
#endif
    AudioSession::CategoryType m_categoryOverride;
    bool inRoutingArbitration { false };
};

AudioSession::AudioSession()
    : m_private(makeUnique<AudioSessionPrivate>())
{
}

AudioSession::~AudioSession() = default;

AudioSession::CategoryType AudioSession::category() const
{
    return m_private->category;
}

void AudioSession::audioOutputDeviceChanged()
{
#if ENABLE(ROUTING_ARBITRATION)
    if (!m_private->playingToBluetooth || *m_private->playingToBluetooth == defaultDeviceTransportIsBluetooth())
        return;

    m_private->playingToBluetooth = WTF::nullopt;
#endif
}

void AudioSession::setIsPlayingToBluetoothOverride(Optional<bool> value)
{
#if ENABLE(ROUTING_ARBITRATION)
    isPlayingToBluetoothOverride = value;
#else
    UNUSED_PARAM(value);
#endif
}

void AudioSession::setCategory(CategoryType category, RouteSharingPolicy)
{
#if ENABLE(ROUTING_ARBITRATION)
    bool playingToBluetooth = defaultDeviceTransportIsBluetooth();
    if (category == m_private->category && m_private->playingToBluetooth && *m_private->playingToBluetooth == playingToBluetooth)
        return;

    m_private->category = category;

    if (m_private->setupArbitrationOngoing) {
        RELEASE_LOG_ERROR(Media, "AudioSession::setCategory() - a beginArbitrationWithCategory is still ongoing");
        return;
    }

    if (!m_routingArbitrationClient)
        return;

    if (m_private->inRoutingArbitration) {
        m_private->inRoutingArbitration = false;
        m_routingArbitrationClient->leaveRoutingAbritration();
    }

    if (category == AmbientSound || category == SoloAmbientSound || category == AudioProcessing || category == None)
        return;

    using RoutingArbitrationError = AudioSessionRoutingArbitrationClient::RoutingArbitrationError;
    using DefaultRouteChanged = AudioSessionRoutingArbitrationClient::DefaultRouteChanged;

    m_private->playingToBluetooth = playingToBluetooth;
    m_private->setupArbitrationOngoing = true;
    m_routingArbitrationClient->beginRoutingArbitrationWithCategory(m_private->category, [this] (RoutingArbitrationError error, DefaultRouteChanged defaultRouteChanged) {
        m_private->setupArbitrationOngoing = false;
        if (error != RoutingArbitrationError::None) {
            RELEASE_LOG_ERROR(Media, "AudioSession::setCategory() - beginArbitrationWithCategory:%s failed with error %s", convertEnumerationToString(m_private->category).ascii().data(), convertEnumerationToString(error).ascii().data());
            return;
        }

        m_private->inRoutingArbitration = true;

        // FIXME: Do we need to reset sample rate and buffer size for the new default device?
        if (defaultRouteChanged == DefaultRouteChanged::Yes)
            LOG(Media, "AudioSession::setCategory() - defaultRouteChanged!");
    });
#else
    m_private->category = category;
#endif
}

AudioSession::CategoryType AudioSession::categoryOverride() const
{
    return m_private->m_categoryOverride;
}

void AudioSession::setCategoryOverride(CategoryType category)
{
    if (m_private->m_categoryOverride == category)
        return;

    m_private->m_categoryOverride = category;
    setCategory(category, RouteSharingPolicy::Default);
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

size_t AudioSession::maximumNumberOfOutputChannels() const
{
    AudioObjectPropertyAddress sizeAddress = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };

    UInt32 size = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(defaultDevice(), &sizeAddress, 0, 0, &size);
    if (result || !size)
        return 0;

    auto listMemory = makeUniqueArray<uint8_t>(size);
    auto* audioBufferList = reinterpret_cast<AudioBufferList*>(listMemory.get());

    result = AudioObjectGetPropertyData(defaultDevice(), &sizeAddress, 0, 0, &size, audioBufferList);
    if (result)
        return 0;

    size_t channels = 0;
    for (UInt32 i = 0; i < audioBufferList->mNumberBuffers; ++i)
        channels += audioBufferList->mBuffers[i].mNumberChannels;
    return channels;
}

bool AudioSession::tryToSetActiveInternal(bool)
{
    notImplemented();
    return true;
}

RouteSharingPolicy AudioSession::routeSharingPolicy() const
{
    return RouteSharingPolicy::Default;
}

String AudioSession::routingContextUID() const
{
    return emptyString();
}

size_t AudioSession::preferredBufferSize() const
{
    UInt32 bufferSize;
    UInt32 bufferSizeSize = sizeof(bufferSize);

    AudioObjectPropertyAddress preferredBufferSizeAddress = {
        kAudioDevicePropertyBufferFrameSize,
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

static OSStatus handleMutePropertyChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData)
{
    callOnMainThread([inClientData] {
        reinterpret_cast<AudioSession*>(inClientData)->handleMutedStateChange();
    });
    return noErr;
}

void AudioSession::handleMutedStateChange()
{
    if (!m_private)
        return;

    bool isCurrentlyMuted = isMuted();
    if (m_private->lastMutedState && *m_private->lastMutedState == isCurrentlyMuted)
        return;

    for (auto* observer : m_observers)
        observer->hardwareMutedStateDidChange(this);

    m_private->lastMutedState = isCurrentlyMuted;
}

void AudioSession::addMutedStateObserver(MutedStateObserver* observer)
{
    m_observers.add(observer);

    if (m_observers.size() > 1)
        return;

    AudioObjectPropertyAddress muteAddress = { kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    AudioObjectAddPropertyListener(defaultDevice(), &muteAddress, handleMutePropertyChange, this);
}

void AudioSession::removeMutedStateObserver(MutedStateObserver* observer)
{
    if (m_observers.size() == 1) {
        AudioObjectPropertyAddress muteAddress = { kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
        AudioObjectRemovePropertyListener(defaultDevice(), &muteAddress, handleMutePropertyChange, this);
    }

    m_observers.remove(observer);
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(MAC)
