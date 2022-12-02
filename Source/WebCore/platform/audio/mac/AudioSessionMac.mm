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
#import "AudioSessionMac.h"

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

static AudioDeviceID defaultDeviceWithoutCaching()
{
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    UInt32 infoSize = sizeof(deviceID);

    AudioObjectPropertyAddress defaultOutputDeviceAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputDeviceAddress, 0, 0, &infoSize, (void*)&deviceID);
    if (result)
        return 0; // error
    return deviceID;
}

#if ENABLE(ROUTING_ARBITRATION)
static std::optional<bool> isPlayingToBluetoothOverride;

static float defaultDeviceTransportIsBluetooth()
{
    if (isPlayingToBluetoothOverride)
        return *isPlayingToBluetoothOverride;

    static const AudioObjectPropertyAddress audioDeviceTransportTypeProperty = {
        kAudioDevicePropertyTransportType,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster,
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    UInt32 transportType = kAudioDeviceTransportTypeUnknown;
    UInt32 transportSize = sizeof(transportType);
    if (AudioObjectGetPropertyData(defaultDeviceWithoutCaching(), &audioDeviceTransportTypeProperty, 0, 0, &transportSize, &transportType))
        return false;

    return transportType == kAudioDeviceTransportTypeBluetooth || transportType == kAudioDeviceTransportTypeBluetoothLE;
}
#endif

void AudioSessionMac::removePropertyListenersForDefaultDevice() const
{
    if (m_hasBufferSizeObserver) {
        AudioObjectRemovePropertyListener(defaultDevice(), &bufferSizeAddress(), handleBufferSizeChange, const_cast<AudioSessionMac*>(this));
        m_hasBufferSizeObserver = false;
    }
    if (m_hasSampleRateObserver) {
        AudioObjectRemovePropertyListener(defaultDevice(), &nominalSampleRateAddress(), handleSampleRateChange, const_cast<AudioSessionMac*>(this));
        m_hasSampleRateObserver = false;
    }
    if (m_hasMuteChangeObserver)
        removeMuteChangeObserverIfNeeded();
}

OSStatus AudioSessionMac::handleDefaultDeviceChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData)
{
    ASSERT(inClientData);
    if (!inClientData)
        return noErr;

    auto* session = static_cast<AudioSessionMac*>(inClientData);
    callOnMainThread([session] {
        bool hadBufferSizeObserver = session->m_hasBufferSizeObserver;
        bool hadSampleRateObserver = session->m_hasSampleRateObserver;
        bool hadMuteObserver = session->m_hasMuteChangeObserver;

        session->removePropertyListenersForDefaultDevice();
        session->m_defaultDevice = defaultDeviceWithoutCaching();

        if (hadBufferSizeObserver)
            session->addBufferSizeObserverIfNeeded();
        if (hadSampleRateObserver)
            session->addSampleRateObserverIfNeeded();
        if (hadMuteObserver)
            session->addMuteChangeObserverIfNeeded();

        if (session->m_bufferSize)
            session->handleBufferSizeChange();
        if (session->m_sampleRate)
            session->handleSampleRateChange();
        if (session->m_lastMutedState)
            session->handleMutedStateChange();
    });

    return noErr;
}

const AudioObjectPropertyAddress& AudioSessionMac::defaultOutputDeviceAddress()
{
    static const AudioObjectPropertyAddress defaultOutputDeviceAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    return defaultOutputDeviceAddress;
}

void AudioSessionMac::addDefaultDeviceObserverIfNeeded() const
{
    if (m_hasDefaultDeviceObserver)
        return;
    m_hasDefaultDeviceObserver = true;

    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &defaultOutputDeviceAddress(), handleDefaultDeviceChange, const_cast<AudioSessionMac*>(this));
}

const AudioObjectPropertyAddress& AudioSessionMac::nominalSampleRateAddress()
{
    static const AudioObjectPropertyAddress nominalSampleRateAddress = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    return nominalSampleRateAddress;
}

void AudioSessionMac::addSampleRateObserverIfNeeded() const
{
    if (m_hasSampleRateObserver)
        return;
    m_hasSampleRateObserver = true;

    AudioObjectAddPropertyListener(defaultDevice(), &nominalSampleRateAddress(), handleSampleRateChange, const_cast<AudioSessionMac*>(this));
}

OSStatus AudioSessionMac::handleSampleRateChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData)
{
    ASSERT(inClientData);
    if (!inClientData)
        return noErr;

    auto* session = static_cast<AudioSessionMac*>(inClientData);
    callOnMainThread([session] {
        session->handleSampleRateChange();
    });
    return noErr;
}

void AudioSessionMac::handleSampleRateChange() const
{
    auto newSampleRate = sampleRateWithoutCaching();
    if (m_sampleRate == newSampleRate)
        return;

    m_sampleRate = newSampleRate;
    m_configurationChangeObservers.forEach([this](auto& observer) {
        observer.sampleRateDidChange(*this);
    });
}

const AudioObjectPropertyAddress& AudioSessionMac::bufferSizeAddress()
{
    static const AudioObjectPropertyAddress bufferSizeAddress = {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    return bufferSizeAddress;
}

void AudioSessionMac::addBufferSizeObserverIfNeeded() const
{
    if (m_hasBufferSizeObserver)
        return;
    m_hasBufferSizeObserver = true;

    AudioObjectAddPropertyListener(defaultDevice(), &bufferSizeAddress(), handleBufferSizeChange, const_cast<AudioSessionMac*>(this));
}

OSStatus AudioSessionMac::handleBufferSizeChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData)
{
    ASSERT(inClientData);
    if (!inClientData)
        return noErr;

    auto* session = static_cast<AudioSessionMac*>(inClientData);
    callOnMainThread([session] {
        session->handleBufferSizeChange();
    });
    return noErr;
}

void AudioSessionMac::handleBufferSizeChange() const
{
    auto newBufferSize = bufferSizeWithoutCaching();
    if (!newBufferSize)
        return;
    if (m_bufferSize == newBufferSize)
        return;

    m_bufferSize = newBufferSize;
    m_configurationChangeObservers.forEach([this](auto& observer) {
        observer.bufferSizeDidChange(*this);
    });
}

void AudioSessionMac::audioOutputDeviceChanged()
{
#if ENABLE(ROUTING_ARBITRATION)
    if (!m_playingToBluetooth || *m_playingToBluetooth == defaultDeviceTransportIsBluetooth())
        return;

    m_playingToBluetooth = std::nullopt;
#endif
}

void AudioSessionMac::setIsPlayingToBluetoothOverride(std::optional<bool> value)
{
#if ENABLE(ROUTING_ARBITRATION)
    isPlayingToBluetoothOverride = value;
#else
    UNUSED_PARAM(value);
#endif
}

void AudioSessionMac::setCategory(CategoryType category, RouteSharingPolicy policy)
{
#if ENABLE(ROUTING_ARBITRATION)
    bool playingToBluetooth = defaultDeviceTransportIsBluetooth();
    if (category == m_category && m_playingToBluetooth && *m_playingToBluetooth == playingToBluetooth)
        return;

    m_category = category;
    m_policy = policy;

    if (m_setupArbitrationOngoing) {
        RELEASE_LOG_ERROR(Media, "AudioSessionMac::setCategory() - a beginArbitrationWithCategory is still ongoing");
        return;
    }

    if (!m_routingArbitrationClient)
        return;

    if (m_inRoutingArbitration) {
        m_inRoutingArbitration = false;
        m_routingArbitrationClient->leaveRoutingAbritration();
    }

    if (category == CategoryType::AmbientSound || category == CategoryType::SoloAmbientSound || category == CategoryType::AudioProcessing || category == CategoryType::None)
        return;

    using RoutingArbitrationError = AudioSessionRoutingArbitrationClient::RoutingArbitrationError;
    using DefaultRouteChanged = AudioSessionRoutingArbitrationClient::DefaultRouteChanged;

    m_playingToBluetooth = playingToBluetooth;
    m_setupArbitrationOngoing = true;
    m_routingArbitrationClient->beginRoutingArbitrationWithCategory(m_category, [this] (RoutingArbitrationError error, DefaultRouteChanged defaultRouteChanged) {
        m_setupArbitrationOngoing = false;
        if (error != RoutingArbitrationError::None) {
            RELEASE_LOG_ERROR(Media, "AudioSessionMac::setCategory() - beginArbitrationWithCategory:%s failed with error %s", convertEnumerationToString(m_category).ascii().data(), convertEnumerationToString(error).ascii().data());
            return;
        }

        m_inRoutingArbitration = true;

        // FIXME: Do we need to reset sample rate and buffer size for the new default device?
        if (defaultRouteChanged == DefaultRouteChanged::Yes)
            LOG(Media, "AudioSessionMac::setCategory() - defaultRouteChanged!");
    });
#else
    m_category = category;
    m_policy = policy;
#endif
}

float AudioSessionMac::sampleRate() const
{
    if (!m_sampleRate) {
        addSampleRateObserverIfNeeded();
        handleSampleRateChange();
        ASSERT(m_sampleRate);
    }
    return *m_sampleRate;
}

float AudioSessionMac::sampleRateWithoutCaching() const
{
    Float64 nominalSampleRate;
    UInt32 nominalSampleRateSize = sizeof(Float64);

    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &nominalSampleRateAddress(), 0, 0, &nominalSampleRateSize, (void*)&nominalSampleRate);
    if (result != noErr) {
        RELEASE_LOG_ERROR(Media, "AudioSessionMac::sampleRate() - AudioObjectGetPropertyData() failed with error %d", result);
        return 44100;
    }

    auto sampleRate = narrowPrecisionToFloat(nominalSampleRate);
    if (!sampleRate) {
        RELEASE_LOG_ERROR(Media, "AudioSessionMac::sampleRate() - AudioObjectGetPropertyData() return an invalid sample rate");
        return 44100;
    }
    return sampleRate;
}

size_t AudioSessionMac::bufferSize() const
{
    if (m_bufferSize)
        return *m_bufferSize;

    addBufferSizeObserverIfNeeded();

    m_bufferSize = bufferSizeWithoutCaching();
    return m_bufferSize.value_or(0);
}

std::optional<size_t> AudioSessionMac::bufferSizeWithoutCaching() const
{
    UInt32 bufferSize;
    UInt32 bufferSizeSize = sizeof(bufferSize);
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &bufferSizeAddress(), 0, 0, &bufferSizeSize, &bufferSize);
    if (result)
        return std::nullopt;

    return bufferSize;
}

AudioDeviceID AudioSessionMac::defaultDevice() const
{
    if (!m_defaultDevice) {
        m_defaultDevice = defaultDeviceWithoutCaching();
        addDefaultDeviceObserverIfNeeded();
    }
    return *m_defaultDevice;
}

size_t AudioSessionMac::numberOfOutputChannels() const
{
    notImplemented();
    return 0;
}

size_t AudioSessionMac::maximumNumberOfOutputChannels() const
{
    AudioObjectPropertyAddress sizeAddress = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioObjectPropertyScopeOutput,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
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

bool AudioSessionMac::tryToSetActiveInternal(bool)
{
    notImplemented();
    return true;
}

String AudioSessionMac::routingContextUID() const
{
    return emptyString();
}

size_t AudioSessionMac::preferredBufferSize() const
{
    return bufferSize();
}

void AudioSessionMac::setPreferredBufferSize(size_t bufferSize)
{
    if (m_bufferSize == bufferSize)
        return;

    AudioValueRange bufferSizeRange = {0, 0};
    UInt32 bufferSizeRangeSize = sizeof(AudioValueRange);
    AudioObjectPropertyAddress bufferSizeRangeAddress = {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &bufferSizeRangeAddress, 0, 0, &bufferSizeRangeSize, &bufferSizeRange);
    if (result)
        return;

    size_t minBufferSize = static_cast<size_t>(bufferSizeRange.mMinimum);
    size_t maxBufferSize = static_cast<size_t>(bufferSizeRange.mMaximum);
    UInt32 bufferSizeOut = std::min(maxBufferSize, std::max(minBufferSize, bufferSize));

    result = AudioObjectSetPropertyData(defaultDevice(), &bufferSizeAddress(), 0, 0, sizeof(bufferSizeOut), (void*)&bufferSizeOut);

    if (!result) {
        m_bufferSize = bufferSizeOut;
        m_configurationChangeObservers.forEach([this](auto& observer) {
            observer.bufferSizeDidChange(*this);
        });
    }

#if !LOG_DISABLED
    if (result)
        LOG(Media, "AudioSessionMac::setPreferredBufferSize(%zu) - failed with error %d", bufferSize, static_cast<int>(result));
    else
        LOG(Media, "AudioSessionMac::setPreferredBufferSize(%zu)", bufferSize);
#endif
}

bool AudioSessionMac::isMuted() const
{
    UInt32 mute = 0;
    UInt32 muteSize = sizeof(mute);
    AudioObjectPropertyAddress muteAddress = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
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

void AudioSessionMac::handleMutedStateChange()
{
    bool isCurrentlyMuted = isMuted();
    if (m_lastMutedState && *m_lastMutedState == isCurrentlyMuted)
        return;

    m_lastMutedState = isCurrentlyMuted;

    m_configurationChangeObservers.forEach([this](auto& observer) {
        observer.hardwareMutedStateDidChange(*this);
    });
}

const AudioObjectPropertyAddress& AudioSessionMac::muteAddress()
{
    static const AudioObjectPropertyAddress muteAddress = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    return muteAddress;
}

void AudioSessionMac::addConfigurationChangeObserver(ConfigurationChangeObserver& observer)
{
    m_configurationChangeObservers.add(observer);

    if (m_configurationChangeObservers.computeSize() > 1)
        return;

    addMuteChangeObserverIfNeeded();
}

void AudioSessionMac::removeConfigurationChangeObserver(ConfigurationChangeObserver& observer)
{
    if (m_configurationChangeObservers.computeSize() == 1)
        removeMuteChangeObserverIfNeeded();

    m_configurationChangeObservers.remove(observer);
}

void AudioSessionMac::addMuteChangeObserverIfNeeded() const
{
    if (m_hasMuteChangeObserver)
        return;

    AudioObjectAddPropertyListener(defaultDevice(), &muteAddress(), handleMutePropertyChange, const_cast<AudioSessionMac*>(this));
    m_hasMuteChangeObserver = true;
}

void AudioSessionMac::removeMuteChangeObserverIfNeeded() const
{
    if (!m_hasMuteChangeObserver)
        return;

    AudioObjectRemovePropertyListener(defaultDevice(), &muteAddress(), handleMutePropertyChange, const_cast<AudioSessionMac*>(this));
    m_hasMuteChangeObserver = false;
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(MAC)
