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

static AudioDeviceID defaultDevice()
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
    if (AudioObjectGetPropertyData(defaultDevice(), &audioDeviceTransportTypeProperty, 0, 0, &transportSize, &transportType))
        return false;

    return transportType == kAudioDeviceTransportTypeBluetooth || transportType == kAudioDeviceTransportTypeBluetoothLE;
}
#endif

void AudioSessionMac::addSampleRateObserverIfNeeded() const
{
    if (m_hasSampleRateObserver)
        return;
    m_hasSampleRateObserver = true;

    AudioObjectPropertyAddress nominalSampleRateAddress = {
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
    AudioObjectAddPropertyListener(defaultDevice(), &nominalSampleRateAddress, handleSampleRateChange, const_cast<AudioSessionMac*>(this));
}

OSStatus AudioSessionMac::handleSampleRateChange(AudioObjectID device, UInt32, const AudioObjectPropertyAddress* sampleRateAddress, void* inClientData)
{
    ASSERT(inClientData);
    if (!inClientData)
        return noErr;

    auto* session = static_cast<AudioSessionMac*>(inClientData);

    Float64 nominalSampleRate;
    UInt32 nominalSampleRateSize = sizeof(Float64);
    OSStatus result = AudioObjectGetPropertyData(device, sampleRateAddress, 0, 0, &nominalSampleRateSize, (void*)&nominalSampleRate);
    if (result)
        return result;

    session->m_sampleRate = narrowPrecisionToFloat(nominalSampleRate);

    callOnMainThread([session] {
        session->handleSampleRateChange();
    });

    return noErr;
}

void AudioSessionMac::handleSampleRateChange() const
{
    m_configurationChangeObservers.forEach([this](auto& observer) {
        observer.sampleRateDidChange(*this);
    });
}

void AudioSessionMac::addBufferSizeObserverIfNeeded() const
{
    if (m_hasBufferSizeObserver)
        return;
    m_hasBufferSizeObserver = true;

    AudioObjectPropertyAddress bufferSizeAddress = {
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
    AudioObjectAddPropertyListener(defaultDevice(), &bufferSizeAddress, handleBufferSizeChange, const_cast<AudioSessionMac*>(this));
}

OSStatus AudioSessionMac::handleBufferSizeChange(AudioObjectID device, UInt32, const AudioObjectPropertyAddress* bufferSizeAddress, void* inClientData)
{
    ASSERT(inClientData);
    if (!inClientData)
        return noErr;

    auto* session = static_cast<AudioSessionMac*>(inClientData);

    UInt32 bufferSize;
    UInt32 bufferSizeSize = sizeof(bufferSize);
    OSStatus result = AudioObjectGetPropertyData(device, bufferSizeAddress, 0, 0, &bufferSizeSize, &bufferSize);
    if (result)
        return result;

    session->m_bufferSize = bufferSize;

    callOnMainThread([session] {
        session->handleBufferSizeChange();
    });

    return noErr;
}

void AudioSessionMac::handleBufferSizeChange() const
{
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

void AudioSessionMac::setCategory(CategoryType category, RouteSharingPolicy)
{
#if ENABLE(ROUTING_ARBITRATION)
    bool playingToBluetooth = defaultDeviceTransportIsBluetooth();
    if (category == m_category && m_playingToBluetooth && *m_playingToBluetooth == playingToBluetooth)
        return;

    m_category = category;

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
#endif
}

void AudioSessionMac::setCategoryOverride(CategoryType category)
{
    if (m_categoryOverride == category)
        return;

    m_categoryOverride = category;
    setCategory(category, RouteSharingPolicy::Default);
}

float AudioSessionMac::sampleRate() const
{
    if (!m_sampleRate) {
        addSampleRateObserverIfNeeded();

        Float64 nominalSampleRate;
        UInt32 nominalSampleRateSize = sizeof(Float64);

        AudioObjectPropertyAddress nominalSampleRateAddress = {
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
        OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &nominalSampleRateAddress, 0, 0, &nominalSampleRateSize, (void*)&nominalSampleRate);
        if (result != noErr) {
            RELEASE_LOG_ERROR(Media, "AudioSessionMac::sampleRate() - AudioObjectGetPropertyData() failed with error %d", result);
            return 44100;
        }

        m_sampleRate = narrowPrecisionToFloat(nominalSampleRate);
        if (!*m_sampleRate) {
            RELEASE_LOG_ERROR(Media, "AudioSessionMac::sampleRate() - AudioObjectGetPropertyData() return an invalid sample rate");
            m_sampleRate = 44100;
        }

        handleSampleRateChange();
    }
    return *m_sampleRate;
}

size_t AudioSessionMac::bufferSize() const
{
    if (m_bufferSize)
        return *m_bufferSize;

    addBufferSizeObserverIfNeeded();

    UInt32 bufferSize;
    UInt32 bufferSizeSize = sizeof(bufferSize);

    AudioObjectPropertyAddress bufferSizeAddress = {
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
    OSStatus result = AudioObjectGetPropertyData(defaultDevice(), &bufferSizeAddress, 0, 0, &bufferSizeSize, &bufferSize);

    if (result)
        return 0;

    m_bufferSize = bufferSize;

    return bufferSize;
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

RouteSharingPolicy AudioSessionMac::routeSharingPolicy() const
{
    return RouteSharingPolicy::Default;
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

    AudioObjectPropertyAddress preferredBufferSizeAddress = {
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

    result = AudioObjectSetPropertyData(defaultDevice(), &preferredBufferSizeAddress, 0, 0, sizeof(bufferSizeOut), (void*)&bufferSizeOut);

    if (!result) {
        m_bufferSize = bufferSizeOut;
        handleBufferSizeChange();
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

void AudioSessionMac::addConfigurationChangeObserver(ConfigurationChangeObserver& observer)
{
    m_configurationChangeObservers.add(observer);

    if (m_configurationChangeObservers.computeSize() > 1)
        return;

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
    AudioObjectAddPropertyListener(defaultDevice(), &muteAddress, handleMutePropertyChange, this);
}

void AudioSessionMac::removeConfigurationChangeObserver(ConfigurationChangeObserver& observer)
{
    if (m_configurationChangeObservers.computeSize() == 1) {
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
        AudioObjectRemovePropertyListener(defaultDevice(), &muteAddress, handleMutePropertyChange, this);
    }

    m_configurationChangeObservers.remove(observer);
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(MAC)
